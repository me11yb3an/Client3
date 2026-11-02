#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
/* Minimal stable C ABI declarations keep this prototype buildable against the
 * WSL runtime libraries even when development headers are not installed. */
typedef unsigned int BZ_uint;
extern int BZ2_bzBuffToBuffDecompress(char *, BZ_uint *, char *, BZ_uint, int, int);
extern int BZ2_bzBuffToBuffCompress(char *, BZ_uint *, char *, BZ_uint, int, int, int);
extern unsigned char *SHA256(const unsigned char *, size_t, unsigned char *);
#define SHA256_DIGEST_LENGTH 32
#define BZ_OK 0
#include <sys/stat.h>
#include <time.h>
#include "vendor/quickjs-vita/quickjs.h"
#include "save_commit.h"
#include "save_login.h"
#include "active_save.h"
#include <malloc.h>
#include <stdatomic.h>
#ifdef __vita__
#include "world_worker.h"
#include <psp2/io/fcntl.h>
#include "r103_save_native_read.h"
extern void ba_cadence_background_flush(void);
#endif

/* Disposable build-225 runner. The only writable tree is the extracted
 * reference's data/players/main directory. No socket/listener is created. */
static const char *data_root = "bren-reference-extract-20261019/serverdata";
static FILE *file_handles[128];
static size_t transport_last_len;
static unsigned transport_last_gen;
static unsigned transport_send_count;
static uint8_t transport_seed[8];
static uint8_t transport_last[32];
static size_t transport_trace_len[16];
static uint8_t transport_trace_first[16];
static uint8_t transport_trace_payload[16][128];
static size_t transport_trace_copied[16];
static unsigned transport_trace_count;
#include "../packet_queue.h"
#include "embedded_server.h"
#include "ba_resource.h"
#include "ba_timing.h"
#ifdef __vita__
#include <psp2/kernel/rng.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#endif
static JSRuntime *server_rt;
static JSContext *server_ctx;
static BaQueue clientbound, serverbound;
/* Queue locks cover only bounded byte copies, never a JS world tick. */
#ifdef __vita__
static pthread_mutex_t bridge_packets=PTHREAD_MUTEX_INITIALIZER;
#define PACKET_LOCK() pthread_mutex_lock(&bridge_packets)
#define PACKET_UNLOCK() pthread_mutex_unlock(&bridge_packets)
#else
#define PACKET_LOCK() ((void)0)
#define PACKET_UNLOCK() ((void)0)
#endif
static void bridge_queue_reset(BaQueue*q,unsigned g){PACKET_LOCK();ba_queue_reset(q,g);PACKET_UNLOCK();}
static int bridge_queue_write(BaQueue*q,unsigned g,const void*b,size_t n){PACKET_LOCK();int r=ba_queue_write(q,g,b,n);PACKET_UNLOCK();return r;}
static size_t bridge_queue_read(BaQueue*q,void*b,size_t n){PACKET_LOCK();size_t r=ba_queue_read(q,b,n);PACKET_UNLOCK();return r;}
static int ba_server_ready_sync(void);
static int ba_server_pump_sync(void);
#ifdef __vita__
static BaWorldWorker world_worker=BA_WORLD_WORKER_INIT;
static void world_enter(void*unused){(void)unused;if(server_rt)JS_UpdateStackTop(server_rt);}
static int world_pump(void*unused){(void)unused;return ba_server_pump_sync();}
static int world_ready(void*unused){(void)unused;return ba_server_ready_sync();}
#define WORLD_GUARD() BaWorldGuard world_guard __attribute__((cleanup(ba_world_leave)))=ba_world_enter(&world_worker)
#else
#define WORLD_GUARD() ((void)0)
#endif
static unsigned generation;
static _Atomic int connected, server_failed;
static const char *save_root;
/* Native calls are backed by the actual build-225 rsbuf and rsmod crates and
 * are serialized by the single QuickJS game-thread caller. */
extern void rv_rsbuf_addPlayer(int32_t); extern void rv_rsbuf_removePlayer(int32_t);
extern void rv_rsbuf_addNpc(int32_t,int32_t); extern void rv_rsbuf_removeNpc(int32_t);
extern bool rv_rsbuf_hasPlayer(int32_t,int32_t); extern bool rv_rsbuf_hasNpc(int32_t,int32_t);
extern int32_t rv_rsbuf_getNpcObservers(int32_t); extern void rv_rsbuf_cleanup(void);
extern void rv_rsbuf_cleanupPlayerBuildArea(int32_t);
extern void rv_rsmod_allocateIfAbsent(int32_t,int32_t,int32_t);
extern void rv_rsmod_deallocateIfPresent(int32_t,int32_t,int32_t);
extern bool rv_rsmod_isZoneAllocated(int32_t,int32_t,int32_t);
extern bool rv_rsmod_canTravel(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint32_t,int32_t);
extern intptr_t rv_rsmod_findPath(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,bool,int32_t,int32_t,int32_t,uint32_t*,size_t);
extern int32_t rv_rsbuf_computeNpc(uint16_t,uint8_t,uint16_t,int32_t,int32_t,bool,int8_t,int8_t,bool,uint32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,const uint8_t*,size_t,int32_t,int32_t,int32_t);
extern int32_t rv_rsbuf_computePlayer(
    uint16_t x,uint8_t y,uint16_t z,uint16_t ox,uint16_t oz,int32_t pid,bool tele,bool jump,int8_t run,int8_t walk,uint8_t vis,bool active,uint32_t masks,
    const uint8_t *appearance,size_t appearance_len,
    int32_t last_appearance,int32_t face_entity,int32_t face_x,int32_t face_z,int32_t orientation_x,int32_t orientation_z,
    int32_t damage,int32_t damage_type,int32_t hp,int32_t base_hp,int32_t anim,int32_t anim_delay,
    const uint8_t *say,size_t say_len,const uint8_t *chat,size_t chat_len,bool has_chat,uint8_t colour,uint8_t effect,uint8_t ignored,
    int32_t graphic,int32_t graphic_height,int32_t graphic_delay,int32_t exact_sx,int32_t exact_sz,int32_t exact_ex,int32_t exact_ez,int32_t exact_start,int32_t exact_end,int32_t exact_direction);
extern intptr_t rv_rsbuf_playerInfo(size_t,int32_t,int32_t,int32_t,bool,uint8_t*,size_t);
extern intptr_t rv_rsbuf_npcInfo(size_t,int32_t,int32_t,int32_t,bool,uint8_t*,size_t);
extern bool rv_rsmod_isFlagged(int32_t,int32_t,int32_t,uint32_t);
extern bool rv_rsmod_reached(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t);
extern bool rv_rsmod_hasLineOfSight(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint32_t);
extern void rv_rsmod_changeNpc(int32_t,int32_t,int32_t,int32_t,bool);
extern void rv_rsmod_changeFloor(int32_t,int32_t,int32_t,bool);
extern int32_t rv_rsmod_locShapeLayer(int32_t);
extern void rv_rsmod_changeLoc(int32_t,int32_t,int32_t,int32_t,int32_t,bool,bool,bool);
extern void rv_rsmod_changeWall(int32_t,int32_t,int32_t,int32_t,int32_t,bool,bool,bool);
extern void rv_rsmod_changeRoof(int32_t,int32_t,int32_t,bool);
extern void rv_rsmod_changePlayer(int32_t,int32_t,int32_t,int32_t,bool);
static int64_t now_ms(void) {
#ifdef __vita__
    return sceKernelGetSystemTimeWide() / 1000;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}
static JSValue err_io(JSContext *ctx, const char *what) {
    return JS_ThrowInternalError(ctx, "%s: %s", what, strerror(errno));
}
static int safe_path(JSContext *ctx, JSValueConst value, int write_mode, char *out, size_t cap) {
    const char *p = JS_ToCString(ctx, value);
    if (!p) return -1;
    size_t n = strlen(p);
    int ok = n > 0 && n < 900 && p[0] != '/' && p[0] != '\\' &&
        !(n > 1 && p[1] == ':') && strstr(p, "..") == NULL && strchr(p, '\\') == NULL;
    if (write_mode && strcmp(p,"data/players/main") && strncmp(p, "data/players/main/", 18) != 0) ok = 0;
    if (ok) {
        int player = !strcmp(p,"data/players/main") || strncmp(p, "data/players/main/", 18) == 0;
        int k = snprintf(out, cap, "%s/%s", player ? save_root : data_root, player ? (p[17] ? p + 18 : "") : p);
        if (k < 0 || (size_t)k >= cap) ok = 0;
    }
    JS_FreeCString(ctx, p);
    return ok ? 1 : 0;
}
static JSValue js_print(JSContext *ctx,JSValueConst this_val,int argc,JSValueConst *argv){
 (void)this_val;static unsigned lines;static FILE*log;
 if(argc&&lines<128){const char*s=JS_ToCString(ctx,argv[0]);if(!s)return JS_EXCEPTION;if(!lines)log=fopen("ux0:/data/RS2Vita/hardware-server-startup.log","w");if(log){fprintf(log,"%.1024s\n",s);if(strstr(s,"ready")||strstr(s,"error")||strstr(s,"Error"))fflush(log);}JS_FreeCString(ctx,s);lines++;if(lines==128&&log){fclose(log);log=NULL;}}
 return JS_UNDEFINED;
}

static JSValue js_now(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv; return JS_NewInt64(ctx, now_ms());
}
static JSValue js_random(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc != 1 || !JS_IsObject(argv[0])) return JS_ThrowTypeError(ctx, "fillRandom expects Uint8Array");
    size_t off = 0, len = 0, bpe = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &off, &len, &bpe);
    if (JS_IsException(ab)) return ab;
    size_t cap = 0; uint8_t *base = JS_GetArrayBuffer(ctx, &cap, ab); JS_FreeValue(ctx, ab);
    if (!base || bpe != 1 || off > cap || len > cap - off) return JS_ThrowTypeError(ctx, "invalid random byte view");
