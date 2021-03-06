/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f�r die Verwendung dieses Sourcecodes siehe unten
*/

#include "userlib.h"
#include "stdio.h"


extern bool enabledEvents;
extern void (*_syscall)(void);

void (**_atexit_funcs)() = 0; // Stores a zero terminated array of function pointers, called at exit


FS_ERROR execute(const char* path, size_t argc, char* argv[])
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(0), "b"(path), "c"(argc), "d"(argv));
    return (ret);
}

// TODO: (2) createThread

void exitProcess(void)
{
    __asm__("call *_syscall" : : "a"(2));
}

bool wait(BLOCKERTYPE reason, void* data, uint32_t timeout)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(3), "b"(reason), "c"(data), "d"(timeout/10)); // HACK. Unbound it from system frequency. cf. scheduler.c
    return (ret);
}

// TODO: (4) createConsoleThread

uint32_t getMyPID(void)
{
    uint32_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(6));
    return (ret);
}

void* userheapAlloc(size_t increase)
{
    uintptr_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(10), "b"(increase));
    return (void*)ret;
}

// TODO: (11) userheapFree

file_t* fopen(const char* path, const char* mode)
{
    file_t* ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(15), "b"(path), "c"(mode));
    return (ret);
}

char fgetc(file_t* file)
{
    char ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(16), "b"(file));
    return (ret);
}

int fputc(char value, file_t* file)
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(17), "b"(value), "c"(file));
    return (ret);
}

int fseek(file_t* file, size_t offset, SEEK_ORIGIN origin)
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(18), "b"(file), "c"(offset), "d"(origin));
    return (ret);
}

int fflush(file_t* file)
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(19), "b"(file));
    return (ret);
}

int fclose(file_t* file)
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(21), "b"(file));
    return (ret);
}

FS_ERROR partition_format(const char* path, FS_t type, const char* name)
{
    FS_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(22), "b"(path), "c"(type), "d"(name));
    return (ret);
}

file_t* ipc_fopen(const char* path, const char* mode)
{
    file_t* ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(25), "b"(path), "c"(mode));
    return (ret);
}

IPC_ERROR ipc_getFolder(const char* path, char* destination, size_t length)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(26), "b"(path), "c"(destination), "d"(length));
    return (ret);
}

IPC_ERROR ipc_getString(const char* path, char* destination, size_t length)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(27), "b"(path), "c"(destination), "d"(length));
    return (ret);
}

IPC_ERROR ipc_setString(const char* path, const char* source)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(28), "b"(path), "c"(source));
    return (ret);
}

IPC_ERROR ipc_getInt(const char* path, int64_t* destination)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(29), "b"(path), "c"(destination));
    return (ret);
}

IPC_ERROR ipc_setInt(const char* path, int64_t* source)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(30), "b"(path), "c"(source));
    return (ret);
}

IPC_ERROR ipc_getDouble(const char* path, double* destination)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(31), "b"(path), "c"(destination));
    return (ret);
}

IPC_ERROR ipc_setDouble(const char* path, double* source)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(32), "b"(path), "c"(source));
    return (ret);
}

IPC_ERROR ipc_deleteKey(const char* path)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(33), "b"(path));
    return (ret);
}

IPC_ERROR ipc_setAccess(const char* path, IPC_RIGHTS permissions, uint32_t task)
{
    IPC_ERROR ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(34), "b"(path), "c"(permissions), "d"(task));
    return (ret);
}

bool waitForEvent(uint32_t timeout)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(37), "b"(timeout));
    return (ret);
}

void event_enable(bool b)
{
    enabledEvents = b;
    if (b)
    {
        __asm__("call *_syscall" : : "a"(38), "b"(b));
    }
}

EVENT_t event_poll(void* destination, size_t maxLength, EVENT_t filter)
{
    EVENT_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(39), "b"(destination), "c"(maxLength), "d"(filter));
    return (ret);
}

uint32_t getCurrentMilliseconds(void)
{
    uint32_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(40));
    return (ret);
}

void systemControl(SYSTEM_CONTROL todo)
{
    __asm__("call *_syscall" : : "a"(50), "b"(todo));
}

// TODO (51) systemRefresh

int putchar(char val)
{
    __asm__("call *_syscall" : : "a"(55), "b"(val));
    return (0); // HACK
}

void textColor(uint8_t color)
{
    __asm__("call *_syscall" : : "a"(56), "b"(color));
}

void setScrollField(uint8_t top, uint8_t bottom)
{
    __asm__("call *_syscall" : : "a"(57), "b"(top), "c"(bottom));
}

void setCursor(position_t pos)
{
    __asm__("call *_syscall" : : "a"(58), "b"(pos));
}

void getCursor(position_t* pos)
{
    __asm__("call *_syscall" : : "a"(59), "b"(pos));
}

void clearScreen(uint8_t backgroundColor)
{
    __asm__("call *_syscall" : : "a"(61), "b"(backgroundColor));
}

void console_setProperties(console_properties_t properties)
{
    __asm__("call *_syscall" : : "a"(62), "b"(properties));
}

void refreshScreen(void)
{
    __asm__("call *_syscall" : : "a"(63));
}

uint16_t TextGUI_ShowMSG(char* title, char* message)
{
   uint16_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(68), "b"(title), "c"(message));
    return (ret);
}

uint16_t TextGUI_AskYN(char* title, char* message, uint8_t defaultselected)
{
   uint16_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(69), "b"(title), "c"(message), "d"(defaultselected));
    return (ret);
}

bool keyPressed(KEY_t key)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(71), "b"(key));
    return (ret);
}

// TODO (72-74) mouse

void beep(uint32_t frequency, uint32_t duration)
{
    __asm__("call *_syscall" : : "a"(80), "b"(frequency), "c"(duration));
}

void dns_getServer(IP_t* server)
{
    __asm__("call *_syscall" : : "a"(83), "b"(server));
}

void dns_setServer(IP_t server)
{
    __asm__("call *_syscall" : : "a"(84), "b"(server));
}

uint32_t tcp_connect(IP_t IP, uint16_t port)
{
    uint32_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(85), "b"(IP), "c"(port));
    return (ret);
}

bool tcp_send(uint32_t ID, void* data, size_t length)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(86), "b"(ID), "c"(data), "d"(length));
    return (ret);
}

bool tcp_close(uint32_t ID)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(87), "b"(ID));
    return (ret);
}

bool udp_send(void* data, uint32_t length, IP_t destIP, uint16_t srcPort, uint16_t destPort)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(88), "b"(data), "c"(length), "d"(destIP), "S"(srcPort), "D"(destPort));
    return (ret);
}

bool udp_bind(uint16_t port)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(89), "b"(port));
    return (ret);
}

bool udp_unbind(uint16_t port)
{
    bool ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(90), "b"(port));
    return (ret);
}

uint32_t getMyIP(void)
{
    uint32_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(91));
    return (ret);
}


 // deprecated, to be substituted
int32_t floppy_dir(void)
{
    int32_t ret;
    __asm__("call *_syscall" : "=a"(ret) : "a"(100));
    return(ret);
}

void printLine(const char* message, uint32_t line, uint8_t attribute)
{
    if (line <= 45) // User must not write outside of client area (size is 45)
    {
        __asm__("call *_syscall" : : "a"(101), "b"(message), "c"(line), "d"(attribute));
    }
}


/*
* Copyright (c) 2009-2013 The PrettyOS Project. All rights reserved.
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
