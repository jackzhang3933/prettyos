/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f�r die Verwendung dieses Sourcecodes siehe unten
*/

#include "util.h"
#include "sys_speaker.h"
#include "scheduler.h"

const int32_t INT_MAX = 2147483647;


void nop() { __asm__ volatile ("nop"); }  // Do nothing

/**********************************************************************/

uint32_t fetchESP()
{
    uint32_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    return esp;
}

uint32_t fetchEBP()
{
    uint32_t ebp;
    __asm__ volatile("mov %%ebp, %0" : "=r"(ebp));
    return ebp;
}

uint32_t fetchSS()
{
    register uint32_t eax __asm__("%eax");
    __asm__ volatile ("movl %ss,%eax");
    return eax;
}

uint32_t fetchCS()
{
    register uint32_t eax __asm__("%eax");
    __asm__ volatile ("movl %cs,%eax");
    return eax;
}

uint32_t fetchDS()
{
    register uint32_t eax __asm__("%eax");
    __asm__ volatile ("movl %ds,%eax");
    return eax;
}

uint64_t rdtsc()
{
    uint64_t val;
    __asm__ volatile ("rdtsc" : "=A"(val)); // "=A" for getting 64 bit value
    return val;
}

/**********************************************************************/

uint8_t inportb(uint16_t port)
{
    uint8_t ret_val;
    __asm__ volatile ("in %%dx,%%al" : "=a"(ret_val) : "d"(port));
    return ret_val;
}

uint16_t inportw(uint16_t port)
{
    uint16_t ret_val;
    __asm__ volatile ("in %%dx,%%ax" : "=a" (ret_val) : "d"(port));
    return ret_val;
}

uint32_t inportl(uint16_t port)
{
    uint32_t ret_val;
    __asm__ volatile ("in %%dx,%%eax" : "=a" (ret_val) : "d"(port));
    return ret_val;
}

void outportb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("out %%al,%%dx" :: "a"(val), "d"(port));
}

void outportw(uint16_t port, uint16_t val)
{
    __asm__ volatile ("out %%ax,%%dx" :: "a"(val), "d"(port));
}

void outportl(uint16_t port, uint32_t val)
{
    __asm__ volatile ("outl %%eax,%%dx" : : "a"(val), "d"(port));
}

/**********************************************************************/

void panic_assert(const char* file, uint32_t line, const char* desc) // why char ?
{
    cli();
    printf("ASSERTION FAILED(%s) at %s:%i\nOPERATING SYSTEM HALTED\n", desc, file, line);
    // Halt by going into an infinite loop.
    for (;;);
}

/**********************************************************************/

void memshow(void* start, size_t count)
{
    const uint8_t* end = (const uint8_t*)(start+count);
    for (; count != 0; count--) printf("%x ",*(end-count));
}

void* memcpy(void* dest, const void* src, size_t count)
{
    const uint8_t* sp = (const uint8_t*)src;
    uint8_t* dp = (uint8_t*)dest;
    for (; count != 0; count--) *dp++ = *sp++;
    return dest;
}

void* memset(void* dest, int8_t val, size_t count)
{
    int8_t* temp = (int8_t*)dest;
    for (; count != 0; count--) *temp++ = val;
    return dest;
}

uint16_t* memsetw(uint16_t* dest, uint16_t val, size_t count)
{
    uint16_t* temp = dest;
    for (; count != 0; count--) *temp++ = val;
    return dest;
}

uint32_t* memsetl(uint32_t* dest, uint32_t val, size_t count)
{
    uint32_t* temp = dest;
    for (; count != 0; count--) *temp++ = val;
    return dest;
}

/**********************************************************************/

void vsnprintf (char *buffer, size_t length, const char *args, va_list ap)
{
    size_t pos = 0;
    char m_buffer[32]; // Larger is not needed at the moment
    buffer[0] = '\0';

    for (; *args && pos < length; args++)
    {
        switch (*args)
        {
            case '%':
                switch (*(++args))
                {
                    case 'u':
                        itoa(va_arg(ap, uint32_t), m_buffer);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 'f':
                        ftoa(va_arg(ap, double), m_buffer);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 'i': case 'd':
                        itoa(va_arg(ap, int32_t), m_buffer);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 'X':
                        i2hex(va_arg(ap, int32_t), m_buffer,8);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 'x':
                        i2hex(va_arg(ap, int32_t), m_buffer,4);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 'y':
                        i2hex(va_arg(ap, int32_t), m_buffer,2);
                        strncat(buffer, m_buffer, length - pos - 1);
                        pos += strlen(m_buffer) - 1;
                        break;
                    case 's':
                        strncat(buffer, va_arg (ap, char*), length - pos - 1);
                        pos = strlen(buffer) - 1;
                        break;
                    case 'c':
                        buffer[pos] = (int8_t)va_arg(ap, int32_t);
                        break;
                    case '%':
                        buffer[pos] = '%';
                        break;
                    default:
                        --args;
                        --pos;
                        break;
                    }
                break;
            default:
                buffer[pos] = (*args);
                break;
        }
        pos++;
        buffer[pos] = '\0';
    }
}
void snprintf (char *buffer, size_t length, const char *args, ...)
{
    va_list ap;
    va_start(ap, args);
    vsnprintf(buffer, length, args, ap);
}