#ifdef __vita__
    if (sceKernelGetRandomNumber(base + off, len) < 0) return JS_ThrowInternalError(ctx, "Vita RNG failed");
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return err_io(ctx, "random source");
    size_t got = fread(base + off, 1, len, f); fclose(f);
    if (got != len) return JS_ThrowInternalError(ctx, "short random read");
#endif
    return JS_UNDEFINED;
}
static JSValue js_file_crc32(JSContext*ctx,JSValueConst this_val,int argc,JSValueConst*argv){
 (void)this_val;char path[PATH_MAX];uint32_t crc,size;
 if(argc!=1||safe_path(ctx,argv[0],0,path,sizeof(path))!=1)return JS_ThrowTypeError(ctx,"fileCrc32 path rejected");
 if(ba_resource_metadata(path,&crc,&size)!=1)return JS_NULL;
 JSValue result=JS_NewArray(ctx);if(JS_IsException(result))return result;
 JS_SetPropertyUint32(ctx,result,0,JS_NewUint32(ctx,crc));
 JS_SetPropertyUint32(ctx,result,1,JS_NewUint32(ctx,size));return result;
}
static JSValue js_file_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; char path[PATH_MAX];
    if (argc != 1 || safe_path(ctx, argv[0], 0, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "fileRead path rejected");
    FILE *f = fopen(path, "rb"); if (!f) return JS_NULL;
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0) { fclose(f); return err_io(ctx, "fileRead seek"); }
    long n = ftell(f); if (n > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) { fclose(f); return JS_ThrowRangeError(ctx, "fileRead size rejected"); }
    uint8_t *buf = (uint8_t *)malloc((size_t)n ? (size_t)n : 1); if (!buf) { fclose(f); return JS_ThrowOutOfMemory(ctx); }
    size_t got = fread(buf, 1, (size_t)n, f); fclose(f);
    if (got != (size_t)n) { free(buf); return JS_ThrowInternalError(ctx, "short fileRead"); }
    JSValue ab = JS_NewArrayBufferCopy(ctx, buf, (size_t)n); free(buf); if (JS_IsException(ab)) return ab;
    JSValue args[3] = { ab, JS_UNDEFINED, JS_UNDEFINED };
    JSValue result = JS_NewTypedArray(ctx, 3, args, JS_TYPED_ARRAY_UINT8); JS_FreeValue(ctx, ab); return result;
}
static int bytes_from_value(JSContext *ctx, JSValueConst value, const uint8_t **ptr, size_t *len, JSValue *ab) {
    size_t off = 0, bpe = 0; *ab = JS_GetTypedArrayBuffer(ctx, value, &off, len, &bpe);
    if (JS_IsException(*ab)) return 0;
    size_t cap = 0; uint8_t *base = JS_GetArrayBuffer(ctx, &cap, *ab);
    if (!base || bpe != 1 || off > cap || *len > cap - off) { JS_FreeValue(ctx, *ab); return 0; }
    *ptr = base + off; return 1;
}

static ActiveSaveClock g17_clock;
static int g17_save_due, g17_last_commit = -1, g17_rejected;
static size_t g17_last_bytes;
static uint64_t g17_write_us, g17_commit_us, g17_snap_us, g17_ser_us;
static int vita_exists(void *ctx, const char *path) { struct stat st; (void)ctx; return stat(path, &st) == 0; }
static int vita_remove(void *ctx, const char *path) { (void)ctx; return remove(path); }
static int vita_rename(void *ctx, const char *from, const char *to) { (void)ctx; return rename(from, to); }
static int vita_read(void *ctx, const char *path, uint8_t **out, size_t *len) {
#ifdef __vita__
    (void)ctx;return r103_save_native_read(path,out,len);
#else
    FILE *f; long n; uint8_t *buf; size_t got; (void)ctx; *out = NULL; *len = 0;
    f = fopen(path, "rb"); if (!f) return -1;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || n > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) { fclose(f); return -1; }
    buf = (uint8_t *)malloc(n ? (size_t)n : 1); if (!buf) { fclose(f); return -1; }
    got = fread(buf, 1, (size_t)n, f); fclose(f);
    if (got != (size_t)n) { free(buf); return -1; }
    *out = buf; *len = (size_t)n; return 0;
#endif
}
static SaveFs vita_save_fs(void) {
    SaveFs fs; memset(&fs, 0, sizeof fs);
    fs.exists = vita_exists; fs.remove_file = vita_remove; fs.rename_file = vita_rename; fs.read_file = vita_read;
    return fs;
}
static JSValue js_mono_us(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv; return JS_NewInt64(ctx, (int64_t)sceKernelGetSystemTimeWide());
}
static JSValue js_save_sample(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int64_t snap = 0, ser = 0; (void)this_val;
    if (argc > 0) JS_ToInt64(ctx, &snap, argv[0]);
    if (argc > 1) JS_ToInt64(ctx, &ser, argv[1]);
    if (snap < 0) snap = 0; if (ser < 0) ser = 0;
    g17_snap_us = (uint64_t)snap; g17_ser_us = (uint64_t)ser; return JS_UNDEFINED;
}
static void content_gc_pressure(JSContext *ctx, int32_t tick);
static JSValue js_tick_mark(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t tick = 0, players = 0; int64_t wall = 0; (void)this_val;
    static int64_t last_mono, last_wall; static unsigned samples, burst_lines, bursts;
    int64_t mono, gap, wall_gap; int burst, wall_jump, log_it; FILE *f;
    if (argc > 0) JS_ToInt32(ctx, &tick, argv[0]);
    if (argc > 1) JS_ToInt64(ctx, &wall, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &players, argv[2]);
    content_gc_pressure(ctx,tick);
    if (players > 0) {
        int64_t prev = g17_clock.started ? g17_clock.last_ms : now_ms();
        int was = g17_clock.started;
        if (active_save_due(&g17_clock, now_ms(), ACTIVE_SAVE_INTERVAL_MS, ACTIVE_SAVE_MAX_GAP_MS)) g17_save_due = 1;
        if (was) { int64_t delta = now_ms() - prev; if (delta < 0 || delta > ACTIVE_SAVE_MAX_GAP_MS) g17_rejected++; }
    }
    mono = (int64_t)sceKernelGetSystemTimeWide();
    gap = last_mono ? mono - last_mono : 0;
    wall_gap = last_wall ? wall - last_wall : 0;
    burst = gap > 0 && gap < 450000;
    wall_jump = wall_gap > 5000 || wall_gap < 0;
    if (burst) bursts++;
    log_it = !last_mono || wall_jump || (tick % 50) == 0 || (burst && burst_lines < 40);
    if (burst && burst_lines < 40) burst_lines++;
    last_mono = mono; last_wall = wall;
    if (log_it) {
        f = fopen("ux0:/data/RS2Vita/tick-gap.csv", samples ? "a" : "w");
        if (f) {
            if (!samples) fputs("tick,mono_us,gap_us,wall_ms,wall_gap_ms,players,bursts,rejected_gaps,active_ms\n", f);
            fprintf(f, "%d,%lld,%lld,%lld,%lld,%d,%u,%d,%lld\n", tick, (long long)mono, (long long)gap, (long long)wall, (long long)wall_gap, players, bursts, g17_rejected, (long long)g17_clock.accumulated_ms);
            fclose(f); samples++;
        }
    }
    return JS_UNDEFINED;
}
static JSValue js_load_committed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char path[PATH_MAX], user[16]; const char *base; size_t bl, root_n; (void)this_val;
    uint8_t *bytes = NULL; size_t len = 0; int status; SaveFs fs; JSValue obj, ab, u8, args[3];
    if (argc != 1 || safe_path(ctx, argv[0], 0, path, sizeof path) != 1) return JS_ThrowTypeError(ctx, "loadCommitted path rejected");
    root_n = strlen(save_root);
    if (strncmp(path, save_root, root_n) || (path[root_n] != '/' && path[root_n] != 0)) return JS_ThrowTypeError(ctx, "loadCommitted path rejected");
    base = strrchr(path, '/'); base = base ? base + 1 : path; bl = strlen(base);
    if (bl < 5 || bl - 4 >= sizeof user || strcmp(base + bl - 4, ".sav") || memchr(base, '.', bl - 4)) return JS_ThrowTypeError(ctx, "loadCommitted path rejected");
    memcpy(user, base, bl - 4); user[bl - 4] = 0;
    if (!save_username_ok(user)) return JS_ThrowTypeError(ctx, "loadCommitted path rejected");
    fs = vita_save_fs(); status = save_login(&fs, path, &bytes, &len);
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, status));
    if (status == SAVE_LOGIN_OK && bytes) {
        ab = JS_NewArrayBufferCopy(ctx, bytes, len); free(bytes); bytes = NULL;
        args[0] = ab; args[1] = JS_UNDEFINED; args[2] = JS_UNDEFINED;
        u8 = JS_NewTypedArray(ctx, 3, args, JS_TYPED_ARRAY_UINT8); JS_FreeValue(ctx, ab);
        JS_SetPropertyStr(ctx, obj, "bytes", u8);
    } else { free(bytes); JS_SetPropertyStr(ctx, obj, "bytes", JS_NULL); }
    return obj;
}
static JSValue js_file_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
 BA_PROFILE_ALWAYS(BA_SAVE);
    (void)this_val; char path[PATH_MAX], temp[PATH_MAX];
    const uint8_t *p = NULL; size_t n = 0; JSValue ab; FILE *f; size_t wrote; int flushed, synced, trunc, closed, rc;
    uint64_t t0, t1; SaveFs fs;
    if (argc != 2 || safe_path(ctx, argv[0], 1, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "fileWrite restricted to disposable players/main");
    if (!bytes_from_value(ctx, argv[1], &p, &n, &ab)) return JS_ThrowTypeError(ctx, "fileWrite expects Uint8Array");
    if (snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp) { JS_FreeValue(ctx, ab); return JS_ThrowRangeError(ctx, "save path too long"); }
    t0 = sceKernelGetSystemTimeWide();
    f = fopen(temp, "wb"); if (!f) { JS_FreeValue(ctx, ab); return err_io(ctx, "fileWrite"); }
    wrote = fwrite(p, 1, n, f);
    flushed = fflush(f);
    synced = fsync(fileno(f));
    trunc = ftruncate(fileno(f), (off_t)wrote);
    closed = fclose(f); JS_FreeValue(ctx, ab);
    t1 = sceKernelGetSystemTimeWide(); g17_write_us = t1 - t0; g17_last_bytes = n;
    /* The live checkpoint is still in place. A short write removes only the temp. */
    if (wrote != n || flushed || synced || trunc || closed) { remove(temp); g17_last_commit = -1; return JS_ThrowInternalError(ctx, "save commit failed"); }
    fs = vita_save_fs();
    rc = save_commit(&fs, path, temp);
    g17_commit_us = sceKernelGetSystemTimeWide() - t1;
    g17_last_commit = rc;
    if (rc != 0) return JS_ThrowInternalError(ctx, "save commit failed");
    return JS_UNDEFINED;
}

