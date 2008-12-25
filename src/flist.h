#include <pthread.h>
#include <stdint.h>

// opened file list
struct flist
{
    pthread_mutex_t lock;
    char        *name;
    char        *real_name;
    int         flags;
    int         fh;
    union
    {
        uint64_t    id;
        struct flist *aid;
    };
    struct flist *next, *prev;
};

void flist_init(void);

struct flist* flist_create(const char *name, 
    const char *real_name, int flags, int fh);



struct flist * flist_item_by_id(uint64_t id);
struct flist ** flist_items_by_eq_name(struct flist * info);


void flist_delete(struct flist * item);
void flist_delete_locked(struct flist * item);


void flist_unlock_file(struct flist * item);

