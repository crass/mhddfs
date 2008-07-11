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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stddef.h>
#include <fuse.h>


#include "parse_options.h"
#include "usage.h"
#include "version.h"
#include "debug.h"
#include "tools.h"

#define MLIMIT_OPTION   "mlimit="
#define LOGFILE_OPTION  "logfile="
#define LOG_LEVEL       "loglevel="


struct mhdd_config mhdd={0};

#define MHDDFS_OPT(t, p, v) { t, offsetof(struct mhdd_config, p), v }
#define MHDD_VERSION_OPT 15121974

static struct fuse_opt mhddfs_opts[]={
    MHDDFS_OPT("mlimit=%s",   mlimit_str, 0),
    MHDDFS_OPT("logfile=%s",  debug_file, 0),
    MHDDFS_OPT("loglevel=%d", loglevel,   0),

    FUSE_OPT_KEY("-V",        MHDD_VERSION_OPT),
    FUSE_OPT_KEY("--version", MHDD_VERSION_OPT),
   
    FUSE_OPT_END
};

static int mhddfs_opt_proc(void *data, 
  const char *arg, int key, struct fuse_args *outargs)
{
  switch(key)
  {
  	case MHDD_VERSION_OPT:
      fprintf(stderr, "mhddfs version: %s\n", VERSION);
      exit(0);

    case FUSE_OPT_KEY_NONOPT:
      if (!mhdd.dirs)
      {
        int count, i;
        char *argcopy=strdup(arg);
        char * next;
        for(next=argcopy, count=0; next; next=strchr(next+1, ','))
          count++;
        if (count==1) usage(stderr);

        mhdd.cdirs=count;
        mhdd.dirs=calloc(count+1, sizeof(char *));
        for(next=argcopy, i=0; next; next=strchr(next, ','))
        {
          if (*next==',') { *next=0; next++; }
          mhdd.dirs[i]=next;
          i++;
        }

        for(i=count=0; i<mhdd.cdirs; i++)
        {
          if (mhdd.dirs[i][0]=='/')
            mhdd.dirs[i]=strdup(mhdd.dirs[i]);
          else
          {
            char cpwd[PATH_MAX+1];
            getcwd(cpwd, PATH_MAX);
            mhdd.dirs[i]=create_path(cpwd, mhdd.dirs[i]);
          }
          count+=strlen(mhdd.dirs[i]);
          count++; // ';'
        }
        
        free(argcopy);

        count+=sizeof("-osubtype=mhddfs,fsname=");
        argcopy=calloc(count+2, sizeof(char *));

#if FUSE_VERSION >= 27
        strcpy(argcopy, "-osubtype=mhddfs,fsname=");
#else
        strcpy(argcopy, "-ofsname=sshfs#");
#endif
        for(i=0; i<mhdd.cdirs; i++)
        {
          if (i) strcat(argcopy, ";");
          strcat(argcopy, mhdd.dirs[i]);
        }
        fuse_opt_insert_arg(outargs, 1, argcopy);
        free(argcopy);
        return 0;
      }

      if(!mhdd.mount)
      {
        if (*arg=='/')
          mhdd.mount=strdup(arg);
        else
        {
          char cpwd[PATH_MAX+1];
          getcwd(cpwd, PATH_MAX);
          mhdd.mount=create_path(cpwd, arg);
        }
        return 1;
      }
      return -1;
  }
  return 1;
}

struct fuse_args * parse_options(int argc, char *argv[])
{
  struct fuse_args * args=calloc(1, sizeof(struct fuse_args));

  {
    struct fuse_args tmp=FUSE_ARGS_INIT(argc, argv);
    memcpy(args, &tmp, sizeof(struct fuse_args));
  }
  
  mhdd.loglevel=MHDD_DEFAULT_DEBUG_LEVEL;
  if (fuse_opt_parse(args, &mhdd, mhddfs_opts, mhddfs_opt_proc)==-1) 
    usage(stderr);

  if (!mhdd.mount) usage(stderr);

  if (mhdd.cdirs)
  {
    int i;
    for(i=0; i<mhdd.cdirs; i++)
    {
      struct stat info;
      if (stat(mhdd.dirs[i], &info))
      {
        fprintf(stderr, "mhddfs: can not stat '%s': %s\n", 
            mhdd.dirs[i], strerror(errno));
        exit(-1);
      }
      if (!S_ISDIR(info.st_mode))
      {
        fprintf(stderr, "mhddfs: '%s' - is not directory\n\n", 
            mhdd.dirs[i]);
        exit(-1);
      }

      fprintf(stderr, "mhddfs: directory '%s' added to list\n",
          mhdd.dirs[i]);
    }
  }

  fprintf(stderr, "mhddfs: mount to: %s\n", mhdd.mount);

  if (mhdd.debug_file)
  {
    fprintf(stderr, "mhddfs: using debug file: %s, loglevel=%d\n",
      mhdd.debug_file, mhdd.loglevel);
    mhdd.debug=fopen(mhdd.debug_file, "a");
    if (!mhdd.debug)
    {
      fprintf(stderr, "Can not open file '%s': %s",
        mhdd.debug_file,
        strerror(errno));
      exit(-1);
    }
    setvbuf(mhdd.debug, NULL, _IONBF, 0);
  }

  mhdd.move_limit=4ll*1024*1024*1024;
  if (mhdd.mlimit_str)
  {
    int len=strlen(mhdd.mlimit_str);
    if (len)
    {
      switch(mhdd.mlimit_str[len-1])
      {
        case 'm':
        case 'M':
          mhdd.mlimit_str[len-1]=0;
          mhdd.move_limit=atoll(mhdd.mlimit_str);
          mhdd.move_limit*=1024*1024;
          break;
        case 'g':
        case 'G':
          mhdd.mlimit_str[len-1]=0;
          mhdd.move_limit=atoll(mhdd.mlimit_str);
          mhdd.move_limit*=1024*1024*1024;
          break;

        case 'k':
        case 'K':
          mhdd.mlimit_str[len-1]=0;
          mhdd.move_limit=atoll(mhdd.mlimit_str);
          mhdd.move_limit*=1024;
          break;

        default:
          mhdd.move_limit=atoll(mhdd.mlimit_str);
          break;
      }
    }
    if (mhdd.move_limit<100*1024*1024) mhdd.move_limit=100*1024*1024;
  }
  fprintf(stderr, "mhddfs: move size limit %lld bytes\n",
    (long long)mhdd.move_limit);

  mhdd_debug(MHDD_MSG, " >>>>> mhdd " VERSION " started <<<<<\n");

  return args;
}
