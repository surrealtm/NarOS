#include "acpi.h"
#include "ctrl.h"
#include "common/common.h"
#include "interrupt/interrupt.h"

typedef struct Fixed_ACPI_Description_Table {
    s8 signature[4];
    u32 length;
    s8 padding_0[40 - 8];
    void *dsdt;
    s8 padding_1[48 - 44];
    void *smi_cmd;
    s8 acpi_enable;
    s8 acpi_disable;
    s8 padding_2[64 - 54];
    void *pm1a_cnt_blk;
    void *pm1b_cnt_blk;
    s8 padding_3[89 - 72];
    s8 pm1_cnt_len;
} Fixed_ACPI_Description_Table;

typedef struct System_Description_Table {
    s8 signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    s8 oem_id[6];
    s8 oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} PACKED_STRUCT System_Description_Table;

typedef struct Root_System_Description_Table {
    System_Description_Table header;
    void *entries[/*(header.Length - sizeof(header)) / sizeof(void*)*/];
} PACKED_STRUCT Root_System_Description_Table;

typedef struct Root_System_Description_Pointer {
    s8 signature[8];
    s8 checksum;
    s8 oem_id[6];
    s8 revision;
    Root_System_Description_Table *rsdt_address;
} Root_System_Description_Pointer;

/**
 * Information extracted during initialization, used for issuing commands during run-time.
 */
typedef struct ACPI_Commands {
    u32 *smi_cmd;
    u8 acpi_enable;
    u8 acpi_disable;
    u32 *pm1a_cnt;
    u32 *pm1b_cnt;
    s16 slp_typa;
    s16 slp_typb;
    s16 slp_en;
    s16 sci_en;
    s8 pm1_cnt_len;
} ACPI_Data;

ACPI_Data data;

/**
 * GCC doesn't allow reading from hex literals as pointer addresses directly, so implement a wrapper function for
 * loading the value at runtime.
 */
static
u16 read_physical_u16(const u32 physical_address) {
    u16 value;
    __asm__ volatile("movw (%1), %0" : "=r"(value) : "r"(physical_address) : "memory");
    return value;
}

static
u32 *find_extended_bios_data_area_start(void) {
    return (u32 *) (((u32) read_physical_u16(0x40e)) << 4);
}

static
u32 *find_extended_bios_data_area_end(const u32 *start) {
    return (u32 *) (((u8 *) start) + 1023);
}

static
b8 validate_table_header(const void *table_header, u32 header_length, const char *expected_signature) {
    if(compare_memory(table_header, expected_signature, string_length(expected_signature)) != 0) {
        return false;
    }

    s8 calculated_checksum = 0;
    for(u32 i = 0; i < header_length; ++i) {
        calculated_checksum += *((u8 *) table_header + i);
    }

    if(calculated_checksum != 0) {
        return false;
    }

    return true;
}

static
const Fixed_ACPI_Description_Table *check_fixed_acpi_description_table(const Fixed_ACPI_Description_Table *facp) {
    if(!validate_table_header(facp, facp->length, "FACP")) {
        return null;
    }

    return facp;
}

static
const System_Description_Table *check_system_description_table(const System_Description_Table *sdt, const char *signature) {
    if(!validate_table_header(sdt, sdt->length, signature)) {
        return null;
    }

    return sdt;
}

static
const Root_System_Description_Table *check_root_system_description_table(const Root_System_Description_Pointer *pointer) {
    if(!validate_table_header(pointer, sizeof(*pointer), "RSD PTR ")) {
        return null;
    }

    const Root_System_Description_Table *table = pointer->rsdt_address;
    if(!validate_table_header(table, table->header.length, "RSDT")) {
        return null;
    }

    return pointer->rsdt_address;
}

