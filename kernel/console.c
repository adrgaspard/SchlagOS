#include <stdint.h>
#include <stdbool.h>

#include "../shared/console.h"
#include "../shared/consts.h"
#include "../shared/debug.h"
#include "../shared/string.h"
#include "cpu.h"

typedef enum foreground_color_t
{
    F_BLACK = 0,
    F_DARK_BLUE = 1,
    F_DARK_GREEN = 2,
    F_CYAN = 3,
    F_DARK_RED = 4,
    F_PURPLE = 5,
    F_BROWN = 6,
    F_GRAY = 7,
    F_DARK_GRAY = 8,
    F_BLUE = 9,
    F_GREEN = 10,
    F_AQUA = 11,
    F_RED = 12,
    F_PINK = 13,
    F_YELLOW = 14,
    F_WHITE = 15
} foreground_color_t;

typedef enum background_color_t
{
    B_BLACK = 0,
    B_DARK_BLUE = 1,
    B_DARK_GREEN = 2,
    B_CYAN = 3,
    B_DARK_RED = 4,
    B_PURPLE = 5,
    B_BROWN = 6,
    B_GRAY = 7
} background_color_t;

typedef struct command_info_t {
    bool command_requested;
    uint8_t params_filled;
    unsigned char command_params[2];
} command_info_t;

typedef struct cursor_info_t {
    uint32_t cursor_line;
    uint32_t cursor_column;
    foreground_color_t last_foreground_color;
    background_color_t last_background_color;
    bool last_blink_state;
} cursor_info_t;

cursor_info_t cursor_info = { 0, 0, F_WHITE, B_BLACK, false };
const command_info_t empty_command_info = { .command_requested = false, .params_filled = 0, .command_params = {} };
command_info_t command_info;

static void treat_char(unsigned char c, foreground_color_t foreground_color, background_color_t background_color, bool blink);

static uint16_t* get_console_address(uint32_t line, uint32_t column)
{
    return (uint16_t *)(CONSOLE_BASE_ADDRESS + sizeof(uint16_t) * (line * CONSOLE_WIDTH + column));
}

static uint16_t get_cursor_position(uint32_t line, uint32_t column)
{
    return column + line * CONSOLE_WIDTH;
}

static void write_char(uint32_t line, uint32_t column, char c, foreground_color_t foreground_color, background_color_t background_color, bool blink)
{
    assert(line < CONSOLE_HEIGHT);
    assert(column < CONSOLE_WIDTH);
    uint16_t *address = get_console_address(line, column);
    uint16_t value = (uint16_t)(uint8_t)c | (blink << 15) | (background_color << 12) | (foreground_color << 8);
    *address = value;
    cursor_info.last_foreground_color = foreground_color;
    cursor_info.last_background_color = background_color;
    cursor_info.last_blink_state = blink;
}

static void set_cursor(uint32_t line, uint32_t column)
{
    assert(line < CONSOLE_HEIGHT);
    assert(column < CONSOLE_WIDTH);
    uint16_t position = get_cursor_position(line, column);
    outb(CURSOR_LOW_COMMAND, CONSOLE_COMMAND_PORT);
    outb((char)(uint8_t)position, CONSOLE_DATA_PORT);
    outb(CURSOR_HIGH_COMMAND, CONSOLE_COMMAND_PORT);
    outb((char)(uint8_t)(position >> 8), CONSOLE_DATA_PORT);
    cursor_info.cursor_line = line;
    cursor_info.cursor_column = column;
}

static void console_clear()
{
    foreground_color_t foreground_color = cursor_info.last_foreground_color;
    background_color_t background_color = cursor_info.last_background_color;
    bool blink = cursor_info.last_blink_state;
    for (uint8_t i = 0; i < CONSOLE_WIDTH; i++)
    {
        for (uint8_t j = 0; j < CONSOLE_HEIGHT; j++)
        {
            write_char(j, i, ' ', foreground_color, background_color, blink);
        }
    }
}