static JSValue js_file_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; char path[PATH_MAX]; if (argc != 1 || safe_path(ctx, argv[0], 0, path, sizeof(path)) != 1) return JS_FALSE;
    struct stat st; return JS_NewBool(ctx, stat(path, &st) == 0);
}
typedef struct {JSContext *ctx;JSValue arr;uint32_t index;} PackList;
static void pack_list_emit(void *opaque,const char *name){PackList*p=opaque;JS_SetPropertyUint32(p->ctx,p->arr,p->index++,JS_NewString(p->ctx,name));}
static JSValue js_file_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; char path[PATH_MAX]; if (argc != 1 || safe_path(ctx, argv[0], 0, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "fileList path rejected");
    PackList list={ctx,JS_NewArray(ctx),0};int packed=ba_resource_list(path,pack_list_emit,&list);
    if(packed>0)return list.arr;JS_FreeValue(ctx,list.arr);if(packed<0)return err_io(ctx,"packed fileList");
    DIR *d = opendir(path); if (!d) return err_io(ctx, "fileList");
    JSValue arr = JS_NewArray(ctx); struct dirent *ent; uint32_t i = 0;
    while ((ent = readdir(d)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        JS_SetPropertyUint32(ctx, arr, i++, JS_NewString(ctx, ent->d_name));
    }
    closedir(d); return arr;
}
static JSValue js_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; char path[PATH_MAX]; if (argc != 1 || safe_path(ctx, argv[0], 1, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "mkdir restricted to disposable players/main");
    if (mkdir(path, 0700) && errno != EEXIST) return err_io(ctx, "mkdir");
    return JS_UNDEFINED;
}
static JSValue js_file_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; char path[PATH_MAX]; int writable = 0;
    if (argc != 2 || safe_path(ctx, argv[0], 0, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "fileOpen arguments rejected");
    writable = JS_ToBool(ctx, argv[1]);
    if (writable) {
        if (safe_path(ctx, argv[0], 1, path, sizeof(path)) != 1) return JS_ThrowTypeError(ctx, "fileOpen write restricted to disposable players/main");
    }
    int slot = -1; for (int i = 0; i < 128; i++) if (!file_handles[i]) { slot = i; break; }
    if (slot < 0) return JS_NewInt32(ctx, -1);
    FILE *f = fopen(path, writable ? "r+b" : "rb");
    if (!f && writable) f = fopen(path, "w+b");
    if (!f) return JS_NewInt32(ctx, -1);
    file_handles[slot] = f; return JS_NewInt32(ctx, slot + 1);
}
static int fd_arg(JSContext *ctx, JSValueConst v) { int32_t fd = 0; if (JS_ToInt32(ctx, &fd, v) < 0 || fd < 1 || fd > 128) return -1; return fd - 1; }
static JSValue js_file_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 1) return JS_ThrowTypeError(ctx, "fileClose(fd)"); int i = fd_arg(ctx, argv[0]);
    if (i < 0 || !file_handles[i]) return JS_ThrowInternalError(ctx, "EBADF fileClose");
    int r = fclose(file_handles[i]); file_handles[i] = NULL; return JS_NewInt32(ctx, r ? -1 : 0);
}
static JSValue js_file_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 1) return JS_ThrowTypeError(ctx, "fileSize(fd)"); int i = fd_arg(ctx, argv[0]);
    if (i < 0 || !file_handles[i]) return JS_NewInt32(ctx, -1);
    FILE *f = file_handles[i]; long cur = ftell(f); if (cur < 0 || fseek(f, 0, SEEK_END)) return JS_NewInt32(ctx, -1);
    long end = ftell(f); if (fseek(f, cur, SEEK_SET)) return JS_NewInt32(ctx, -1); return JS_NewInt64(ctx, end);
}
static JSValue js_file_read_at(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 3) return JS_ThrowTypeError(ctx, "fileReadAt(fd, offset, length)"); int i = fd_arg(ctx, argv[0]); int64_t off = 0; int32_t len = 0;
    if (i < 0 || !file_handles[i] || JS_ToInt64(ctx, &off, argv[1]) < 0 || JS_ToInt32(ctx, &len, argv[2]) < 0 || off < 0 || len < 0 || len > 16 * 1024 * 1024) return JS_ThrowRangeError(ctx, "fileReadAt arguments");
    uint8_t *buf = (uint8_t *)malloc(len ? (size_t)len : 1); if (!buf) return JS_ThrowOutOfMemory(ctx);
    FILE *f = file_handles[i]; if (fseek(f, (long)off, SEEK_SET)) { free(buf); return JS_ThrowInternalError(ctx, "fileReadAt seek"); }
    size_t n = fread(buf, 1, (size_t)len, f); if (ferror(f)) { free(buf); return JS_ThrowInternalError(ctx, "fileReadAt read"); }
    JSValue ab = JS_NewArrayBufferCopy(ctx, buf, n); free(buf); if (JS_IsException(ab)) return ab;
    JSValue args[3] = { ab, JS_UNDEFINED, JS_UNDEFINED }; JSValue out = JS_NewTypedArray(ctx, 3, args, JS_TYPED_ARRAY_UINT8); JS_FreeValue(ctx, ab); return out;
}
static JSValue js_file_write_at(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 3) return JS_ThrowTypeError(ctx, "fileWriteAt(fd, offset, bytes)"); int i = fd_arg(ctx, argv[0]); int64_t off = 0;
    if (i < 0 || !file_handles[i] || JS_ToInt64(ctx, &off, argv[1]) < 0 || off < 0) return JS_ThrowRangeError(ctx, "fileWriteAt arguments");
    const uint8_t *p = NULL; size_t n = 0; JSValue ab; if (!bytes_from_value(ctx, argv[2], &p, &n, &ab)) return JS_ThrowTypeError(ctx, "fileWriteAt expects Uint8Array");
    if (fseek(file_handles[i], (long)off, SEEK_SET)) { JS_FreeValue(ctx, ab); return JS_NewInt32(ctx, -1); }
    size_t wrote = fwrite(p, 1, n, file_handles[i]); fflush(file_handles[i]); JS_FreeValue(ctx, ab); return JS_NewInt64(ctx, (int64_t)wrote);
}
static JSValue typed_u8(JSContext *ctx, const uint8_t *bytes, size_t len) {
    JSValue ab = JS_NewArrayBufferCopy(ctx, bytes, len); if (JS_IsException(ab)) return ab;
    JSValue args[3] = { ab, JS_UNDEFINED, JS_UNDEFINED };
    JSValue out = JS_NewTypedArray(ctx, 3, args, JS_TYPED_ARRAY_UINT8); JS_FreeValue(ctx, ab); return out;
}
static JSValue js_bzip_decompress(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
 BA_PROFILE_ALWAYS(BA_BZIP);
    (void)this_val;
    if (argc != 4) return JS_ThrowTypeError(ctx, "bzip2Decompress(bytes, outputLength, prependHeader, containsLength)");
    const uint8_t *src = NULL; size_t src_len = 0; JSValue ab;
    if (!bytes_from_value(ctx, argv[0], &src, &src_len, &ab)) return JS_ThrowTypeError(ctx, "bzip2Decompress expects Uint8Array");
    int32_t out_len = 0; if (JS_ToInt32(ctx, &out_len, argv[1]) < 0 || out_len < 0 || out_len > 64 * 1024 * 1024) { JS_FreeValue(ctx, ab); return JS_ThrowRangeError(ctx, "bzip2 output length rejected"); }
    int prepend = JS_ToBool(ctx, argv[2]); int contains_len = JS_ToBool(ctx, argv[3]);
    if (contains_len) {
        if (src_len < 4) { JS_FreeValue(ctx, ab); return JS_ThrowRangeError(ctx, "bzip2 length header truncated"); }
        uint32_t declared = ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 8) | src[3];
        if (declared != (uint32_t)out_len) { JS_FreeValue(ctx, ab); return JS_ThrowRangeError(ctx, "bzip2 declared output length mismatch"); }
        src += 4; src_len -= 4;
    }
    size_t wrapped_len = src_len + (prepend ? 4 : 0); uint8_t *wrapped = (uint8_t *)malloc(wrapped_len ? wrapped_len : 1);
    uint8_t *out = (uint8_t *)malloc(out_len ? (size_t)out_len : 1);
    if (!wrapped || !out) { free(wrapped); free(out); JS_FreeValue(ctx, ab); return JS_ThrowOutOfMemory(ctx); }
    if (prepend) { memcpy(wrapped, "BZh1", 4); memcpy(wrapped + 4, src, src_len); } else memcpy(wrapped, src, src_len);
    unsigned int actual = (unsigned int)out_len; int bz = BZ2_bzBuffToBuffDecompress((char *)out, &actual, (char *)wrapped, (unsigned int)wrapped_len, 0, 0);
    free(wrapped); JS_FreeValue(ctx, ab);
    if (bz != BZ_OK || actual != (unsigned int)out_len) { free(out); return JS_ThrowInternalError(ctx, "bzip2 decode failed (code %d, got %u expected %d)", bz, actual, out_len); }
    JSValue result = typed_u8(ctx, out, actual); free(out); return result;
}
static JSValue js_bzip_compress(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 1) return JS_ThrowTypeError(ctx, "bzip2Compress(bytes)");
    const uint8_t *src = NULL; size_t src_len = 0; JSValue ab;
    if (!bytes_from_value(ctx, argv[0], &src, &src_len, &ab)) return JS_ThrowTypeError(ctx, "bzip2Compress expects Uint8Array");
    if (src_len > 64 * 1024 * 1024) { JS_FreeValue(ctx, ab); return JS_ThrowRangeError(ctx, "bzip2 input length rejected"); }
    unsigned int cap = (unsigned int)(src_len + src_len / 100 + 601); char *out = (char *)malloc(cap ? cap : 1);
    if (!out) { JS_FreeValue(ctx, ab); return JS_ThrowOutOfMemory(ctx); }
    int r = BZ2_bzBuffToBuffCompress(out, &cap, (char *)src, (unsigned int)src_len, 1, 0, 30); JS_FreeValue(ctx, ab);
    if (r != BZ_OK) { free(out); return JS_ThrowInternalError(ctx, "bzip2 encode failed (%d)", r); }
    JSValue result = typed_u8(ctx, (uint8_t *)out, cap); free(out); return result;
}
static JSValue js_sha256(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; if (argc != 1) return JS_ThrowTypeError(ctx, "sha256(string)");
    const char *s = JS_ToCString(ctx, argv[0]); if (!s) return JS_EXCEPTION;
    unsigned char digest[SHA256_DIGEST_LENGTH]; SHA256((const unsigned char *)s, strlen(s), digest); JS_FreeCString(ctx, s);
    char hex[SHA256_DIGEST_LENGTH * 2 + 1]; static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(digest); i++) { hex[i * 2] = digits[digest[i] >> 4]; hex[i * 2 + 1] = digits[digest[i] & 15]; }
    hex[sizeof(hex) - 1] = 0; return JS_NewString(ctx, hex);
}
static JSValue js_rsbuf_cleanup_empty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; (void)argv; if (argc) return JS_ThrowTypeError(ctx,"rsbuf_cleanup()");
    rv_rsbuf_cleanup(); return JS_UNDEFINED;
}
static int to_i32s(JSContext*c,int n,JSValueConst*a,int expected,int32_t*v){if(n!=expected)return -1;for(int i=0;i<n;i++)if(JS_ToInt32(c,&v[i],a[i])<0)return -1;return 0;}
static JSValue js_rsbuf_add_player(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; int32_t v[1]; if(to_i32s(ctx,argc,argv,1,v)<0)return JS_EXCEPTION; rv_rsbuf_addPlayer(v[0]);
    return JS_UNDEFINED;
}
static JSValue js_rsbuf_remove_player(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[1];(void)t;if(to_i32s(c,n,a,1,v)<0)return JS_EXCEPTION;rv_rsbuf_removePlayer(v[0]);return JS_UNDEFINED;}
static JSValue js_rsbuf_add_npc(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[2];(void)t;if(to_i32s(c,n,a,2,v)<0)return JS_EXCEPTION;rv_rsbuf_addNpc(v[0],v[1]);return JS_UNDEFINED;}
static JSValue js_rsbuf_remove_npc(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[1];(void)t;if(to_i32s(c,n,a,1,v)<0)return JS_EXCEPTION;rv_rsbuf_removeNpc(v[0]);return JS_UNDEFINED;}
static JSValue js_rsbuf_has_player(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[2];(void)t;if(to_i32s(c,n,a,2,v)<0)return JS_EXCEPTION;return JS_NewBool(c,rv_rsbuf_hasPlayer(v[0],v[1]));}
static JSValue js_rsbuf_has_npc(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[2];(void)t;if(to_i32s(c,n,a,2,v)<0)return JS_EXCEPTION;return JS_NewBool(c,rv_rsbuf_hasNpc(v[0],v[1]));}
static JSValue js_rsbuf_observers(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[1];(void)t;if(to_i32s(c,n,a,1,v)<0)return JS_EXCEPTION;return JS_NewInt32(c,rv_rsbuf_getNpcObservers(v[0]));}
static JSValue js_rsbuf_buildarea(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[1];(void)t;if(to_i32s(c,n,a,1,v)<0)return JS_EXCEPTION;rv_rsbuf_cleanupPlayerBuildArea(v[0]);return JS_UNDEFINED;}
#include "r64_banker_defaults.h"
static void r64_bank_default(int32_t*v){
    static int enabled=-1,audit=-1;static unsigned char seen[8192];
    if(!r64_banker_type(v[4]))return;
    if(enabled<0){enabled=1;FILE*f=fopen("ux0:/data/RS2Vita/banker-default-facing.txt","r");if(f){fscanf(f,"%d",&enabled);fclose(f);}}
    if(audit<0){audit=0;FILE*f=fopen("ux0:/data/RS2Vita/banker-facing-audit.txt","r");if(f){fscanf(f,"%d",&audit);fclose(f);}}
    int ox=v[11],oz=v[12],changed=0;
    if(enabled)changed=r64_counter_facing(v[0],v[1],v[2],v[4],v[8],v[5]>=0||v[6]>=0,v[9],v[10],&v[11],&v[12]);
    if(audit&&v[3]>=0&&v[3]<8192&&!seen[v[3]]){
        seen[v[3]]=1;FILE*f=fopen("ux0:/data/RS2Vita/banker-facing.csv","a");
        if(f){fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",v[3],v[4],v[0],v[1],v[2],v[8],v[9],v[10],ox,oz,v[11],v[12],changed);fclose(f);}
    }
}
static JSValue js_rsbuf_compute_npc(JSContext*c,JSValueConst t,int n,JSValueConst*a){
    int32_t v[22]; (void)t;
    if(n!=25)return JS_ThrowTypeError(c,"rsbuf_computeNpc requires 25 build-225 arguments");
    int k=0; const int idx[]={0,1,2,3,4,6,7,9,10,11,12,13,14,15,16,17,18,19,20,22,23,24};
    for(size_t i=0;i<sizeof(idx)/sizeof(idx[0]);i++)if(JS_ToInt32(c,&v[k++],a[idx[i]])<0)return JS_EXCEPTION;
    const uint8_t *say=NULL; size_t say_len=0; const char *tmp=NULL;
    if(!JS_IsNull(a[21])&&!JS_IsUndefined(a[21])){tmp=JS_ToCString(c,a[21]);if(!tmp)return JS_EXCEPTION;say=(const uint8_t*)tmp;say_len=strlen(tmp);}
    r64_bank_default(v);
    int32_t r=rv_rsbuf_computeNpc((uint16_t)v[0],(uint8_t)v[1],(uint16_t)v[2],v[3],v[4],JS_ToBool(c,a[5]),(int8_t)v[5],(int8_t)v[6],JS_ToBool(c,a[8]),(uint32_t)v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],v[15],v[16],v[17],v[18],say,say_len,v[19],v[20],v[21]);
    if(tmp)JS_FreeCString(c,tmp); if(r<0)return JS_ThrowRangeError(c,"rsbuf_computeNpc rejected build-225 arguments (%d)",r); return JS_UNDEFINED;
}
static JSValue js_rsbuf_compute_player(JSContext*c,JSValueConst t,int n,JSValueConst*a){
    (void)t; if(n!=41)return JS_ThrowTypeError(c,"rsbuf_computePlayer requires 41 build-225 arguments");
    const int idx[]={0,1,2,3,4,5,8,9,10,12,14,15,16,17,18,19,20,21,22,23,24,25,28,29,30,31,32,33,34,35,36,37,38,39,40};
    int32_t v[35]; for(size_t i=0;i<sizeof(idx)/sizeof(idx[0]);i++)if(JS_ToInt32(c,&v[i],a[idx[i]])<0)return JS_EXCEPTION;
    const uint8_t *appearance=NULL,*chat=NULL,*say=NULL;size_t appearance_len=0,chat_len=0,say_len=0;JSValue app_ab=JS_UNDEFINED,chat_ab=JS_UNDEFINED;const char *say_s=NULL;
    if(!bytes_from_value(c,a[13],&appearance,&appearance_len,&app_ab))return JS_ThrowTypeError(c,"player appearance must be Uint8Array");
    bool has_chat=!JS_IsNull(a[27])&&!JS_IsUndefined(a[27]);
    if(has_chat&&!bytes_from_value(c,a[27],&chat,&chat_len,&chat_ab)){JS_FreeValue(c,app_ab);return JS_ThrowTypeError(c,"player chat must be Uint8Array or null");}
    if(!JS_IsNull(a[26])&&!JS_IsUndefined(a[26])){say_s=JS_ToCString(c,a[26]);if(!say_s){if(has_chat)JS_FreeValue(c,chat_ab);JS_FreeValue(c,app_ab);return JS_EXCEPTION;}say=(const uint8_t*)say_s;say_len=strlen(say_s);}
    int32_t r=rv_rsbuf_computePlayer((uint16_t)v[0],(uint8_t)v[1],(uint16_t)v[2],(uint16_t)v[3],(uint16_t)v[4],v[5],JS_ToBool(c,a[6]),JS_ToBool(c,a[7]),(int8_t)v[6],(int8_t)v[7],(uint8_t)v[8],JS_ToBool(c,a[11]),(uint32_t)v[9],appearance,appearance_len,v[10],v[11],v[12],v[13],v[14],v[15],v[16],v[17],v[18],v[19],v[20],v[21],say,say_len,chat,chat_len,has_chat,(uint8_t)v[22],(uint8_t)v[23],(uint8_t)v[24],v[25],v[26],v[27],v[28],v[29],v[30],v[31],v[32],v[33],v[34]);
    if(say_s)JS_FreeCString(c,say_s);if(has_chat)JS_FreeValue(c,chat_ab);JS_FreeValue(c,app_ab);if(r<0)return JS_ThrowRangeError(c,"rsbuf_computePlayer rejected build-225 arguments (%d)",r);return JS_UNDEFINED;
}
static JSValue js_rsbuf_info(JSContext*c,JSValueConst t,int n,JSValueConst*a,int npc){
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_PACKET,(++sample%32)?0:ba_timing_now()};int32_t v[4];(void)t;if(n!=5)return JS_ThrowTypeError(c,"rsbuf info encoder requires 5 arguments");for(int i=0;i<4;i++)if(JS_ToInt32(c,&v[i],a[i])<0)return JS_EXCEPTION;bool rebuild=JS_ToBool(c,a[4]);size_t cap=1024*1024;uint8_t *buf=malloc(cap);if(!buf)return JS_ThrowOutOfMemory(c);intptr_t len=npc?rv_rsbuf_npcInfo((size_t)v[0],v[1],v[2],v[3],rebuild,buf,cap):rv_rsbuf_playerInfo((size_t)v[0],v[1],v[2],v[3],rebuild,buf,cap);if(len<0){free(buf);return JS_ThrowInternalError(c,"rsbuf %sInfo failed (%ld)",npc?"npc":"player",(long)len);}JSValue out=typed_u8(c,buf,(size_t)len);free(buf);return out;}
static JSValue js_rsbuf_player_info(JSContext*c,JSValueConst t,int n,JSValueConst*a){return js_rsbuf_info(c,t,n,a,0);}
static JSValue js_rsbuf_npc_info(JSContext*c,JSValueConst t,int n,JSValueConst*a){return js_rsbuf_info(c,t,n,a,1);}
static JSValue js_rsmod_allocate(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];(void)t;if(to_i32s(c,n,a,3,v)<0)return JS_EXCEPTION;rv_rsmod_allocateIfAbsent(v[0],v[1],v[2]);return JS_UNDEFINED;}
static JSValue js_rsmod_deallocate(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];(void)t;if(to_i32s(c,n,a,3,v)<0)return JS_EXCEPTION;rv_rsmod_deallocateIfPresent(v[0],v[1],v[2]);return JS_UNDEFINED;}
static JSValue js_rsmod_is_zone(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];(void)t;if(to_i32s(c,n,a,3,v)<0)return JS_EXCEPTION;return JS_NewBool(c,rv_rsmod_isZoneAllocated(v[0],v[1],v[2]));}
static JSValue js_rsmod_is_flagged(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];uint32_t mask;(void)t;if(n!=4)return JS_ThrowTypeError(c,"rsmod_isFlagged(x,z,level,masks)");if(to_i32s(c,3,a,3,v)<0||JS_ToUint32(c,&mask,a[3])<0)return JS_EXCEPTION;return JS_NewBool(c,rv_rsmod_isFlagged(v[0],v[1],v[2],mask));}
static JSValue js_rsmod_reached(JSContext*c,JSValueConst t,int n,JSValueConst*a){
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_COLLISION,(++sample%32)?0:ba_timing_now()};int32_t v[11];(void)t;if(n!=11)return JS_ThrowTypeError(c,"rsmod_reached requires 11 arguments");if(to_i32s(c,n,a,11,v)<0)return JS_EXCEPTION;if(v[5]<0||v[5]>255||v[6]<0||v[6]>255||v[7]<0||v[7]>255||v[8]<0||v[8]>255||v[9]<-128||v[9]>127||v[10]<0||v[10]>255)return JS_ThrowRangeError(c,"rsmod_reached arguments outside rsmod-pathfinder ABI ranges");return JS_NewBool(c,rv_rsmod_reached(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10]));}
/* BA_NATIVE_LINE_ADAPTERS: real upstream build-225 pathfinder entry points. */
extern bool rv_rsmod_hasLineOfWalk(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint32_t);
extern intptr_t rv_rsmod_line(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint32_t,int32_t,uint32_t*,size_t);
static JSValue js_rsmod_has_low(JSContext*c,JSValueConst t,int n,JSValueConst*a){
 int32_t v[9];uint32_t extra;(void)t;
 if(n!=10)return JS_ThrowTypeError(c,"rsmod_hasLineOfWalk requires 10 arguments");
 if(to_i32s(c,9,a,9,v)<0||JS_ToUint32(c,&extra,a[9])<0)return JS_EXCEPTION;
 for(int i=5;i<9;i++)if(v[i]<0||v[i]>255)return JS_ThrowRangeError(c,"rsmod dimensions outside ABI ranges");
 return JS_NewBool(c,rv_rsmod_hasLineOfWalk(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],extra));
}
static JSValue js_rsmod_line_result(JSContext*c,int n,JSValueConst*a,int mode){
 int32_t v[9],collision=0;uint32_t extra;
 if(n!=(mode==2?11:10))return JS_ThrowTypeError(c,"rsmod line/path argument count mismatch");
 if(to_i32s(c,9,a,9,v)<0||JS_ToUint32(c,&extra,a[9])<0)return JS_EXCEPTION;
 for(int i=5;i<9;i++)if(v[i]<0||v[i]>255)return JS_ThrowRangeError(c,"rsmod dimensions outside ABI ranges");
 if(mode==2){if(JS_ToInt32(c,&collision,a[10])<0)return JS_EXCEPTION;if(collision<0||collision>4)return JS_ThrowRangeError(c,"invalid rsmod collision strategy");}
 uint32_t out[256];intptr_t len=rv_rsmod_line(mode,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],extra,collision,out,256);
 if(len<0)return JS_ThrowRangeError(c,"rsmod line/path result exceeds native bounds (%ld)",(long)len);
 JSValue arr=JS_NewArray(c);if(JS_IsException(arr))return arr;
 for(uint32_t i=0;i<(uint32_t)len;i++)if(JS_SetPropertyUint32(c,arr,i,JS_NewUint32(c,out[i]))<0){JS_FreeValue(c,arr);return JS_EXCEPTION;}
 return arr;
}
static JSValue js_rsmod_low(JSContext*c,JSValueConst t,int n,JSValueConst*a){(void)t;return js_rsmod_line_result(c,n,a,0);}
static JSValue js_rsmod_los_path(JSContext*c,JSValueConst t,int n,JSValueConst*a){(void)t;return js_rsmod_line_result(c,n,a,1);}
static JSValue js_rsmod_naive(JSContext*c,JSValueConst t,int n,JSValueConst*a){
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_PATHFIND,(++sample%32)?0:ba_timing_now()};(void)t;return js_rsmod_line_result(c,n,a,2);}

