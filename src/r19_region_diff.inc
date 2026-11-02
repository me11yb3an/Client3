/* Exact source-pixel comparison within a known interface rectangle. */
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
static inline int r19_four_equal(const uint32_t*a,const uint32_t*b){
#if defined(__ARM_NEON)
 uint32x4_t eq=vceqq_u32(vld1q_u32(a),vld1q_u32(b));
 uint32x2_t both=vand_u32(vget_low_u32(eq),vget_high_u32(eq));
 return (vget_lane_u32(both,0)&vget_lane_u32(both,1))==UINT32_MAX;
#else
 return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];
#endif
}
static int r19_changed_region(const uint32_t*before,const uint32_t*after,int width,int height,int x,int y,int w,int h,H6Rect*out){
 if(!before||!after||width<=0||height<=0||w<=0||h<=0)return 0;
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
 if(x>=width||y>=height||w<=0||h<=0)return 0;
 if(w>width-x)w=width-x;if(h>height-y)h=height-y;
 int left=x+w,right=x,top=y+h,bottom=y;
 for(int row=y;row<y+h;row++){
  const uint32_t*a=before+(size_t)row*width,*b=after+(size_t)row*width;
  int l=x,r=x+w;
  while(r-l>=4&&r19_four_equal(a+l,b+l))l+=4;
  while(l<r&&a[l]==b[l])l++;
  if(l==r)continue;
  while(r-l>=4&&r19_four_equal(a+r-4,b+r-4))r-=4;
  while(r>l&&a[r-1]==b[r-1])r--;
  if(l<left)left=l;if(r>right)right=r;if(row<top)top=row;bottom=row+1;
 }
 if(right<=left)return 0;*out=(H6Rect){left,top,right-left,bottom-top};return 1;
}
