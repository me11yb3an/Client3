#include "save_restore.h"

#include <stdio.h>

static int backup_name(char *out, size_t cap, const char *path) {
    int n = snprintf(out, cap, "%s.bak", path);
    return n > 0 && (size_t)n < cap;
}

static int corrupt_name(char *out, size_t cap, const char *path, int index) {
    int n = index == 0 ? snprintf(out, cap, "%s.corrupt", path)
                       : snprintf(out, cap, "%s.corrupt.%d", path, index);
    return n > 0 && (size_t)n < cap;
}

int save_promote_backup(const SaveFs *fs, const char *path) {
    char bak[1024];
    if (!fs || !path || !backup_name(bak, sizeof bak, path)) return -1;
    if (fs->exists(fs->ctx, path)) return -1;
    if (!fs->exists(fs->ctx, bak)) return -1;
    return fs->rename_file(fs->ctx, bak, path) == 0 ? 0 : -1;
}

int save_quarantine(const SaveFs *fs, const char *path) {
    char dest[1024];
    int n;
    if (!fs || !path || !fs->exists || !fs->rename_file) return -1;
    if (!fs->exists(fs->ctx, path)) return -1;
    for (n = 0; n < 100; n++) {
        if (!corrupt_name(dest, sizeof dest, path, n)) return -1;
        if (!fs->exists(fs->ctx, dest)) break;
    }
    if (n == 100) return -1;
    return fs->rename_file(fs->ctx, path, dest) == 0 ? 0 : -1;
}

int save_quarantine_and_promote(const SaveFs *fs, const char *path) {
    char bak[1024];
    if (!fs || !path || !backup_name(bak, sizeof bak, path)) return -1;
    if (!fs->exists(fs->ctx, path) || !fs->exists(fs->ctx, bak)) return -1;
    if (save_quarantine(fs, path) != 0) return -1;
    return fs->rename_file(fs->ctx, bak, path) == 0 ? 0 : -1;
}