static JSValue js_rsmod_has_los(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[9];uint32_t extra;(void)t;if(n!=10)return JS_ThrowTypeError(c,"rsmod_hasLineOfSight requires 10 arguments");if(to_i32s(c,9,a,9,v)<0||JS_ToUint32(c,&extra,a[9])<0)return JS_EXCEPTION;if(v[5]<0||v[5]>255||v[6]<0||v[6]>255||v[7]<0||v[7]>255||v[8]<0||v[8]>255)return JS_ThrowRangeError(c,"rsmod_hasLineOfSight dimensions outside rsmod-pathfinder ABI ranges");return JS_NewBool(c,rv_rsmod_hasLineOfSight(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],extra));}
static JSValue js_rsmod_change_npc(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[4];(void)t;if(n!=5)return JS_ThrowTypeError(c,"rsmod_changeNpc(x,z,level,size,add)");if(to_i32s(c,4,a,4,v)<0)return JS_EXCEPTION;rv_rsmod_changeNpc(v[0],v[1],v[2],v[3],JS_ToBool(c,a[4]));return JS_UNDEFINED;}
static JSValue js_rsmod_change_floor(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];(void)t;if(n!=4)return JS_ThrowTypeError(c,"rsmod_changeFloor(x,z,level,add)");if(to_i32s(c,3,a,3,v)<0)return JS_EXCEPTION;rv_rsmod_changeFloor(v[0],v[1],v[2],JS_ToBool(c,a[3]));return JS_UNDEFINED;}
static JSValue js_rsmod_loc_shape_layer(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[1];(void)t;if(to_i32s(c,n,a,1,v)<0)return JS_EXCEPTION;return JS_NewInt32(c,rv_rsmod_locShapeLayer(v[0]));}
static JSValue js_rsmod_change_loc(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[5];(void)t;if(n!=8)return JS_ThrowTypeError(c,"rsmod_changeLoc(...)");if(to_i32s(c,5,a,5,v)<0)return JS_EXCEPTION;rv_rsmod_changeLoc(v[0],v[1],v[2],v[3],v[4],JS_ToBool(c,a[5]),JS_ToBool(c,a[6]),JS_ToBool(c,a[7]));return JS_UNDEFINED;}
static JSValue js_rsmod_change_wall(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[5];(void)t;if(n!=8)return JS_ThrowTypeError(c,"rsmod_changeWall(...)");if(to_i32s(c,5,a,5,v)<0)return JS_EXCEPTION;rv_rsmod_changeWall(v[0],v[1],v[2],v[3],v[4],JS_ToBool(c,a[5]),JS_ToBool(c,a[6]),JS_ToBool(c,a[7]));return JS_UNDEFINED;}
static JSValue js_rsmod_change_roof(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[3];(void)t;if(n!=4)return JS_ThrowTypeError(c,"rsmod_changeRoof(...)");if(to_i32s(c,3,a,3,v)<0)return JS_EXCEPTION;rv_rsmod_changeRoof(v[0],v[1],v[2],JS_ToBool(c,a[3]));return JS_UNDEFINED;}
static JSValue js_rsmod_change_player(JSContext*c,JSValueConst t,int n,JSValueConst*a){int32_t v[4];(void)t;if(n!=5)return JS_ThrowTypeError(c,"rsmod_changePlayer(...)");if(to_i32s(c,4,a,4,v)<0)return JS_EXCEPTION;rv_rsmod_changePlayer(v[0],v[1],v[2],v[3],JS_ToBool(c,a[4]));return JS_UNDEFINED;}
static JSValue js_rsmod_can_travel(JSContext*c,JSValueConst t,int n,JSValueConst*a){
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_COLLISION,(++sample%32)?0:ba_timing_now()};int32_t v[8];(void)t;if(to_i32s(c,n,a,8,v)<0)return JS_EXCEPTION;return JS_NewBool(c,rv_rsmod_canTravel(v[0],v[1],v[2],v[3],v[4],v[5],(uint32_t)v[6],v[7]));}
static JSValue js_rsmod_find_path(JSContext*c,JSValueConst t,int n,JSValueConst*a){
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_PATHFIND,(++sample%32)?0:ba_timing_now()};int32_t v[13];(void)t;if(n!=14)return JS_ThrowTypeError(c,"rsmod_findPath requires 14 arguments");for(int i=0,j=0;i<n;i++){if(i==10)continue;if(JS_ToInt32(c,&v[j++],a[i])<0)return JS_EXCEPTION;}uint32_t out[256];intptr_t len=rv_rsmod_findPath(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],JS_ToBool(c,a[10]),v[10],v[11],v[12],out,256);if(len<0)return JS_ThrowRangeError(c,"rsmod_findPath failed (%ld)",(long)len);JSValue arr=JS_NewArray(c);if(JS_IsException(arr))return arr;for(uint32_t i=0;i<(uint32_t)len;i++)if(JS_SetPropertyUint32(c,arr,i,JS_NewUint32(c,out[i]))<0){JS_FreeValue(c,arr);return JS_EXCEPTION;}return arr;}
static JSValue js_sp_send_to_client(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc != 2) return JS_ThrowTypeError(ctx, "spSendToClient(Uint8Array, generation)");
    const uint8_t *bytes = NULL; size_t len = 0; JSValue ab;
    if (!bytes_from_value(ctx, argv[0], &bytes, &len, &ab)) return JS_ThrowTypeError(ctx, "spSendToClient expects Uint8Array");
    uint32_t gen = 0; if (JS_ToUint32(ctx, &gen, argv[1]) < 0) { JS_FreeValue(ctx, ab); return JS_EXCEPTION; }
    /* The first packet is the local 8-byte build-225 seed. Later client bytes
     * are not accepted in this startup-only probe. */
    transport_last_len = len; transport_last_gen = gen; transport_send_count++;
    memset(transport_last, 0, sizeof(transport_last));
    if (len > 0) memcpy(transport_last, bytes, len < sizeof(transport_last) ? len : sizeof(transport_last));
    if (transport_send_count == 1 && len == sizeof(transport_seed)) memcpy(transport_seed, bytes, sizeof(transport_seed));
    else if (transport_trace_count < sizeof(transport_trace_first)) {
        unsigned at = transport_trace_count++;
        transport_trace_len[at] = len;
        transport_trace_first[at] = len ? bytes[0] : 0;
        transport_trace_copied[at] = len < sizeof(transport_trace_payload[at]) ? len : sizeof(transport_trace_payload[at]);
        if (transport_trace_copied[at]) memcpy(transport_trace_payload[at], bytes, transport_trace_copied[at]);
    }
    if (connected && bridge_queue_write(&clientbound, gen, bytes, len) != 0) {
        server_failed = 1;
        JS_FreeValue(ctx, ab);
        return JS_ThrowInternalError(ctx, "client packet queue overflow or stale generation");
    }
    JS_FreeValue(ctx, ab); return JS_UNDEFINED;
}
static JSValue js_profile_server(JSContext *ctx,JSValueConst this_val,int argc,JSValueConst *argv){
 (void)this_val;const unsigned stages[]={BA_SERVER_TICK,BA_SERVER_NPC,BA_PACKET};
 int64_t ms[20];int n=argc<20?argc:20;
 for(int i=0;i<n;i++){ms[i]=0;if(JS_ToInt64(ctx,&ms[i],argv[i])<0||ms[i]<0)ms[i]=0;}
 for(int i=0;i<3&&i<n;i++)ba_timing_record(stages[i],(uint64_t)ms[i]*1000);
 /* Args after the original three are world, player, zone, login, logout, cleanup,
  * then cleanup sub-times, then npc count, fast turns, slow turns, inventory
  * count, inventories whose restock rate is due, and npc info milliseconds.
  * The line is written after the JS tick has already stored its own times.
  * tick-log.txt "1" enables the file. Absent or any other value keeps the
  * low-overhead path: the three stage timers still record, and no file is written. */
 static int tick_log=-1;
 if(tick_log<0){
  FILE *flag=fopen("ux0:/data/RS2Vita/tick-log.txt","r");
  int ch=flag?fgetc(flag):EOF;
  if(flag)fclose(flag);
  tick_log=ch=='1';
 }
 if(tick_log&&n>=9){
  static FILE *parts;
  if(!parts){parts=fopen("ux0:/data/RS2Vita/tick-parts.csv","w");if(parts)fputs("tick_ms,npc_ms,packet_ms,world_ms,player_ms,zone_ms,login_ms,logout_ms,cleanup_ms,clean_zone_ms,clean_player_ms,clean_npc_ms,clean_inv_ms,clean_buf_ms,npc_n,turn_fast,turn_slow,inv_n,inv_scan,info_ms\n",parts);}
  if(parts){fprintf(parts,"%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",(long long)ms[0],(long long)ms[1],(long long)ms[2],(long long)ms[3],(long long)ms[4],(long long)ms[5],(long long)ms[6],(long long)ms[7],(long long)ms[8],n>9?(long long)ms[9]:0,n>10?(long long)ms[10]:0,n>11?(long long)ms[11]:0,n>12?(long long)ms[12]:0,n>13?(long long)ms[13]:0,n>14?(long long)ms[14]:0,n>15?(long long)ms[15]:0,n>16?(long long)ms[16]:0,n>17?(long long)ms[17]:0,n>18?(long long)ms[18]:0,n>19?(long long)ms[19]:0);fflush(parts);}
 }
 return JS_UNDEFINED;
}
size_t ba_server_heap_bytes(void);
static BaProgressFn progress_fn;
static void *progress_ctx;
void ba_progress_set(BaProgressFn fn, void *ctx) { progress_fn = fn; progress_ctx = ctx; }
static JSValue js_load_progress(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    int percent = 0;
    if (argc > 0) JS_ToInt32(ctx, &percent, argv[0]);
    const char *text = "Building world";
    const char *owned = NULL;
    if (argc > 1) {
        owned = JS_ToCString(ctx, argv[1]);
        if (owned) text = owned;
    }
    {extern void ba_cadence_diagnostic(const char*);char line[160];snprintf(line,sizeof line,"# boot_progress %llu percent %d heap %llu",(unsigned long long)ba_timing_now(),percent,(unsigned long long)ba_server_heap_bytes());ba_cadence_diagnostic(line);}
    if (progress_fn) progress_fn(progress_ctx, percent, text);
    if (owned) JS_FreeCString(ctx, owned);
    return JS_UNDEFINED;
}
static const JSCFunctionListEntry funcs[] = {
    JS_CFUNC_DEF("profileServer",3,js_profile_server),
    JS_CFUNC_DEF("loadProgress", 2, js_load_progress),
    JS_CFUNC_DEF("print", 1, js_print), JS_CFUNC_DEF("printError", 1, js_print),
    JS_CFUNC_DEF("now", 0, js_now), JS_CFUNC_DEF("fillRandom", 1, js_random),
    JS_CFUNC_DEF("fileCrc32", 1, js_file_crc32),
    JS_CFUNC_DEF("fileRead", 1, js_file_read), JS_CFUNC_DEF("fileWrite", 2, js_file_write), JS_CFUNC_DEF("loadCommitted", 1, js_load_committed), JS_CFUNC_DEF("monoUs", 0, js_mono_us), JS_CFUNC_DEF("saveSample", 2, js_save_sample), JS_CFUNC_DEF("tickMark", 3, js_tick_mark),
    JS_CFUNC_DEF("fileExists", 1, js_file_exists), JS_CFUNC_DEF("fileList", 1, js_file_list),
    JS_CFUNC_DEF("mkdir", 1, js_mkdir), JS_CFUNC_DEF("fileOpen", 2, js_file_open),
    JS_CFUNC_DEF("fileClose", 1, js_file_close), JS_CFUNC_DEF("fileSize", 1, js_file_size),
    JS_CFUNC_DEF("fileReadAt", 3, js_file_read_at), JS_CFUNC_DEF("fileWriteAt", 3, js_file_write_at),
    JS_CFUNC_DEF("bzip2Decompress", 4, js_bzip_decompress), JS_CFUNC_DEF("bzip2Compress", 1, js_bzip_compress),
    JS_CFUNC_DEF("sha256", 1, js_sha256), JS_CFUNC_DEF("rsbuf_cleanup", 0, js_rsbuf_cleanup_empty),
    JS_CFUNC_DEF("rsbuf_addPlayer", 1, js_rsbuf_add_player),
    JS_CFUNC_DEF("rsbuf_removePlayer", 1, js_rsbuf_remove_player),
    JS_CFUNC_DEF("rsbuf_addNpc", 2, js_rsbuf_add_npc), JS_CFUNC_DEF("rsbuf_removeNpc", 1, js_rsbuf_remove_npc),
    JS_CFUNC_DEF("rsbuf_hasPlayer", 2, js_rsbuf_has_player), JS_CFUNC_DEF("rsbuf_hasNpc", 2, js_rsbuf_has_npc),
    JS_CFUNC_DEF("rsbuf_getNpcObservers", 1, js_rsbuf_observers), JS_CFUNC_DEF("rsbuf_cleanupPlayerBuildArea", 1, js_rsbuf_buildarea),
    JS_CFUNC_DEF("rsbuf_computeNpc", 25, js_rsbuf_compute_npc),
    JS_CFUNC_DEF("rsbuf_computePlayer", 41, js_rsbuf_compute_player),
    JS_CFUNC_DEF("rsbuf_playerInfo", 5, js_rsbuf_player_info), JS_CFUNC_DEF("rsbuf_npcInfo", 5, js_rsbuf_npc_info),
    JS_CFUNC_DEF("rsmod_allocateIfAbsent", 3, js_rsmod_allocate), JS_CFUNC_DEF("rsmod_deallocateIfPresent", 3, js_rsmod_deallocate),
    JS_CFUNC_DEF("rsmod_isZoneAllocated", 3, js_rsmod_is_zone),
    JS_CFUNC_DEF("rsmod_isFlagged", 4, js_rsmod_is_flagged),
    JS_CFUNC_DEF("rsmod_hasLineOfSight", 10, js_rsmod_has_los),
    JS_CFUNC_DEF("rsmod_hasLineOfWalk",10,js_rsmod_has_low),
    JS_CFUNC_DEF("rsmod_lineOfWalk",10,js_rsmod_low),
    JS_CFUNC_DEF("rsmod_lineOfSight",10,js_rsmod_los_path),
    JS_CFUNC_DEF("rsmod_findNaivePath",11,js_rsmod_naive),
    JS_CFUNC_DEF("rsmod_changeNpc", 5, js_rsmod_change_npc), JS_CFUNC_DEF("rsmod_changeFloor", 4, js_rsmod_change_floor),
    JS_CFUNC_DEF("rsmod_locShapeLayer", 1, js_rsmod_loc_shape_layer),
    JS_CFUNC_DEF("rsmod_changeLoc", 8, js_rsmod_change_loc), JS_CFUNC_DEF("rsmod_changeWall", 8, js_rsmod_change_wall),
    JS_CFUNC_DEF("rsmod_changeRoof", 4, js_rsmod_change_roof), JS_CFUNC_DEF("rsmod_changePlayer", 5, js_rsmod_change_player),
    JS_CFUNC_DEF("rsmod_canTravel", 8, js_rsmod_can_travel),
    JS_CFUNC_DEF("rsmod_findPath", 14, js_rsmod_find_path),
    JS_CFUNC_DEF("rsmod_reached", 11, js_rsmod_reached),
    JS_CFUNC_DEF("spSendToClient", 2, js_sp_send_to_client)
};
static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    if (fseek(f, 0, SEEK_END)) { fclose(f); return NULL; }
    long n = ftell(f); if (n < 0 || n > 16 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    char *p = (char *)malloc((size_t)n + 1); if (!p) { fclose(f); return NULL; }
    *len = fread(p, 1, (size_t)n, f); fclose(f); p[*len] = 0; return p;
}
static int replace_once(char **source, size_t *length, const char *needle, const char *replacement) {
    char *at = strstr(*source, needle); if (!at) return 0;
    size_t prefix = (size_t)(at - *source), old_len = strlen(needle), new_len = strlen(replacement);
    size_t total = *length - old_len + new_len;
    char *out = (char *)malloc(total + 1); if (!out) return -1;
    memcpy(out, *source, prefix); memcpy(out + prefix, replacement, new_len);
    memcpy(out + prefix + new_len, at + old_len, *length - prefix - old_len + 1);
    free(*source); *source = out; *length = total; return 1;
}
static void report_exception(JSContext *ctx, const char *where) {
    JSValue e = JS_GetException(ctx); const char *s = JS_ToCString(ctx, e);
    fprintf(stderr, "EXCEPTION %s: %s\n", where, s ? s : "<unprintable>");
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, e);
}
static void drain_jobs(JSRuntime *rt, JSContext *ctx) {
    JSContext *jobctx = NULL; int r;
    while ((r = JS_ExecutePendingJob(rt, &jobctx)) > 0) {}
    if (r < 0) report_exception(jobctx ? jobctx : ctx, "pending job");
}
static int call_global(JSContext *ctx, const char *name, int argc, JSValueConst *argv) {
    JSValue g = JS_GetGlobalObject(ctx), fn = JS_GetPropertyStr(ctx, g, name); JS_FreeValue(ctx, g);
    if (!JS_IsFunction(ctx, fn)) { JS_FreeValue(ctx, fn); fprintf(stderr, "missing global %s\n", name); return -1; }
    JSValue r = JS_Call(ctx, fn, JS_UNDEFINED, argc, argv); JS_FreeValue(ctx, fn);
    if (JS_IsException(r)) { report_exception(ctx, name); return -1; }
    JS_FreeValue(ctx, r); return 0;
}
static int install_missing_module_adapters(JSContext *ctx) { (void)ctx; return 0; }
/* Accurate live-size accounting without a libc malloc_usable_size dependency. */
static _Atomic size_t js_live_bytes;
size_t ba_server_heap_bytes(void){return js_live_bytes;}
/* QuickJS's adaptive cycle-GC threshold can grow beyond the fixed heap cap.
 * Collect unreachable cyclic world/zone state at an existing safe tick callback,
 * with headroom and hysteresis. Never collect from inside the allocator. */
