/*
 * Simple memory pool implementation
 *
 * Copyright(C) 2013-2015 NEC Laboratories Europe. All rights reserved.
 *                        Simon Kuenzer <simon.kuenzer@neclab.eu>
 */
#ifndef _MEMPOOL_H_
#define _MEMPOOL_H_

#include <stdint.h>
#include <errno.h>

#include "dlist.h"
#include "likely.h"

/*
 * MEMPOOL OBJECT: MEMORY LAYOUT
 *
 *          ++--------------------++
 *          || struct mempool_obj ||
 *          ||                    ||
 *          || - -private area- - ||
 *          ++--------------------++
 *
 * *base -> +----------------------+\
 *          |      HEAD ROOM       | |
 *          |   ^              ^   |  > lhr
 * *data ->/+---|--------------|---+/
 *        | |                      |
 *   len <  |     OBJECT           |
 *        | |     DATA AREA        |
 *        | |                      |
 *         \+---|--------------|---+\
 *          |   v              v   | |
 *          |      TAIL ROOM       |  > ltr
 *          +----------------------+/
 *
 * Object data area can be increased afterwards by using the space of
 * the object's head- and tailroom (e.g., for packet encapsulation)
 *
 * If an align is passed to the memory pool allocator, the beginning of
 * the object data area (regardingless to head- and tailroom) will be aligned.
 *
 * NOTE: this object meta data should never be changed other than by
 *       functions and macros defined by this header file
 */
struct mempool_obj {
  struct mempool *p_ref; /* ptr to depending mempool */
  void *private;         /* ptr to private meta data area */
  void *base;            /* ptr to data base */
  void *data;            /* ptr to current data */
  size_t lhr;            /* left headroom space */
  size_t ltr;            /* left tailroom space */
  size_t len;            /* length of data area */
  dlist_el(flst);        /* element of mempool list for free objects */
};

/*
 * MEMPOOL MEMORY: LAYOUT
 *
 *          ++--------------------++\
 *          ||   struct mempool   || |
 *          ||                    ||  > h_size
 *          ++--------------------++ |
 *          |    // initial //     | |
 *          |    // padding //     | |
 *          +======================+/\
 *          |       OBJECT 1       |  |
 *          +----------------------+   > o_size
 *          | // padding in obj // |  |
 *          +======================+ /
 *          |       OBJECT 2       |
 *          +----------------------+
 *          | // padding in obj // |
 *          +======================+
 *          |       OBJECT 3       |
 *          +----------------------+
 *          | // padding in obj // |
 *          +======================+
 *          |         ...          |
 *          v                      v
 */
struct mempool {
  dlist_head(free_objs);
  void (*obj_pick_func)(struct mempool_obj *, void *);
  void *obj_pick_func_argp;
  size_t obj_size;
  size_t obj_headroom;
  size_t obj_tailroom;
  void (*obj_put_func)(struct mempool_obj *, void *);
  void *obj_put_func_argp;
  uint32_t nb_objs;
  uint32_t nb_free_objs;
  size_t pool_size;
  void *obj_data_area; /* points to data allocation when sep_obj_data = 1 */
};

/*
 * Callback obj_init_func will be called while objects are initialized for this memory pool
 *  void obj_init_func(struct mempool_obj *obj, void *argp)
 * Callback obj_pick_func will be called whenever objects are picked from this memory pool
 *  void obj_pick_func(struct mempool_obj *obj, void *argp)
 * Callback obj_put_func will be called whenever objects are put back to this memory pool
 *  void obj_put_func(struct mempool_obj *obj, void *argp)
 * split_obj_data (bool) defines if object data shall be splitted from meta data allocation.
 *  Depending on the object data alignments, this might be more memory space efficient
 */
struct mempool *alloc_enhanced_mempool(uint32_t nb_objs,
  size_t obj_size, size_t obj_data_align, size_t obj_headroom, size_t obj_tailroom, size_t obj_private_len, int sep_obj_data,
  void (*obj_init_func)(struct mempool_obj *, void *), void *obj_init_func_argp,
  void (*obj_pick_func)(struct mempool_obj *, void *), void *obj_pick_func_argp,
  void (*obj_put_func)(struct mempool_obj *, void *), void *obj_put_func_argp);
#define alloc_mempool(nb_objs, obj_size, obj_data_align, obj_headroom, obj_tailroom, obj_pick_func, obj_pick_func_argp, obj_private_len) \
  alloc_enhanced_mempool((nb_objs), (obj_size), (obj_data_align), (obj_headroom), (obj_tailroom), (obj_private_len), 0, NULL, NULL, (obj_pick_func), (obj_pick_func_argp), NULL, NULL)
#define alloc_simple_mempool(nb_objs, obj_size) \
  alloc_enhanced_mempool((nb_objs), (obj_size), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL)

/* mempool allocation variant where final pool memory size can be specified
 * is specified instead by number of objects
 * Note: the actual allocation size might still less or slightly more because of alignments */
struct mempool *alloc_enhanced_mempool2(size_t pool_size,
  size_t obj_size, size_t obj_data_align, size_t obj_headroom, size_t obj_tailroom, size_t obj_private_len, int sep_obj_data,
  void (*obj_init_func)(struct mempool_obj *, void *), void *obj_init_func_argp,
  void (*obj_pick_func)(struct mempool_obj *, void *), void *obj_pick_func_argp,
  void (*obj_put_func)(struct mempool_obj *, void *), void *obj_put_func_argp);
