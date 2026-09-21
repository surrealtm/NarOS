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

typedef struct Interrupt_Register_State {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 signal, error_code;
    u32 eip, cs, eflags, useresp, ss;
} Interrupt_Register_State;

static Interrupt_Descriptor_Table_Entry interrupt_descriptor_table[INTERRUPT_DESCRIPTOR_COUNT];
static Interrupt_Descriptor_Table_Pointer interrupt_descriptor_table_pointer;

extern void interrupt_0(void);
extern void interrupt_1(void);
extern void interrupt_2(void);

static
void install_interrupt_descriptor(const u8 signal, const u32 base, const u16 segment, const u8 flags) {
    interrupt_descriptor_table[signal] = (Interrupt_Descriptor_Table_Entry) { ((base >> 0) & 0xffff), segment, 0, flags, ((base >> 16) & 0xffff) };
}

extern
void interrupt_handler(const Interrupt_Register_State *state) {
    (void) state;
}

void write_output_port(const u16 port, const u8 value) {
    __asm__ volatile ("outb %0, %1" :: "a" (value), "Nd" (port));
}

void initialize_interrupt_handlers(void) {
    interrupt_descriptor_table_pointer.limit = sizeof(interrupt_descriptor_table) - 1;
    interrupt_descriptor_table_pointer.base  = (u32) &interrupt_descriptor_table;
    set_memory(interrupt_descriptor_table, 0, sizeof(interrupt_descriptor_table));

    const u16 segment = 0x8;
    const u8 flags = 0x8e;
    install_interrupt_descriptor(0x00, (u32) interrupt_0, segment, flags);
    install_interrupt_descriptor(0x0e, (u32) interrupt_1, segment, flags);
    install_interrupt_descriptor(0x20, (u32) interrupt_2, segment, flags);
    __asm__ volatile ("lidt %0" :: "m"(interrupt_descriptor_table_pointer));
    __asm__ volatile ("sti");
}

