/* Keep VitaGL's allocator/initialization order, but resolve its compiler inside
 * this application. The module is supplied by the user for this local build. */
#if defined(__vita__) && defined(GL11) && defined(BUILD_A_EMBEDDED)
#include <stdio.h>
#include <vitashark.h>

int __real_shark_init(const char *path);
int __wrap_shark_init(const char *path);

int __wrap_shark_init(const char *path) {
    (void)path;
    const char *bundled = "app0:/module/libshacccg.suprx";
    int result = __real_shark_init(bundled);
    FILE *log = fopen("ux0:/data/RS2Vita/shader-loader.log", "a");
    if (log) {
        fprintf(log, "compiler=%s result=%d\n", bundled, result);
        fclose(log);
    }
    return result;
}
#endif
