#include "interrupt.h"
#include "port/port.h"

#define PIT_HZ 1193180LL
#define TICKS_PER_SECOND 1000LL
#define NANOSECONDS_PER_SECONDS 1000000000LL
#define INTERRUPT_DESCRIPTOR_COUNT 256
#define INTERRUPT_CALLBACK_CAPACITY 4

typedef struct Interrupt_Descriptor_Table_Entry {
    u16 base_lo;
    u16 segment;
    u8 padding;
    u8 flags;
    u16 base_hi;
} PACKED_STRUCT Interrupt_Descriptor_Table_Entry;

typedef struct Interrupt_Descriptor_Table_Pointer {
    u16 limit;
    u32 base;
} PACKED_STRUCT Interrupt_Descriptor_Table_Pointer;

typedef struct Interrupt_Register_State {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 signal, error_code;
    u32 eip, cs, eflags, useresp, ss;
} Interrupt_Register_State;

typedef struct Interrupt_Callback_List {
    Interrupt_Callback callbacks[INTERRUPT_CALLBACK_CAPACITY];
} Interrupt_Callback_List;

static Interrupt_Descriptor_Table_Entry interrupt_descriptor_table[INTERRUPT_DESCRIPTOR_COUNT];
static Interrupt_Descriptor_Table_Pointer interrupt_descriptor_table_pointer;
static Interrupt_Callback_List interrupt_callback_lists[INTERRUPT_DESCRIPTOR_COUNT];

static volatile u64 tick_counter = 0; // This is modified by an interrupt handler, which confuses the optimizer when used in loops

extern void interrupt_dummy_master(void);
extern void interrupt_dummy_slave(void);

static
void install_interrupt_descriptor(const u8 signal, const u32 base, const u16 segment, const u8 flags) {
    interrupt_descriptor_table[signal] = (Interrupt_Descriptor_Table_Entry) { ((base >> 0) & 0xffff), segment, 0, flags, ((base >> 16) & 0xffff) };
}

static
b8 has_interrupt_handler(const u8 signal) {
    const u32 handler = (((u32) interrupt_descriptor_table[signal].base_hi << 16) | interrupt_descriptor_table[signal].base_lo);
    return handler != (u32) interrupt_dummy_master && handler != (u32) interrupt_dummy_slave;
}

static
void remap_interrupt_handlers(void) {
    // In protected mode, IDT entry 8 is a double fault. Without remapping, every time IRQ0 fires, we would get
    // a double fault exception, which is *not* actually what's happening.
    // Therefore, we tell the interrupt controllers to remap IRQ0 to IDT entries 32 to 47
    port_write_u8(0x20, 0x11);
    port_write_u8(0xa0, 0x11);
    port_write_u8(0x21, 0x20);
    port_write_u8(0xa1, 0x28);
    port_write_u8(0x21, 0x04);
    port_write_u8(0xa1, 0x02);
    port_write_u8(0x21, 0x01);
    port_write_u8(0xa1, 0x01);
    port_write_u8(0x21, 0x00);
    port_write_u8(0xa1, 0x00);
}

static
void setup_interrupt_descriptor_table(void) {
    const u16 segment = 0x8; // This is the offset in bytes into the bootloader's GDT that points at the code segment descriptor
    const u8 flags = 0x8e;

    interrupt_descriptor_table_pointer.limit = sizeof(interrupt_descriptor_table) - 1;
    interrupt_descriptor_table_pointer.base  = (u32) &interrupt_descriptor_table;

    // Every interrupt descriptor needs to have a valid entry, otherwise the CPU will jump to an invalid memory
    // address. For signals that we don't care about, we'll set up a dummy interrupt which should be less overhead.
    const s16 last_master_signal = 39;
    for(s16 signal = 0; signal <= last_master_signal; ++signal) {
        install_interrupt_descriptor(signal, (u32) interrupt_dummy_master, segment, flags);
    }
    const s16 last_slave_signal = (s16) ARRAY_COUNT(interrupt_descriptor_table) - 1;
    for(s16 signal = last_master_signal + 1; signal <= last_slave_signal; ++signal) {
        install_interrupt_descriptor(signal, (u32) interrupt_dummy_slave, segment, flags);
    }

    // Install the interrupt handlers we actually care about. Only these will result in a call to `interrupt_handler`.
#define install(id) extern void interrupt_##id(void); install_interrupt_descriptor(0x##id, (u32) interrupt_##id, segment, flags)

    install(20);
    install(21);
    install(22);
    install(23);
    install(24);
    install(25);
    install(26);
    install(27);
    install(28);
    install(29);
    install(2a);
    install(2b);
    install(2c);
    install(2d);
    install(2e);
    install(2f);

#undef install
}

static
void tick_handler(void) {
    ++tick_counter;
}

static
void initialize_tick_counter(void) {
    const int divisor = PIT_HZ / TICKS_PER_SECOND;
    port_write_u8(0x43, 0x36);
    port_write_u8(0x40, divisor & 0xff);
    port_write_u8(0x40, divisor >> 8);
    interrupt_register_callback(INTERRUPT_SIGNAL_Timer, tick_handler);
}

/**
 * Called from the assembly interrupt routines.
 * Prevent the compiler from optimizing out this entire function.
 */
void interrupt_handler(const volatile Interrupt_Register_State *state) {
    if (state->signal < ARRAY_COUNT(interrupt_callback_lists)) {
        const Interrupt_Callback_List *list = &interrupt_callback_lists[state->signal];
        for (u32 callback_idx = 0; callback_idx < ARRAY_COUNT(list->callbacks); ++callback_idx) {
            if(list->callbacks[callback_idx]) list->callbacks[callback_idx]();
        }
    }

    if (state->signal >= 40) {
        // If the signal is greater than or equal to 40, then we need to send an EOI to
        // the slave interrupt controller
        port_write_u8(0xa0, 0x20);
    }

    // Send an EOI to the master interrupt controller
    port_write_u8(0x20, 0x20);
}

void interrupt_initialize(void) {
    remap_interrupt_handlers();
    setup_interrupt_descriptor_table();
    __asm__ volatile ("lidt %0" :: "m"(interrupt_descriptor_table_pointer));
    __asm__ volatile ("sti");
    initialize_tick_counter();
}

void interrupt_register_callback(const u32 signal, const Interrupt_Callback callback) {
    if (signal >= ARRAY_COUNT(interrupt_callback_lists)) {
        return;
    }

    sanity_check(has_interrupt_handler(signal), "Tried to register a callback for an unhandled signal.");

    Interrupt_Callback_List *list = &interrupt_callback_lists[signal];
    for (u32 callback_idx = 0; callback_idx < ARRAY_COUNT(list->callbacks); ++callback_idx) {
        if (list->callbacks[callback_idx] == null) {
            list->callbacks[callback_idx] = callback;
            return;
        }
    }

    panic(str8("Couldn't register another interrupt callback on this signal, reached the maximum capacity!"));
}

u64 interrupt_get_tick(void) {
    return tick_counter;
}

u64 interrupt_ticks_from_nanoseconds(const u64 nanoseconds) {
    return (nanoseconds * TICKS_PER_SECOND) / NANOSECONDS_PER_SECONDS;
}

u64 interrupt_nanoseconds_from_ticks(const u64 ticks) {
    return (ticks / TICKS_PER_SECOND) * NANOSECONDS_PER_SECONDS;
}