#define alloc_mempool2(pool_size, obj_size, obj_data_align, obj_headroom, obj_tailroom, obj_pick_func, obj_pick_func_argp, obj_private_len) \
  alloc_enhanced_mempool2((pool_size), (obj_size), (obj_data_align), (obj_headroom), (obj_tailroom), (obj_private_len), 0, NULL, NULL, (obj_pick_func), (obj_pick_func_argp), NULL, NULL)
#define alloc_simple_mempool2(pool_size, obj_size) \
  alloc_enhanced_mempool2((pool_size), (obj_size), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL)

void free_mempool(struct mempool *p);

#define mempool_reset_obj(obj)						  \
  do {									  \
    struct mempool *_p = (obj)->p_ref;					  \
    (obj)->len = _p->obj_size;						  \
    (obj)->lhr = _p->obj_headroom;					  \
    (obj)->ltr = _p->obj_tailroom;					  \
    (obj)->data = (void *) (((uintptr_t) (obj)->base) + _p->obj_headroom); \
    dlist_init_el((obj), flst); \
  } while(0)

/*
 * Pick an object from a memory pool
 * Returns NULL on failure
 */
static inline struct mempool_obj *mempool_pick(struct mempool *p)
{
  struct mempool_obj *obj;

  if (p->nb_free_objs == 0)
	return NULL;

  /* get object from free list */
  obj = dlist_first_el(p->free_objs, struct mempool_obj);
  dlist_unlink(obj, p->free_objs, flst);
  p->nb_free_objs--;

  /* initialize object */
  mempool_reset_obj(obj);

  /* call user's callback */
  if (p->obj_pick_func)
	p->obj_pick_func(obj, p->obj_pick_func_argp);
  return obj;
}

/*
 * Returns 0 on success, -1 on failure
 */
static inline int mempool_pick_multiple(struct mempool *p, struct mempool_obj *objs[], uint32_t count)
{
  uint32_t i;

  if (p->nb_free_objs < count)
	return -1;
  p->nb_free_objs -= count;

  for (i=0; i<count; i++) {
        /* get object from free list */
        objs[i] = dlist_first_el(p->free_objs, struct mempool_obj);
        dlist_unlink(objs[i], p->free_objs, flst);

	/* initialize object */
	mempool_reset_obj(objs[i]);
  }

  /* call user's callback */
  if (p->obj_pick_func)
	for (i=0; i<count; i++)
	  p->obj_pick_func(objs[i], p->obj_pick_func_argp);
  return 0;
}

#define mempool_free_count(p) ((p)->nb_free_objs)

#define mempool_nb_objs(p) ((p)->nb_objs)

#define mempool_size(p) ((p)->pool_size)

/*
 * Put an object back to its depending memory pool.
 * This is like free() for memory pool objects
 */
static inline void mempool_put(struct mempool_obj *obj)
{
  struct mempool *p = obj->p_ref;

  dlist_prepend(obj, p->free_objs, flst);
  p->nb_free_objs++;

  /* call user's callback */
  if (p->obj_put_func)
	p->obj_put_func(obj, p->obj_put_func_argp);
}

/*
 * Caution: Use this function only if you are sure that all objects were picked from the same memory pool!
 *          Otherwise, you have to use mempool_put for each object
 */
static inline void mempool_put_multiple(struct mempool_obj *objs[], uint32_t count)
{
  struct mempool *p;
  uint32_t i;

  if (unlikely(count == 0))
    return;

  p = objs[0]->p_ref;

  /* call user's callback */
  if (p->obj_put_func) {
	for (i=0; i<count; i++)
		p->obj_put_func(objs[i], p->obj_put_func_argp);
  }

  for (i=0; i<count; i++)
          dlist_prepend(objs[i], p->free_objs, flst);

  p->nb_free_objs += count;
}

/*
 * Caution: This function does not check if object resizing is safe
 */
static inline void mempool_obj_prepend_nocheck(struct mempool_obj *obj, ssize_t len)
{
  obj->lhr -= len;
  obj->len += len;
  obj->data = (void *)((uintptr_t) obj->data - len);
}

/*
 * Caution: This function does not check if object resizing is safe
 */
static inline void mempool_obj_append_nocheck(struct mempool_obj *obj, ssize_t len)
{
  obj->ltr -= len;
  obj->len += len;
}

/*
 * Returns 0 on success, -1 on failure
 */
static inline int mempool_obj_prepend(struct mempool_obj *obj, ssize_t len)
{
  if (unlikely(len > obj->lhr || (len < 0 && (-len) > obj->len))) {
	errno = ENOSPC;
	return -1;
  }
  mempool_obj_prepend_nocheck(obj, len);
  return 0;
}

/*
 * Returns 0 on success, -1 on failure
 */
static inline int mempool_obj_append(struct mempool_obj *obj, ssize_t len)
{
  if (unlikely(len > obj->ltr || (len < 0 && (-len) > obj->len))) {
	errno = ENOSPC;
	return -1;
  }
  mempool_obj_append_nocheck(obj, len);
  return 0;
}

/*
 * NOTE:
 * Using the famous container_of() macro does not work with structs
 * defined in the data area of these memory pool objects. This is
 * because obj->data is a reference to the head of your struct
 * that is located in the data field. It is not the head of the
 * struct itself.
 * In case you enable sep_obj_data, obj->data even points to a complete
 * different memory location.
 *
 * If you need to back reference to the memory pool container,
 * it is recommended to add reference to it in your struct definition.
 */
#endif /* _MEMPOOL_H_ */
