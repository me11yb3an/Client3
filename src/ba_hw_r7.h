#pragma once
#include <stdint.h>
void h7_init(void);
int h7_worker_enabled(void);
void h7_live_poll(void);
void h7_live_finish(void);
void h7_native_alloc(int id,unsigned type,unsigned requested);
int h7_owner_thread_id(void);
void h7_owner_snapshot(uint64_t*rpc_calls,uint64_t*wait_ns,unsigned*peak);
