#pragma once
#include <stddef.h>
#include <stdint.h>
/* Read-only resources, indexed in place. No extraction and no save interception. */
int ba_resource_init(void);
int ba_resource_list(const char *path, void (*emit)(void*,const char*), void *ctx);
void ba_resource_summary(uint64_t *opens,uint64_t *reads,uint64_t *bytes,uint64_t *index_bytes);

/* Immutable packed resource metadata:1found,0missing,-1not a valid pack. */
int ba_resource_metadata(const char*path,uint32_t*crc,uint32_t*size);
uint64_t ba_resource_metadata_calls(void);