static size_t content_gc_next = 72u * 1024u * 1024u;
static unsigned content_gc_count;
static void content_gc_pressure(JSContext *ctx, int32_t tick) {
    const size_t floor = 72u * 1024u * 1024u, ceiling = 88u * 1024u * 1024u;
    if (js_live_bytes < content_gc_next) return;
    size_t before = js_live_bytes;
    int64_t start = sceKernelGetSystemTimeWide();
    JS_RunGC(JS_GetRuntime(ctx));
    int64_t elapsed = sceKernelGetSystemTimeWide() - start;
    size_t after = js_live_bytes;
    content_gc_next = after + 8u * 1024u * 1024u;
    if (content_gc_next < floor) content_gc_next = floor;
    if (content_gc_next > ceiling) content_gc_next = ceiling;
    if (++content_gc_count <= 64) {
        FILE *f = fopen("ux0:/data/RS2Vita/content-gc.csv", content_gc_count == 1 ? "w" : "a");
        if (f) {
            if (content_gc_count == 1) fputs("count,tick,before,after,reclaimed,us\n", f);
            fprintf(f, "%u,%d,%zu,%zu,%zu,%lld\n", content_gc_count, tick, before, after, before > after ? before - after : 0, (long long)elapsed);
            fclose(f);
        }
    }
}


