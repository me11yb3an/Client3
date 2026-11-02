#include "ba_hw_r3.h"
#include "h8_remote.h"
#ifdef __vita__
#include <psp2/kernel/threadmgr.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "ba_cadence_queue.inc"
#include "r44_scene_loading_clock.h"
extern int r16_diag_verbose(void);
void ba_cadence_diagnostic(const char*line){ba_cadence_line(line);}
/* Opt-in probe request only: drain after a quiet title interval. Normal
 * frame producers and login behavior do not perform this synchronous I/O. */
void ba_cadence_probe_flush(void){ba_cadence_flush();}
uint64_t ba_scene_motion_recover_until;

/* Record real world-space motion at presentation boundaries, including the
 * first frames after a region rebuild. No entity, camera or clock is changed. */
static void ba_travel_present(Client*c,uint64_t now){
 static uint64_t previous;static int px,pz,eye_x,eye_z,level,base_x,base_z;
 if(!c->ingame||!c->local_player){previous=0;ba_scene_motion_recover_until=0;return;}
 if(c->scene_state!=2)return;
 PathingEntity*actor=&c->local_player->pathing_entity;
 int x=c->sceneBaseTileX*128+actor->x,z=c->sceneBaseTileZ*128+actor->z;
 int ex=c->sceneBaseTileX*128+c->cameraX,ez=c->sceneBaseTileZ*128+c->cameraZ;
 if(previous&&level==c->currentLevel){
  int dx=x-px,dz=z-pz,cx=ex-eye_x,cz=ez-eye_z;
  uint64_t gap=now-previous;
  if(gap>=250000u||dx>256||dx< -256||dz>256||dz< -256||base_x!=c->sceneBaseTileX||base_z!=c->sceneBaseTileZ){
   char line[320];snprintf(line,sizeof line,"# travel_motion %llu gap %llu from %d %d to %d %d delta %d %d camera_delta %d %d base %d %d previous_base %d %d level %d path %d scene_delta %d",(unsigned long long)now,(unsigned long long)gap,px,pz,x,z,dx,dz,cx,cz,c->sceneBaseTileX,c->sceneBaseTileZ,base_x,base_z,c->currentLevel,actor->pathLength,c->scene_delta);ba_cadence_line(line);
  }
 }
 previous=now;px=x;pz=z;eye_x=ex;eye_z=ez;level=c->currentLevel;base_x=c->sceneBaseTileX;base_z=c->sceneBaseTileZ;
}

static void ba_cadence_atexit(void) {
    ba_cadence_flush();
    ba_timing_flush_final();
}

