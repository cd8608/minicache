/*
 * Simon's HashFS (SHFS) for Mini-OS
 *
 * Copyright(C) 2013-2014 NEC Laboratories Europe. All rights reserved.
 *                        Simon Kuenzer <simon.kuenzer@neclab.eu>
 */
#include <stdio.h>

#include "hexdump.h"
#include "shfs.h"
#include "shfs_btable.h"
#include "shfs_tools.h"
#include "shfs_cache.h"
#include "shfs_fio.h"
#include "shell.h"

#ifdef HAVE_CTLDIR
#include "target/ctldir.h"
#endif

static int shcmd_shfs_ls(FILE *cio, int argc, char *argv[])
{
	struct htable_el *el;
	struct shfs_bentry *bentry;
	struct shfs_hentry *hentry;
	char str_hash[(shfs_vol.hlen * 2) + 1];
	char str_mime[sizeof(hentry->mime) + 1];
	char str_name[sizeof(hentry->name) + 1];
	char str_date[20];

	down(&shfs_mount_lock);
	if (!shfs_mounted)
		goto out;

	str_hash[(shfs_vol.hlen * 2)] = '\0';
	str_name[sizeof(hentry->name)] = '\0';

	foreach_htable_el(shfs_vol.bt, el) {
		bentry = el->private;
		hentry = (struct shfs_hentry *)
			((uint8_t *) shfs_vol.htable_chunk_cache[bentry->hentry_htchunk]
			 + bentry->hentry_htoffset);
		hash_unparse(*el->h, shfs_vol.hlen, str_hash);
		strncpy(str_name, hentry->name, sizeof(hentry->name));
		strncpy(str_mime, hentry->mime, sizeof(hentry->mime));
		strftimestamp_s(str_date, sizeof(str_date),
		                "%b %e, %g %H:%M", hentry->ts_creation);
		fprintf(cio, "%c%s %12"PRIchk" %12"PRIchk" %-24s %-16s %s\n",
		        SHFS_HASH_INDICATOR_PREFIX,
		        str_hash,
		        hentry->chunk,
		        DIV_ROUND_UP(hentry->len + hentry->offset, shfs_vol.chunksize),
		        str_mime,
		        str_date,
		        str_name);
	}

 out:
	up(&shfs_mount_lock);
	return 0;
}

static int shcmd_shfs_lsof(FILE *cio, int argc, char *argv[])
{
	struct htable_el *el;
	struct shfs_bentry *bentry;
	char str_hash[(shfs_vol.hlen * 2) + 1];

	down(&shfs_mount_lock);
	if (!shfs_mounted)
		goto out;

	str_hash[(shfs_vol.hlen * 2)] = '\0';

	foreach_htable_el(shfs_vol.bt, el) {
		bentry = el->private;
		if (bentry->refcount > 0) {
			hash_unparse(*el->h, shfs_vol.hlen, str_hash);
			fprintf(cio, "%c%s %12"PRIchk"\n",
			        SHFS_HASH_INDICATOR_PREFIX,
			        str_hash,
			        bentry->refcount);
		}
	}

 out:
	up(&shfs_mount_lock);
	return 0;
}

static int shcmd_shfs_file(FILE *cio, int argc, char *argv[])
{
	char str_mime[128];
	uint64_t fsize;
	unsigned int i;
	SHFS_FD f;
	int ret = 0;

	if (argc <= 1) {
		fprintf(cio, "Usage: %s [file]...\n", argv[0]);
		return -1;
	}

	for (i = 1; i < argc; ++i) {
		f = shfs_fio_open(argv[i]);
		if (!f) {
			fprintf(cio, "%s: Could not open: %s\n", argv[i], strerror(errno));
			return -1;
		}
		shfs_fio_mime(f, str_mime, sizeof(str_mime));
		shfs_fio_size(f, &fsize);

		fprintf(cio, "%s: %s, ", argv[i], str_mime);
		if (fsize < 1024ll)
			fprintf(cio, "%"PRIu64" B\n",
				fsize);
		else if (fsize < (1024ll * 1024ll))
			fprintf(cio, "%"PRIu64".%01"PRIu64" KiB\n",
				fsize / (1024ll),
				(fsize / (1024ll / 10)) % 10);
		else if (fsize < (1024ll * 1024ll * 1024ll))
			fprintf(cio, "%"PRIu64".%02"PRIu64" MiB\n",
				fsize / (1024ll * 1024ll),
				(fsize / ((1024ll * 1024ll) / 100)) % 100);
		else
			fprintf(cio, "%"PRIu64".%02"PRIu64" GiB\n",
				fsize / (1024ll * 1024ll * 1024ll),
				(fsize / ((1024ll * 1024ll * 1024ll) / 100)) % 100);

		shfs_fio_close(f);
	}
	return ret;
}


