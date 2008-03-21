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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <utime.h>
#include <fcntl.h>

#include "tools.h"
#include "debug.h"
#include "parse_options.h"

struct files_info *files=0;
static pthread_mutex_t files_lock;

static uint64_t get_new_id(void)
{
  static uint64_t id=0;
  struct files_info * next;

  for(id++;;id++)
  {
    if (!id) continue;
    for(next=files; next; next=next->next)
    {
      if (next->id==id) break;
    }
    if (next) continue;
    return id;
  }
}

void lock_files(void)
{
  pthread_mutex_lock(&files_lock);
}

void unlock_files(void)
{
  pthread_mutex_unlock(&files_lock);
}

// add file to list
struct files_info * add_file_list(const char *name, 
  const char *real_name, int flags, int fh)
{
  struct files_info * add=calloc(1, sizeof(struct files_info));

  lock_files();

  add->flags=flags;
  add->id=get_new_id();
  add->name=strdup(name);
  add->real_name=strdup(real_name);
  add->fh=fh;
  pthread_mutex_init(&add->lock, 0);

  if (files)
  {
    files->prev=add;
    add->next=files;
    files=add;
  }
  else
  {
    files=add;
  }

  unlock_files();
  return add;
}

struct files_info * get_info_by_id(uint64_t id)
{
  struct files_info * next;
  for(next=files; next; next=next->next)
  {
    if (next->id==id) return next;
  }

  mhdd_debug(MHDD_DEBUG, "get_info_by_id: "
    "fileno %llu not found, fileno list:", id);
  for (next=files; next; next=next->next)
  {
    mhdd_debug(MHDD_DEBUG, " %llu", next->id);
  }
  mhdd_debug(MHDD_DEBUG, "\n");
  return 0;
}
void mhdd_tools_init(void)
{
  pthread_mutex_init(&files_lock, 0);
}

// delete from file list
void del_file_list(struct files_info * item)
{
  struct files_info *next;
  lock_files();

  for (next=files; next; next=next->next)
  {
    if (next==item) 
    {
      pthread_mutex_lock(&item->lock);
      if (item==files)
      {
        if (files->next) files=files->next;
        else files=files->prev;
      }

      if (item->next) item->next->prev=item->prev;
      if (item->prev) item->prev->next=item->next;
      
      pthread_mutex_unlock(&item->lock);
      free(item->name);
      free(item->real_name);
      free(item);
      break;
    }
  }
  unlock_files();
}

// get diridx for maximum free space
int get_free_dir(void)
{
  int i, max, cfg=-1;
  struct statvfs stf;
  fsblkcnt_t max_space=0;

  for (max=i=0; i<mhdd.cdirs; i++)
  {
    if (statvfs(mhdd.dirs[i], &stf)!=0) continue;
    fsblkcnt_t space  = stf.f_bsize;
    space *= stf.f_bavail;

    if(space>max_space)
    {
      max_space=space;
      max=i;
    }
    if (cfg==-1 && space>=mhdd.move_limit) cfg=i;
  }

  return (cfg==-1)?max:cfg;
}

// get diridx for maximum free space
int get_free_dir_by_path(const char *path)
{
  if (strcmp(path, "/")==0) return get_free_dir();

  int i, max_id;
  fsblkcnt_t max_space=0;
  
  for (max_id=-1,i=0; i<mhdd.cdirs; i++)
  {
    struct stat st;
    char *dir=create_path(mhdd.dirs[i], path);
    int ret=lstat(dir, &st);
    free(dir);
    if (ret!=0) continue;
    struct statvfs stf;
    if (statvfs(mhdd.dirs[i], &stf)==0)
    {
      fsblkcnt_t space  = stf.f_bsize;
      space *= stf.f_bavail;

      if (space>max_space||max_id==-1) 
      {
        max_space=space;
        max_id=i;
      }
    }
  }
  return max_id;
}

// find mount point with free space > size
// -1 if not found
static int find_free_space(off_t size)
{
  int i, max;
  struct statvfs stf;
  fsblkcnt_t max_space=0;

  for (max=-1,i=0; i<mhdd.cdirs; i++)
  {
    if (statvfs(mhdd.dirs[i], &stf)!=0) continue;
    fsblkcnt_t space  = stf.f_bsize;
    space *= stf.f_bavail;

    if (space>size+mhdd.move_limit) return i;

    if (space>size)
    {
      max_space=space;
      max=i;
    }
  }
  return max;
}

static int reopen_files(struct files_info * file, const char *new_name)
{
  struct files_info * next;

  // move to top list
  if (file!=files)
  {
    lock_files();
    if (file->next) file->next->prev=file->prev;
    if (file->prev) file->prev->next=file->next;
    file->next=files;
    file->prev=0;
    files=file;
    unlock_files();
  }

  // reopen files
  for (next=files; next; next=next->next)
  {
    if (strcmp(next->name, file->name)!=0) continue;
    off_t seek=lseek(next->fh, 0, SEEK_CUR);
    int flags=next->flags;
    if (flags&=~(O_EXCL|O_TRUNC));

    
    // open
    int fh=open(new_name, flags);
    if (fh==-1)
    {
      mhdd_debug(MHDD_INFO, "reopen_files: error reopen: %s\n",
        strerror(errno));
      return -errno;
    }

    // seek
    if (seek!=lseek(fh, seek, SEEK_SET))
    {
      mhdd_debug(MHDD_INFO, "reopen_files: error seek %s\n",
        strerror(errno));
      close(fh);
      return(-1);
    }

    // filehandle
    if (dup2(fh, next->fh)!=next->fh)
    {
      mhdd_debug(MHDD_INFO, "reopen_files: error dup2 %s\n",
        strerror(errno));
      close(fh);
      return(-1);
    }

    close(fh);
    mhdd_debug(MHDD_MSG, "reopen_files: file %s (to %s) old h=%x new h=%x\n",
      next->real_name, new_name, next->fh, fh);

    free(next->real_name);
    next->real_name=strdup(new_name);
  }
  return 0;
}

