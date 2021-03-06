#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"


void  i2hex(uint32_t val, char* dest, uint32_t len); // -> Userlib
char* itoa(int32_t n, char* s); // -> Userlib
char* utoa(unsigned int n, char* s); // -> Userlib
void  ftoa(float f, char* buffer); // -> Userlib


FILE* stderr;
FILE* stdin;
FILE* stdout;


FILE* fopen(const char* path, const char* mode); // -> Syscall
FILE* tmpfile(); /// TODO
FILE* freopen(const char* filename, const char* mode, FILE* file); /// TODO
int fclose(FILE* file); // -> Syscall
int remove(const char* path); /// TODO
int rename(const char* oldpath, const char* newpath); /// TODO
int fputc(char c, FILE* file); // -> Syscall
int putc(char c, FILE* file)
{
    return(fputc(c, file));
}
char fgetc(FILE* file); // -> Syscall
char getc(FILE* file)
{
    return(fgetc(file));
}
int ungetc(char c, FILE* file); /// TODO
char* fgets(char* dest, size_t num, FILE* file); /// TODO
int fputs(const char* src, FILE* file)
{
    for(; *src; src++)
        fputc(*src, file);
    fputc('\n', file);
    return(0);
}
size_t fread(void* dest, size_t size, size_t count, FILE* file)
{
    for (size_t i = 0; i < count*size; i++)
        ((uint8_t*)dest)[i] = fgetc(file);
    return (count*size);
}
size_t fwrite(const void* src, size_t size, size_t count, FILE* file)
{
    for (size_t i = 0; i < count*size; i++)
        fputc(((uint8_t*)src)[i], file);
    return (count*size);
}
int fflush(FILE* file); // -> Syscall
size_t ftell(FILE* file); /// TODO
int fseek(FILE* file, size_t offset, SEEK_ORIGIN origin); // -> Syscall
int rewind(FILE* file); /// TODO
int feof(FILE* file); /// TODO
int ferror(FILE* file); /// TODO
void clearerr(FILE* file); /// TODO
int fgetpos(FILE* file, fpos_t* position); /// TODO
int fsetpos(FILE* file, const fpos_t* position); /// TODO
int vfprintf(FILE* file, const char* format, va_list arg); /// TODO
int fprintf(FILE* file, const char* format, ...); /// TODO
int vfscanf(FILE* file, const char* format, va_list arg); /// TODO
int fscanf(FILE* file, const char* format, ...); /// TODO
void setbuf(FILE* file, char* buffer); /// TODO
int setvbuf(FILE* file, char* buffer, int mode, size_t size); /// TODO
char* tmpnam(char* str); /// TODO



void perror(const char* string); /// TODO



char getcharar(); // -> Syscall
char* gets(char* str)
{
    int32_t i = 0;
    char c;
    do
    {
        c = getchar();
        if (c=='\b')  // Backspace
        {
           if (i>0)
           {
              putchar(c);
              str[i-1]='\0';
              --i;
           }
        }
        else
        {
            if (c != '\n')
            {
                str[i] = c;
                i++;
            }
            putchar(c);
        }
    }
    while (c != '\n'); // Linefeed
    str[i]='\0';

    return str;
}

int vscanf(const char* format, va_list arg); /// TODO
int scanf(const char* format, ...); /// TODO
int putchar(char c); // -> Syscall
int puts(const char* str)
{
    for (size_t i = 0; str[i] != 0; i++)
    {
        putchar(str[i]);
    }
    return (0);
}

int vprintf(const char* format, va_list arg)
{
    char buffer[32]; // Larger is not needed at the moment

    int pos = 0;
    for (; *format; format++)
    {
        switch (*format)
        {
        case '%':
            switch (*(++format))
            {
            case 'u':
                utoa(va_arg(arg, uint32_t), buffer);
                puts(buffer);
                pos += strlen(buffer);
                break;
            case 'f':
                ftoa(va_arg(arg, double), buffer);
                puts(buffer);
                pos += strlen(buffer);
                break;
            case 'i': case 'd':
                itoa(va_arg(arg, int32_t), buffer);
                puts(buffer);
                pos += strlen(buffer);
                break;
            case 'X':
                i2hex(va_arg(arg, int32_t), buffer,8);
                puts(buffer);
                pos += 8;
                break;
            case 'x':
                i2hex(va_arg(arg, int32_t), buffer,4);
                puts(buffer);
                pos += 4;
                break;
            case 'y':
                i2hex(va_arg(arg, int32_t), buffer,2);
                puts(buffer);
                pos += 2;
                break;
            case 's':
            {
                char* temp = va_arg (arg, char*);
                puts(temp);
                pos += strlen(temp);
                break;
            }
            case 'c':
                putchar((int8_t)va_arg(arg, int32_t));
                pos++;
                break;
            case '%':
                putchar('%');
                pos++;
                break;
            default:
                --format;
                --pos;
                break;
            }
            break;
        default:
            putchar(*format);
            pos++;
            break;
        }
    }
    return (pos);
}