static void up_shift_console()
{
    for (uint32_t i = 1; i < CONSOLE_HEIGHT; i++)
    {
        memmove((void *)get_console_address(i - 1, 0), (void *)get_console_address(i, 0), CONSOLE_WIDTH * sizeof(uint16_t));
    }
    for (uint32_t j = 0; j < CONSOLE_WIDTH; j++)
    {
        write_char(CONSOLE_HEIGHT - 1, j, ' ', cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
    }
}

static void delete_last_char()
{
    if (cursor_info.cursor_column == 0)
    {
        if (cursor_info.cursor_line != 0)
        {
            set_cursor(cursor_info.cursor_line - 1, CONSOLE_WIDTH - 1);
            treat_char((unsigned char) ' ', cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
            set_cursor(cursor_info.cursor_line - 1, CONSOLE_WIDTH - 1);
        }
    }
    else
    {
        treat_char((unsigned char) '\b', cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
        treat_char((unsigned char) ' ', cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
        treat_char((unsigned char) '\b', cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
    }
}

static void treat_char(unsigned char c, foreground_color_t foreground_color, background_color_t background_color, bool blink)
{
    if (command_info.command_requested)
    {
        switch (command_info.params_filled)
        {
            case 0:
                switch (c)
                {
                case 'B':
                case 'b':
                case 'F':
                case 'f':
                    command_info.command_params[0] = c;
                    command_info.params_filled++;
                    return;
                case 'D':
                case 'd':
                    cursor_info.last_background_color = B_BLACK;
                    cursor_info.last_foreground_color = F_WHITE;
                    cursor_info.last_blink_state = false;
                    command_info = empty_command_info;
                    return;                    
                default:
                    command_info = empty_command_info;
                    return;
                }
            case 1:
                switch (command_info.command_params[0])
                {
                    case 'B':
                    case 'b':
                        switch (c)
                        {
                            case '0':
                            case '8':
                                cursor_info.last_background_color = B_BLACK;
                                break;
                            case '1':
                            case '9':
                                cursor_info.last_background_color = B_DARK_BLUE;
                                break;
                            case '2':
                            case 'A':
                            case 'a':
                                cursor_info.last_background_color = B_DARK_GREEN;
                                break;
                            case '3':
                            case 'B':
                            case 'b':
                                cursor_info.last_background_color = B_CYAN;
                                break;
                            case '4':
                            case 'C':
                            case 'c':
                                cursor_info.last_background_color = B_DARK_RED;
                                break;
                            case '5':
                            case 'D':
                            case 'd':
                                cursor_info.last_background_color = B_PURPLE;
                                break;
                            case '6':
                            case 'E':
                            case 'e':
                                cursor_info.last_background_color = B_BROWN;
                                break;
                            case '7':
                            case 'F':
                            case 'f':
                                cursor_info.last_background_color = B_GRAY;
                                break;
                        }
                        cursor_info.last_blink_state = c > '7';
                        command_info = empty_command_info;
                        return;
                    case 'F':
                    case 'f':
                        switch (c)
                        {
                            case '0':
                                cursor_info.last_foreground_color = F_BLACK;
                                break;
                            case '1':
                                cursor_info.last_foreground_color = F_DARK_BLUE;
                                break;
                            case '2':
                                cursor_info.last_foreground_color = F_DARK_GREEN;
                                break;
                            case '3':
                                cursor_info.last_foreground_color = F_CYAN;
                                break;
                            case '4':
                                cursor_info.last_foreground_color = F_DARK_RED;
                                break;
                            case '5':
                                cursor_info.last_foreground_color = F_PURPLE;
                                break;
                            case '6':
                                cursor_info.last_foreground_color = F_BROWN;
                                break;
                            case '7':
                                cursor_info.last_foreground_color = F_GRAY;
                                break;
                            case '8':
                                cursor_info.last_foreground_color = F_DARK_GRAY;
                                break;
                            case '9':
                                cursor_info.last_foreground_color = F_BLUE;
                                break;
                            case 'A':
                                cursor_info.last_foreground_color = F_GREEN;
                                break;
                            case 'B':
                                cursor_info.last_foreground_color = F_AQUA;
                                break;
                            case 'C':
                                cursor_info.last_foreground_color = F_RED;
                                break;
                            case 'D':
                                cursor_info.last_foreground_color = F_PINK;
                                break;
                            case 'E':
                                cursor_info.last_foreground_color = F_YELLOW;
                                break;
                            case 'F':
                                cursor_info.last_foreground_color = F_WHITE;
                                break;
                        }
                        command_info = empty_command_info;
                        return;
                default:
                    assert(false);
                }
            default:
                command_info = empty_command_info;
                return;
        }
    }
    switch (c)
    {
    case '\b':
        if (cursor_info.cursor_column > 0) 
        {
            set_cursor(cursor_info.cursor_line, cursor_info.cursor_column - 1);
        }
        return;
    case '\t':
        uint32_t t_gap = TABULATION_SIZE - (cursor_info.cursor_column % TABULATION_SIZE);
        for (uint32_t i = 0; i < t_gap; i++)
        {
            treat_char(' ', foreground_color, background_color, blink);
        }
        return;
    case '\n':
        uint32_t n_gap = CONSOLE_WIDTH - cursor_info.cursor_column - 1;
        for (uint32_t i = 0; i < n_gap; i++)
        {
            treat_char(' ', foreground_color, background_color, blink);
        }
        return;
    case '\f':
        cursor_info.last_foreground_color = foreground_color;
        cursor_info.last_background_color = background_color;
        cursor_info.last_blink_state = blink;
        console_clear();
        set_cursor(0, 0);
        return;
    case '\r':
        set_cursor(cursor_info.cursor_line, 0);
        return;
    case '\033':
        command_info = empty_command_info;
        command_info.command_requested = true;
        return;
    case 127:
        delete_last_char();
        return;
    default:
        if ((c >= 32 && c <= 126) || c >= 161) 
        {
            uint32_t line = cursor_info.cursor_line;
            uint32_t column = cursor_info.cursor_column;
            write_char(line, column, c, foreground_color, background_color, blink);
            column++;
            if (column >= CONSOLE_WIDTH - 1)
            {
                column = 0;
                if (line >= CONSOLE_HEIGHT - 1)
                {
                    up_shift_console();
                }
                else
                {
                    line++;
                }
            }
            set_cursor(line, column);
        }
        return;
    }
}

void console_putbytes(const char *str, int length)
{
    for (int i = 0; i < length; i++)
    {
        treat_char((unsigned char) str[i], cursor_info.last_foreground_color, cursor_info.last_background_color, cursor_info.last_blink_state);
    }
}

void cons_write(const char *str, long size)
{
    console_putbytes(str, size);
}

void print_top_right(const char *str)
{
    cursor_info_t previous_cursor = cursor_info;
    // Calcul de la longueur de la chaîne à afficher
    uint32_t len = strlen(str);
    set_cursor(0, CONSOLE_WIDTH - (len + 1));
    console_putbytes(str, len);
    set_cursor(previous_cursor.cursor_line, previous_cursor.cursor_column);
}