static int shcmd_shfs_cat(FILE *cio, int argc, char *argv[])
{
	char buf[1024];
	uint64_t fsize, left, cur, dlen, plen;
	unsigned int i;
	SHFS_FD f;
	int ret = 0;

	if (argc <= 1) {
		fprintf(cio, "Usage: %s [file]...\n", argv[0]);
		return -1;
	}

	for (i = 1; i < argc; ++i) {
		f = shfs_fio_open(argv[i]);
		if (!f) {
			fprintf(cio, "%s: Could not open: %s\n", argv[i], strerror(errno));
			return -1;
		}
		shfs_fio_size(f, &fsize);

		left = fsize;
		cur = 0;
		while (left) {
			dlen = min(left, sizeof(buf) - 1);

			ret = shfs_fio_cache_read(f, cur, buf, dlen);
			if (ret < 0) {
				fprintf(cio, "%s: Read error: %s\n", argv[i], strerror(-ret));
				shfs_fio_close(f);
				goto out;
			}
			buf[dlen] = '\0'; /* set terminating character for fprintf */
			plen = 0;
			while (plen < dlen) {
				plen += fprintf(cio, "%s", buf + plen);
				if (plen < dlen) {
					/* terminating character found earlier than expected
					 * continue printing after this character */
					++plen;
				}
			}
			fflush(cio);
			left -= dlen;
			cur += dlen;
		}
		shfs_fio_close(f);
	}
 out:
	fflush(cio);
	return ret;
}


static int shcmd_shfs_dumpfile(FILE *cio, int argc, char *argv[])
{
	SHFS_FD f;
	char buf[1024];
	uint64_t fsize, left, cur, dlen;
	int ret = 0;

	if (argc <= 1) {
		fprintf(cio, "Usage: %s [file]\n", argv[0]);
		return -1;
	}

	f = shfs_fio_open(argv[1]);
	if (!f) {
		fprintf(cio, "%s: Could not open: %s\n", argv[1], strerror(errno));
		return -1;
	}
	shfs_fio_size(f, &fsize);

	left = fsize;
	cur = 0;
	while (left) {
		dlen = min(left, sizeof(buf));
		ret = shfs_fio_cache_read(f, cur, buf, dlen);
		if (ret < 0) {
			fprintf(cio, "%s: Read error: %s\n", argv[1], strerror(-ret));
			goto out;
		}
		hexdump(cio, buf, dlen, "", HDAT_RELATIVE, cur, 16, 4, 1);
		left -= dlen;
		cur += dlen;
	}

 out:
	shfs_fio_close(f);
	return ret;
}

#ifdef CAN_DETECT_BLKDEVS
static int shcmd_lsbd(FILE *cio, int argc, char *argv[])
{
    blkdev_id_t id[64];
    char str_id[64];
    unsigned int nb_bds;
    struct blkdev *bd;
    unsigned int i;

    nb_bds = detect_blkdevs(id, sizeof(id));

    for (i = 0; i < nb_bds; ++i) {
	    bd = open_blkdev(id[i], 0x0);
	    blkdev_id_unparse(id[i], str_id, sizeof(str_id));

	    if (bd) {
		    fprintf(cio, " %s: block size = %"PRIsctr" bytes, size = %lu bytes%s\n",
		            str_id,
		            blkdev_ssize(bd),
		            blkdev_size(bd),
		            blkdev_refcount(bd) >= 2 ? ", in use" : "");
		    close_blkdev(bd);
	    } else {
		    if (errno == EBUSY)
			    fprintf(cio, " %s: in exclusive use\n", str_id);
	    }
    }
    return 0;
}
#endif

