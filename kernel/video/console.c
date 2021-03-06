/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f?r die Verwendung dieses Sourcecodes siehe unten
*/

#include "console.h"
#include "util/util.h"
#include "kheap.h"
#include "tasking/task.h"
#include "network/netutils.h"


console_t* reachableConsoles[11]; // Mainconsole + up to 10 subconsoles

console_t kernelConsole = // The console of the kernel task. It is a global variable because it should be initialized as soon as possible.
{
    .ID = KERNELCONSOLE_ID, .name = 0, .properties = CONSOLE_AUTOSCROLL|CONSOLE_AUTOREFRESH|CONSOLE_SHOWINFOBAR,
    .scrollBegin = 0, .scrollEnd = 39, .cursor = {0, 0}, .mutex = 0, .tasks = 0
};

console_t*          console_current   = &kernelConsole; // The console of the active task
console_t* volatile console_displayed = &kernelConsole; // Currently visible console

static bool scroll_flag = true;


static void scroll();


inline uint8_t getTextColor(void)
{
    return (currentTask->attrib);
}

inline void textColor(uint8_t color) // bit 0-3: foreground bit 4-7: background
{
    if (currentTask)
    {
        currentTask->attrib = color;
    }
}

void kernel_console_init(void)
{
    kernelConsole.tasks = list_create();
    kernelConsole.mutex = mutex_create();
    memset(kernelConsole.vidmem, 0, COLUMNS * LINES * sizeof(uint16_t));

    reachableConsoles[KERNELCONSOLE_ID] = &kernelConsole;
    memset(reachableConsoles+1, 0, 10*sizeof(console_t*));
}

void console_init(console_t* console, const char* name)
{
    console->name        = malloc(strlen(name)+1, 0, "console-name");
    console->cursor.x    = 0;
    console->cursor.y    = 0;
    console->scrollBegin = 0;
    console->scrollEnd   = USER_END-USER_BEGIN;
    console->properties  = CONSOLE_AUTOREFRESH|CONSOLE_AUTOSCROLL;
    console->tasks       = list_create();
    console->mutex       = mutex_create();
    strcpy(console->name, name);
    memset(console->vidmem, 0, COLUMNS * LINES * 2);

    for (uint8_t i = 1; i < 11; i++) // The next free place in our console-list will be filled with the new console
    {
        if (reachableConsoles[i] == 0)
        {
            console->ID = i;
            reachableConsoles[i] = console;
            console_display(i); //Switching to the new console
            return;
        }
    }
    console->ID = 255;
}

void console_exit(console_t* console)
{
    free(console->name);
    list_free(console->tasks);
    mutex_delete(console->mutex);
}

void console_cleanup(task_t* task)
{
    list_delete(task->console->tasks, list_find(task->console->tasks, task));
    if (task->console->tasks->head == 0)
    {
        // Delete current task's console from list of our reachable consoles, if it is in that list
        for (uint8_t i = 1; i < 11; i++)
        {
            if (task->console == reachableConsoles[i])
            {
                reachableConsoles[i] = 0;
                break;
            }
        }

        // Switch back to kernel console, if the tasks console is displayed at the moment
        if (task->console == console_displayed)
        {
            console_display(KERNELCONSOLE_ID);
        }

        // Free memory
        console_exit(task->console);
        free(task->console);
    }
}

bool console_display(uint8_t ID)
{
    // Changing visible console, returning false, if this console is not available.
    if (ID > 11 || reachableConsoles[ID] == 0)
        return (false);

    console_displayed = reachableConsoles[ID];

    if(console_displayed->properties & CONSOLE_AUTOREFRESH)
        refreshScreen();

    vga_updateCursor();
    return (true);
}

void setScrollField(uint8_t begin, uint8_t end)
{
    console_current->scrollBegin = begin;
    console_current->scrollEnd = end;
}

void console_setProperties(console_properties_t properties)
{
    console_current->properties = properties;

    if(properties & CONSOLE_SHOWINFOBAR)
        console_current->scrollEnd = min(console_current->scrollEnd, 42);
    if(properties & CONSOLE_AUTOREFRESH)
        refreshScreen();
}

void console_clear(uint8_t backcolor)
{
    mutex_lock(console_current->mutex);
    // Erasing the content of the active console
    memsetw(console_current->vidmem, 0x20 | (backcolor << 8), COLUMNS * LINES);
    console_current->cursor.x = 0;
    console_current->cursor.y = 0;

    if (console_current == console_displayed && (console_current->properties & CONSOLE_AUTOREFRESH)) // If it is also displayed at the moment, refresh screen
    {
        refreshUserScreen();
        vga_updateCursor();
    }

    mutex_unlock(console_current->mutex);
}

