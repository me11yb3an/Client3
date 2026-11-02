/* vitaGL FFP cache paths are compiled in. vglSetShaderCachePath does not
 * change them. Redirect only those two prefixes into this title's directory. */
#ifdef __vita__
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
static const char *g17_cache_path(const char *p, char *out, size_t size) {
    static const char *prefixes[] = {"ux0:data/shader_cache", "ux0:/data/shader_cache"};
    unsigned i;
    if (!p) return p;
    for (i = 0; i < 2; i++) {
        size_t n = strlen(prefixes[i]);
        if (!strncmp(p, prefixes[i], n) && (!p[n] || p[n] == '/')) {
            int count = snprintf(out, size, "ux0:/data/RS2Vita/shader_cache%s", p + n);
            if (count < 0 || (size_t)count >= size) { errno = ENAMETOOLONG; return NULL; }
            return out;
        }
    }
    return p;
}
SceUID __real_sceIoOpen(const char *, int, SceMode);
int __real_sceIoMkdir(const char *, SceMode);
SceUID __wrap_sceIoOpen(const char *p, int flags, SceMode mode) {
    char path[512];
    p = g17_cache_path(p, path, sizeof path);
    return p ? __real_sceIoOpen(p, flags, mode) : -1;
}
int __wrap_sceIoMkdir(const char *p, SceMode mode) {
    char path[512];
    p = g17_cache_path(p, path, sizeof path);
    return p ? __real_sceIoMkdir(p, mode) : -1;
}
#endif
