#ifndef HW_R17G_SAVE_RESTORE_H
#define HW_R17G_SAVE_RESTORE_H

#include "save_commit.h"

/* `path` is absent and `path.bak` exists. Rename the backup onto `path`.
 * Returns -1 without changing either file when `path` already exists.
 */
int save_promote_backup(const SaveFs *fs, const char *path);

/* Move `path` to the first free `path.corrupt` or `path.corrupt.N` name.
 * Never deletes and never replaces an existing quarantine file.
 * Does not read or move `path.bak`.
 */
int save_quarantine(const SaveFs *fs, const char *path);

/* Quarantine `path`, then move `path.bak` onto `path`. If the backup rename
 * fails, the bad bytes stay under the quarantine name and the backup stays.
 */
int save_quarantine_and_promote(const SaveFs *fs, const char *path);

#endif
