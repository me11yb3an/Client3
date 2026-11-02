#include "save_commit.h"

#include "save_format.h"
#include "save_restore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int backup_name(char *out, size_t cap, const char *path) {
    int n = snprintf(out, cap, "%s.bak", path);
    return n > 0 && (size_t)n < cap;
}

static int park_name(char *out, size_t cap, const char *path) {
    int n = snprintf(out, cap, "%s.bak.prev", path);
    return n > 0 && (size_t)n < cap;
}

/* 1 valid, 0 not an image, -1 unreadable. `exact` rejects trailing bytes. */
static int file_status(const SaveFs *fs, const char *path, int exact) {
    uint8_t *data = NULL;
    size_t len = 0, image = 0;
    int found;
    if (!fs->read_file || fs->read_file(fs->ctx, path, &data, &len) != 0 || !data) return -1;
    found = save_find_image(data, len, &image);
    free(data);
    if (!found) return 0;
    if (exact && image != len) return 0;
    return 1;
}

static int park_existing(const SaveFs *fs, const char *bak, const char *park) {
    if (!fs->exists(fs->ctx, bak)) return 0;
    /* A leftover park is older than the live file this commit already proved. */
    if (fs->exists(fs->ctx, park) && fs->remove_file(fs->ctx, park) != 0) return -1;
    return fs->rename_file(fs->ctx, bak, park) == 0 ? 0 : -1;
}

static void restore_park(const SaveFs *fs, const char *bak, const char *park) {
    if (!fs->exists(fs->ctx, bak) && fs->exists(fs->ctx, park))
        fs->rename_file(fs->ctx, park, bak);
}

static void drop_park(const SaveFs *fs, const char *park) {
    if (fs->exists(fs->ctx, park)) fs->remove_file(fs->ctx, park);
}

int save_heal(const SaveFs *fs, const char *path) {
    char bak[1024];
    int status;
    if (!fs || !path || !fs->exists || !fs->rename_file || !backup_name(bak, sizeof bak, path)) return -1;
    if (fs->exists(fs->ctx, path)) return 0;
    if (!fs->exists(fs->ctx, bak)) return 0;
    if (!fs->read_file) return -1;
    status = file_status(fs, bak, 0);
    if (status != 1) return -1;
    return fs->rename_file(fs->ctx, bak, path) == 0 ? 0 : -1;
}

int save_commit(const SaveFs *fs, const char *path, const char *temp) {
    char bak[1024], park[1024];
    int live_status;
    if (!fs || !path || !temp || !fs->exists || !fs->read_file || !fs->remove_file || !fs->rename_file)
        return -1;
    if (!backup_name(bak, sizeof bak, path) || !park_name(park, sizeof park, path)) return -1;
    if (!fs->exists(fs->ctx, temp)) return -1;
    /* Refuse before any rename. A partial temp must not displace a good save. */
    if (file_status(fs, temp, 1) != 1) return -1;

    if (!fs->exists(fs->ctx, path)) {
        /* The backup, valid or not, stays beside the new commit. */
        return fs->rename_file(fs->ctx, temp, path) == 0 ? 0 : -1;
    }

    live_status = file_status(fs, path, 0);
    if (live_status < 0) return -1;
    if (live_status == 0) {
        if (save_quarantine(fs, path) != 0) return -1;
        if (fs->rename_file(fs->ctx, temp, path) != 0) return -1;
        return 0;
    }

    if (park_existing(fs, bak, park) != 0) return -1;
    if (fs->rename_file(fs->ctx, path, bak) != 0) {
        restore_park(fs, bak, park);
        return -1;
    }
    if (fs->rename_file(fs->ctx, temp, path) != 0) {
        if (!fs->exists(fs->ctx, path) && fs->exists(fs->ctx, bak))
            fs->rename_file(fs->ctx, bak, path);
        restore_park(fs, bak, park);
        return -1;
    }
    drop_park(fs, park);
    return 0;
}

int save_commit_unsafe(const SaveFs *fs, const char *path, const char *temp) {
    if (!fs || !path || !temp) return -1;
    fs->remove_file(fs->ctx, path);
    if (fs->rename_file(fs->ctx, temp, path) != 0) {
        fs->remove_file(fs->ctx, temp);
        return -1;
    }
    return 0;
}
