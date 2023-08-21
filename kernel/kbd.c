#include "kbd.h"
#include "cpu.h"
#include <stdint.h>
#include <stdbool.h>
#include "../shared/stdio.h"
#include "message_queue.h"

#define KEY_BUFFER_SIZE 512
#define INDEXES_LOCK_VALUE -1
#define PS2_CONTROL_PORT 0x64
#define PS2_DATA_PORT 0x60

bool echo_enabled;
static int stdin_fid = -1;

static void echo(unsigned char c)
{
    switch (c)
    {   
        case '\r':
            printf("\n");
            break;
        default:
            if (c == 9 || (c >= 32 && c <= 127))
            {
                printf("%c", c);
            }
            else if (c < 32)
            {
                printf("^%c", 64 + c);
            }
            break;
    }
}

void keyboard_interrupt(void){
    unsigned char code = inb(PS2_DATA_PORT);
    outb(0x20, 0x20);
    do_scancode(code);
}

void stdin_init(void)
{
    assert(stdin_fid == -1);
    stdin_fid = pcreate(KEY_BUFFER_SIZE);
    assert(stdin_fid >= 0);
    echo_enabled = true;
}

void keyboard_data(char *str)
{
    int count;
    while (*str != '\0')
    {
        assert(pcount(stdin_fid, &count) == 0);
        if (count > KEY_BUFFER_SIZE)
        {
            return;
        }
        assert(psend(stdin_fid, (int)*str) == 0);
        str++;
    }
}

unsigned long cons_read(char *str, unsigned long length)
{
    assert(length > 0);
    int receiver;
    uint64_t i = 0;
    while (i < length)
    {
        assert(preceive(stdin_fid, &receiver) == 0);
        unsigned char c = (unsigned char)receiver;
        if (c == '\r')
        {
            if (echo_enabled)
            {
                echo(c);
            }
            //str[i] = '\0';
            return i;
        }
        if (c == 127)
        {
            if (i > 0)
            {
                if (echo_enabled)
                {
                    echo(c);
                }
                i--;
            }
        }
        else if (c < 127)
        {
            if (echo_enabled)
            {
                echo(c);
            }
            str[i] = c;
            i++;
        }
    }
    //assert(preceive(stdin_fid, &receiver) == 0);
    //str[length - 1] = '\0';
    if (echo_enabled)
    {
        echo('\r');
    }
    return i;
}

void cons_echo(bool on)
{
    echo_enabled = on;
}

void kbd_leds(unsigned char leds){
    outb(0xED, PS2_CONTROL_PORT);
    outb(leds, PS2_DATA_PORT);
}