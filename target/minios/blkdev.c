#include <mini-os/os.h>
#include <mini-os/types.h>
#include <mini-os/xmalloc.h>
#include <xenbus.h>
#include <limits.h>
#include <errno.h>

#include <target/blkdev.h>

#ifndef container_of
/* NOTE: This is copied from linux kernel.
 * It probably makes sense to move this to mini-os's kernel.h */
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:the pointer to the member.
 * @type:the type of the container struct this is embedded in.
 * @member:the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({\
  const typeof( ((type *)0)->member ) *__mptr = (ptr);\
  (type *)( (char *)__mptr - offsetof(type,member) );})
#endif /* container_of */

struct blkdev *_open_bd_list = NULL;

int blkdev_id_parse(const char *id, blkdev_id_t *out)
{
  int ival, ret;

  ret = sscanf(id, "%d", &ival);
  if (ret != 1 || ival < 0)
    return -1;
  *out = (blkdev_id_t) ival;
  return 0;
}

void blkdev_id_unparse(blkdev_id_t id, char *out, size_t maxlen)
{
  snprintf(out, maxlen, "%u", id);
}

unsigned int detect_blkdevs(blkdev_id_t ids_out[], unsigned int max_nb)
{
  register unsigned int i = 0;
  register unsigned int found = 0;
  char path[128];
  char *xb_errmsg;
  char **vbd_entries;

  snprintf(path, sizeof(path), "/local/domain/%u/device/vbd", xenbus_get_self_id());
  xb_errmsg = xenbus_ls(XBT_NIL, path, &vbd_entries);
  if (xb_errmsg || (!vbd_entries)) {
    if (xb_errmsg)
      free(xb_errmsg);
    return 0;
  }

  /* interate through list */
  while (vbd_entries[i] != NULL) {
    if (found < max_nb) {
      if (blkdev_id_parse(vbd_entries[i], &ids_out[found]) >= 0) {
	found++;
      }
    }
    free(vbd_entries[i++]);
  }

  free(vbd_entries);
  return found;
}

struct blkdev *open_blkdev(blkdev_id_t id, int mode)
{
  struct blkdev *bd;

  /* search in blkdev list if device is already open */
  for (bd = _open_bd_list; bd != NULL; bd = bd->_next) {
	  if (blkdev_id_cmp(blkdev_id(bd), id) == 0) {
		  /* found: device is already open,
		   *  now we check if it was/shall be opened
		   *  exclusively and requested permissions
		   *  are available */
		  if (mode & O_EXCL ||
		      bd->exclusive) {
			  errno = EBUSY;
			  goto err;
		  }
		  if (((mode & O_WRONLY) && !(bd->info.mode & (O_WRONLY | O_RDWR))) ||
		      ((mode & O_RDWR) && !(bd->info.mode & O_RDWR))) {
			  errno = EACCES;
			  goto err;
		  }

		  ++bd->refcount;
		  return bd;
	  }
  }

  bd = xmalloc(struct blkdev);
  if (!bd) {
	errno = ENOMEM;
	goto err;
  }

  bd->reqpool = alloc_simple_mempool(MAX_REQUESTS, sizeof(struct _blkdev_req));
  if (!bd->reqpool) {
	errno = ENOMEM;
	goto err_free_bd;
  }

  bd->id = id;
  bd->refcount = 1;
  bd->exclusive = !!(mode & O_EXCL);
  snprintf(bd->nname, sizeof(bd->nname), "device/vbd/%u", id);

  bd->dev = init_blkfront(bd->nname, &(bd->info));
  if (!bd->dev) {
  	errno = ENODEV;
	goto err_free_reqpool;
  }

  if (((mode & O_WRONLY) && !(bd->info.mode & (O_WRONLY | O_RDWR))) ||
      ((mode & O_RDWR) && !(bd->info.mode & O_RDWR))) {
	errno = EACCES;
	goto err_shutdown_blkfront;
  }

  /* link new element to the head of _open_bd_list */
  bd->_prev = NULL;
  bd->_next = _open_bd_list;
  _open_bd_list = bd;
  if (bd->_next)
    bd->_next->_prev = bd;
  return bd;

 err_shutdown_blkfront:
  shutdown_blkfront(bd->dev);
 err_free_reqpool:
  free_mempool(bd->reqpool);
 err_free_bd:
  xfree(bd);
 err:
  return NULL;
}

void close_blkdev(struct blkdev *bd)
{
  --bd->refcount;
  if (bd->refcount == 0) {
    /* unlink element from _open_bd_list */
    if (bd->_next)
      bd->_next->_prev = bd->_prev;
    if (bd->_prev)
      bd->_prev->_next = bd->_next;
    else
      _open_bd_list = bd->_next;

    shutdown_blkfront(bd->dev);
    free_mempool(bd->reqpool);
    xfree(bd);
  }
}

void _blkdev_async_io_cb(struct blkfront_aiocb *aiocb, int ret)
{
	struct mempool_obj *robj;
	struct _blkdev_req *req;

	req = container_of(aiocb, struct _blkdev_req, aiocb);
	robj = req->p_obj;

	if (req->cb)
		req->cb(ret, req->cb_argp); /* user callback */

	mempool_put(robj);
}

void _blkdev_sync_io_cb(int ret, void *argp)
{
	struct _blkdev_sync_io_sync *iosync = argp;

	iosync->ret = ret;
	up(&iosync->sem);
}
