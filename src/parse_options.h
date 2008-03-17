#ifndef __PARSE__OPTIONS__H__
#define __PARSE__OPTIONS__H__

#include <stdio.h>

struct mhdd_config
{
  char *  mount;    // mount point
  char ** dirs;     // dir list

  int  cdirs;       // count dirs in dirs

  off_t move_limit; // no limits

  FILE  *debug;
  char  *debug_file;
};

extern struct mhdd_config mhdd;

void parse_options(int * argc, char *argv[]);

#endif
