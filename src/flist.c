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

#include "flist.h"
#include "debug.h"

struct flist *files=0;
static pthread_mutex_t files_lock;

// init
void flist_init(void)
{
	pthread_mutex_init(&files_lock, 0);
}

/* internal function */
static void lock_files(void)
{
	pthread_mutex_lock(&files_lock);
}

/* internal function */
static void unlock_files(void)
{
	pthread_mutex_unlock(&files_lock);
}


/* unlock one locked file */
void flist_unlock_file(struct flist * item)
{
	mhdd_debug(MHDD_DEBUG, "flist_unlock_file: %s (%s)\n",
			item->name, item->real_name);
	pthread_mutex_unlock(&item->lock);
}

static void flist_lock_file(struct flist * item)
{
	mhdd_debug(MHDD_DEBUG, "flist_lock_file: %s (%s)\n",
			item->name, item->real_name);
	pthread_mutex_lock(&item->lock);
}

// add file to list
struct flist * flist_create(const char *name,
		const char *real_name, int flags, int fh)
{
	struct flist * add=calloc(1, sizeof(struct flist));

	add->flags=flags;
	add->id=0;
	add->aid=add;
	add->name=strdup(name);
	add->real_name=strdup(real_name);
	add->fh=fh;
	pthread_mutex_init(&add->lock, 0);
	flist_lock_file(add);
	lock_files();

	add->next=files;
	if (files) files->prev=add;
	files=add;
	unlock_files();
	return add;
}

/* return (malloced & locked) array for list files with name == name */
struct flist ** flist_items_by_eq_name(struct flist * info)
{
	struct flist * next;
	struct flist ** result;
	int i, count;

	mhdd_debug(MHDD_INFO, "flist_items_by_eq_name: %s\n", info->name);
	lock_files();
	for (next=files, count=0; next; next=next->next, count++);
	if (!count) { unlock_files(); return 0; }

	result=calloc(count+1, sizeof(struct flist *));

	for (next=files, i=0; next; next=next->next)
	{
		if (strcmp(info->name, next->name)==0) result[i++]=next;
		if (info==next) continue;
		flist_lock_file(next);
	}

	unlock_files();

	if (!result[0]) { free(result); return 0; }
	return result;
}

/* return (locked) item from flist */
struct flist * flist_item_by_id(uint64_t id)
{
	struct flist * next;
	lock_files();
	for(next=files; next; next=next->next)
	{
		if (next->id!=id) continue;
		flist_lock_file(next);
		unlock_files();
		return next;
	}
	unlock_files();
	return 0;
}


/* internal function */
static void delete_item(struct flist * item, int locked)
{
	struct flist *next;
	lock_files();

	for (next=files; next; next=next->next)
	{
		if (next==item)
		{
			if (!locked) flist_lock_file(item);
			if (item->next) item->next->prev=item->prev;
			if (item->prev) item->prev->next=item->next;
			if (files==item) files=item->next;

			mhdd_debug(MHDD_DEBUG, "delete_item: %s (%s)\n",
					item->name, item->real_name);
			free(item->name);
			free(item->real_name);
			free(item);
			pthread_mutex_destroy(&item->lock);
			break;
		}
	}
	unlock_files();
}


// delete from file list
void flist_delete(struct flist * item)
{
	delete_item(item, 0);
}

// delete locked file from list
void flist_delete_locked(struct flist * item)
{
	delete_item(item, 1);
}
