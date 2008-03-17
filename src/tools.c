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
#include "parse_options.h"

struct files_info *files=0;

static uint64_t get_new_id(void)
{
  static uint64_t id=0;
  struct files_info * next;

  for(id++;;id++)
  {
    for(next=files; next; next=next->next)
    {
      if (next->id==id) break;
    }
    if (next) continue;
    return id;
  }
}

// add file to list
struct files_info * add_file_list(const char *name, 
  const char *real_name, int flags, int fh)
{
  struct files_info * add=calloc(1, sizeof(struct files_info));

  add->flags=flags;
  add->id=get_new_id();
  add->name=strdup(name);
  add->real_name=strdup(real_name);
  add->fh=fh;

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
  return add;
}

struct files_info * get_info_by_id(uint64_t id)
{
  struct files_info * next;
  for(next=files; next; next=next->next)
  {
    if (next->id==id) return next;
  }

  fprintf(mhdd.debug, "get_info_by_id: "
    "fileno %llu not found, fileno list:", id);
  for (next=files; next; next=next->next)
  {
    fprintf(mhdd.debug, " %llu", next->id);
  }
  fprintf(mhdd.debug, "\n");
  return 0;
}

// delete from file list
void del_file_list(struct files_info * item)
{
  if (item==files)
  {
    if (files->next) files=files->next;
    else files=files->prev;
  }

  if (item->next) item->next->prev=item->prev;
  if (item->prev) item->prev->next=item->next;
  free(item->name);
  free(item->real_name);
  free(item);
}

// get diridx for maximum free space
int get_free_dir(void)
{
  int i, max, cfg=-1;
  struct statvfs *stats=calloc(mhdd.cdirs, sizeof(struct statvfs));
  fsblkcnt_t * free_size=calloc(mhdd.cdirs, sizeof(fsblkcnt_t));

  for (max=i=0; i<mhdd.cdirs; i++)
  {
    statvfs(mhdd.dirs[i], stats+i);
    free_size[i] =stats[i].f_bsize;
    free_size[i] *=stats[i].f_bfree;
    if(free_size[i]>free_size[max]) max=i;
    if (cfg==-1 && free_size[i]>=mhdd.move_limit) cfg=i;
  }

  free(stats);
  free(free_size);
  if (cfg==-1) return max;
  fprintf(mhdd.debug, "cfg:max = %d:%d\n", cfg, max);
  return cfg;
}

// find mount point with free space > size
// -1 if not found
int find_free_space(off_t size)
{
  int i, max, cfg=-1;
  struct statvfs *stats=calloc(mhdd.cdirs, sizeof(struct statvfs));
  fsblkcnt_t * free_size=calloc(mhdd.cdirs, sizeof(fsblkcnt_t));

  for (max=i=0; i<mhdd.cdirs; i++)
  {
    statvfs(mhdd.dirs[i], stats+i);
    free_size[i] =stats[i].f_bsize;
    free_size[i] *=stats[i].f_bfree;

    fprintf(mhdd.debug, "directory %s free: %llu size: %llu\n", mhdd.dirs[i], free_size[i], size);

    if (free_size[i]>free_size[max]) max=i;
    if (cfg==-1 && free_size[i]>=size+mhdd.move_limit) cfg=i;
  }

  if (free_size[max]<size) max=-1;

  free(stats);
  free(free_size);
  if (cfg==-1) return max;
  return cfg;
}

static int reopen_files(struct files_info * file, const char *new_name)
{
  struct files_info * next;

  // move to top list
  if (file!=files)
  {
    if (file->next) file->next->prev=file->prev;
    if (file->prev) file->prev->next=file->next;
    file->next=files;
    file->prev=0;
    files=file;
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
      fprintf(mhdd.debug, "reopen_files: error reopen: %s\n",
        strerror(errno));
      return -errno;
    }

    // seek
    if (seek!=lseek(fh, seek, SEEK_SET))
    {
      fprintf(mhdd.debug, "reopen_files: error seek %s\n",
        strerror(errno));
      close(fh);
      return(-1);
    }

    // filehandle
    fprintf(mhdd.debug, "reopen_files: file %s old h=%x new h=%x\n",
      next->real_name, next->fh, fh);
    next->fh=fh;

    fprintf(mhdd.debug, "reopen_files: file %s reopened to %s\n",
      next->real_name, new_name);
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

  fprintf(mhdd.debug, "move_file: move %s to %s\n", from, to);

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
      fprintf(mhdd.debug, "move_file: error move data\n");
      return -1;
    }
  }
  free(buf);

  fprintf(mhdd.debug, "move_file: done move data\n");
  fclose(input);
  fclose(output);

  struct stat st;
  if (stat(to, &st)!=0)
  {
    fprintf(mhdd.debug, "move_file: error stat %s\n", to);
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
  fprintf(mhdd.debug, "move_file: end, code=%d\n", ret);
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

/*   fprintf(mhdd.debug, "create_path: %s\n", path); */
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


void create_parent_dirs(int dir_id, const char *path)
{
  char *parent=strdup(path);
  int len=strlen(path);

  // remove child name from path
  if (len && parent[len-1]=='/') parent[--len]=0;
  while(len && parent[len-1]!='/') parent[--len]=0;
  if (len && parent[len-1]=='/') parent[--len]=0;
  if (!len) { free(parent); return; }

  struct stat st;

  char *full_path=create_path(mhdd.dirs[dir_id], parent);

  // parent exists
  if (stat(full_path, &st)==0)
  {
    free(parent);
    free(full_path);
    return;
  }
  free(full_path);

  // path must be exists in one dirs
  char *exists=find_path(parent);
  if (exists) free(exists);
  else { free(parent); return;  }
  
  // create path
  char *end;
  
  for(end=strchr(parent, '/');;end=strchr(end+1, '/'))
  {
    if (end) *end=0;

    exists=find_path(parent);
    lstat(exists, &st);
    char *fpath=create_path(mhdd.dirs[dir_id], parent);

    struct stat est;
    if (stat(fpath, &est)!=0)
    {
      mkdir(fpath, st.st_mode&(S_IRWXU|S_IRWXG|S_IRWXO));
      chown(fpath, st.st_uid, st.st_gid);
      chmod(fpath, st.st_mode);
    }
    free(fpath);

    if (end) *end='/';
    else break;
  }
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
