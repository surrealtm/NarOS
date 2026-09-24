#include "interrupt.h"
#include "common/common.h"

/* ----------------------------------------------- Interrupts ----------------------------------------------- */

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

extern void interrupt_dummy_master(void);
extern void interrupt_dummy_slave(void);
extern void interrupt_20(void);

static
void install_interrupt_descriptor(const u8 signal, const u32 base, const u16 segment, const u8 flags) {
    interrupt_descriptor_table[signal] = (Interrupt_Descriptor_Table_Entry) { ((base >> 0) & 0xffff), segment, 0, flags, ((base >> 16) & 0xffff) };
}

static
void remap_interrupt_handlers(void) {
    // In protected mode, IDT entry 8 is a double fault. Without remapping, every time IRQ0 fires, we would get
    // a double fault exception, which is *not* actually what's happening.
    // Therefore, we tell the interrupt controllers to remap IRQ0 to IDT entries 32 to 47
    write_output_port_u8(0x20, 0x11);
    write_output_port_u8(0xa0, 0x11);
    write_output_port_u8(0x21, 0x20);
    write_output_port_u8(0xa1, 0x28);
    write_output_port_u8(0x21, 0x04);
    write_output_port_u8(0xa1, 0x02);
    write_output_port_u8(0x21, 0x01);
    write_output_port_u8(0xa1, 0x01);
    write_output_port_u8(0x21, 0x00);
    write_output_port_u8(0xa1, 0x00);
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
    install_interrupt_descriptor(0x20, (u32) interrupt_20, segment, flags);
}

/**
 * Called from the assembly interrupt routines.
 * Prevent the compiler from optimizing out this entire function.
 */
void interrupt_handler(const volatile Interrupt_Register_State *state) {
    if(state->signal < ARRAY_COUNT(interrupt_callbacks) && interrupt_callbacks[state->signal]) {
        interrupt_callbacks[state->signal]();
    }

    if(state->signal >= 40) {
        // If the signal is greater than or equal to 40, then we need to send an EOI to
        // the slave interrupt controller
        write_output_port_u8(0xa0, 0x20);
    }

    // Send an EOI to the master interrupt controller
    write_output_port_u8(0x20, 0x20);
}

void write_output_port_u8(const u16 port, const u8 value) {
    __asm__ volatile ("outb %0, %1" :: "a" (value), "Nd" (port));
}

void write_output_port_u16(const u16 port, const u16 value) {
    __asm__ volatile ("outw %0, %1" :: "a" (value), "Nd" (port));
}

u16 read_input_port_u16(const u16 port) {
    u16 value;
    __asm__ volatile ("inw %w1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

void initialize_interrupt_handlers(void) {
    remap_interrupt_handlers();
    setup_interrupt_descriptor_table();
    __asm__ volatile ("lidt %0" :: "m"(interrupt_descriptor_table_pointer));
    __asm__ volatile ("sti");
}

void register_interrupt_callback(Interrupt_Signal signal, Interrupt_Callback callback) {
    if(signal < 0 || signal >= ARRAY_COUNT(interrupt_callbacks)) {
        return;
    }
    interrupt_callbacks[signal] = callback;
}

/* ------------------------------------------------- Timing ------------------------------------------------- */

#define PIT_HZ 1193180LL
#define TICKS_PER_SECOND 1000LL
#define NANOSECONDS_TO_SECONDS 1000000000LL

volatile u64 tick_counter = 0; // This is modified by an interrupt handler, which confuses the optimizer when used in loops

static
void tick_handler(void) {
    ++tick_counter;
}

void initialize_tick_counter(void) {
    const int divisor = PIT_HZ / TICKS_PER_SECOND;
    write_output_port_u8(0x43, 0x36);
    write_output_port_u8(0x40, divisor & 0xff);
    write_output_port_u8(0x40, divisor >> 8);
    register_interrupt_callback(INTERRUPT_SIGNAL_Timer, tick_handler);
}

u64 current_tick_counter(void) {
    return tick_counter;
}

u64 ticks_from_nanoseconds(const u64 nanoseconds) {
    return (nanoseconds / NANOSECONDS_TO_SECONDS) * TICKS_PER_SECOND;
}
