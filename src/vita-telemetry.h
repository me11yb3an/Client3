#pragma once
#include <stdint.h>
/* Optional transport; metrics supplied by game main thread. No strings/saves uploaded. */
typedef struct {
 uint64_t wall_us,client_updates,render_calls,present_calls,logic_us,render_us,convert_us,upload_us,server_ticks,server_us;
 uint64_t main_cpu_us,telemetry_cpu_us,ram_reserved,ram_used,cdram_reserved,cdram_used,phy_reserved,phy_used;
 uint64_t graphics_attempts,gxm_attempts;int cpu_valid,memory_valid,probe_only;
 uint64_t native_phy_live,native_cdram_live,native_phy_peak,native_cdram_peak,os_user_free,os_phy_free;
 int os_valid,native_valid,kind,tracked_threads,batch_start,tid[4],thread_rc[4];uint64_t thread_clocks[4];
} TelemetrySample;
int telemetry_start(const char*config,const char*run,const char*build);
void telemetry_push(const TelemetrySample*s);
void telemetry_stop(void);
int telemetry_acknowledged(void);
uint64_t telemetry_thread_cpu_us(int*valid);

int telemetry_thread_id(void);

void telemetry_stats(unsigned *drops,unsigned *errors);
