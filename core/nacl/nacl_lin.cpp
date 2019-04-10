#define PUTS_REAL puts

#include <types.h>
#include <stdarg.h>

int nacl_printf(const wchar_t* text,...)
{
  va_list args;

  wchar_t temp[2048];
  va_start(args, text);
  int rv = vsprintf(temp, text, args);
  va_end(args);

  PUTS_REAL(temp);
  return rv;
}

