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
static Interrupt_Callback interrupt_callbacks[INTERRUPT_DESCRIPTOR_COUNT];

extern void interrupt_00(void);
extern void interrupt_0e(void);
extern void interrupt_20(void);

static
void install_interrupt_descriptor(const u8 signal, const u32 base, const u16 segment, const u8 flags) {
    interrupt_descriptor_table[signal] = (Interrupt_Descriptor_Table_Entry) { ((base >> 0) & 0xffff), segment, 0, flags, ((base >> 16) & 0xffff) };
}

static
void remap_interrupt_handlers() {
    // In protected mode, IDT entry 8 is a double fault. Without remapping, every time IRQ0 fires, we would get
    // a double fault exception, which is *not* actually what's happening.
    // Therefore, we tell the interrupt controllers to remap IRQ0 to IDT entries 32 to 47
    write_output_port(0x20, 0x11);
    write_output_port(0xa0, 0x11);
    write_output_port(0x21, 0x20);
    write_output_port(0xa1, 0x28);
    write_output_port(0x21, 0x04);
    write_output_port(0xa1, 0x02);
    write_output_port(0x21, 0x01);
    write_output_port(0xa1, 0x01);
    write_output_port(0x21, 0x00);
    write_output_port(0xa1, 0x00);
}

extern
void interrupt_handler(const volatile Interrupt_Register_State *state) {
    if(state->signal < ARRAY_COUNT(interrupt_callbacks) && interrupt_callbacks[state->signal]) {
        interrupt_callbacks[state->signal]();
    }

    if(state->signal >= 40) {
        // If the signal is greater than or equal to 40, then we need to send an EOI to
        // the slave interrupt controller
        write_output_port(0xa0, 0x20);
    }

    // Send an EOI to the master interrupt controller
    write_output_port(0x20, 0x20);
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
    remap_interrupt_handlers();
    install_interrupt_descriptor(0x00, (u32) interrupt_00, segment, flags);
    // @Incomplete: Register a dummy signal for all unused interrupts, so that the CPU can call a valid address at least...
    for(s16 s = 0x01; s < (s16) ARRAY_COUNT(interrupt_descriptor_table); ++s) {
        install_interrupt_descriptor(s, (u32) interrupt_0e, segment, flags);
    }
    install_interrupt_descriptor(0x20, (u32) interrupt_20, segment, flags);
    __asm__ volatile ("lidt %0" :: "m"(interrupt_descriptor_table_pointer));
    __asm__ volatile ("sti");
}

void register_interrupt_callback(Interrupt_Signal signal, Interrupt_Callback callback) {
    if(signal < 0 || signal >= ARRAY_COUNT(interrupt_callbacks)) {
        return;
    }
    interrupt_callbacks[signal] = callback;
}
