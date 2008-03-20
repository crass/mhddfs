#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "debug.h"
#include "parse_options.h"

int debug_level=MHDD_DEFAULT_DEBUG_LEVEL;

int mhdd_debug(int level, const char *fmt, ...)
{
  if (level<debug_level) return 0;
  if (!mhdd.debug) return 0;
  
  char tstr[64];
  time_t t=time(0);
  struct tm *lt;
  lt=localtime(&t);
  strftime(tstr, 64, "%Y-%m-%d %H:%M:%S", lt);
  fprintf(mhdd.debug, "mhddfs [%s]", tstr);

  switch(level)
  {
    case MHDD_DEBUG: fprintf(mhdd.debug, " (debug): "); break;
    case MHDD_INFO:  fprintf(mhdd.debug, " (info): ");  break;
    default:         fprintf(mhdd.debug, ": ");  break;
  }
  
  va_list ap;
  va_start(ap, fmt);
  int res=vfprintf(mhdd.debug, fmt, ap);
  va_end(ap);
  return res;
}

