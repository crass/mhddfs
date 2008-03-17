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

#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include "parse_options.h"
#include "usage.h"
#include "version.h"


#define MLIMIT_OPTION   "mlimit="
#define DEBUG_OPTION    "logfile="

struct mhdd_config mhdd={0};

static void remove_option(char * optarg, const char * optname)
{
  char *begin, *end;
  while((begin=strstr(optarg, optname)))
  {
    if ((end=strchr(begin, ',')))
    {
      end++;
      strcpy(begin, end);
    }
    else
    {
      *begin=0;
      if (begin>optarg) 
      {
        begin--;
        *begin=0;
      }
    }
  }
}

static char** extract_options(const char * optarg)
{
  int count, i;
  char *temp, *end, **opts;

  while(*optarg==',') optarg++;
  
  char *optstr=strdup(optarg);

  for (count=0, temp=optstr; temp; temp=strchr(temp+1, ',')) count++;

  opts=(char **)calloc(count+1, sizeof(char *));

  for (i=0, temp=optstr; temp && i<count; i++)
  {
    opts[i]=strdup(temp);
    if ((end=strchr(opts[i], ','))) *end=0;

    temp=strchr(temp, ',');
    if (temp) temp++;

    fprintf(stderr, "option: %s (%d)\n", opts[i], count);
  }

  return opts;

}

void parse_options(int * pargc, char *argv[])
{
  int argc=*pargc;
  int opt, count, i;
  char *tmp, *value;
  char **opts;

  mhdd.move_limit=4ll*1024*1024*1024;
  mhdd.debug=stderr;
  
  // parse command-line
  for(i=0; (opt=getopt(argc, argv, "Vho:"))!=-1; i++)
  {
    switch(opt)
    {
      case 'V':
        printf("mhddfs: version: %s\n", VERSION);
        exit(0);

      case 'h':
        usage(stdout);

      case 'o':

        opts=extract_options(optarg);

        for (i=0; opts[i]; i++)
        {
          value=strchr(opts[i], '=');
          if (value) value++;
          
          // mlimit=
          if (strstr(opts[i], MLIMIT_OPTION)==opts[i])
          {
            remove_option(optarg, MLIMIT_OPTION);
            if ((count=strlen(value)))
            {
              switch(value[count-1])
              {
                case 'm':
                case 'M':
                  value[count-1]=0;
                  mhdd.move_limit=atoll(value);
                  mhdd.move_limit*=1024*1024;
                  break;
                case 'g':
                case 'G':
                  value[count-1]=0;
                  mhdd.move_limit=atoll(value);
                  mhdd.move_limit*=1024*1024*1024;
                  break;

                case 'k':
                case 'K':
                  value[count-1]=0;
                  mhdd.move_limit=atoll(value);
                  mhdd.move_limit*=1024;
                  break;

                default:
                  mhdd.move_limit=atoll(value);
                  break;
              }
            }
          }


          // debug=
          if (strstr(opts[i], DEBUG_OPTION)==opts[i])
          {
            remove_option(optarg, DEBUG_OPTION);
            mhdd.debug_file=strdup(value);
          }
          free(opts[i]);
        }
        free(opts);
        break;


      default: 
        usage(stderr);
    }
  }

  // create dirlist
  if (argc-optind!=2) usage(stderr);
  char * mount_dirs=argv[optind];
  tmp=mount_dirs; count=1;

  while((tmp=strchr(tmp, ','))) { count++; tmp++; }
  if (count==1) usage(stderr);
  
  mhdd.dirs=(char **)calloc(count+1, sizeof(char *));
  mhdd.cdirs=count;
  
  for (i=0, tmp=mount_dirs; tmp; tmp=strchr(tmp, ','), i++)
  {
    if (i) { *tmp=0; tmp++; }
    mhdd.dirs[i]=tmp;
  }
  
  // verify dirlist
  for (i=0; i<count; i++)
  {
    struct stat info;
    if (stat(mhdd.dirs[i], &info))
    {
      fprintf(stderr, "mhddfs: '%s': %s\n", 
        mhdd.dirs[i], strerror(errno));
      exit(-1);
    }
    if (S_ISDIR(info.st_mode))
    {
      if (mhdd.dirs[i][0]!='/')
      {
        char *cpwd=(char *)calloc(PATH_MAX+1, sizeof(char));
        getcwd(cpwd, PATH_MAX);
        if (cpwd[strlen(cpwd)-1]=='/')
        {
          tmp=(char *)malloc(strlen(cpwd)+strlen(mhdd.dirs[i])+1);
          sprintf(tmp, "%s%s", cpwd, mhdd.dirs[i]);
          mhdd.dirs[i]=tmp;
        }
        else
        {
          tmp=(char *)malloc(strlen(cpwd)+strlen(mhdd.dirs[i])+2);
          sprintf(tmp, "%s/%s", cpwd, mhdd.dirs[i]);
          mhdd.dirs[i]=tmp;
        }
        free(cpwd);
      }
    }
    else
    {
      fprintf(stderr, "mhddfs: '%s' - is not directory\n\n", 
        mhdd.dirs[i]);
      exit(-1);
    }
  }
  
  // remove dirlist argument from argv
  argv[argc-2]=argv[argc-1]; argc--;

  // if -o value is empty - remove option
  for (i=1; i<argc; i++)
  {
    if (strcmp(argv[i], "-o")==0)
    {
      if (strlen(argv[i+1])) break;
      memmove(argv+i, argv+i+2, argc-i-2);
      argc-=2;
      break;
    }
  }

  if (mhdd.move_limit<100*1024*1024)
    mhdd.move_limit=100*1024*1024;

  mhdd.mount=argv[argc-1];
  for (i=0; i<mhdd.cdirs; i++)
    fprintf(stderr, "mhddfs: directory '%s' added to list\n",
      mhdd.dirs[i]);
  fprintf(stderr, "mhddfs: move size limit %lld bytes\n",
    (long long)mhdd.move_limit);
  fprintf(stderr, "mhddfs: mount point '%s'\n", mhdd.mount);

  

  if (mhdd.debug_file)
  {
    fprintf(stderr, "Using debug file: %s\n", mhdd.debug_file);
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

  char tstr[64];
  time_t t=time(0);
  struct tm *lt;
  lt=localtime(&t);
  strftime(tstr, 64, "%Y-%m-%d %H:%M:%S", lt);
  fprintf(mhdd.debug, "\nmhddfs: >> started at %s <<\n", tstr);

  *pargc=argc;

  for (i=0; i<argc; i++) fprintf(stderr, "argv[%d]=%s\n", i, argv[i]);
}