static void ba_cadence_arm(void) {
    static int armed;
    if (!armed) {
        atexit(ba_cadence_atexit);
        armed = 1;
    }
}
/* Build A default: 60 FPS presentation target, independent fixed client logic. */
static bool ba_run_paced(Client *c) {
    FILE *config=fopen("ux0:/data/RS2Vita/render_fps.txt","r");
    int target=60;if(config){fscanf(config,"%d",&target);fclose(config);}
    if(target!=30&&target!=60)target=60;
    uint64_t logic_due=ba_timing_now(),render_due=logic_due,last=logic_due;
    unsigned frames=0,ticks=0,fps_frames=0;
    uint32_t draw_us_window[128];
    unsigned draw_n=0;
    unsigned draw_hist[12]={0};
    unsigned sleep_n=0;
    uint64_t sleep_req=0,sleep_act=0;
    uint64_t fps_at=logic_due,last_render_us=0;
    uint64_t last_present=0,present_gap_max=0;unsigned present_over50=0;
    uint64_t render_period=1000000u/(unsigned)target;
    uint64_t target_check=logic_due;
    while(c->shell->state>=0){
      {
        h3_frame_begin();H3_SCOPE(H3_FRAME);if(!h5_requested_run())h4_poll();
        int h5ready=c->ingame&&c->scene_state==2;
        int h5x=c->local_player?c->sceneBaseTileX+c->local_player->pathing_entity.x/128:0;
        int h5z=c->local_player?c->sceneBaseTileZ+c->local_player->pathing_entity.z/128:0;
        h8_game_step(c);
        if(h5_step(h5ready,h5x,h5z)){rs2_sleep(100);continue;}
        uint64_t now=ba_timing_now();
        /* Loading before login does not accrue gameplay logic backlog. */
        if(now-last>1000000u&&!c->ingame){logic_due=now;render_due=now;}
        last=now;
        if(!h5_requested_run()&&r16_diag_verbose()&&now-target_check>=1000000u){
            FILE *setting=fopen("ux0:/data/RS2Vita/render_fps.txt","r");int requested=target;
            if(setting){fscanf(setting,"%d",&requested);fclose(setting);}
            if((requested==30||requested==60)&&requested!=target){
                target=requested;render_period=1000000u/(unsigned)target;render_due=now;
                FILE *log=fopen("ux0:/data/RS2Vita/presentation-targets.csv","a");
                if(log){fprintf(log,"%llu,%d\n",(unsigned long long)now,target);fclose(log);}
            }
            target_check=now;
        }
        {H3_SCOPE(H3_INPUT);platform_poll_events(c);}
        while(now>=logic_due){
            uint64_t began=ba_timing_now();bool was_ingame=c->ingame,was_world_ready=c->scene_state==2;
            {H3_SCOPE(H3_LOGIC);client_update(c);}
            uint64_t logic_us=ba_timing_now()-began;
            if(logic_us>40000u&&!r16_diag_verbose()){char line[96];snprintf(line,sizeof line,"# logic_hitch %llu %llu",(unsigned long long)ba_timing_now(),(unsigned long long)logic_us);ba_cadence_line(line);}
            ba_timing_record(BA_LOGIC,logic_us);ticks++;
            /* Synchronous login/loading is not elapsed gameplay time. */
            if((!was_ingame&&c->ingame)||(!was_world_ready&&c->scene_state==2)){now=ba_timing_now();logic_due=now;}
            c->shell->mouse_click_button=0;
            c->shell->key_queue_read_pos=c->shell->key_queue_write_pos;
            logic_due+=(uint64_t)c->shell->deltime*1000u;
            if(c->shell->state>0&&--c->shell->state==0){ba_cadence_flush();ba_timing_flush_final();gameshell_shutdown(c);return true;}
        }
        uint64_t began=ba_timing_now();
        {H3_SCOPE(H3_SERVER);if(ba_server_ready()>=0)ba_server_pump();}
        ba_timing_record(BA_SERVER_PUMP,ba_timing_now()-began);
        now=ba_timing_now();
        if(now>=render_due&&!h5_silent()){
            extern unsigned r16_bake_count(void);
            unsigned scenery_before=r16_bake_count();
            ba_profile_render();began=ba_timing_now();{H3_SCOPE(H3_RENDER);client_draw(c);}
            last_render_us=ba_timing_now()-began;
            if(draw_n<128)draw_us_window[draw_n++]=(uint32_t)last_render_us;
            ba_timing_record(BA_RENDER,last_render_us);
            gameshell_update_touch(c);
            began=ba_timing_now();{H3_SCOPE(H3_PRESENT);platform_update_surface();}
            uint64_t present_us=ba_timing_now()-began;
            if(present_us>40000u&&!r16_diag_verbose()){char line[96];snprintf(line,sizeof line,"# present_hitch %llu %llu",(unsigned long long)ba_timing_now(),(unsigned long long)present_us);ba_cadence_line(line);}
            ba_timing_record(BA_PRESENT,present_us);
            frames++;
            now=ba_timing_now();
            uint64_t previous_due=logic_due;
            if(r44_finish_scene_loading(now,last_render_us,scenery_before,r16_bake_count(),c->ingame&&c->scene_state==2,c->shell->deltime,&logic_due)){
                ba_scene_motion_recover_until=now+4000000u;
                char line[160];snprintf(line,sizeof line,"# scene_loading_clock %llu draw_us %llu avoided_client_backlog_us %llu",(unsigned long long)now,(unsigned long long)last_render_us,(unsigned long long)(now>previous_due?now-previous_due:0));ba_cadence_line(line);
            }
            ba_travel_present(c,now);
            if(last_present){uint64_t gap=now-last_present;if(gap>present_gap_max)present_gap_max=gap;if(gap>50000u)present_over50++;}
            last_present=now;
            /* Drop missed presentations, never run extra simulation to render. */
            render_due+=render_period;
            if(render_due<now)render_due=now;
            if(now-fps_at>=1000000u){
                uint64_t window_us=now-fps_at;
                c->shell->fps=(int)((uint64_t)(frames-fps_frames)*1000000u/window_us);
                unsigned n=draw_n;
                uint32_t p50=0,p95=0,pmax=0;
                if(n){
                    for(unsigned i=1;i<n;i++){
                        uint32_t v=draw_us_window[i];
                        unsigned j=i;
                        while(j&&draw_us_window[j-1]>v){draw_us_window[j]=draw_us_window[j-1];j--;}
                        draw_us_window[j]=v;
                    }
                    p50=draw_us_window[(n-1)/2];
                    p95=draw_us_window[((n*95u)+99u)/100u-1u];
                    pmax=draw_us_window[n-1];
                    static const uint32_t edges[11]={8000,12000,16667,20000,33333,50000,100000,200000,400000,700000,1000000};
                    for(unsigned i=0;i<n;i++){
                        unsigned bucket=0;
                        while(bucket<11&&draw_us_window[i]>=edges[bucket])bucket++;
                        draw_hist[bucket]++;
                    }
                }
                unsigned sleep_report_n=sleep_n;
                uint64_t sleep_report_req=sleep_req,sleep_report_act=sleep_act;
                uint64_t gap_report=present_gap_max;unsigned gap_over_report=present_over50;present_gap_max=0;present_over50=0;
                fps_at=now;fps_frames=frames;draw_n=0;sleep_n=0;sleep_req=0;sleep_act=0;
                {
                extern int r16_diag_verbose(void);extern void r16_io_add(int,unsigned long long);
                uint64_t io0=ba_timing_now();
                if(r16_diag_verbose()){
                FILE *guest=fopen("ux0:/data/RS2Vita/guest-fps.txt","w");
                if(guest){fprintf(guest,"%d %llu %u %u %u %u\n",c->shell->fps,(unsigned long long)last_render_us,n,p50,p95,pmax);fclose(guest);}
                FILE *cadence=fopen("ux0:/data/RS2Vita/frame-cadence.txt","a");
                if(cadence){
                    fprintf(cadence,"%llu %llu %d %u %u %u %u",(unsigned long long)now,(unsigned long long)window_us,c->shell->fps,n,p50,p95,pmax);
                    for(int bucket=0;bucket<12;bucket++)fprintf(cadence," %u",draw_hist[bucket]);
                    fprintf(cadence," sleep %u %llu %llu present_gap %llu %u",sleep_report_n,(unsigned long long)sleep_report_req,(unsigned long long)sleep_report_act,(unsigned long long)gap_report,gap_over_report);
                    fputc('\n',cadence);
                    fclose(cadence);
                }
                }else{
                ba_cadence_arm();
                char guest_line[128];snprintf(guest_line,sizeof guest_line,"%d %llu %u %u %u %u\n",c->shell->fps,(unsigned long long)last_render_us,n,p50,p95,pmax);
                ba_cadence_set_guest(guest_line);
                extern unsigned r16_bake_count(void),r16_roof_swaps(void),r16_door_updates(void),r16_skill_updates(void),r16_item_updates(void);
                extern int r16_fallback_reason(void);
                char state[256];PathingEntity*actor=c->local_player?&c->local_player->pathing_entity:NULL;
                snprintf(state,sizeof state,"# state %llu world %d %d path %d yaw %d pitch %d npcs %d bakes %u swaps %u doors %u resources %u items %u fallback %d",(unsigned long long)now,
                    actor?c->sceneBaseTileX+actor->x/128:0,actor?c->sceneBaseTileZ+actor->z/128:0,actor?actor->pathLength:0,c->orbit_camera_yaw,c->orbit_camera_pitch,c->npc_count,r16_bake_count(),r16_roof_swaps(),r16_door_updates(),r16_skill_updates(),r16_item_updates(),r16_fallback_reason());
                ba_cadence_line(state);
                char line[384];
                int used=snprintf(line,sizeof line,"%llu %llu %d %u %u %u %u",(unsigned long long)now,(unsigned long long)window_us,c->shell->fps,n,p50,p95,pmax);
                for(int bucket=0;bucket<12&&used>0&&used<(int)sizeof line;bucket++){
                    int step=snprintf(line+used,sizeof line-(size_t)used," %u",draw_hist[bucket]);
                    if(step<0||used+step>=(int)sizeof line){used=-1;break;}
                    used+=step;
                }
                if(used>0){
                    int step=snprintf(line+used,sizeof line-(size_t)used," sleep %u %llu %llu present_gap %llu %u",sleep_report_n,(unsigned long long)sleep_report_req,(unsigned long long)sleep_report_act,(unsigned long long)gap_report,gap_over_report);
                    if(step>0&&used+step<(int)sizeof line)ba_cadence_line(line);
                    else ba_cadence_note_drop();
                }else ba_cadence_note_drop();
                /* The world worker persists this queue without stalling presentation. */
                }
                r16_io_add(3,ba_timing_now()-io0);
                }
            }
        }
        extern void ba_vita_sample(unsigned,unsigned);
        {H3_SCOPE(H3_DIAGNOSTICS);ba_vita_sample(frames,ticks);}
        now=ba_timing_now();
        if(!h5_silent()){
            /* Logic stays on its 20ms quantum, but that wait was the presentation
             * clock: a 13ms frame slept about 6ms and landed near 50Hz. Wait for
             * the next presentation only. An overdue logic step runs at the top
             * of the next iteration. */
            if(render_due>now){
                uint64_t wait=render_due-now,began=now;
                for(;;){
                    /* One clock sample: crossing the deadline between two
                     * samples must never underflow into a multi-minute sleep. */
                    uint64_t at=ba_timing_now();
                    if(at>=render_due)break;
                    uint64_t left=render_due-at;
                    if(left>2000u)sceKernelDelayThreadCB((SceUInt)(left-1000u));
                    else break;
                }
                while(ba_timing_now()<render_due){}
                sleep_n++;sleep_req+=wait;sleep_act+=ba_timing_now()-began;
            }
        }else if(logic_due>now){
            H3_SCOPE(H3_SLEEP);rs2_sleep((int)((logic_due-now+999u)/1000u));
        }
      } /* Close all frame scopes before resetting summary counters. */
      {H3_SCOPE(H3_DIAGNOSTICS);if(!h5_measuring())ba_timing_flush();}
    }
    ba_cadence_flush();
    ba_timing_flush_final();
    if(c->shell->state==-1)gameshell_shutdown(c);
    return true;
}

