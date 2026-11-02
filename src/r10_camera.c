/* Independently authored local diagnostic. Disabled unless explicitly configured.
 * Camera only: no player movement, server changes, remote commands or save access. */
#include "client.h"
#include "ba_timing.h"
#include <stdio.h>
#include <stdint.h>
int r10_camera_replay(Client*c){
 static int checked,enabled,last_phase=-1;static uint64_t began;
 if(!checked){checked=1;FILE*f=fopen("ux0:/data/RS2Vita/hw-r10-camera-test.txt","r");int value=0;if(f){if(fscanf(f,"%d",&value)==1&&value==1)enabled=1;fclose(f);}}
 if(!enabled)return 0;
 uint64_t now=ba_timing_now();if(!began)began=now;
 unsigned seconds=(unsigned)((now-began)/1000000u);
 int phase=seconds<12?0:seconds<36?1+(seconds-12)/3:9;
 extern int recovery_reference_yaw,recovery_reference_pitch;
 int yaw=(recovery_reference_yaw+(phase>0&&phase<9?phase*256:0))&2047;
 int pitch=recovery_reference_pitch;
 c->orbit_camera_yaw=yaw;c->orbit_camera_pitch=pitch;
 c->camera_anticheat_offset_x=c->camera_anticheat_offset_z=c->camera_anticheat_angle=0;
 if(phase!=last_phase){FILE*f=fopen("ux0:/data/RS2Vita/r10-camera.csv",last_phase<0?"w":"a");
  if(f){if(last_phase<0)fputs("time_us,phase,yaw,pitch\n",f);fprintf(f,"%llu,%d,%d,%d\n",(unsigned long long)now,phase,yaw,pitch);fclose(f);}last_phase=phase;
 }
 return 1;
}
