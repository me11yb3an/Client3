#pragma once
#include <stdint.h>
#include "collisionmap.h"
/* Match the ordinary client pathfinder's directional collision masks. This
 * is a read-only check for the exact continuous interpolation path; it sends
 * no movement request and never changes the authoritative route. */
static int r45_cell_clear(const CollisionMap*m,int x,int z,unsigned mask){
 return x>=0&&z>=0&&x<m->sizeX&&z<m->sizeZ&&m->flags[x]&&!(m->flags[x][z]&mask);
}
static int r45_step_clear(const CollisionMap*m,int x,int z,int nx,int nz){
 int dx=nx-x,dz=nz-z;
 if(dx< -1||dx>1||dz< -1||dz>1)return 0;
 if(!dx&&!dz)return 1;
 unsigned horizontal=dx<0?0x280108u:0x280180u,vertical=dz<0?0x280102u:0x280120u;
 if(!dz)return r45_cell_clear(m,nx,nz,horizontal);
 if(!dx)return r45_cell_clear(m,nx,nz,vertical);
 unsigned diagonal=dx<0?(dz<0?0x28010eu:0x280138u):(dz<0?0x280183u:0x2801e0u);
 return r45_cell_clear(m,nx,nz,diagonal)&&r45_cell_clear(m,nx,z,horizontal)&&r45_cell_clear(m,x,nz,vertical);
}
static int r45_clear_recovery_segment(const CollisionMap*m,int ax,int az,int bx,int bz){
 if(!m||!m->flags||m->sizeX<=0||m->sizeZ<=0||ax<0||az<0||bx<0||bz<0)return 0;
 int64_t dx=(int64_t)bx-ax,dz=(int64_t)bz-az;
 if(dx< -1024||dx>1024||dz< -1024||dz>1024)return 0;
 int x=ax/128-m->offsetX,z=az/128-m->offsetZ;
 if(x<0||z<0||x>=m->sizeX||z>=m->sizeZ)return 0;
 while(ax!=bx||az!=bz){
  int sx=bx>ax?1:bx<ax?-1:0,sz=bz>az?1:bz<az?-1:0;
  ax+=sx;az+=sz;
  int nx=ax/128-m->offsetX,nz=az/128-m->offsetZ;
  /* Opposite directions cross integer cell boundaries one fine unit apart.
   * Keep both side-cell guards even at that corner, as the BFS does. */
  if(sx&&sz&&(nx!=x||nz!=z)&&!r45_step_clear(m,x,z,x+sx,z+sz))return 0;
  if(!r45_step_clear(m,x,z,nx,nz))return 0;x=nx;z=nz;
 }
 return 1;
}