int move_file(struct files_info * file, off_t wsize)
{
  char *from, *to, *buf;
  off_t size;
  FILE *input, *output;
  int ret;
  
  size=lseek(file->fh, 0, SEEK_END);
  if (size==-1) return -errno;

  if (size<wsize) size=wsize;

  int dir_id=find_free_space(size);

  if (dir_id==-1) return -1;
  from=file->real_name;

  create_parent_dirs(dir_id, file->name);

  input=fopen(from, "r");

  if (!input) { return -errno; }
  
  to=create_path(mhdd.dirs[dir_id], file->name);
  output=fopen(to, "w+");
  if (!output)
  {
    free(to);
    return -errno;
  }

  mhdd_debug(MHDD_MSG, "move_file: move %s to %s\n", from, to);

  // move data
  buf=(char *)calloc(sizeof(char), MOVE_BLOCK_SIZE);
  while((size=fread(buf, sizeof(char), MOVE_BLOCK_SIZE, input)))
  {
    if (size!=fwrite(buf, sizeof(char), size, output))
    {
      fclose(output);
      fclose(input);
      free(buf);
      unlink(to);
      free(to);
      mhdd_debug(MHDD_MSG, "move_file: error move data\n");
      return -1;
    }
  }
  free(buf);

  mhdd_debug(MHDD_MSG, "move_file: done move data\n");
  fclose(input);
  fclose(output);

  struct stat st;
  if (stat(to, &st)!=0)
  {
    mhdd_debug(MHDD_MSG, "move_file: error stat %s\n", to);
    unlink(to);
    free(to);
    return -1;
  }

  // owner/group
  chmod(to, st.st_mode);
  chown(to, st.st_uid, st.st_gid);

  // time
  struct utimbuf ftime={0};
  ftime.actime=st.st_atime;
  ftime.modtime=st.st_mtime;
  utime(to, &ftime);

  from=strdup(from);
  ret=reopen_files(file, to);
  
  if (ret==0) unlink(from);
  else unlink(to);
  mhdd_debug(MHDD_MSG, "move_file: end, code=%d\n", ret);
  free(to);
  free(from);
  return ret;
}

char * create_path(const char *dir, const char * file)
{
  if (file[0]=='/') file++;
  int plen=strlen(dir);
  int flen=strlen(file);

  char *path=(char *)calloc(plen+flen+2, sizeof(char));

  if (dir[plen-1]=='/')
  {
    sprintf(path, "%s%s", dir, file);
  }
  else
  {
    sprintf(path, "%s/%s", dir, file);
  }

  plen=strlen(path);
  if (plen>1 && path[plen-1]=='/') path[plen-1]=0;

  mhdd_debug(MHDD_DEBUG, "create_path: %s\n", path);
  return(path);
}

char * find_path(const char *file)
{
  int i;
  struct stat st;

  for (i=0; i<mhdd.cdirs; i++)
  {
    char *path=create_path(mhdd.dirs[i], file);
    if (lstat(path, &st)==0) return path;
    free(path);
  }
  return 0;
}

int find_path_id(const char *file)
{
  int i;
  struct stat st;

  for (i=0; i<mhdd.cdirs; i++)
  {
    char *path=create_path(mhdd.dirs[i], file);
    if (lstat(path, &st)==0) 
    {
      free(path);
      return i;
    }
    free(path);
  }
  return -1;
}


int create_parent_dirs(int dir_id, const char *path)
{
  mhdd_debug(MHDD_DEBUG, "create_parent_dirs: dir_id=%d, path=%s\n", dir_id, path);
  char *parent=get_parent_path(path);
  if (!parent) return 0;

  char *exists=find_path(parent);
  if (!exists) { free(parent); errno=EFAULT; return -errno; }


  char *path_parent=create_path(mhdd.dirs[dir_id], parent);
  struct stat st;

  // already exists
  if (stat(path_parent, &st)==0)
  {
    free(exists);
    free(path_parent);
    free(parent);
    return 0;
  }
  
  // create parent dirs
  int res=create_parent_dirs(dir_id, parent);

  if (res!=0)
  {
    free(path_parent);
    free(parent); 
    free(exists); 
    return res; 
  }
  
  // get stat from exists dir
  if (stat(exists, &st)!=0)
  {
    free(exists);
    free(path_parent);
    free(parent);
    return -errno;
  }
  res=mkdir(path_parent, st.st_mode);
  if (res==0)
  {
    chown(path_parent, st.st_uid, st.st_gid);
    chmod(path_parent, st.st_mode);
  }
  else
  {
    res=-errno;
    mhdd_debug(MHDD_DEBUG, "create_parent_dirs: can not create dir %s: %s\n",
      path_parent,
      strerror(errno));
  }
  free(exists);
  free(path_parent);
  free(parent);
  return res;
}

char * get_parent_path(const char * path)
{
  char *dir=strdup(path);
  int len=strlen(dir);
  if (len && dir[len-1]=='/') dir[--len]=0;
  while(len && dir[len-1]!='/') dir[--len]=0;
  if (len>1 && dir[len-1]=='/') dir[--len]=0;
  if (len) return dir;
  free(dir);
  return 0;
}

char * get_base_name(const char *path)
{
  char *dir=strdup(path);
  int len=strlen(dir);
  if (len && dir[len-1]=='/') dir[--len]=0;
  char *file=strrchr(dir, '/');
  if (file)
  {
    file++;
    strcpy(dir, file);
  }
  return dir;
}
