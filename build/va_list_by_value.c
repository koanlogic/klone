#include <stdarg.h>
#include <stdio.h>

#define SZ  64

int va_func (const char *fmt, ...)
{
    va_list ap;
    char b1[SZ], b2[SZ];

    va_start(ap, fmt);
    (void) vsnprintf(b1, SZ, fmt, ap);
    (void) vsnprintf(b2, SZ, fmt, ap);
    va_end(ap);

    return strcmp(b1, b2) == 0 ? 0 : 1;
}

int main (void)
{
    return va_func("%s %d %c", "string", 100, 'x');
}
