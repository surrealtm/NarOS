#include "terminal.h"
#include "input.h"
#include "ctrl.h"
#include "display.h"
#include "terminal/text_input.h"

#define BACKLOG_WIDTH 80
#define BACKLOG_HEIGHT 25

typedef struct Cell {
    char character;
    OS_Display_Color color;
} Cell;

typedef struct Terminal {
    Cell backlog[BACKLOG_WIDTH * BACKLOG_HEIGHT];
    u32 backlog_cursor_x;
    u32 backlog_cursor_y;
    Text_Input text_input;
} Terminal;

static
Cell query_cell(const Terminal *terminal, u32 x, u32 y) {
    const u32 idx = (y * BACKLOG_WIDTH) + x;
    return terminal->backlog[idx];
}

static
void scroll_one_line(Terminal *terminal) {
    const u32 start_of_first_line  = 0;
    const u32 start_of_second_line = BACKLOG_WIDTH;
    const u32 start_of_last_line   = BACKLOG_WIDTH * (BACKLOG_HEIGHT - 1);
    const u32 one_plus_end_of_last_line = BACKLOG_HEIGHT * BACKLOG_WIDTH;
    move_memory(&terminal->backlog[start_of_first_line], &terminal->backlog[start_of_second_line], (one_plus_end_of_last_line - start_of_second_line) * sizeof(Cell));
    set_memory(&terminal->backlog[start_of_last_line], 0, (one_plus_end_of_last_line - start_of_last_line) * sizeof(Cell));
}

static
void advance_cursor_vertically(Terminal *terminal) {
    if(terminal->backlog_cursor_y < BACKLOG_HEIGHT - 1) {
        ++terminal->backlog_cursor_y;
    } else {
        scroll_one_line(terminal);
    }
}

static
void advance_cursor_horizontally(Terminal *terminal) {
    if(terminal->backlog_cursor_x < BACKLOG_WIDTH - 1) {
        ++terminal->backlog_cursor_x;
    } else {
        advance_cursor_vertically(terminal);
        terminal->backlog_cursor_x = 0;
    }
}

static
void print_character(Terminal *terminal, const char character, const OS_Display_Color color) {
    if(character == '\r') {
        terminal->backlog_cursor_x = 0;
    } else if(character == '\n') {
        advance_cursor_vertically(terminal);
        terminal->backlog_cursor_x = 0;
    } else {
        const u32 idx = (terminal->backlog_cursor_y * BACKLOG_WIDTH) + terminal->backlog_cursor_x;
        terminal->backlog[idx] = (Cell) { character, color };
        advance_cursor_horizontally(terminal);
    }
}

static
void print_string(Terminal *terminal, const char *text, const OS_Display_Color color) {
    while(*text) {
        print_character(terminal, *text, color);
        ++text;
    }
}

static
u32 calculate_required_digits_for_unsigned(u64 integer) {
    if(integer == 0) return 1;

    u32 count = 0;
    while(integer > 0) {
        integer /= 10;
        ++count;
    }

    return count;
}

static
u64 radix_power(const u64 index) {
    u64 power = 1;
    for(u32 i = 0; i < index; ++i) power *= 10;
    return power;
}

static
void print_unsigned_integer(Terminal *terminal, u64 integer, const OS_Display_Color color) {
    const u32 number_of_digits = calculate_required_digits_for_unsigned(integer);
    s64 index = number_of_digits - 1;

    while(index >= 0) {
        u64 power = radix_power(index);
        u64 digit = integer / power;
        print_character(terminal, digit + '0', color);
        integer -= digit * power;
        --index;
    }
}

static
void blit_to_screen(const Terminal *terminal) {
    // @Speed: We could just memcpy this into the video memory...
    for(u32 y = 0; y < BACKLOG_HEIGHT; ++y) {
        for(u32 x = 0; x < BACKLOG_WIDTH; ++x) {
            const Cell cell = query_cell(terminal, x, y);
            os_display_set_character(x, y, cell.character, cell.color);
        }
    }

    os_display_set_character(terminal->backlog_cursor_x, terminal->backlog_cursor_y, ' ', OS_DISPLAY_White);
    os_display_set_cursor_position(terminal->backlog_cursor_x, terminal->backlog_cursor_y);
}

static
void wait_for_input(void) {
    while(!os_input_has_event()) {
        os_ctrl_halt();
    }
    OS_Input_Event event;
    os_input_pop_event(&event);
}

void terminal_enter(void) {
    Terminal terminal = { 0 };

    (void) print_unsigned_integer;

    os_display_clear(' ', OS_DISPLAY_White);
    print_string(&terminal, "Hello World\nThis is the second line!", OS_DISPLAY_White);
    print_string(&terminal, " This is the continuation of the second line!\n", OS_DISPLAY_White);
    print_string(&terminal, "And this is a really important message!\n", OS_DISPLAY_Red);

    while(true) {
        blit_to_screen(&terminal);
        text_input_update(&terminal.text_input);
        wait_for_input();
    }
}