static void move_cursor_right(void)
{
    ++console_current->cursor.x;

    if (console_current->cursor.x >= COLUMNS)
    {
        ++console_current->cursor.y;
        console_current->cursor.x = 0;
        scroll();
    }
}

static void move_cursor_left(void)
{
    if (console_current->cursor.x)
    {
        --console_current->cursor.x;
    }
    else if (console_current->cursor.y > 0)
    {
        console_current->cursor.x = COLUMNS-1;
        --console_current->cursor.y;
    }
}

static void move_cursor_home(void)
{
    console_current->cursor.x = 0;
}

void setCursor(position_t pos)
{
    pos.x = min(COLUMNS-1, pos.x);
    pos.y = min(LINES-1, pos.y);
    console_current->cursor = pos;

    if(console_current == console_displayed)
        vga_updateCursor();
}

void getCursor(position_t* pos)
{
    *pos = console_current->cursor;
}

void console_setPixel(uint8_t x, uint8_t y, uint16_t value)
{
    mutex_lock(console_current->mutex);
    console_current->vidmem[y*COLUMNS + x] = value;
    mutex_unlock(console_current->mutex);

    if (console_current == console_displayed && (console_current->properties & CONSOLE_AUTOREFRESH))
    {
        vga_setPixel(x, y+2, value);
    }
}

static void putCP437ch(uint8_t uc)
{
    switch (uc)
    {
        case 0x08: // backspace: move the cursor one space backwards and delete
            move_cursor_left();
            *(console_current->vidmem + console_current->cursor.y * COLUMNS + console_current->cursor.x) = ' ' | getTextColor() << 8;
            if (console_displayed == console_current && (console_current->properties & CONSOLE_AUTOREFRESH)) // Print to screen, if current console is displayed at the moment
                vga_setPixel(console_current->cursor.x, console_current->cursor.y+2, ' ' | getTextColor() << 8);
            break;
        case 0x09: // tab: increment cursor.x (divisible by 8)
            console_current->cursor.x = alignUp(console_current->cursor.x+1, 8);
            if (console_current->cursor.x>=COLUMNS)
            {
                ++console_current->cursor.y;
                console_current->cursor.x=0;
                scroll();
            }
            break;
        case '\r': // cr: cursor back to the margin
            move_cursor_home();
            break;
        case '\n': // newline: like 'cr': cursor to the margin and increment cursor.y
            ++console_current->cursor.y; move_cursor_home();
            scroll();
            break;
        default:
            if (uc != 0)
            {
                uint32_t att = getTextColor() << 8;
                *(console_current->vidmem + console_current->cursor.y * COLUMNS + console_current->cursor.x) = uc | att; // character AND attributes: color
                if (console_displayed == console_current && (console_current->properties & CONSOLE_AUTOREFRESH)) // Print to screen, if current console is displayed at the moment
                    vga_setPixel(console_current->cursor.x, console_current->cursor.y+2, uc | att); // character AND attributes: color
                move_cursor_right();
            }
            break;
    }
}

static void atomic_putch(char c) // Does neither lock mutex nor move vga cursor
{
    uint8_t uc = AsciiToCP437((uint8_t)c); // no negative values
    putCP437ch(uc);
}

void putch(char c)
{
    uint8_t uc = AsciiToCP437((uint8_t)c); // no negative values
    mutex_lock(console_current->mutex);
    putCP437ch(uc);

    if(console_current == console_displayed)
        vga_updateCursor();
    mutex_unlock(console_current->mutex);
}

void puts(const char* text)
{
    mutex_lock(console_current->mutex);
    for (; *text; atomic_putch(*text), ++text);

    if(console_current == console_displayed)
        vga_updateCursor();
    mutex_unlock(console_current->mutex);
}

static void scroll(void)
{
    mutex_lock(console_current->mutex);

    if ((console_current->properties & CONSOLE_AUTOSCROLL) && scroll_flag)
    {
        uint8_t scroll_begin = console_current->scrollBegin;
        uint8_t scroll_end = min(console_current->scrollEnd, ((console_current->properties & CONSOLE_FULLSCREEN)?LINES:(USER_END-USER_BEGIN)));

        if (console_current->cursor.y >= scroll_end)
        {
            uint8_t lines = console_current->cursor.y - scroll_end + 1;
            memcpy((uint16_t*)console_current->vidmem + scroll_begin*COLUMNS, (uint16_t*)console_current->vidmem + scroll_begin*COLUMNS + lines * COLUMNS, (scroll_end - lines) * COLUMNS * sizeof(uint16_t));
            memsetw((uint16_t*)console_current->vidmem + (scroll_end - lines) * COLUMNS, getTextColor() << 8, COLUMNS);
            console_current->cursor.y = scroll_end - 1;

            if(console_current == console_displayed && (console_current->properties & CONSOLE_AUTOREFRESH))
            {
                refreshUserScreen();
                vga_updateCursor();
            }
        }
    }

    mutex_unlock(console_current->mutex);
}