typedef union { long double alignment; struct { size_t size; } meta; } BaAllocHeader;
static void *ba_js_alloc(JSMallocState *s,size_t n){
    if(!n||n>SIZE_MAX-sizeof(BaAllocHeader))return NULL;
    size_t total=n+sizeof(BaAllocHeader);
    if(total>s->malloc_limit||s->malloc_size>s->malloc_limit-total)return NULL;
    BaAllocHeader *h=malloc(total);if(!h)return NULL;
    h->meta.size=n;s->malloc_size+=total;s->malloc_count++;js_live_bytes+=total;
    return h+1;
}
static void ba_js_free(JSMallocState *s,void *p){
    if(!p)return;
    BaAllocHeader *h=(BaAllocHeader *)p-1;
    js_live_bytes-=h->meta.size+sizeof(*h);s->malloc_size-=h->meta.size+sizeof(*h);s->malloc_count--;free(h);
}
static size_t ba_js_size(const void *p){return p?((const BaAllocHeader *)p-1)->meta.size:0;}
static void *ba_js_realloc(JSMallocState *s,void *p,size_t n){
    if(!p)return ba_js_alloc(s,n);
    if(!n){ba_js_free(s,p);return NULL;}
    if(n>SIZE_MAX-sizeof(BaAllocHeader))return NULL;
    BaAllocHeader *old=(BaAllocHeader *)p-1;
    size_t before=old->meta.size+sizeof(*old),after=n+sizeof(*old);
    if(after>s->malloc_limit||s->malloc_size-before>s->malloc_limit-after)return NULL;
    BaAllocHeader *h=realloc(old,after);if(!h)return NULL;
    js_live_bytes=js_live_bytes-before+after;h->meta.size=n;s->malloc_size=s->malloc_size-before+after;return h+1;
}
static const JSMallocFunctions ba_js_allocator={ba_js_alloc,ba_js_free,ba_js_realloc,ba_js_size};

