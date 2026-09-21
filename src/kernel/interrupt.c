#define INTERRUPT_DESCRIPTOR_COUNT 256

typedef struct Interrupt_Descriptor_Table_Entry {
    u16 base_lo;
    u16 segment;
    u8 padding;
    u8 flags;
    u16 base_hi;
} packed_struct Interrupt_Descriptor_Table_Entry;

typedef struct Interrupt_Descriptor_Table_Pointer {
    u16 limit;
    u32 base;
} packed_struct Interrupt_Descriptor_Table_Pointer;

static Interrupt_Descriptor_Table_Entry interrupt_descriptor_table[INTERRUPT_DESCRIPTOR_COUNT];
static Interrupt_Descriptor_Table_Pointer interrupt_descriptor_table_pointer;

static
void load_interrupt_descriptor_table(void) {
    interrupt_descriptor_table_pointer.limit = sizeof(interrupt_descriptor_table) - 1;
    interrupt_descriptor_table_pointer.base  = *(u32 *) &interrupt_descriptor_table_pointer;
    set_memory(interrupt_descriptor_table, 0, sizeof(interrupt_descriptor_table));
    __asm__ volatile ("lidt %0" :: "m"(interrupt_descriptor_table_pointer));
}

void write_output_port(const u8 port, const u8 value) {
    __asm__ volatile ("outb %1, %0" :: "dN" (port), "a" (value));
}

