#include "terminal.h"
#include "input.h"
#include "ctrl.h"
#include "display.h"

#define BACKLOG_WIDTH 80
#define BACKLOG_HEIGHT 25

typedef struct Cell {
    char character;
    OS_Display_Color color;
} Cell;

typedef struct Terminal {
    Cell cells[BACKLOG_WIDTH * BACKLOG_HEIGHT];
} Terminal;

static
Cell query_cell(const Terminal *terminal, u32 x, u32 y) {
    const u32 idx = (y * BACKLOG_WIDTH) + x;
    return terminal->cells[idx];
}

static
void blit_to_screen(const Terminal *terminal) {
    for(u32 y = 0; y < BACKLOG_HEIGHT; ++y) {
        for(u32 x = 0; x < BACKLOG_WIDTH; ++x) {
            const Cell cell = query_cell(terminal, x, y);
            os_display_set_character(x, y, cell.character, cell.color);
        }
    }
}

static
void wait_for_input(void) {
    while(!os_input_has_event()) {
        os_ctrl_halt();
    }
}

void terminal_enter(void) {
    Terminal terminal = { 0 };

    while(true) {
        wait_for_input();
        blit_to_screen(&terminal);
    }
}
