#ifndef __TOOLS__H__
#define __TOOLS__H__

#include <stdint.h>

// opened file list
struct files_info
{
  char        *name;
  char        *real_name;
  int         flags;
  int         fh;
  uint64_t    id;
  struct files_info   *next, *prev;
};

extern struct files_info *files;


struct files_info * add_file_list(const char *name, 
  const char *real_name, int flags, int fh);

struct files_info * get_info_by_id(uint64_t id);
void del_file_list(struct files_info * item);
int get_free_dir(void);
char * create_path(const char *dir, const char * file);
char * find_path(const char *file);
int find_path_id(const char *file);

void create_parent_dirs(int dir_id, const char *path);
int find_free_space(off_t size);


// true if success
int move_file(struct files_info * file, off_t size);


// paths
char * get_parent_path(char * path);
char * get_base_name(char *path);

#define MOVE_BLOCK_SIZE     32768

#endif
