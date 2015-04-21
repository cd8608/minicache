#ifndef _SHFS_FIO_
#define _SHFS_FIO_

#include "shfs_defs.h"
#include "shfs.h"
#include "shfs_btable.h"
#include "shfs_cache.h"
#include "likely.h"

#define SHFS_HASH_INDICATOR_PREFIX '?' /* has to be the same as HTTPURL_ARGS_INDICATOR_PREFIX in http.c */

typedef struct shfs_bentry *SHFS_FD;

/**
 * Opens a file/object via hash or name depending on
 * the first character of path:
 *
 * Hash: "?024a5bec"
 * Name: "index.html"
 */
SHFS_FD shfs_fio_open(const char *path);
void shfs_fio_close(SHFS_FD f);

/**
 * File/object information
 */
void shfs_fio_mime(SHFS_FD f, char *out, size_t outlen); /* null-termination is ensured */
void shfs_fio_name(SHFS_FD f, char *out, size_t outlen); /* null-termination is ensured */
void shfs_fio_hash(SHFS_FD f, hash512_t out);
void shfs_fio_size(SHFS_FD f, uint64_t *out);
#define shfs_fio_islink(f) \
	(SHFS_HENTRY_ISLINK((f)->hentry))

/* file container size in chunks */
#define shfs_fio_size_chks(f) \
	(DIV_ROUND_UP(((f)->hentry->f_attr.offset + (f)->hentry->f_attr.len), shfs_vol.chunksize))

/* volume chunk address of file chunk address */
#define shfs_volchk_fchk(f, fchk) \
	((f)->hentry->f_attr.chunk + (fchk))

/* volume chunk address of file byte offset */
#define shfs_volchk_foff(f, foff) \
	(((f)->hentry->f_attr.offset + (foff)) / shfs_vol.chunksize + (f)->hentry->f_attr.chunk)
/* byte offset in volume chunk of file byte offset */
#define shfs_volchkoff_foff(f, foff) \
	(((f)->hentry->f_attr.offset + (foff)) % shfs_vol.chunksize)

/* Check macros to test if a address is within file bounds */
#define shfs_is_fchk_in_bound(f, fchk) \
	(shfs_fio_size_chks((f)) > (fchk))
#define shfs_is_foff_in_bound(f, foff) \
	((f)->hentry->f_attr.len > (foff))

/*
 * Simple but synchronous file read
 * Note: Busy-waiting is used
 */
/* direct read */
int shfs_fio_read(SHFS_FD f, uint64_t offset, void *buf, uint64_t len);
/* read is using cache */
int shfs_fio_cache_read(SHFS_FD f, uint64_t offset, void *buf, uint64_t len);

/*
 * Async file read
 */
static inline int shfs_fio_cache_aread(SHFS_FD f, chk_t offset, shfs_aiocb_t *cb, void *cb_cookie, void *cb_argp, struct shfs_cache_entry **cce_out, SHFS_AIO_TOKEN **t_out)
{
    register chk_t addr;

    if (!(shfs_is_fchk_in_bound(f, offset)))
	return -EINVAL;
    addr = shfs_volchk_fchk(f, offset);
    return shfs_cache_aread(addr, cb, cb_cookie, cb_argp, cce_out, t_out);
}

#endif /* _SHFS_FIO_ */
