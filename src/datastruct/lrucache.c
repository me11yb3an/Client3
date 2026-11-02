#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "doublylinklist.h"
#include "hashtable.h"
#include "linkable.h"
#include "lrucache.h"

LruCache *lrucache_new(int size) {
#ifdef HW_R5_HOST
    /* Host-only scaling experiment; never alters the Vita baseline cache. */
    const char *limit=getenv("HW_R5_CACHE_PERCENT");
    if(limit){int pct=atoi(limit);if(pct>=10&&pct<=100){size=size*pct/100;if(size<1)size=1;}}
#endif
    LruCache *cache = calloc(1, sizeof(LruCache));
    cache->capacity = size;
    cache->available = size;
    cache->hashtable = hashtable_new(1024);
    cache->history = doublylinklist_new();
    return cache;
}

void lrucache_free(LruCache *cache) {
    hashtable_free(cache->hashtable);
    doublylinklist_free(cache->history);
    free(cache);
}

void lrucache_free_entries(LruCache *cache, void (*destroy)(DoublyLinkable *)) {
    DoublyLinkable *node;
    while ((node = doublylinklist_pop(cache->history)) != NULL) {
        linkable_unlink(&node->link);
        if (destroy) {
            destroy(node);
        }
    }
    lrucache_free(cache);
}

DoublyLinkable *lrucache_get(LruCache *cache, int64_t key) {
    DoublyLinkable *node = (DoublyLinkable *)hashtable_get(cache->hashtable, key);
    if (node) {
        doublylinklist_push(cache->history, node);
    }

    return node;
}

void lrucache_put(LruCache *cache, int64_t key, DoublyLinkable *value) {
    if (cache->available == 0) {
        DoublyLinkable *node = doublylinklist_pop(cache->history);
        linkable_unlink(&node->link);
        doublylinkable_uncache(node);
    } else {
        cache->available--;
    }
    hashtable_put(cache->hashtable, key, &value->link);
    doublylinklist_push(cache->history, value);
}

void lrucache_clear(LruCache *cache) {
    while (true) {
        DoublyLinkable *node = doublylinklist_pop(cache->history);
        if (!node) {
            cache->available = cache->capacity;
            return;
        }

        linkable_unlink(&node->link);
        doublylinkable_uncache(node);
    }
}
