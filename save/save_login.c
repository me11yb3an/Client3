#include "save_login.h"

#include "save_format.h"
#include "save_restore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int save_username_ok(const char *username) {
    size_t n, i;
    if (!username) return 0;
    n = strlen(username);
    if (n < 1 || n > 12) return 0;
    for (i = 0; i < n; i++) {
        char c = username[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_';
        if (!ok) return 0;
    }
    return 1;
}

int save_main_path(char *out, size_t cap, const char *username) {
    int n;
    if (!out || !save_username_ok(username)) return -1;
    n = snprintf(out, cap, "data/players/main/%s.sav", username);
    return n > 0 && (size_t)n < cap ? 0 : -1;
}

static int read_existing(const SaveFs *fs, const char *path, uint8_t **out, size_t *len) {
    *out = NULL;
    *len = 0;
    if (!fs->exists(fs->ctx, path)) return 1;
    if (!fs->read_file || fs->read_file(fs->ctx, path, out, len) != 0 || !*out) return -1;
    return 0;
}

static int copy_image(const uint8_t *data, size_t length, uint8_t **out, size_t *out_len) {
    size_t image = 0;
    uint8_t *copy;
    if (!save_find_image(data, length, &image)) return 0;
    copy = (uint8_t *)malloc(image);
    if (!copy) return -1;
    memcpy(copy, data, image);
    *out = copy;
    *out_len = image;
    return 1;
}

int save_login(const SaveFs *fs, const char *path, uint8_t **out, size_t *out_len) {
    uint8_t *sav = NULL, *bak = NULL;
    size_t sav_n = 0, bak_n = 0;
    char bak_path[1024];
    int sav_state, bak_state, image, n;
    if (out) *out = NULL;
    if (out_len) *out_len = 0;
    if (!fs || !path || !out || !out_len) return SAVE_LOGIN_FAIL;
    n = snprintf(bak_path, sizeof bak_path, "%s.bak", path);
    if (n < 0 || (size_t)n >= sizeof bak_path) return SAVE_LOGIN_FAIL;
    sav_state = read_existing(fs, path, &sav, &sav_n);
    bak_state = read_existing(fs, bak_path, &bak, &bak_n);
    if (sav_state == 0) {
        image = copy_image(sav, sav_n, out, out_len);
        if (image < 0) {
            free(sav);
            free(bak);
            return SAVE_LOGIN_FAIL;
        }
        if (image == 1) {
            free(sav);
            free(bak);
            return SAVE_LOGIN_OK;
        }
    }
    if (bak_state == 0) {
        image = copy_image(bak, bak_n, out, out_len);
        if (image == 1) {
            /* Primary absent: the backup is the committed generation.
             * Primary present but not an image: keep those bytes under a
             * quarantine name, then promote. Primary present but unreadable:
             * do not rename either file; the returned bytes still log in. */
            if (sav_state == 1) save_promote_backup(fs, path);
            else if (sav_state == 0) save_quarantine_and_promote(fs, path);
            free(sav);
            free(bak);
            return SAVE_LOGIN_OK;
        }
    }
    free(sav);
    free(bak);
    if (sav_state == 1 && bak_state == 1) return SAVE_LOGIN_NEW;
    return SAVE_LOGIN_FAIL;
}