size_t strlen(const char* str)
{
    size_t retval;
    for (retval = 0; *str != '\0'; ++str)
        ++retval;
    return retval;
}

// Compare two strings. Returns -1 if str1 < str2, 0 if they are equal or 1 otherwise.
int32_t strcmp(const char* s1, const char* s2)
{
    while ((*s1) && (*s1 == *s2))
    {
        ++s1;
        ++s2;
    }
    return (*s1 - *s2);
}

/// http://en.wikipedia.org/wiki/Strcpy
// Copy the NUL-terminated string src into dest, and return dest.
char* strcpy(char* dest, const char* src)
{
   char* save = dest;
   while ((*dest++ = *src++));
   return save;
}

char* strncpy(char* dest, const char* src, unsigned int n) // okay?
{
    if (n != 0)
    {
        char* d       = dest;
        const char* s = src;
        do
        {
            if ((*d++ = *s++) == 0)
            {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0)
                   *d++ = 0;
                break;
            }
        }
        while (--n != 0);
     }
     return(dest);
}


/// http://en.wikipedia.org/wiki/Strcat
char* strcat(char* dest, const char* src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char* strncat(char* dest, const char* src, size_t n)
{
    strncpy(dest + strlen(dest), src, n);
    return dest;
}

/**********************************************************************/

/// http://en.wikipedia.org/wiki/Itoa
void reverse(char* s)
{
    char c;

    for (int32_t i=0, j=strlen(s)-1; i<j; i++, j--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

int8_t ctoi(char c) {
    if (c < 48 || c > 57) {
        return(-1);
    }
    return(c-48);
}

/// http://en.wikipedia.org/wiki/Itoa
void itoa(int32_t n, char* s)
{
    int32_t i, sign;
    if ((sign = n) < 0)  // record sign
    {
        n = -n;         // make n positive
    }
    i=0;
    do /* generate digits in reverse order */
    {
        s[i++] = n % 10 + '0';  // get next digit
    }
    while ((n /= 10) > 0);     // delete it

    if (sign < 0)
    {
        s[i++] = '-';
    }
    s[i] = '\0';
    reverse(s);
}

void i2hex(uint32_t val, char* dest, int32_t len)
{
    char* cp;
    char  x;
    uint32_t n;
    n = val;
    cp = &dest[len];
    while (cp > dest)
    {
        x = n & 0xF;
        n >>= 4;
        *--cp = x + ((x > 9) ? 'A' - 10 : '0');
    }
    dest[len]  ='h';
    dest[len+1]='\0';
}

float atof(const char* s)
{
    int32_t i = 0;
    int8_t sign = 1;
    while(s[i] == ' ' || s[i] == '+' || s[i] == '-')
    {
        if(s[i] == '-')
        {
            sign *= -1;
        }
        i++;
    }

    float val;
    for (val = 0.0; isdigit(s[i]); i++)
    {
        val = 10.0 * val + s[i] - '0';
    }
    if (s[i] == '.')
    {
        i++;
    }
    float pow;
    for (pow = 1.0; isdigit(s[i]); i++)
    {
        val = 10.0 * val + s[i] - '0';
        pow *= 10.0;
    }
    return(sign * val / pow);
}

void ftoa(float f, char* buffer)
{
    if (f < 0.0)
    {
        *buffer = '-';
        ++buffer;
        f = -f;
    }

    char tmp[32];
    int32_t i = f;
    int32_t index = sizeof(tmp) - 1;
    while (i > 0)
    {
        tmp[index] = ('0' + (i % 10));
        i /= 10;
        --index;
    }
    memcpy((void*)buffer, (void*)&tmp[index + 1], sizeof(tmp) - 1 - index);
    buffer += sizeof(tmp) - 1 - index;
    *buffer = '.';
    ++buffer;

    *buffer++ = ((uint32_t)(f * 10.0) % 10) + '0';
    *buffer++ = ((uint32_t)(f * 100.0) % 10) + '0';
    *buffer++ = ((uint32_t)(f * 1000.0) % 10) + '0';
    *buffer++ = ((uint32_t)(f * 10000.0) % 10) + '0';
    *buffer++ = ((uint32_t)(f * 100000.0) % 10) + '0';
    *buffer++ = ((uint32_t)(f * 1000000.0) % 10) + '0';
    *buffer   = '\0';
}


uint32_t alignUp(uint32_t val, uint32_t alignment)
{
    if (! alignment)
        return val;
    --alignment;
    return (val+alignment) & ~alignment;
}

uint32_t alignDown(uint32_t val, uint32_t alignment)
{
    if (! alignment)
        return val;
    return val & ~(alignment-1);
}

uint8_t PackedBCD2Decimal(uint8_t PackedBCDVal)
{
    return ((PackedBCDVal >> 4) * 10 + (PackedBCDVal & 0xF));
}

/**********************************************************************/

void reboot()
{
    int32_t temp; // A temporary int for storing keyboard info. The keyboard is used to reboot
    do //flush the keyboard controller
    {
       temp = inportb(0x64);
       if (temp & 1)
         inportb(0x60);
    }
    while (temp & 2);

    // Reboot
    outportb(0x64, 0xFE);
}

// BOOTSCREEN

void bootscreen() {
    printf("\n\n");
    textColor(0x0E);
    printf("       ");
    textColor(0x08);
    printf("######                    ##    ##               #####       ####\n");
    printf("      ");
    textColor(0x0F);
    printf("######");
    textColor(0x08);
    printf("##                  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#              ");
    textColor(0x0F);
    printf("#####");
    textColor(0x08);
    printf("##     ");
    textColor(0x0F);
    printf("####");
    textColor(0x08);
    printf("##\n");
    printf("      ");
    textColor(0x0F);
    printf("#######");
    textColor(0x08);
    printf("#                  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#             ");
    textColor(0x0F);
    printf("#######");
    textColor(0x08);
    printf("##   ");
    textColor(0x0F);
    printf("######");
    textColor(0x08);
    printf("#\n");
    printf("      ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ## ##    ####  #");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("###");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#####    ##  ");
    textColor(0x0F);
    printf("###   ###");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##\n");
    printf("      ##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  #");
    textColor(0x0F);
    printf("####");
    textColor(0x08);
    printf("##");
    textColor(0x0F);
    printf("##############");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#     ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("##\n");
    printf("     ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##  #####  #######");
    textColor(0x08);
    printf("#");
    textColor(0x0F);
    printf("##############");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##  ##      ##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("##\n");
    printf("     ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("##");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("####    ##");
    textColor(0x08);
    printf("###");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("## ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#      ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("###\n");
    printf("     ");
    textColor(0x0F);
    printf("#######  ###");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("########  ##");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#      ");
    textColor(0x0F);
    printf("##     ####");
    textColor(0x08);
    printf("#\n");
    printf("     ");
    textColor(0x0F);
    printf("######   ###    ######    ##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#");
    textColor(0x0F);
    printf("###  ##");
    textColor(0x08);
    printf("#     ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#       ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#\n");
    printf("     ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#      ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#    ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#      ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("##    ");
    textColor(0x0F);
    printf("##   ");
    textColor(0x08);
    printf("##   ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#\n");
    printf("    ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("#      ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#    ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("###### ");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("###");
    textColor(0x0F);
    printf("###");
    textColor(0x08);
    printf("##  ");
    textColor(0x0F);
    printf("#####   ###");
    textColor(0x08);
    printf("####");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("##");
    textColor(0x08);
    printf("###");
    textColor(0x0F);
    printf("###\n");
    printf("    ###      ###     #######");
    textColor(0x08);
    printf("# ");
    textColor(0x0F);
    printf("##### ####");
    textColor(0x08);
    printf("#  ");
    textColor(0x0F);
    printf("####     ########   #######\n");
    printf("    ##       ##       ######   ###   ###   ###       #####      #####\n");
    printf("                                           ##\n");
    printf("                                          ##\n");
    printf("                                         ##\n");
    printf("\n\n");
    textColor(0x0E);
    printf("                    Copyright (c) 2010  The PrettyOS Team\n");
    printf("\n\n");
    textColor(0x0F);
    // Melody
    // C Es F G F Es
    // C E F G F E C
    // http://www.flutepage.de/deutsch/goodies/frequenz.shtml (German)
    // http://www.flutepage.de/englisch/goodies/frequenz.shtml (English)

    beep(523,200); // C
    printf("                     This");
    beep(622,200); // Es
    printf(" bootscreen");
    beep(689,200); // F
    printf(" has");
    beep(784,200); // G
    printf(" been");
    beep(689,200); // F
    printf(" created");
    beep(622,200); // Es
    printf(" by\n\n");

    beep(523,200); // C
    printf("                     Cuervo,");
    beep(659,200); // E
    printf(" member");
    beep(689,200); // F
    printf(" of the");
    beep(784,200); // G
    printf(" PrettyOS");
    beep(689,200); // F
    printf(" team");
    beep(659,200); // E

    beep(523,1000); // C

    #ifdef _DIAGNOSIS_
    log_task_list();
    sleepSeconds(5);
    #endif
}


uint32_t max(uint32_t a, uint32_t b)
{
    return(a >= b ? a : b);
}

uint32_t min(uint32_t a, uint32_t b)
{
    return(a <= b ? a : b);
}

int32_t power(int32_t base, int32_t n)
{
    if (n == 0 || base == 1)
    {
        return 1;
    }
    int32_t p = 1;
    for (; n > 0; --n)
    {
        p *= base;
    }
    return(p);
}

double fabs(double x)
{
    double result;
    __asm__ volatile("fabs" : "=t" (result) : "0" (x));
    return result;
}

/*
* Copyright (c) 2009-2010 The PrettyOS Project. All rights reserved.
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
