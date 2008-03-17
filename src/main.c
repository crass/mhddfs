#define _XOPEN_SOURCE 500
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/stat.h>

#include "parse_options.h"
#include "tools.h"

// getattr
static int mhdd_stat(const char *file_name, struct stat *buf)
{
  fprintf(mhdd.debug, "mhdd_stat: %s\n", file_name);
  char *path=find_path(file_name);
  if (path)
  {
    int ret=lstat(path, buf);
    free(path);
    if (ret==-1) return -errno;
    return 0;
  }
/*   fprintf(mhdd.debug, "mhdd_stat: %s not found\n", file_name); */
  memset(buf, 0, sizeof(struct stat));
  errno=ENOENT;
  return -errno;
}


static int mhdd_statfs(const char *path, struct statvfs *buf)
{
  int i;
  struct statvfs * stats;

  fprintf(mhdd.debug, "mhdd_statfs: %s\n", path);

  stats=calloc(mhdd.cdirs, sizeof(struct statvfs));

  for (i=0; i<mhdd.cdirs; i++)
  {
    int ret=statvfs(mhdd.dirs[i], stats+i);
    if (ret!=0)
    {
      free(stats);
      return -errno;
    }
  }

  unsigned long 
    min_block=stats[0].f_bsize,
    min_frame=stats[0].f_frsize;

/*   fprintf(mhdd.debug, "bs fs min/max: %lu/%lu %lu/%lu\n", */
/*     min_block, max_block, min_frame, max_frame); */

  for (i=1; i<mhdd.cdirs; i++)
  {
    if (min_block>stats[i].f_bsize) min_block=stats[i].f_bsize;
    if (min_frame>stats[i].f_frsize) min_frame=stats[i].f_frsize;
  }
  if (!min_block) min_block=512;
  if (!min_frame) min_frame=512;

  for (i=0; i<mhdd.cdirs; i++)
  {
    if (stats[i].f_bsize>min_block)
    {
      stats[i].f_bfree    *=  stats[i].f_bsize/min_block;
      stats[i].f_bavail   *=  stats[i].f_bsize/min_block;
      stats[i].f_bsize    =   min_block;
    }
    if (stats[i].f_frsize>min_frame)
    {
      stats[i].f_blocks   *=  stats[i].f_frsize/min_frame;
      stats[i].f_frsize   =   min_frame;
    }
  }

  memcpy(buf, stats, sizeof(struct statvfs));
  
  for (i=1; i<mhdd.cdirs; i++)
  {
    if (buf->f_namemax<stats[i].f_namemax)
    {
      buf->f_namemax=stats[i].f_namemax;
    }
    buf->f_ffree  +=  stats[i].f_ffree;
    buf->f_files  +=  stats[i].f_files;
    buf->f_favail +=  stats[i].f_favail;
    buf->f_bavail +=  stats[i].f_bavail;
    buf->f_bfree  +=  stats[i].f_bfree;
    buf->f_blocks +=  stats[i].f_blocks;
  }
  free(stats);

  return 0;
}

static int mhdd_readdir(
  const char *dirname,
  void *buf,
  fuse_fill_dir_t filler,
  off_t offset,
  struct fuse_file_info * fi)
{
  int i, j;
  
  fprintf(mhdd.debug, "mhdd_readdir: %s\n", dirname);
  char **dirs=(char **)calloc(mhdd.cdirs+1, sizeof(char *));
  
  struct dir_item
  {
    char            * name;
    struct stat     * st;
    struct dir_item * next, * prev;
  } *dir=0, *item;


  struct stat st;

  // find all dirs
  for(i=j=0; i<mhdd.cdirs; i++)
  {
    char *path=create_path(mhdd.dirs[i], dirname);
    if (stat(path, &st)==0)
    {
      if (S_ISDIR(st.st_mode))
      {
        dirs[j]=path;
        j++;
        continue;
      }
    }
    free(path);
  }

  // read directories
  for (i=0; dirs[i]; i++)
  {
    DIR * dh=opendir(dirs[i]);
    if (!dh) continue;
    struct dirent *de;
    
    while((de=readdir(dh)))
    {
      // find dups
      struct dir_item *prev;
      for(prev=dir; prev; prev=prev->next)
        if (strcmp(prev->name, de->d_name)==0) break;
      if (prev) continue;

      // add item
      char *object_name=create_path(dirs[i], de->d_name);
      struct dir_item *new_item=calloc(1, sizeof(struct dir_item));
      
      new_item->name=calloc(strlen(de->d_name)+1, sizeof(char));
      strcpy(new_item->name, de->d_name);
      new_item->st=calloc(1, sizeof(struct stat));
      lstat(object_name, new_item->st);

      if (dir) { dir->prev=new_item; new_item->next=dir; }
      dir=new_item;
      free(object_name);
    }
    
    closedir(dh);
  }

