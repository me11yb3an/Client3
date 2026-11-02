#ifndef HW_R17G_SAVE_LOGIN_H
#define HW_R17G_SAVE_LOGIN_H

#include "save_commit.h"

#include <stddef.h>
#include <stdint.h>

/* Matches the login shim. 0 loads `out`. 4 is a genuinely new character.
 * 7 is a recovery failure: no file was created or replaced.
 */
#define SAVE_LOGIN_OK 0
#define SAVE_LOGIN_NEW 4
#define SAVE_LOGIN_FAIL 7

/* Transaction protocol for one character name. There is no generation
 * counter in the file. Newest means the live name when its image validates,
 * and otherwise the backup name.
 *   name.sav           newest committed generation, when its image validates
 *   name.sav.bak       previous committed generation
 *   name.sav.tmp       uncommitted, never selected
 *   name.sav.bak.prev  older bytes parked during a commit, never selected
 *   name.sav.corrupt*  quarantined bytes, never selected or replaced
 * A validating image is the longest prefix with magic 0x2004, version 0..6,
 * a matching CRC32, and the structural rule in save_format.h.
 * The live name wins over the backup. The temp name never wins.
 * On OK, `out` is a malloc'd copy of the selected image. The caller frees it.
 */
int save_login(const SaveFs *fs, const char *path, uint8_t **out, size_t *out_len);

/* Usernames that can be joined onto data/players/main/ without escaping it. */
int save_username_ok(const char *username);
int save_main_path(char *out, size_t cap, const char *username);

#endif