static
const Root_System_Description_Table *find_root_system_description_table(void) {
    // Search the extended BIOS Data Area for the RSD
    const u32 *ebda_start = find_extended_bios_data_area_start();
    const u32 *ebda_end   = find_extended_bios_data_area_end(ebda_start);
    for(const u32 *address = ebda_start; address <= ebda_end; address += 0x10 / sizeof(address)) {
        const Root_System_Description_Table *rsd = check_root_system_description_table((Root_System_Description_Pointer *) address);
        if(rsd != null) {
            return rsd;
        }
    }

    // Search below the 1mb mark for the RSDP signature
    for(const u32 *address = (u32 *) 0x000e0000; address < (u32 *) 0x00100000; address += 0x10 / sizeof(address)) {
        const Root_System_Description_Table *table = check_root_system_description_table((Root_System_Description_Pointer *) address);
        if(table != null) {
            return table;
        }
    }

    return null;
}

static
b8 is_valid_aml_signature(const char *address) {
    if(*(address + 4) != 0x12) return false;
    if(*(address - 1) == 0x08) return true;
    if(*(address - 2) == 0x08 && *(address - 1) == '\\') return true;
    return false;
}

static
b8 wait_for_configured_port(const u32 *port) {
    for(int i = 0; i < 300; ++i) {
        if((read_input_port_u16((u16) (u32) port) & data.sci_en) == 1) {
            return true;
        }
        os_ctrl_sleep(10000000);
    }

    return false;
}

static
b8 enable_acpi(void) {
    if(data.smi_cmd == 0 || data.acpi_enable == 0) {
        return false;
    }

    write_output_port_u16((u16) (u32) data.smi_cmd, data.acpi_enable);
    return wait_for_configured_port(data.pm1a_cnt) && (data.pm1b_cnt == 0 || wait_for_configured_port(data.pm1b_cnt));
}

b8 acpi_initialize(void) {
    const Root_System_Description_Table *rsdt = find_root_system_description_table();
    if(rsdt == null) {
        return false;
    }

    const s32 entry_count = (rsdt->header.length - sizeof(System_Description_Table)) / sizeof(void*);
    for(s32 i = 0; i < entry_count; ++i) {
        const Fixed_ACPI_Description_Table *facp = check_fixed_acpi_description_table(rsdt->entries[i]);
        if(!facp) continue;

        // Ensure that the referenced SDT defines a "Differeniated System Description Table"
        const System_Description_Table *dsdt = check_system_description_table(facp->dsdt, "DSDT");
        if(!dsdt) continue;

        const char *s5_address = null;

        // Search for the S5 package in the DSDT
        {
            const u32 address_length = dsdt->length - sizeof(*dsdt);
            for(u32 i = 0; i < address_length; ++i) {
                const char *address = ((const char *) dsdt) + sizeof(*dsdt) + i;
                const char *signature = "_S5_";
                if(compare_memory(address, signature, string_length(signature)) == 0) {
                    s5_address = address;
                    break;
                }
            }
        }

        if(!s5_address || !is_valid_aml_signature(s5_address)) continue;

        // Extract the relevant information into persistent storage
        s5_address += 5;
        s5_address += ((*s5_address & 0xc0) >> 6) + 2; // Calculate PkgLength size

        if(*s5_address == 0x0a) ++s5_address; // Skip byteprefix
        data.slp_typa = *s5_address << 10;
        ++s5_address;

        if(*s5_address == 0x0a) ++s5_address; // Skip byteprefix
        data.slp_typb = *s5_address << 10;
        ++s5_address;

        data.smi_cmd = facp->smi_cmd;
        data.acpi_enable = facp->acpi_enable;
        data.acpi_disable = facp->acpi_disable;
        data.pm1a_cnt = facp->pm1a_cnt_blk;
        data.pm1b_cnt = facp->pm1b_cnt_blk;
        data.pm1_cnt_len = facp->pm1_cnt_len;
        data.slp_en = 1 << 13;
        data.sci_en = 1;
        break;
    }

    return data.sci_en != 0;
}

b8 acpi_shut_down(void) {
    if(data.sci_en == 0) {
        return false;
    }

    if(!enable_acpi()) {
        return false;
    }

    // Send the actual shutdown command
    write_output_port_u16((u16) (u32) data.pm1a_cnt, data.slp_typa | data.slp_en);
    if(data.pm1b_cnt != 0) {
        write_output_port_u16((u16) (u32) data.pm1b_cnt, data.slp_typb | data.slp_en);
    }

    // If we've gotten this far... It hasn't worked.
    return false;
}
