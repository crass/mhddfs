/*
	mhddfs - Multi HDD [FUSE] File System
	Copyright (C) 2008 Dmitry E. Oboukhov <dimka@avanto.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
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