static int shcmd_shfs_mount(FILE *cio, int argc, char *argv[])
{
    blkdev_id_t id[MAX_NB_TRY_BLKDEVS];
    char str_id[64];
    unsigned int count;
    unsigned int i, j;
    int ret;

    if ((argc + 1) > MAX_NB_TRY_BLKDEVS) {
	    fprintf(cio, "At most %u block devices are supported\n", MAX_NB_TRY_BLKDEVS);
	    return -1;
    }
    if ((argc) == 1) {
	    /* no arguments were passed */
	    down(&shfs_mount_lock);
	    if (shfs_mounted) {
		    /* list mounted filesystem and exit */
		    for (i = 0; i < shfs_vol.nb_members; ++i) {
			    blkdev_id_unparse(blkdev_id(shfs_vol.member[i].bd), str_id, sizeof(str_id));
			    if (i == 0)
				    fprintf(cio, "%s", str_id);
			    else
				    fprintf(cio, ",%s", str_id);
		    }
		    up(&shfs_mount_lock);
		    fprintf(cio, " on / type shfs (ro)\n");
	    } else {
		    /* show usage and exit */
		    up(&shfs_mount_lock);
		    fprintf(cio, "No filesystem mounted\n");
		    fprintf(cio, "\nUsage: %s [block device]...\n", argv[0]);

	    }
	    return 0;
    }
    for (i = 1; i < argc; ++i) {
	    if (blkdev_id_parse(argv[i], &id[i- 1]) < 0) {
		    fprintf(cio, "Invalid argument %u\n", i);
		    return -1;
	    }
    }
    count = argc - 1;

    /* search for duplicates in the list
     * This is unfortunately an ugly & slow way of how it is done here... */
    for (i = 0; i < count; ++i)
	    for (j = 0; j < count; ++j)
		    if (i != j && (blkdev_id_cmp(id[i], id[j]) == 0)) {
			    fprintf(cio, "Found duplicates in the list\n");
			    return -1;
		    }

    ret = mount_shfs(id, count);
    if (ret == -EALREADY) {
	    fprintf(cio, "A filesystem is already mounted\nPlease unmount it first\n");
	    return -1;
    }
    if (ret < 0)
	    fprintf(cio, "Could not mount: %s\n", strerror(-ret));
    return ret;
}

static int shcmd_shfs_umount(FILE *cio, int argc, char *argv[])
{
    int force = 0;
    int ret;

    if ((argc == 2) && (strcmp(argv[1], "-f") == 0))
	    force = 1;

    ret = umount_shfs(force);
    if (ret < 0)
	    fprintf(cio, "Could not unmount: %s\n", strerror(-ret));
    return ret;
}

static int shcmd_shfs_remount(FILE *cio, int argc, char *argv[])
{
    int ret;

    ret = remount_shfs();
    if (ret < 0)
	    fprintf(cio, "Could not remount: %s\n", strerror(-ret));
    return ret;
}

static int shcmd_shfs_flush_cache(FILE *cio, int argc, char *argv[])
{
    if (!shfs_mounted) {
	    fprintf(cio, "No SHFS filesystem is mounted\n");
	    return -1;
    }

    shfs_flush_cache();
    return 0;
}

static int shcmd_shfs_prefetch_cache(FILE *cio, int argc, char *argv[])
{
	SHFS_FD f;
	uint64_t fsize, left, cur, dlen;
	char buf[SHFS_MIN_CHUNKSIZE];
	int ret = 0;

	if (argc <= 1) {
		fprintf(cio, "Usage: %s [file]\n", argv[0]);
		ret = -1;
		goto out;
	}
	if (!shfs_mounted) {
		fprintf(cio, "No SHFS filesystem is mounted\n");
		return -1;
	}

	f = shfs_fio_open(argv[1]);
	if (!f) {
		fprintf(cio, "Could not open %s: %s\n", argv[1], strerror(errno));
		ret = -1;
		goto out;
	}
	shfs_fio_size(f, &fsize);

	left = fsize;
	dlen = min(left, sizeof(buf));
	cur = fsize - dlen;
	while (left) {
		ret = shfs_fio_cache_read(f, cur, buf, dlen);
		if (unlikely(ret < 0)) {
			fprintf(cio, "%s: Read error: %s\n", argv[1], strerror(-ret));
			goto close_f;
		}

		left -= dlen;
		dlen = min(left, sizeof(buf));
		cur -= dlen;
	}

 close_f:
	shfs_fio_close(f);
 out:
	return ret;
}

