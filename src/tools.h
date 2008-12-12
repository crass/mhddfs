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
#ifndef __TOOLS__H__
#define __TOOLS__H__

#include <stdint.h>
#include <pthread.h>

// opened file list
struct files_info
{
    pthread_mutex_t lock;
    char        *name;
    char        *real_name;
    int         flags;
    int         fh;
    uint64_t    id;
    struct files_info   *next, *prev;
};

extern struct files_info *files;

void mhdd_tools_init(void);

struct files_info * add_file_list(const char *name, 
  const char *real_name, int flags, int fh);

struct files_info * get_info_by_id(uint64_t id);
void del_file_list(struct files_info * item);
int get_free_dir(void);
char * create_path(const char *dir, const char * file);
char * find_path(const char *file);
int find_path_id(const char *file);

int create_parent_dirs(int dir_id, const char *path);


// true if success
int move_file(struct files_info * file, off_t size);


// paths
char * get_parent_path(const char * path);
char * get_base_name(const char *path);

void lock_files(void);
void unlock_files(void);

#define MOVE_BLOCK_SIZE     32768

#endif