int printf(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    int retval = vprintf(format, arg);
    va_end(arg);
    return (retval);
}




int vsprintf(char* dest, const char* format, va_list arg)
{
    int pos = 0;
    char m_buffer[32]; // Larger is not needed at the moment

    for (; *format; format++)
    {
        switch (*format)
        {
            case '%':
                switch (*(++format))
                {
                    case 'u':
                        utoa(va_arg(arg, uint32_t), m_buffer);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 'f':
                        ftoa(va_arg(arg, double), m_buffer);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 'i': case 'd':
                        itoa(va_arg(arg, int32_t), m_buffer);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 'X':
                        i2hex(va_arg(arg, int32_t), m_buffer,8);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 'x':
                        i2hex(va_arg(arg, int32_t), m_buffer,4);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 'y':
                        i2hex(va_arg(arg, int32_t), m_buffer,2);
                        strcpy(dest+pos, m_buffer);
                        pos += strlen(m_buffer);
                        break;
                    case 's':
                    {
                        char* buf = va_arg(arg, char*);
                        strcpy(dest+pos, buf);
                        pos += strlen(buf);
                        break;
                    }
                    case 'c':
                        dest[pos] = (int8_t)va_arg(arg, int32_t);
                        pos++;
                        break;
                    case '%':
                        dest[pos] = '%';
                        pos++;
                        break;
                    default:
                        --format;
                        break;
                    }
                break;
            default:
                dest[pos] = (*format);
                pos++;
                break;
        }
        dest[pos] = '\0';
    }
    return (pos);
}

int sprintf(char* dest, const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    int retval = vsprintf(dest, format, arg);
    va_end(arg);
    return (retval);
}

void vsnprintf(char* buffer, size_t length, const char* args, va_list ap)
{
    char m_buffer[32]; // Larger is not needed at the moment

    size_t pos;
    for (pos = 0; *args && pos < length; args++)
    {
        switch (*args)
        {
            case '%':
                switch (*(++args))
                {
                    case 'u':
                        utoa(va_arg(ap, uint32_t), m_buffer);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += strlen(m_buffer);
                        break;
                    case 'f':
                        ftoa(va_arg(ap, double), m_buffer);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += strlen(m_buffer);
                        break;
                    case 'i': case 'd':
                        itoa(va_arg(ap, int32_t), m_buffer);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += strlen(m_buffer);
                        break;
                    case 'X':
                        i2hex(va_arg(ap, int32_t), m_buffer, 8);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += 8;
                        break;
                    case 'x':
                        i2hex(va_arg(ap, int32_t), m_buffer, 4);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += 4;
                        break;
                    case 'y':
                        i2hex(va_arg(ap, int32_t), m_buffer, 2);
                        strncpy(buffer+pos, m_buffer, length - pos);
                        pos += 2;
                        break;
                    case 's':
                    {
                        const char* string = va_arg(ap, const char*);
                        strncpy(buffer+pos, string, length - pos);
                        pos += strlen(string);
                        break;
                    }
                    case 'c':
                        buffer[pos] = (char)va_arg(ap, int32_t);
                        pos++;
                        buffer[pos] = 0;
                        break;
                    case '%':
                        buffer[pos] = '%';
                        pos++;
                        buffer[pos] = 0;
                        break;
                    default:
                        --args;
                        break;
                    }
                break;
            default:
                buffer[pos] = (*args);
                pos++;
                break;
        }
    }
    if(pos < length)
        buffer[pos] = 0;
}

void snprintf(char *buffer, size_t length, const char *args, ...)
{
    va_list ap;
    va_start(ap, args);
    vsnprintf(buffer, length, args, ap);
    va_end(ap);
}

int vsscanf(const char* src, const char* format, va_list arg); /// TODO
int sscanf(const char* src, const char* format, ...); /// TODO