static int shcmd_shfs_info(FILE *cio, int argc, char *argv[])
{
	unsigned int m;
	char str_uuid[37];
	char str_date[20];
	char str_bdid[64];
	int ret = 0;

	down(&shfs_mount_lock);
	if (!shfs_mounted) {
		fprintf(cio, "No SHFS filesystem is mounted\n");
		ret = -1;
		goto out;
	}

	fprintf(cio, "SHFS version:       0x%02x%02x\n",
	        SHFSv1_VERSION1,
	        SHFSv1_VERSION0);
	fprintf(cio, "Volume name:        %s\n", shfs_vol.volname);
	uuid_unparse(shfs_vol.uuid, str_uuid);
	fprintf(cio, "Volume UUID:        %s\n", str_uuid);
	strftimestamp_s(str_date, sizeof(str_date),
	                "%b %e, %g %H:%M", shfs_vol.ts_creation);
	fprintf(cio, "Creation date:      %s\n", str_date);
	fprintf(cio, "Chunksize:          %"PRIu32" KiB\n",
	        shfs_vol.chunksize / 1024);
	fprintf(cio, "Volume size:        %"PRIchk" KiB\n",
	        CHUNKS_TO_BYTES(shfs_vol.volsize, shfs_vol.chunksize) / 1024);
	fprintf(cio, "Hash table:         %"PRIu32" entries in %ld buckets\n" \
	        "                    %"PRIchk" chunks (%"PRIchk" KiB)\n" \
	        "                    %s\n",
	        shfs_vol.htable_nb_entries, shfs_vol.htable_nb_buckets,
	        shfs_vol.htable_len, (shfs_vol.htable_len * shfs_vol.chunksize) / 1024,
	        shfs_vol.htable_bak_ref ? "2nd copy enabled" : "No copy");
	fprintf(cio, "Entry size:         %u Bytes (raw: %zu Bytes)\n",
	        SHFS_HENTRY_SIZE, sizeof(struct shfs_hentry));

	fprintf(cio, "\n");
	fprintf(cio, "Member stripe size: %"PRIu32" KiB\n", shfs_vol.stripesize / 1024);
	fprintf(cio, "Member stripe mode: %s\n", (shfs_vol.stripemode == SHFS_SM_COMBINED ?
	                                          "Combined" : "Independent" ));
	fprintf(cio, "Volume members:     %u device(s)\n", shfs_vol.nb_members);
	for (m = 0; m < shfs_vol.nb_members; m++) {
		uuid_unparse(shfs_vol.member[m].uuid, str_uuid);
		blkdev_id_unparse(blkdev_id(shfs_vol.member[m].bd), str_bdid, sizeof(str_bdid));
		fprintf(cio, "  Member %2u:\n", m);
		fprintf(cio, "    Device:         %s\n", str_bdid);
		fprintf(cio, "    UUID:           %s\n", str_uuid);
		fprintf(cio, "    Block size:     %"PRIu32"\n", blkdev_ssize(shfs_vol.member[m].bd));
	}

 out:
	up(&shfs_mount_lock);
	return ret;
}

#ifdef HAVE_CTLDIR
int register_shfs_tools(struct ctldir *cd)
#else
int register_shfs_tools(void)
#endif
{
#ifdef HAVE_CTLDIR
	/* ctldir entries (ignore errors) */
	if (cd) {
		ctldir_register_shcmd(cd, "mount", shcmd_shfs_mount);
		ctldir_register_shcmd(cd, "umount", shcmd_shfs_umount);
		ctldir_register_shcmd(cd, "remount", shcmd_shfs_remount);
		ctldir_register_shcmd(cd, "flush", shcmd_shfs_flush_cache);
		ctldir_register_shcmd(cd, "prefetch", shcmd_shfs_prefetch_cache);
	}
#endif

	/* shell commands (ignore errors) */
#ifdef CAN_DETECT_BLKDEVS
	shell_register_cmd("lsbd", shcmd_lsbd);
#endif
	shell_register_cmd("mount", shcmd_shfs_mount);
	shell_register_cmd("umount", shcmd_shfs_umount);
	shell_register_cmd("remount", shcmd_shfs_remount);
	shell_register_cmd("ls", shcmd_shfs_ls);
	shell_register_cmd("lsof", shcmd_shfs_lsof);
	shell_register_cmd("file", shcmd_shfs_file);
	shell_register_cmd("df", shcmd_shfs_dumpfile);
	shell_register_cmd("cat", shcmd_shfs_cat);
	shell_register_cmd("flush", shcmd_shfs_flush_cache);
	shell_register_cmd("prefetch", shcmd_shfs_prefetch_cache);
	shell_register_cmd("shfs-info", shcmd_shfs_info);
#ifdef SHFS_CACHE_INFO
	shell_register_cmd("cache-info", shcmd_shfs_cache_info);
#endif

	return 0;
}

void uuid_unparse(const uuid_t uu, char *out)
{
	sprintf(out, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	        uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
	        uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

void hash_unparse(const hash512_t h, uint8_t hlen, char *out)
{
	uint8_t i;

	for (i = 0; i < hlen; i++)
		snprintf(out + (2*i), 3, "%02x", h[i]);
}

size_t strftimestamp_s(char *s, size_t slen, const char *fmt, uint64_t ts_sec)
{
	struct tm *tm;
	time_t *tsec = (time_t *) &ts_sec;
	tm = localtime(tsec);
	return strftime(s, slen, fmt, tm);
}
