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

static struct flist *files = 0;

#define flist_foreach(__next) \
	for(__next = files; __next; __next = __next->next)


static pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t files_lock;
// init
void flist_init(void)
{
	pthread_rwlock_init(&files_lock, 0);
}

// unlock semaphore
void flist_unlock(void)
{
	pthread_rwlock_unlock(&files_lock);
}

// lock for read
void flist_rdlock(void)
{
	pthread_rwlock_rdlock(&files_lock);
}

// lock for write
void flist_wrlock(void)
{
	/* get mutex before wrlock */
	pthread_mutex_lock(&files_mutex);
	pthread_rwlock_wrlock(&files_lock);
	pthread_mutex_unlock(&files_mutex);
}

// lock for write for locked
void flist_wrlock_locked(void)
{
	/* Only one thread can change lock method */
	pthread_mutex_lock(&files_mutex);
	pthread_rwlock_unlock(&files_lock);
	pthread_rwlock_wrlock(&files_lock);
	pthread_mutex_unlock(&files_mutex);
}

// add file to list
struct flist * flist_create(const char *name,
		const char *real_name, int flags, int fh)
{
	struct flist * add = calloc(1, sizeof(struct flist));

	add->flags = flags;
	add->id = 0;
	add->aid = add;
	add->name = strdup(name);
	add->real_name = strdup(real_name);
	add->fh = fh;

	flist_wrlock();
	add->next = files;
	if (files)
		files->prev = add;
	files = add;
	return add;
}

/* return (malloced & locked) array for list files with name == name */
struct flist ** flist_items_by_eq_name(struct flist * info)
{
	struct flist * next;
	struct flist ** result;
	int i = 0, count = 0;

	mhdd_debug(MHDD_INFO, "flist_items_by_eq_name: %s\n", info->name);

	flist_foreach(next)
		count++;

	if (!count)
		return 0;


	result=calloc(count+1, sizeof(struct flist *));

	flist_foreach(next) {
		if (strcmp(info->name, next->name) == 0)
			result[i++] = next;
	}

	if (!result[0]) {
		free(result);
		return 0;
	}
	return result;
}

/* return (locked) item from flist */
struct flist * flist_item_by_id(uint64_t id) {
	struct flist * next;
	flist_rdlock();
	flist_foreach(next) {
		if (next->id == id)
			return next;
	}
	flist_unlock();
	return 0;
}


/* internal function */
static void delete_item(struct flist *item, int locked)
{
	struct flist *next;

	if (locked) {
		flist_wrlock_locked();
	} else {
		flist_wrlock();
	}

	flist_foreach(next) {
		if (next == item) {
			if (item->next)
				item->next->prev = item->prev;
			if (item->prev)
				item->prev->next = item->next;
			if (files==item)
				files = item->next;

			mhdd_debug(MHDD_DEBUG, "delete_item: %s (%s)\n",
					item->name, item->real_name);
			free(item->name);
			free(item->real_name);
			free(item);
			break;
		}
	}
	flist_unlock();
}


// delete from file list
void flist_delete(struct flist *item)
{
	delete_item(item, 0);
}

// delete locked file from list
void flist_delete_locked(struct flist *item)
{
	delete_item(item, 1);
}