  // fill list
  for(item=dir; item; item=item->next)
  {
    if (filler(buf, item->name, item->st, 0)) break;
  }

  // free memory
  while(dir)
  {
    free(dir->name);
    free(dir->st);
    if (dir->next) 
    { 
      dir=dir->next; 
      free(dir->prev); 
      continue;
    }
    free(dir); dir=0;
  }
  for (i=0; dirs[i]; i++) free(dirs[i]);
  free(dirs);
  return 0;
}

// readlink
static int mhdd_readlink(const char *path, char *buf, size_t size)
{
  fprintf(mhdd.debug, "mhdd_readlink: %s, size=%d\n", path, size);

  char *link=find_path(path);
  if (link)
  {
    int res=readlink(link, buf, size);
    free(link);
    if (res>=0) return 0;
  }
  return -1;
}

// create
static int mhdd_create(const char *file, 
  mode_t mode, struct fuse_file_info *fi)
{
  fprintf(mhdd.debug, "mhdd_create: %s, handle=%d\n", file, fi->flags);
  
  char *path=find_path(file);

  if (!path)
  {
    int dir_id=get_free_dir();
    create_parent_dirs(dir_id, file);
    path=create_path(mhdd.dirs[dir_id], file);
  }
  
  int fd=open(path, fi->flags, mode);
  if (fd==-1)
  {
    free(path);
    return -errno;
  }
  struct files_info *add=add_file_list(file, path, fi->flags, fd);
  fi->fh=add->id;
  free(path);
  return 0;
}


// open
static int mhdd_fileopen(const char *file, struct fuse_file_info *fi)
{
  fprintf(mhdd.debug, "mhdd_fileopen: %s, flags=%04X\n", file, fi->flags);

  char *path=find_path(file);

  if (!path)
  {
    int dir_id=get_free_dir();
    create_parent_dirs(dir_id, file);
    path=create_path(mhdd.dirs[dir_id], file);
  }
  
  int fd=open(path, fi->flags);
  if (fd==-1)
  {
    free(path);
    return -errno;
  }
  struct files_info *add=add_file_list(file, path, fi->flags, fd);
  fi->fh=add->id;
  free(path);
  return 0;
}



// close
static int mhdd_release(const char *path, struct fuse_file_info *fi)
{
  fprintf(mhdd.debug, "mhdd_release: %s, handle=%lld\n", path, fi->fh);
  struct files_info *del=get_info_by_id(fi->fh);
  if (!del)
  {
    fprintf(mhdd.debug, "mhdd_release: unknown file number: %llu\n", fi->fh);
    errno=EBADF;
    return -errno;
  }

  close(del->fh);
  del_file_list(del);
  return 0;
}

// read
static int mhdd_read(const char *path, char *buf, size_t count, off_t offset,
         struct fuse_file_info *fi)
{
  ssize_t res;
  struct files_info *info=get_info_by_id(fi->fh);
/*   fprintf(mhdd.debug, "mhdd_read: %s, handle=%lld\n", path, fi->fh); */

  if (!info)
  {
    errno=EBADF;
    return -errno;
  }

  res=pread(info->fh, buf, count, offset);
  if (res==-1) return -errno;
  return res;
}

// write
static int mhdd_write(const char *path, const char *buf, size_t count,
  off_t offset, struct fuse_file_info *fi)
{
  ssize_t res;
  struct files_info *info=get_info_by_id(fi->fh);
/*   fprintf(mhdd.debug, "mhdd_write: %s, handle=%lld\n", path, fi->fh); */

  if (!info)
  {
    fprintf(mhdd.debug, "mhdd_write: unknown file number: %llu", fi->fh);
    errno=EBADF;
    return -errno;
  }
  res=pwrite(info->fh, buf, count, offset);
  if (res==-1)
  {
    // end free space
    if (errno==ENOSPC)
    {
      if (move_file(info, offset+count)==0) 
      {
        res=pwrite(info->fh, buf, count, offset);
        if (res==-1) 
        {
/*           fprintf(mhdd.debug, "mhdd_write: error restart write: %s\n", */
/*             strerror(errno)); */
          return -errno;
        }
        return res;
      }
      errno=ENOSPC;
    }
    return -errno;
  }
  return res;
}

