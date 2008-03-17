#include <stdlib.h>
#include "usage.h"

void usage(FILE * to)
{
  const char *usage=
    "\n"
    "Multi-hdd FUSE filesystem\n"
    "     Copyright (C) 2008, Dmitry E. Oboukhov <dimka@avanto.org>\n"
    "\n"
    "Usage:\n"
    " mhddfs dir1,dir2.. mountpoint [ -o OPTIONS ]\n"
    "\n"
    "OPTIONS:\n"
    "  mlimit=xxx - limit for move files (default 4G, minimum 100M)\n"
    "  logfile=/path/to/file - log file\n"
    "\n"
    " see fusermount(1) for information about other options\n"
    "";
  fprintf(to, usage);
  if (to==stdout) exit(0);
  exit(-1);
}