/// TODO: make it standardized!
// vprintf(...): supports %u, %d/%i, %f, %y/%x/%X, %s, %c, %% and the PrettyOS-specific %v, %I and %M
size_t vprintf(const char* args, va_list ap)
{
    mutex_lock(console_current->mutex);

    uint8_t attribute = getTextColor();
    char buffer[32]; // Larger is not needed at the moment

    size_t pos; // variable used for return value

    for (pos = 0; *args; ++args)
    {
        switch (*args)
        {
            case '%':
                switch (*(++args))
                {
                    case 'u':
                        utoa(va_arg(ap, uint32_t), buffer);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 'f':
                        ftoa(va_arg(ap, double), buffer);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 'i': case 'd':
                        itoa(va_arg(ap, int32_t), buffer);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 'X': /// TODO: make it standardized
                        i2hex(va_arg(ap, int32_t), buffer, 8);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 'x':
                        i2hex(va_arg(ap, int32_t), buffer, 4);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 'y':
                        i2hex(va_arg(ap, int32_t), buffer, 2);
                        puts(buffer);
                        pos += strlen(buffer);
                        break;
                    case 's':
                    {
                        char* temp = va_arg(ap, char*);
                        puts(temp);
                        pos += strlen(temp);
                        break;
                    }
                    case 'c':
                        atomic_putch((int8_t)va_arg(ap, int32_t));
                        pos++;
                        break;
                    case 'v':
                        textColor((attribute >> 4) | (attribute << 4));
                        atomic_putch(*(++args));
                        textColor(attribute);
                        pos++;
                        break;
                    case 'S': // Size: prints a size in bytes. Can autoscale them to KiB, MiB and GiB
                    {
                        uint64_t size = va_arg(ap, uint64_t);
                        switch(toLower(*++args)) // Second letter determines several properties:
                        {
                            case 'g': // GiB at maximum
                            case 'a': // autodetect
                                if((size+0x2000000)/1024/1024/1024 >= 10)
                                {
                                    pos+=printf("%u GiB", (size+0x2000000)/1024/1024/1024);
                                    break;
                                }
                            case 'm': // MiB at maximum
                                if((size+0x80000)/1024/1024 >= 10)
                                {
                                    pos+=printf("%u MiB", (size+0x80000)/1024/1024);
                                    break;
                                }
                            case 'k': // KiB at maximum
                                if((size+0x200)/1024 >= 10)
                                    pos+=printf("%u KiB", (size+0x200)/1024);
                                else
                                    pos+=printf("%u B", size);
                                break;
                        }
                        break;
                    }
                    case 'I': // IP address
                    {
                        IP_t IP = va_arg(ap, IP_t);
                        pos += printf("%u.%u.%u.%u", IP.IP[0], IP.IP[1], IP.IP[2], IP.IP[3]);
                        break;
                    }
                    case 'M': // MAC address
                    {
                        uint8_t* MAC = va_arg(ap, uint8_t*);
                        pos += printf("%y-%y-%y-%y-%y-%y", MAC[0], MAC[1], MAC[2], MAC[3], MAC[4], MAC[5]);
                        break;
                    }
                    case '%':
                        atomic_putch('%');
                        pos++;
                        break;
                    default:
                        --args;
                        --pos;
                        break;
                }
                break;
            default:
                atomic_putch(*args);
                pos++;
                break;
        }
    }

    if(console_current == console_displayed)
        vga_updateCursor();
    mutex_unlock(console_current->mutex);
    return (pos);
}

size_t printf(const char* args, ...)
{
    va_list ap;
    va_start(ap, args);
    size_t retval = vprintf(args, ap);
    va_end(ap);
    return (retval);
}

size_t cprintf(const char* message, uint32_t line, uint8_t attribute, ...)
{
    mutex_lock(console_current->mutex);
    uint8_t old_attrib = getTextColor();
    position_t cOld = console_current->cursor;
    scroll_flag = false;

    textColor(attribute);
    console_current->cursor.x = 0; console_current->cursor.y = line;

    // Call usual printf routines
    va_list ap;
    va_start(ap, attribute);
    size_t retval = vprintf(message, ap);
    va_end(ap);

    scroll_flag = true;
    textColor(old_attrib);
    console_current->cursor = cOld;
    mutex_unlock(console_current->mutex);

    return (retval);
}


/*
* Copyright (c) 2010-2013 The PrettyOS Project. All rights reserved.
*
* http://www.c-plusplus.de/forum/viewforum-var-f-is-62.html
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