// truncate
static int mhdd_truncate(const char *path, off_t size)
{
  char *file=find_path(path);
  fprintf(mhdd.debug, "mhdd_truncate: %s\n", path);
  if (file)
  {
    int res=truncate(file, size);
    free(file);
    if (res==-1) return -errno;
    return 0;
  }
  errno=ENOENT;
  return -errno;
}

// ftrucate
static int mhdd_ftruncate(const char *path, off_t size, 
  struct fuse_file_info *fi)
{
  int res;
  struct files_info *info=get_info_by_id(fi->fh);
  fprintf(mhdd.debug, "mhdd_ftruncate: %s, handle=%lld\n", path, fi->fh);

  if (!info)
  {
    errno=EBADF;
    return -errno;
  }

  res=ftruncate(info->fh, size);
  if (res==-1) return -errno;
  return 0;
}

// access
static int mhdd_access(const char *path, int mask)
{
  fprintf(mhdd.debug, "mhdd_access: %s mode=%04X\n", path, mask);
  char *file=find_path(path);
  if (file)
  {
    int res=access(file, mask);
    free(file);
    if (res==-1) return -errno;
    return 0;
  }
  
  errno=ENOENT;
  return -errno;
}

// mkdir
static int mhdd_mkdir(const char * path, mode_t mode)
{
  char *parent=strdup(path);
  int len=strlen(parent);
  if (len && parent[len-1]=='/') parent[--len]=0;
  while(len && parent[len-1]!='/') parent[--len]=0;

  char *parent_full=find_path(parent);
  if (parent_full)
  {
    char *npath=strdup(path);
    int len=strlen(path);
    if (len && npath[len-1]=='/') npath[--len]=0;

    char * object=strrchr(npath, '/');
    if (object) object++;
    else object=npath;

    object=create_path(parent_full, object);
    free(npath);
      
    int res=mkdir(object, mode);
    free(parent_full);
    free(parent);
    free(object);
    if (res==-1)
    {
      if (errno==ENOSPC)
      {
        fprintf(mhdd.debug, "mhdd_mkdir: %s, change hdd\n", strerror(errno));
        int dir_id=get_free_dir();
        create_parent_dirs(dir_id, path);
        object=create_path(mhdd.dirs[dir_id], path);
        res=mkdir(object, mode);
        free(object);
        if (res==0) return 0;
      }
      return -errno;
    }
    return 0;
  }
  free(parent);
  errno=EFAULT;
  return -errno;
}

// rmdir
static int mhdd_rmdir(const char * path)
{
  char *dir;
  while((dir=find_path(path)))
  {
    int res=rmdir(dir);
    free(dir);
    if (res==-1) return -errno;
  }
  return 0;
}

// unlink
static int mhdd_unlink(const char *path)
{
  char *file=find_path(path);
  if (file)
  {
    int res=unlink(file);
    free(file);
    if (res==-1) return -errno;
    return 0;
  }
  errno=ENOENT;
  return -errno;
}

// rename
static int mhdd_rename(const char *from, const char *to)
{
  int from_dir_id=find_path_id(from);

  if (from_dir_id==-1)
  {
    errno=ENOENT;
    return -errno;
  }

  char * to_parent=get_parent_path(to);
  if (to_parent) 
  {
    char * to_find_parent=find_path(to_parent);
    free(to_parent);
    
    if (!to_find_parent)
    {
      errno=ENOENT;
      return -errno;
    }
    free(to_find_parent);
    // to-parent exists
    // from exists
    create_parent_dirs(from_dir_id, to);
    char *obj_to    = create_path(mhdd.dirs[from_dir_id], to);
    char *obj_from  = create_path(mhdd.dirs[from_dir_id], from);

    int res=rename(obj_from, obj_to);
    free(obj_to);
    free(obj_from);

    if (res==-1) return -errno;
    return 0;
  }
  else
  {
    errno=ENOENT;
    return -errno;
  }


  return 0;
}

// functions links
static struct fuse_operations mhdd_oper = 
{
  .getattr    = mhdd_stat,
  .statfs     = mhdd_statfs,
  .readdir    = mhdd_readdir,
  .readlink   = mhdd_readlink,
  .open       = mhdd_fileopen,
  .release    = mhdd_release,
  .read       = mhdd_read,
  .write      = mhdd_write,
  .create     = mhdd_create,
  .truncate   = mhdd_truncate,
  .ftruncate  = mhdd_ftruncate,
  .access     = mhdd_access,
  .mkdir      = mhdd_mkdir,
  .rmdir      = mhdd_rmdir,
  .unlink     = mhdd_unlink,
  .rename     = mhdd_rename,
};


// start
int main(int argc, char *argv[])
{
  parse_options(&argc, argv);
  return fuse_main(argc, argv, &mhdd_oper, 0);
}