extern void rv_native_reset(void);
#ifndef BA_JS_LIMIT
#define BA_JS_LIMIT (96u*1024u*1024u)
#endif
static int eval_file(const char *path) {
    size_t n = 0;
    char *text = read_file(path, &n);
    if (!text) { perror(path); return -1; }
    JSValue v = JS_Eval(server_ctx, text, n, path, JS_EVAL_TYPE_GLOBAL);
    free(text);
    if (JS_IsException(v)) { report_exception(server_ctx,path); return -1; }
    JS_FreeValue(server_ctx,v);
    drain_jobs(server_rt,server_ctx);
    return 0;
}
int ba_server_start(const char *data, const char *saves, const char *bootstrap, const char *bundle) {
    h5_mark("embedded_server_start");
    if(server_rt) return -1;
    data_root=data; save_root=saves; server_failed=connected=0;
#ifdef __vita__
    FILE *previous_error=fopen("ux0:/data/RS2Vita/hardware-server-error.log","w");if(previous_error)fclose(previous_error);
#endif
    memset(file_handles,0,sizeof(file_handles));
    rv_native_reset();
    bridge_queue_reset(&clientbound,0); bridge_queue_reset(&serverbound,0);
    content_gc_next=72u*1024u*1024u; content_gc_count=0;
    server_rt=JS_NewRuntime2(&ba_js_allocator,NULL); if(!server_rt) return -1;
    /* Explicit JS heap bound; native adapter and client budgets are separate. */
    JS_SetMemoryLimit(server_rt,BA_JS_LIMIT);
    JS_SetMaxStackSize(server_rt,512u*1024u);
    server_ctx=JS_NewContext(server_rt);
    if(!server_ctx) { ba_server_stop(); return -1; }
    JSValue g=JS_GetGlobalObject(server_ctx), h=JS_NewObject(server_ctx);
    JS_SetPropertyFunctionList(server_ctx,h,funcs,sizeof(funcs)/sizeof(funcs[0]));
    JS_SetPropertyStr(server_ctx,g,"__host",h);
    JSValue env=JS_NewObject(server_ctx);
    JS_SetPropertyStr(server_ctx,env,"SP_SKIP_LOGIN_CRC",JS_NewString(server_ctx,"true"));
    JS_SetPropertyStr(server_ctx,env,"BUILD_SRC_DIR",JS_NewString(server_ctx,"data/src"));
    /* Existing bundle streaming keeps all resources available on disk while
     * limiting resident map state. ensureResident handles login/teleports. */
    JS_SetPropertyStr(server_ctx,env,"SP_MAP_RADIUS",JS_NewString(server_ctx,"1"));
    JS_SetPropertyStr(server_ctx,env,"SP_MAP_CX",JS_NewString(server_ctx,"48"));
    JS_SetPropertyStr(server_ctx,env,"SP_MAP_CZ",JS_NewString(server_ctx,"48"));
    JS_SetPropertyStr(server_ctx,env,"SP_STREAM_RADIUS",JS_NewString(server_ctx,"1"));
    JS_SetPropertyStr(server_ctx,env,"SP_MAX_SQUARES",JS_NewString(server_ctx,"16"));
    JS_SetPropertyStr(server_ctx,env,"SP_STREAM_BUDGET_MS",JS_NewString(server_ctx,"10"));
    JS_SetPropertyStr(server_ctx,g,"__sp_env",env); JS_FreeValue(server_ctx,g);
    if(h5_requested_run()){
      const char*seed="Math.random=(()=>{let x=0x48575235;return ()=>{x^=x<<13;x^=x>>>17;x^=x<<5;return (x>>>0)/4294967296}})()";
      JSValue seeded=JS_Eval(server_ctx,seed,strlen(seed),"<HW-R5-disposable-seed>",JS_EVAL_TYPE_GLOBAL);
      if(JS_IsException(seeded)){JS_FreeValue(server_ctx,seeded);ba_server_stop();return -1;}JS_FreeValue(server_ctx,seeded);
    }
    if(install_missing_module_adapters(server_ctx)){ba_server_stop();return -1;}
    h5_mark("server_adapters_ready");
    if(eval_file(bootstrap)){ba_server_stop();return -1;}
    h5_mark("server_bootstrap_evaluated");
    if(eval_file(bundle)){ba_server_stop();return -1;}
    h5_mark("server_bundle_evaluated");
#ifdef __vita__
    JSValue audit=JS_Eval(server_ctx,"JSON.stringify({dateNow:Date.now(),iso:new Date().toISOString(),localYear:new Date().getFullYear(),timezoneOffset:new Date().getTimezoneOffset(),monotonic:__host.now()})",strlen("JSON.stringify({dateNow:Date.now(),iso:new Date().toISOString(),localYear:new Date().getFullYear(),timezoneOffset:new Date().getTimezoneOffset(),monotonic:__host.now()})"),"<clock-audit>",JS_EVAL_TYPE_GLOBAL);
    const char *clock_text=JS_ToCString(server_ctx,audit);
    FILE *clock_log=fopen("ux0:/data/RS2Vita/clock-audit.log","a");
    if(clock_log){fprintf(clock_log,"JS_CLOCK %s\n",clock_text?clock_text:"conversion failed");fclose(clock_log);}
    JS_FreeCString(server_ctx,clock_text);JS_FreeValue(server_ctx,audit);
#endif
#ifdef __vita__
    int initial=ba_server_ready_sync();
    int worker_rc=ba_world_start(&world_worker,NULL,world_pump,world_ready,world_enter,initial);
    if(worker_rc){fprintf(stderr,"Offline world worker start failed: %d\n",worker_rc);ba_server_stop();return -1;}
#endif
    return 0;
}
static int ba_server_ready_sync(void) {
    if(!server_ctx||server_failed) return -1;
    JSValue g=JS_GetGlobalObject(server_ctx);
    JSValue err=JS_GetPropertyStr(server_ctx,g,"__sp_bootError");
    JSValue ready=JS_GetPropertyStr(server_ctx,g,"__sp_worldReady");
    int result=JS_IsUndefined(err)?JS_ToBool(server_ctx,ready):-1;
    if(result<0&&!JS_IsUndefined(err)){const char*message=JS_ToCString(server_ctx,err);FILE*f=fopen("ux0:/data/RS2Vita/hardware-server-error.log","w");if(f){fprintf(f,"%.4096s\n",message?message:"unprintable boot error");fclose(f);}JS_FreeCString(server_ctx,message);}
    JS_FreeValue(server_ctx,err); JS_FreeValue(server_ctx,ready); JS_FreeValue(server_ctx,g);
    static int h5_ready_mark;if(result==1&&!h5_ready_mark){h5_ready_mark=1;h5_mark("server_world_ready");}
    if(result<0) server_failed=1;
    return result;
}
static int ba_server_pump_sync(void) {
 static unsigned sample;BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={BA_SERVER_JS,(++sample%32)?0:ba_timing_now()};
    if(!server_ctx||server_failed) return -1;
    uint8_t input[16384];
    size_t n=bridge_queue_read(&serverbound,input,sizeof(input));
    if(n) {
        JSValue bytes=typed_u8(server_ctx,input,n);
        if(JS_IsException(bytes)) { report_exception(server_ctx,"input allocation"); server_failed=1; return -1; }
        int r=call_global(server_ctx,"__sp_onClientData",1,(JSValueConst *)&bytes);
        JS_FreeValue(server_ctx,bytes);
        if(r) { server_failed=1; return -1; }
    }
    JSValue now=JS_NewInt64(server_ctx,now_ms());
    int r=call_global(server_ctx,"__sp_pump",1,(JSValueConst *)&now);
    JS_FreeValue(server_ctx,now); drain_jobs(server_rt,server_ctx);
    if(r) server_failed=1;
    if (g17_save_due && connected && !server_failed && ba_server_ready() == 1) {
        uint64_t save_at = ba_timing_now();
        int32_t count = -1;
        struct mallinfo heap;
        SceKernelFreeMemorySizeInfo mem;
        g17_save_due = 0;
        g17_last_commit = -1; g17_write_us = g17_commit_us = g17_snap_us = g17_ser_us = 0; g17_last_bytes = 0;
        JSValue saved = JS_Eval(server_ctx, "__sp_saveNow()", strlen("__sp_saveNow()"), "<g17-autosave>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(saved)) report_exception(server_ctx, "g17 autosave");
        else JS_ToInt32(server_ctx, &count, saved);
        JS_FreeValue(server_ctx, saved); drain_jobs(server_rt, server_ctx);
        heap = mallinfo(); memset(&mem, 0, sizeof mem); mem.size = sizeof mem; sceKernelGetFreeMemorySize(&mem);
#ifdef __vita__
        FILE *log = fopen("ux0:/data/RS2Vita/save-timing.csv", "a");
        if (log) {
            static int header;
            if (!header) { fputs("mono_us,active_ms,snap_us,ser_us,write_us,commit_us,bytes,rc,count,pump_us,heap_bytes,user_free,rejected_gaps\n", log); header = 1; }
            fprintf(log, "%llu,%lld,%llu,%llu,%llu,%llu,%u,%d,%d,%llu,%d,%u,%d\n",
                (unsigned long long)sceKernelGetSystemTimeWide(), (long long)g17_clock.accumulated_ms,
                (unsigned long long)g17_snap_us, (unsigned long long)g17_ser_us,
                (unsigned long long)g17_write_us, (unsigned long long)g17_commit_us,
                (unsigned)g17_last_bytes, g17_last_commit, count,
                (unsigned long long)(ba_timing_now() - save_at), heap.uordblks, mem.size_user, g17_rejected);
            fclose(log);
        }
#endif
        ba_timing_record(BA_SAVE, ba_timing_now() - save_at);
    }

#ifdef __vita__
    ba_cadence_background_flush();
#endif
    return server_failed?-1:0;
}
int ba_server_ready(void){
#ifdef __vita__
    if(atomic_load(&world_worker.started))return atomic_load_explicit(&world_worker.ready,memory_order_acquire);
#endif
    return ba_server_ready_sync();
}
int ba_server_pump(void){
#ifdef __vita__
    if(atomic_load(&world_worker.started))return server_failed?-1:0;
#endif
    return ba_server_pump_sync();
}
int ba_server_connect(void) {
    WORLD_GUARD();
    if(connected||ba_server_ready()!=1) return -1;
    ++generation; if(!generation) ++generation;
    bridge_queue_reset(&clientbound,generation); bridge_queue_reset(&serverbound,generation);
    connected=1;
    if(call_global(server_ctx,"__sp_resumeClock",0,NULL)) { connected=0;return -1; }
    JSValue gen=JS_NewUint32(server_ctx,generation);
    int r=call_global(server_ctx,"__sp_clientConnect",1,(JSValueConst *)&gen);
    JS_FreeValue(server_ctx,gen); drain_jobs(server_rt,server_ctx);
    if(r) { connected=0; server_failed=1; }
    return r;
}
int ba_server_write(const void *bytes, size_t n) {
    if(!connected||server_failed||bridge_queue_write(&serverbound,generation,bytes,n)) return -1;
    return (int)n;
}
int ba_server_read(void *bytes,size_t n) {
    if(ba_server_pump()||!connected) return -1;
    return (int)bridge_queue_read(&clientbound,bytes,n);
}
int ba_server_disconnect(void) {
    WORLD_GUARD();
    if(!connected) return 0;
    /* Save while the connected player's object is still owned by the world. */
    int r=call_global(server_ctx,"__sp_saveNow",0,NULL);
    drain_jobs(server_rt,server_ctx);
    JSValue gen=JS_NewUint32(server_ctx,generation);
    if(call_global(server_ctx,"__sp_clientDisconnect",1,(JSValueConst *)&gen)) r=-1;
    JS_FreeValue(server_ctx,gen); drain_jobs(server_rt,server_ctx);
    connected=0;
    bridge_queue_reset(&clientbound,0); bridge_queue_reset(&serverbound,0);
    return r;
}
int ba_server_stop(void) {
#ifdef __vita__
    ba_world_stop(&world_worker);world_enter(NULL);
#endif
    int r=server_ctx?ba_server_disconnect():0;
    for(int i=0;i<128;i++) if(file_handles[i]) { fclose(file_handles[i]); file_handles[i]=NULL; }
    if(server_ctx) JS_FreeContext(server_ctx);
    if(server_rt) JS_FreeRuntime(server_rt);
    server_ctx=NULL; server_rt=NULL;
    rv_native_reset();
    return r;
}
