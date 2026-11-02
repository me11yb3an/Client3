#ifndef HW_R17G_SAVE_COMMIT_H
#define HW_R17G_SAVE_COMMIT_H

#include <stddef.h>
#include <stdint.h>

/* Replace `path` with a finished temp file without unlinking the only good
 * copy first. The previous commit is kept as `path.bak`.
 *
 * `temp` must already be flushed and fsynced, and its whole file must be one
 * valid save image. An invalid or partial temp is refused and no name changes.
 * `read_file` and `remove_file` are required. There is no directory fsync in
 * this interface, so this does not by itself prove a power cut.
 *
 * Live name missing: install `temp` and leave `path.bak` where it is, valid
 * or not. Live name unreadable: change nothing. Live bytes invalid: quarantine
 * that file and install `temp`, without deleting the backup. Live bytes valid:
 * park the older backup, move the live file to `path.bak`, then move `temp`
 * onto the live name. The parked file is removed only after that rename.
 */

typedef struct SaveFs {
    int (*exists)(void *ctx, const char *path);
    int (*remove_file)(void *ctx, const char *path);
    int (*rename_file)(void *ctx, const char *from, const char *to);
    /* 0 and a malloc'd buffer on success. The caller frees it. */
    int (*read_file)(void *ctx, const char *path, uint8_t **out, size_t *len);
    void *ctx;
} SaveFs;

/* If `path` is missing and `path.bak` is a valid image, rename the backup
 * onto `path`. An invalid or unreadable backup stays where it is.
 * Returns 0 when `path` is already present or no backup exists.
 */
int save_heal(const SaveFs *fs, const char *path);
int save_commit(const SaveFs *fs, const char *path, const char *temp);

/* The installed writer: unlink `path`, then rename `temp` over it.
 * If the rename fails, `temp` is removed too. Host tests use this to show
 * the data-loss window. The Vita build must not call it.
 */
int save_commit_unsafe(const SaveFs *fs, const char *path, const char *temp);

#endif
