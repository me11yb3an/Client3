#include "r13_capture.h"
#include "r11_diag.h"
#include "gl11.h"
#include "r10_scene.h"
#include "h8_remote.h"
#ifndef BA_R10_COMPACT
#define BA_R10_COMPACT 0
#endif
#if BA_R10_COMPACT
#include "r10_compact.h"
#endif
#include "r16_fast.h"
#include "vita_enhanced.h"
#include "ba_timing.h"
#include "draw_phases.h"
#include "ba_hw_r4.h"
#ifndef BA_R9_UPLOAD_MODE
#define BA_R9_UPLOAD_MODE 0
#endif
#if BA_R9_UPLOAD_MODE < 0 || BA_R9_UPLOAD_MODE > 2
#error Invalid HW-R9 upload mode
#endif
#if BA_R9_UPLOAD_MODE >= 1
#include "r9_upload_cache.h"
#endif

#ifdef GL11

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "custom.h"
#include "defines.h"
#include "pix2d.h"
#include "pix3d.h"

extern Pix3D _Pix3D;
extern Pix2D _Pix2D;
extern Custom _Custom;
extern SceneData _World3D;

#define PI_DEGREES 180.0f
#define RS_TO_DEGREES (360.0f / 2048.0f)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TAU (M_PI * 2)
#define RS_TO_RADIANS (TAU / 2048.0f)
#define FRUSTUM_SCALE (25.0f / 256.0f)
#define DEFAULT_ZOOM 512.0f

#define Z_NEAR 50
// #define Z_FAR 3500
// NOTE: Z_FAR has to be 4500 to match software?
#define Z_FAR (4500.0f*world3d_draw_radius()/25.0f)

#define ATLAS_TEXTURE_COUNT 64 // White slot plus the loaded texture count, rounded to a power of two.
#define CLEAR_COLOR 0.0f, 0.0f, 0.0f, 1.0f

Vertex *verts;
static size_t vertex_capacity,index_capacity;
int vertcount;
uint32_t *indices;
int elementcount;
/* Reserve before writes, including model indices emitted before model vertices. */
void gl_reserve_geometry(unsigned nv,unsigned ni) {
    size_t needs[2]={(size_t)vertcount+nv,(size_t)elementcount+ni};
    size_t *caps[2]={&vertex_capacity,&index_capacity};
    for(unsigned i=0;i<2;i++) if(needs[i]>*caps[i]) {
        size_t cap=*caps[i]?*caps[i]:(i?100000u:300000u),stride=i?sizeof(*indices):sizeof(*verts);
        while(cap<needs[i]&&cap<=SIZE_MAX/2)cap*=2;
        if(cap<needs[i]||cap>INT32_MAX/stride){fprintf(stderr,"HW_R3_GEOMETRY_SIZE_FAILURE %u %u %u\n",i,nv,ni);abort();}
        void *p=realloc(i?(void*)indices:(void*)verts,cap*stride);
        if(!p){fprintf(stderr,"HW_R3_GEOMETRY_ALLOC_FAILURE %u %u\n",i,(unsigned)(cap*stride));abort();}
        if(i)indices=p;else verts=p;
        *caps[i]=cap;
        h4_geometry(vertcount,elementcount,vertex_capacity,index_capacity,1);
    }
}


uint32_t texture_atlas;
#ifdef __vita__
static GLuint hw_world_vbo;
#if BA_R9_UPLOAD_MODE >= 1
static R9UploadCache r9_world_shadow;
static uint64_t r9_resource_epoch;
static uint64_t r9_compare_us,r9_upload_us,r9_shadow_us;
#endif
static int hw_use_vbo=1;
#endif

// Opt-in diagnostic only: 0 original atlas UVs, 1 corrected UVs, absent normal play.
int recovery_water_reference=-1;
int recovery_reference_yaw=0;
int recovery_reference_pitch=128;
static bool use_opengl11;
static bool use_anisotropic;
#endif

// NOTE: everything gl-related not in this file is also behind GL11 define OR _Custom.use_opengl11

void gl_start_frame(void) {
 r11_begin();
#ifdef GL11
    if(h5_no_init()){_Custom.use_opengl11=false;return;}
    static uint64_t reference_check;
    uint64_t reference_now=rs2_now();
    if(reference_now>=reference_check){
        reference_check=reference_now+1000;
        FILE*f=fopen("ux0:/data/RS2Vita/water-reference.txt","r");int mode=-1,yaw=0,pitch=128;
        if(f){if(fscanf(f,"%d %d %d",&mode,&yaw,&pitch)<1)mode=-1;fclose(f);}
        recovery_water_reference=(mode>=0&&mode<=2)?mode:-1;
        recovery_reference_yaw=yaw&2047;
        recovery_reference_pitch=pitch<128?128:pitch>383?383:pitch;
    }
    use_opengl11 = _Custom.use_opengl11;
    glClearColor(CLEAR_COLOR);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
}

void gl_end_frame(void) {
#ifdef GL11
    _Custom.use_opengl11 = use_opengl11;
#endif
}

void gl_start_drawscene(void) {
    r10_frame_begin();
#ifdef GL11
#endif
}

void gl_discard_geometry(void){
#ifdef GL11
 vertcount=0;elementcount=0;
 /* Match normal post-world handoff: item icons and interface models are software. */
 _Custom.use_opengl11=false;
#endif
}

void gl_end_drawscene(Client *c) {
 r13_scene(_World3D.eyeX,_World3D.eyeY,_World3D.eyeZ,c->cameraYaw,c->cameraPitch);
 R11_SCOPE(R11_SUBMIT);
    if(!h4_world_enabled()){gl_discard_geometry();return;}
    (void)c;
    dp_begin(DP_GL_PREP);
#ifdef GL11
    if (elementcount > 0 || r16_has_static()) {
#if BA_R10_COMPACT
        unsigned r10_before=vertcount,r10_nv=vertcount;uint64_t r10_compact_at=ba_timing_now();
        int r10_compact_ok=r10_compact_stream(verts,sizeof(Vertex),&r10_nv,indices,elementcount,8u*1024u*1024u);
        if(r10_compact_ok)vertcount=r10_nv;
        static unsigned r10_compact_samples;
        if((++r10_compact_samples%30u)==0)rs2_log("HW_R10_COMPACT before=%u after=%u ok=%d us=%llu\n",r10_before,(unsigned)vertcount,r10_compact_ok,(unsigned long long)(ba_timing_now()-r10_compact_at));
#endif
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        int offsetX = 0;
        int offsetY = 0;
        /* Classic focal length on a 960-wide view shows 960/683 of the ground.
         * Match the 683-wide coverage on both axes. */
        float zoom = DEFAULT_ZOOM;
        if (ve_enabled()) zoom = DEFAULT_ZOOM * ((float)SCREEN_FB_WIDTH / (float)GAME_VIEW_WIDTH);

        int left = (int)(((float)(offsetX - _Pix3D.center_x) * 512.0f) / zoom);
        int right = (int)(((float)(offsetX + c->area_viewport->width - _Pix3D.center_x) * 512.0f) / zoom);
        int top = (int)(((float)(offsetY - _Pix3D.center_y) * 512.0f) / zoom);
        int bottom = (int)(((float)(offsetY + c->area_viewport->height - _Pix3D.center_y) * 512.0f) / zoom);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(left * FRUSTUM_SCALE, right * FRUSTUM_SCALE, -bottom * FRUSTUM_SCALE, -top * FRUSTUM_SCALE, Z_NEAR, Z_FAR);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(PI_DEGREES, 1, 0, 0);

        if (c->cameraPitch != 0) {
            glRotatef(c->cameraPitch * RS_TO_DEGREES, 1, 0, 0);
        }
        if (c->cameraYaw != 0) {
            glRotatef(c->cameraYaw * RS_TO_DEGREES, 0, 1, 0);
        }
        glTranslatef(-_World3D.eyeX, -_World3D.eyeY, -_World3D.eyeZ);

        // leave a black line on right side of viewport (see pix2d.c)
        int viewport_xoff = 8;
    #ifdef __vita__
        viewport_xoff += SCREEN_CENTER_XOFF;
    #endif
        int classic_clip = !ve_enabled();
        GLboolean previous_scissor = GL_FALSE;
        GLint saved_scissor[4];
        if (!classic_clip) glViewport(0,0,SCREEN_FB_WIDTH,SCREEN_FB_HEIGHT);
        else {
            int view_y = SCREEN_FB_HEIGHT - 11 - c->area_viewport->height;
            int view_w = c->area_viewport->width - 1;
            glViewport(viewport_xoff, view_y, view_w, c->area_viewport->height);
            /* The world must not paint over the surrounding fixed Classic frame. */
            previous_scissor = glIsEnabled(GL_SCISSOR_TEST);
            glGetIntegerv(GL_SCISSOR_BOX, saved_scissor);
            glEnable(GL_SCISSOR_TEST);
            glScissor(viewport_xoff, view_y, view_w, c->area_viewport->height);
        }
        glBindTexture(GL_TEXTURE_2D, texture_atlas);

        dp_begin(DP_GL_SUBMIT);
        uint64_t submit_at=ba_timing_now();
        /* Palette color 0 is transparent. Those texels are alpha 0. Discard
         * them so they do not blend over the black clear or write depth. */
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        if (r16_has_static()) r16_draw_static();
        if (r16_has_static()) r16_draw_water();
        r16_draw_dynamic();
        char buf[MAX_STR];
        uint64_t last = rs2_now();
        if (elementcount > 0) {
#ifdef __vita__
        if(hw_use_vbo && vertcount>0){
            /* One interleaved upload; vitaGL otherwise copies the entire
               stride separately for color, position and UV client arrays. */
            glBindBuffer(GL_ARRAY_BUFFER,hw_world_vbo);
#if BA_R9_UPLOAD_MODE == 2
            size_t r9_bytes=(size_t)vertcount*sizeof(Vertex);
            uint64_t r9_at=ba_timing_now();
            int r9_upload=r9_upload_needed(&r9_world_shadow,hw_world_vbo,r9_resource_epoch,verts,r9_bytes);
            r9_compare_us+=ba_timing_now()-r9_at;
            if(r9_upload){
                r9_at=ba_timing_now();
                /* Retained storage must not use vitaGL's transient stream pool. */
                r11_buffer_data(GL_ARRAY_BUFFER,(GLsizei)r9_bytes,verts,GL_STATIC_DRAW);
                int r9_ok=glGetError()==GL_NO_ERROR;
                r9_upload_us+=ba_timing_now()-r9_at;
                r9_at=ba_timing_now();
                r9_upload_commit(&r9_world_shadow,hw_world_vbo,r9_resource_epoch,verts,r9_bytes,r9_ok,8u*1024u*1024u);
                r9_shadow_us+=ba_timing_now()-r9_at;
            }
            if(r9_world_shadow.checks%30u==0 && !h8_is_running() && !h5_measuring())r10_upload_flush();
            if(r9_world_shadow.checks%120u==0)
                rs2_log("HW_R9_UPLOAD checks=%llu hits=%llu uploaded=%llu saved=%llu compared=%llu failures=%llu compare_us=%llu upload_us=%llu shadow_us=%llu shadow_capacity=%u\n",
                    (unsigned long long)r9_world_shadow.checks,(unsigned long long)r9_world_shadow.hits,
                    (unsigned long long)r9_world_shadow.uploaded_bytes,(unsigned long long)r9_world_shadow.saved_bytes,
                    (unsigned long long)r9_world_shadow.compared_bytes,(unsigned long long)r9_world_shadow.failures,
                    (unsigned long long)r9_compare_us,(unsigned long long)r9_upload_us,
                    (unsigned long long)r9_shadow_us,(unsigned)r9_world_shadow.capacity);
#elif BA_R9_UPLOAD_MODE == 1
            uint64_t r9_at=ba_timing_now();
            r11_buffer_data(GL_ARRAY_BUFFER,(GLsizei)((size_t)vertcount*sizeof(Vertex)),verts,GL_STATIC_DRAW);
            r9_upload_us+=ba_timing_now()-r9_at;r9_world_shadow.checks++;r9_world_shadow.uploaded_bytes+=(uint64_t)vertcount*sizeof(Vertex);
            if(r9_world_shadow.checks%30u==0 && !h8_is_running() && !h5_measuring())r10_upload_flush();
#else
            r11_buffer_data(GL_ARRAY_BUFFER,(GLsizei)((size_t)vertcount*sizeof(Vertex)),verts,GL_STREAM_DRAW);
#endif
            glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));
            glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));
            glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));
        }else
#endif
        {
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &verts[0].r);
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &verts[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &verts[0].u);
        }

        h4_geometry(vertcount,elementcount,vertex_capacity,index_capacity,0);
 r11_geometry(vertcount,elementcount,vertex_capacity*sizeof(Vertex)+index_capacity*sizeof(uint32_t));
        ba_profile_scene(world3d_draw_radius(),_World3D.eyeX/128,_World3D.eyeZ/128,c->cameraYaw,c->cameraPitch,vertcount,elementcount);
        static int h5_captured,h5_capture_checked,h5_capture_requested;
        if(!h5_capture_checked){h5_capture_checked=1;FILE*f=fopen("ux0:/data/RS2Vita/capture.txt","r");if(f){fscanf(f,"%d",&h5_capture_requested);fclose(f);}}
        if(h5_capture_requested==1&&!h5_captured&&!h5_measuring()&&h5_requested_run()){
          h5_captured=1;FILE*f=fopen("ux0:/data/RS2Vita/draw-capture.bin","wb");if(f){uint32_t header[]={0x35435748,1,sizeof(Vertex),vertcount,elementcount};fwrite(header,sizeof(header),1,f);fwrite(verts,sizeof(Vertex),vertcount,f);fwrite(indices,sizeof(uint32_t),elementcount,f);fclose(f);}h5_mark("draw_capture_saved");
        }
#if BA_R10_CACHE
        if (r16_fast_frame()) glDrawElements(GL_TRIANGLES, elementcount, GL_UNSIGNED_INT, indices);
        else r10_draw(hw_world_vbo,hw_use_vbo);
#else
        if(r14_selected_begin((unsigned)elementcount)){for(int k=0;k<elementcount;k++)r14_selected_vertex(verts+indices[k]);r14_selected_end();}
        r11_run(0,0,elementcount);
        if(elementcount){
            unsigned hi=0,lo=UINT32_MAX;
            for(int k=0;k<elementcount;k++){if(indices[k]>hi)hi=indices[k];if(indices[k]<lo)lo=indices[k];}
            r13_draw(lo,hi,elementcount);
            if(hi<=65535u && elementcount<=16384){
                static uint16_t short_idx[16384];
                for(int k=0;k<elementcount;k++) short_idx[k]=(uint16_t)indices[k];
#ifdef __vita__
                glDrawRangeElements(GL_TRIANGLES,lo,hi,elementcount,GL_UNSIGNED_SHORT,short_idx);
#else
                glDrawElements(GL_TRIANGLES, elementcount, GL_UNSIGNED_SHORT, short_idx);
#endif
            } else {
#ifdef __vita__
                /* The exact range was already scanned above. Supply it so
                 * vitaGL need not scan the same dynamic index list again. */
                glDrawRangeElements(GL_TRIANGLES,lo,hi,elementcount,GL_UNSIGNED_INT,indices);
#else
                glDrawElements(GL_TRIANGLES, elementcount, GL_UNSIGNED_INT, indices);
#endif
            }
        }
#endif
#ifdef __vita__
        if(hw_use_vbo)glBindBuffer(GL_ARRAY_BUFFER,0);
#endif
        }
        ba_timing_record(BA_GL_SUBMIT,ba_timing_now()-submit_at);
        /* Glass vertices are already GL alpha 47. Draw them after opaque
         * scenery, water and dynamic meshes so the pane blends over the
         * exterior and the depth-biased post keeps the joint. */
        r16_draw_body_layers();
        if (r16_has_static()) r16_draw_static_alpha();
        dp_end();
        dp_begin(DP_GL_TAIL);
        ve_hud_draw_metrics((int)(rs2_now()-last),vertcount,elementcount);
        if (!ve_enabled() && _Custom.show_performance && !ve_overlay_held()) {
            sprintf(buf, "glDrawElements: %lu ms", rs2_now() - last);
            drawStringRight(c->font_plain11, 507, 226, classic_perf_display(0,buf), YELLOW, true);

            sprintf(buf, "verts/indices: %d %d", vertcount, elementcount);
            drawStringRight(c->font_plain11, 507, 239, classic_perf_display(1,buf), YELLOW, true);
        }

        vertcount = 0;
        elementcount = 0;

        /* 2D interface quads sit at z 0. A leftover test hides them behind the world. */
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        if (classic_clip) {
            glScissor(saved_scissor[0], saved_scissor[1], saved_scissor[2], saved_scissor[3]);
            if (!previous_scissor) glDisable(GL_SCISSOR_TEST);
        }
        glViewport(0, 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        dp_end();
    }
    dp_end();

    // scene is rendered with gl, the rest must be in software so pixmaps don't draw over interface models
    _Custom.use_opengl11 = false;
#endif
}

#if defined(GL11) && defined(__vita__)
static GLuint paving_texture;
static int paving_attempted;
#endif
uint32_t gl_paving_texture(void){
#if defined(GL11) && defined(__vita__)
    if(paving_attempted)return paving_texture;
    paving_attempted=1;
    const int id=46,size=_Pix3D.lowMemory?64:128;
    int *texels=pix3d_get_texels(id);if(!texels)return 0;
    uint32_t *pixels=malloc((size_t)size*size*4);if(!pixels)return 0;
    int transparent=0;
    for(int i=0;i<size*size;i++){uint32_t rgb=(uint32_t)texels[i];if(!rgb)transparent++;pixels[i]=rgb?rgb|0xff000000u:0u;}
    /* The dedicated opaque paving sampler must not reinterpret cutout art. */
    GLenum error=GL_NO_ERROR;
    if(!transparent){
        glGenTextures(1,&paving_texture);glBindTexture(GL_TEXTURE_2D,paving_texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,size,size,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,pixels);
        glGenerateMipmap(GL_TEXTURE_2D);error=glGetError();
        if(error!=GL_NO_ERROR&&paving_texture){glDeleteTextures(1,&paving_texture);paving_texture=0;}
    }
    free(pixels);
    FILE*f=fopen("ux0:/data/RS2Vita/paving-sampler.csv","w");
    if(f){fprintf(f,"texture,size,transparent,gl_error,ready\n%d,%d,%d,%u,%d\n",id,size,transparent,error,paving_texture!=0);fclose(f);}
    glBindTexture(GL_TEXTURE_2D,texture_atlas);return paving_texture;
#else
    return 0;
#endif
}

void gl_set_brightness(void) {
    r10_reset();
#if defined(GL11) && defined(__vita__)
    if(paving_texture)glDeleteTextures(1,&paving_texture);paving_texture=0;paving_attempted=0;
#endif
#ifdef GL11
    if(h5_no_init()||(h5_silent()&&h5_measuring())){_Custom.use_opengl11=false;return;}
    int texture_size = 128; // _Pix3D.textures[id]->width/height
    if (_Pix3D.lowMemory) {
        texture_size = 64;
    }

    int atlas_width = ATLAS_TEXTURE_COUNT * texture_size;

    uint32_t *pixels = malloc(atlas_width * texture_size * sizeof(uint32_t));
    /* Empty atlas slots stay transparent so a sample past a texture edge is discarded. */
    memset(pixels, 0, (size_t)atlas_width * (size_t)texture_size * sizeof(uint32_t));
    if (use_anisotropic) {
        // fixes gouraud triangles without changing uvs
        memset(pixels, 0xff, atlas_width * texture_size * sizeof(uint32_t));
    } else {
        for (int y = 0; y < texture_size; y++) {
            for (int x = 0; x < texture_size; x++) {
                pixels[y * atlas_width + x] = 0xffffffff; // white texture at idx 0
            }
        }
    }

    for (int id = 0; id < _Pix3D.textureCount && id + 1 < ATLAS_TEXTURE_COUNT; id++) {
        int *texels = pix3d_get_texels(id);

        if (!texels) {
            continue;
        }

        int atlas_xoff = texture_size * (id + 1);

        for (int y = 0; y < texture_size; y++) {
            for (int x = 0; x < texture_size; x++) {
                int rgb = texels[y * texture_size + x];

                if (rgb != 0) {
                    rgb |= 0xff000000;
                }
                pixels[y * atlas_width + atlas_xoff + x] = rgb;
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, texture_atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_width, texture_size, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    free(pixels);

    if (use_anisotropic) {
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
        float amount = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &amount);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, amount);

#define GL_GENERATE_MIPMAP                0x8191
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // this is clamped manually in pix3d now since all textures are combined
#ifdef __vita__
    gl_paving_texture(); /* Prepare at texture/brightness load, outside steady gameplay. */
    glBindTexture(GL_TEXTURE_2D,texture_atlas);
#endif
#endif
}

#ifdef GL11
// https://www.opengl.org/archives/resources/features/OGLextensions/
static int isExtensionSupported(const char *extension) {
  const GLubyte *extensions = NULL;
  const GLubyte *start;
  GLubyte *where, *terminator;

  /* Extension names should not have spaces. */
  where = (GLubyte *) strchr(extension, ' ');
  if (where || *extension == '\0')
    return 0;
  extensions = glGetString(GL_EXTENSIONS);
  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  start = extensions;
  for (;;) {
    where = (GLubyte *) strstr((const char *) start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ')
      if (*terminator == ' ' || *terminator == '\0')
        return 1;
    start = terminator;
  }
  return 0;
}

static bool gl_load_extension(const char *ext) {
    if (isExtensionSupported(ext)) {
        rs2_log("%s available\n", ext);
        return true;
    }
    return false;
}

// source:
// https://rune-server.org/threads/texture-mapping-conversions-pmn-uv-pmn.701381/
// https://rune-server.org/threads/better-texture-mapping-conversions.706373/

UV pmn_to_uv(int xA, int yA, int zA, int xB, int yB, int zB, int xC, int yC, int zC, int xP, int yP, int zP, int xM, int yM, int zM, int xN, int yN, int zN) {
    float mX = xM - xP;
    float mY = yM - yP;
    float mZ = zM - zP;

    float nX = xN - xP;
    float nY = yN - yP;
    float nZ = zN - zP;

    float aX = xA - xP;
    float aY = yA - yP;
    float aZ = zA - zP;

    float bX = xB - xP;
    float bY = yB - yP;
    float bZ = zB - zP;

    float cX = xC - xP;
    float cY = yC - yP;
    float cZ = zC - zP;

    float MxNx = mY * nZ - mZ * nY;
    float MxNy = mZ * nX - mX * nZ;
    float MxNz = mX * nY - mY * nX;

    float uX = nY * MxNz - nZ * MxNy;
    float uY = nZ * MxNx - nX * MxNz;
    float uZ = nX * MxNy - nY * MxNx;

    float mU = 1.0f / (uX * mX + uY * mY + uZ * mZ);
    float uA = (uX * aX + uY * aY + uZ * aZ) * mU;
    float uB = (uX * bX + uY * bY + uZ * bZ) * mU;
    float uC = (uX * cX + uY * cY + uZ * cZ) * mU;

    float vX = mY * MxNz - mZ * MxNy;
    float vY = mZ * MxNx - mX * MxNz;
    float vZ = mX * MxNy - mY * MxNx;

    float mV = 1.0f / (vX * nX + vY * nY + vZ * nZ);
    float vA = (vX * aX + vY * aY + vZ * aZ) * mV;
    float vB = (vX * bX + vY * bY + vZ * bZ) * mV;
    float vC = (vX * cX + vY * cY + vZ * cZ) * mV;

    return (UV){uA, uB, uC, vA, vB, vC};
}

static inline float clamp01(float x) {
    if (!use_anisotropic) {
        return x < 0 ? 0 : (x > 1 ? 1 : x);
    } else {
        float min = 0.01f;
        float max = 0.99f;
        return x <= min ? min : (x >= max ? max : x);
    }
}
#endif

void gl_load(void) {
    r10_shutdown();gl_actor_cache_reset();
#ifdef GL11
    if(h5_no_init()){_Custom.use_opengl11=false;return;}
    rs2_log("OpenGL: %s, %s, %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    int size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);

    int lowmem_atlas_size = ATLAS_TEXTURE_COUNT * 64;
    int highmem_atlas_size = ATLAS_TEXTURE_COUNT * 128;

    if (size < lowmem_atlas_size) {
        rs2_error("GL_MAX_TEXTURE_SIZE: %d/%d won't fit in texture atlas\n", size, lowmem_atlas_size);
        exit(1);
    }

    if (!_Pix3D.lowMemory && size < highmem_atlas_size) {
        _Pix3D.lowMemory = true;
        rs2_log("GL_MAX_TEXTURE_SIZE: %d/%d, using low memory textures\n", size, highmem_atlas_size);
    }

    ba_profile_gl_limits(size,_Pix3D.lowMemory);
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (!extensions) {
        rs2_error("No extensions found\n");
        exit(1);
    }

    if (!gl_load_extension("GL_EXT_bgra")) {
        rs2_error("GL_EXT_bgra extension not found\n");
        exit(1);
    }

    if (gl_load_extension("GL_SGIS_generate_mipmap") && gl_load_extension("GL_EXT_texture_filter_anisotropic")) {
        // NOTE: makes transparent textures like fishing spots/fountain water darker and hard to see
        // NOTE: also creates some visible gaps, it would work better with modern gl texture arrays/shaders
        // use_anisotropic = true;
    }

    rs2_log("\n");

#ifdef __vita__
    FILE*hw_vbo_file=fopen("ux0:/data/RS2Vita/hw-r2-vbo.txt","r");
    if(hw_vbo_file){int value=1;if(fscanf(hw_vbo_file,"%d",&value)==1&&(value==0||value==1))hw_use_vbo=value;fclose(hw_vbo_file);}
#if BA_R9_UPLOAD_MODE >= 1
    r9_upload_destroy(&r9_world_shadow);r9_resource_epoch++;
#endif
    if(hw_use_vbo)r11_gen_buffers(1,&hw_world_vbo);
#endif
    glGenTextures(1, &texture_atlas);

    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    // glEnable(GL_DEPTH_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#endif
}

void glGouraudTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int zA, int zB, int zC, int colorA, int colorB, int colorC, int alpha) {
 R11_SCOPE(R11_EMIT);
    (void)xA, (void)xB, (void)xC, (void)yA, (void)yB, (void)yC, (void)zA, (void)zB, (void)zC, (void)colorA, (void)colorB, (void)colorC, (void)alpha;
#ifdef GL11
    Vertex v0 = {(_Pix3D.palette[colorA] >> 16) & 0xff, (_Pix3D.palette[colorA] >> 8) & 0xff, _Pix3D.palette[colorA] & 0xff, alpha, xA, yA, zA, 0, 0};
    Vertex v1 = {(_Pix3D.palette[colorB] >> 16) & 0xff, (_Pix3D.palette[colorB] >> 8) & 0xff, _Pix3D.palette[colorB] & 0xff, alpha, xB, yB, zB, 0, 0};
    Vertex v2 = {(_Pix3D.palette[colorC] >> 16) & 0xff, (_Pix3D.palette[colorC] >> 8) & 0xff, _Pix3D.palette[colorC] & 0xff, alpha, xC, yC, zC, 0, 0};

    gl_reserve_geometry(3,0);
    verts[vertcount++] = v0;
    verts[vertcount++] = v1;
    verts[vertcount++] = v2;

    // glTexCoord2f(0, 0);
    // glColor4ub((_Pix3D.palette[colorA] >> 16) & 0xff, (_Pix3D.palette[colorA] >> 8) & 0xff, _Pix3D.palette[colorA] & 0xff, alpha);
    // glVertex3i(xA, yA, zA);
    // glColor4ub((_Pix3D.palette[colorB] >> 16) & 0xff, (_Pix3D.palette[colorB] >> 8) & 0xff, _Pix3D.palette[colorB] & 0xff, alpha);
    // glVertex3i(xB, yB, zB);
    // glColor4ub((_Pix3D.palette[colorC] >> 16) & 0xff, (_Pix3D.palette[colorC] >> 8) & 0xff, _Pix3D.palette[colorC] & 0xff, alpha);
    // glVertex3i(xC, yC, zC);
#endif
}

#include "r40_water_gate.h"
#include "r93_lava_scroll.h"
#include "r106_scenery_speed.h"
static int r40_model_water;
int gl_model_water_possible(void){return r40_model_water;}

void glTextureTriangle(int xA, int xB, int xC, int yA, int yB, int yC, int zA, int zB, int zC, int shadeA, int shadeB, int shadeC, UV uv, int texture) {
 R11_SCOPE(R11_EMIT);
    (void)xA, (void)xB, (void)xC, (void)yA, (void)yB, (void)yC, (void)zA, (void)zB, (void)zC, (void)shadeA, (void)shadeB, (void)shadeC, (void)uv, (void)texture;
#ifdef GL11
    extern ClientData _Client;
    if (!_Client.lowmem) {
        // scrolling textures from updateTextures
        if ((texture == 17 || texture == 24) && !r16_freeze_scroll()) {
            float offset = (recovery_water_reference==0||recovery_water_reference==1)?0.0f:r106_water_offset(rs2_now());
            uv.vA += offset;
            uv.vB += offset;
            uv.vC += offset;
        }
    }

    /* Fire cape50 is separate from the retained water pass and its probes.
     * Actor templates reject textured models, so these UVs refresh normally. */
    if (texture == 50 && !r16_freeze_scroll()) {
        static int last_cycle = -1, last_rows;
        static float lava_offset;
        int rows = _Pix3D.lowMemory ? 64 : 128;
        if (last_cycle != _Client.loop_cycle || last_rows != rows) {
            last_cycle = _Client.loop_cycle; last_rows = rows;
            lava_offset = r93_lava_offset(rs2_now(), rows);
        }
        uv.vA += lava_offset; uv.vB += lava_offset; uv.vC += lava_offset;
    }

    int shadeShiftA = 255 - (shadeA << 1);
    int shadeShiftB = 255 - (shadeB << 1);
    int shadeShiftC = 255 - (shadeC << 1);

    // manual clamp since opengl will no longer do it for us
    uv.uA = clamp01(uv.uA);
    uv.uB = clamp01(uv.uB);
    uv.uC = clamp01(uv.uC);

    if(r40_texture_may_water(texture))r40_model_water=1;
    int texture_idx = 1 + texture; // after default white texture
    float inset=recovery_water_reference==0?0.0f:0.5f/(_Pix3D.lowMemory?64.0f:128.0f);
    uv.uA = (texture_idx + inset + uv.uA*(1.0f-2.0f*inset)) / ATLAS_TEXTURE_COUNT;
    uv.uB = (texture_idx + inset + uv.uB*(1.0f-2.0f*inset)) / ATLAS_TEXTURE_COUNT;
    uv.uC = (texture_idx + inset + uv.uC*(1.0f-2.0f*inset)) / ATLAS_TEXTURE_COUNT;

    Vertex v0 = {shadeShiftA, shadeShiftA, shadeShiftA, 0xff, xA, yA, zA, uv.uA, uv.vA};
    Vertex v1 = {shadeShiftB, shadeShiftB, shadeShiftB, 0xff, xB, yB, zB, uv.uB, uv.vB};
    Vertex v2 = {shadeShiftC, shadeShiftC, shadeShiftC, 0xff, xC, yC, zC, uv.uC, uv.vC};

    gl_reserve_geometry(3,0);
    verts[vertcount++] = v0;
    verts[vertcount++] = v1;
    verts[vertcount++] = v2;

    // glColor4ub(shadeShiftA, shadeShiftA, shadeShiftA, 0xff);
    // glTexCoord2f(uv.uA, uv.vA);
    // glVertex3i(xA, yA, zA);

    // glColor4ub(shadeShiftB, shadeShiftB, shadeShiftB, 0xff);
    // glTexCoord2f(uv.uB, uv.vB);
    // glVertex3i(xB, yB, zB);

    // glColor4ub(shadeShiftC, shadeShiftC, shadeShiftC, 0xff);
    // glTexCoord2f(uv.uC, uv.vC);
    // glVertex3i(xC, yC, zC);
#endif
}

/* Transform each shared vertex once for both retained scenery and actors.
 * Face order, hidden faces, authored colors and alpha match the scalar path. */
#include "r79_actor_indices.inc"
static int gl_emit_untextured(Model *model, int sceneX, int sceneY, int sceneZ, int yaw) {
    enum { FLAT_MAX = 8192 };
    static int flat_x[FLAT_MAX], flat_y[FLAT_MAX], flat_z[FLAT_MAX];
    int verts_n, faces, yaw_sin, yaw_cos, origin_x, origin_y, origin_z;
    if (!model || model->textured_face_count > 0) return 0;
    verts_n = model->vertex_count;
    faces = model->face_count;
    if (verts_n <= 0 || verts_n > FLAT_MAX || faces <= 0 || faces > 20000) return 0;
    if (!model->vertices_x || !model->vertices_y || !model->vertices_z) return 0;
    if (!model->face_indices_a || !model->face_indices_b || !model->face_indices_c) return 0;
    if (!model->face_color_a || !model->face_color_b || !model->face_color_c) return 0;
    if (!_Pix3D.palette) return 0;
    if (model->face_infos) {
        for (int t = 0; t < faces; t++) {
            if (model->face_infos[t] == -1) continue;
            int type = model->face_infos[t] & 3;
            if (type == 2 || type == 3) return 0;
        }
    }
    yaw_sin = _Pix3D.sin_table[(2048 - yaw) & 0x7ff];
    yaw_cos = _Pix3D.cos_table[(2048 - yaw) & 0x7ff];
    origin_x = sceneX + _World3D.eyeX;
    origin_y = sceneY + _World3D.eyeY;
    origin_z = sceneZ + _World3D.eyeZ;
    for (int v = 0; v < verts_n; v++) {
        int x = model->vertices_x[v];
        int y = model->vertices_y[v];
        int z = model->vertices_z[v];
        int tmp = x;
        x = (tmp * yaw_cos - z * yaw_sin) >> 16;
        z = (tmp * yaw_sin + z * yaw_cos) >> 16;
        flat_x[v] = x + origin_x;
        flat_y[v] = y + origin_y;
        flat_z[v] = z + origin_z;
    }
    if(r79_emit(model,flat_x,flat_y,flat_z))return 1;
    gl_reserve_geometry((unsigned)faces * 3u, (unsigned)faces * 3u);
    for (int t = 0; t < faces; t++) {
        if (model->face_infos && model->face_infos[t] == -1) continue;
        int a = model->face_indices_a[t];
        int b = model->face_indices_b[t];
        int c = model->face_indices_c[t];
        int color_a = model->face_color_a[t];
        int color_b = model->face_color_b[t];
        int color_c = model->face_color_c[t];
        int info = model->face_infos ? model->face_infos[t] : 0;
        int alpha = model->face_alphas ? (0xff - model->face_alphas[t]) : 0xff;
        int base = vertcount;
        int xyz[3] = {a, b, c};
        int colors[3];
        if ((info & 3) == 1) color_c = color_b = color_a;
        colors[0] = color_a;
        colors[1] = color_b;
        colors[2] = color_c;
        for (int k = 0; k < 3; k++) {
            int rgb = _Pix3D.palette[colors[k]];
            Vertex *dst = &verts[vertcount++];
            dst->r = (unsigned char)((rgb >> 16) & 0xff);
            dst->g = (unsigned char)((rgb >> 8) & 0xff);
            dst->b = (unsigned char)(rgb & 0xff);
            dst->a = (unsigned char)alpha;
            dst->x = (float)flat_x[xyz[k]];
            dst->y = (float)flat_y[xyz[k]];
            dst->z = (float)flat_z[xyz[k]];
            dst->u = 0.0f;
            dst->v = 0.0f;
        }
        indices[elementcount++] = (uint32_t)base;
        indices[elementcount++] = (uint32_t)base + 1;
        indices[elementcount++] = (uint32_t)base + 2;
    }
    return 1;
}

void gl_start_model(Model *model, int sceneX, int sceneY, int sceneZ, int yaw) {
#ifdef GL11
    r40_model_water=0;
#endif
 R11_SCOPE(R11_EMIT);
    (void)model, (void)sceneX, (void)sceneY, (void)sceneZ, (void)yaw;
#ifdef GL11
    if (!_Custom.use_opengl11 || r16_suppress_emit()) {
        return;
    }
#if BA_R10_CACHE
    int r10_x=sceneX+_World3D.eyeX,r10_y=sceneY+_World3D.eyeY,r10_z=sceneZ+_World3D.eyeZ;
    if(!r16_emitting() && r10_model_hit(model,r10_x,r10_y,r10_z,yaw))return;
    int r10_base=vertcount;uint64_t r10_at=ba_timing_now();
#endif
    int *triangleColorsA = model->face_color_a;
    int *triangleColorsB = model->face_color_b;
    int *triangleColorsC = model->face_color_c;

    if (!triangleColorsA || !triangleColorsB || !triangleColorsC) {
        return;
    }

    int *verticesX = model->vertices_x;
    int *verticesY = model->vertices_y;
    int *verticesZ = model->vertices_z;

    int *triangleA = model->face_indices_a;
    int *triangleB = model->face_indices_b;
    int *triangleC = model->face_indices_c;

    int *triangleColors = model->face_colors;

    int *triangleAlphas = model->face_alphas;

    int *triangleInfos = model->face_infos;

    int *textureMappingP = model->textured_p_coordinate;
    int *textureMappingM = model->textured_m_coordinate;
    int *textureMappingN = model->textured_n_coordinate;

    int triangleCount = model->face_count;
    int yawSin = _Pix3D.sin_table[(2048 - yaw) & 0x7ff];
    int yawCos = _Pix3D.cos_table[(2048 - yaw) & 0x7ff];
    int emitIndex = r16_emitting();
    if (emitIndex && gl_emit_untextured(model, sceneX, sceneY, sceneZ, yaw)) return;

    // glPushMatrix();
    // extern SceneData _World3D;
    // glTranslatef(sceneX + _World3D.eyeX, sceneY + _World3D.eyeY, sceneZ + _World3D.eyeZ);
    // glRotatef(yaw * RS_TO_DEGREES, 0, 1, 0);

    for (int t = 0; t < triangleCount; t++) {
        int a = triangleA[t];
        int b = triangleB[t];
        int c = triangleC[t];

        int xa = verticesX[a];
        int ya = verticesY[a];
        int za = verticesZ[a];

        int xb = verticesX[b];
        int yb = verticesY[b];
        int zb = verticesZ[b];

        int xc = verticesX[c];
        int yc = verticesY[c];
        int zc = verticesZ[c];

        int colorA = triangleColorsA[t];
        int colorB = triangleColorsB[t];
        int colorC = triangleColorsC[t];

        /* Source 0 is opaque. Source 208 keeps 48/256 of the glass color.
         * GL alpha is 255 minus that source value. Textured faces do not
         * use this value; their cutout is texel alpha 0. */
        int alpha = 0xff;
        if (triangleAlphas) {
            alpha = 0xff - triangleAlphas[t];
        }

        int info = 0;
        if (triangleInfos) {
            info = triangleInfos[t];
        }
        /* A merged butt joint stores -1. The software rasterizer skips that
         * face. Info -1 also selects the textured branch with shade 0, which
         * this emitter would draw as an opaque white cap. */
        if (info == -1) {
            continue;
        }

        int type = info & 0x3;

        // Flat shading
        if (type == 1 || type == 3) {
            colorC = colorB = colorA;
        }

        int textureId = -1;

        int tmp = xa;
        xa = (tmp * yawCos - za * yawSin) >> 16;
        za = (tmp * yawSin + za * yawCos) >> 16;

        tmp = xb;
        xb = (tmp * yawCos - zb * yawSin) >> 16;
        zb = (tmp * yawSin + zb * yawCos) >> 16;

        tmp = xc;
        xc = (tmp * yawCos - zc * yawSin) >> 16;
        zc = (tmp * yawSin + zc * yawCos) >> 16;

        // translate
        xa += sceneX + _World3D.eyeX;
        xb += sceneX + _World3D.eyeX;
        xc += sceneX + _World3D.eyeX;
        ya += sceneY + _World3D.eyeY;
        yb += sceneY + _World3D.eyeY;
        yc += sceneY + _World3D.eyeY;
        za += sceneZ + _World3D.eyeZ;
        zb += sceneZ + _World3D.eyeZ;
        zc += sceneZ + _World3D.eyeZ;

        int base = 0;
        if (emitIndex) {
            gl_reserve_geometry(3, 3);
            base = vertcount;
        }
        // Textured
        if ((type == 2 || type == 3) && triangleColors) {
            textureId = triangleColors[t];

            int texCoord = info >> 2;
            int p = textureMappingP[texCoord];
            int m = textureMappingM[texCoord];
            int n = textureMappingN[texCoord];

            UV uv = pmn_to_uv(verticesX[a], verticesY[a], verticesZ[a], verticesX[b], verticesY[b], verticesZ[b], verticesX[c], verticesY[c], verticesZ[c], verticesX[p], verticesY[p], verticesZ[p], verticesX[m], verticesY[m], verticesZ[m], verticesX[n], verticesY[n], verticesZ[n]);

            glTextureTriangle(xa, xb, xc, ya, yb, yc, za, zb, zc, colorA, colorB, colorC, uv, textureId);
        } else {
            glGouraudTriangle(xa, xb, xc, ya, yb, yc, za, zb, zc, colorA, colorB, colorC, alpha);
        }
        if (emitIndex) {
            indices[elementcount++] = (uint32_t)base;
            indices[elementcount++] = (uint32_t)base + 1;
            indices[elementcount++] = (uint32_t)base + 2;
        }
    }

#if BA_R10_CACHE
    if (!r16_emitting()) {
        r10_emit_time(ba_timing_now()-r10_at);
        r10_model_store(model,r10_x,r10_y,r10_z,yaw,r10_base);
    }
#endif
    // glPopMatrix();
#endif
}

void r10_upload_flush(void){
#if defined(__vita__) && BA_R9_UPLOAD_MODE >= 1
    static unsigned logged;
    FILE*f=fopen("ux0:/data/RS2Vita/r9-upload.csv",logged?"a":"w");if(!f)return;
    if(!logged)fputs("mode,checks,hits,uploaded_bytes,saved_bytes,compared_bytes,failures,compare_us,upload_us,shadow_us,shadow_capacity\n",f);
    fprintf(f,"%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u\n",BA_R9_UPLOAD_MODE,(unsigned long long)r9_world_shadow.checks,(unsigned long long)r9_world_shadow.hits,(unsigned long long)r9_world_shadow.uploaded_bytes,(unsigned long long)r9_world_shadow.saved_bytes,(unsigned long long)r9_world_shadow.compared_bytes,(unsigned long long)r9_world_shadow.failures,(unsigned long long)r9_compare_us,(unsigned long long)r9_upload_us,(unsigned long long)r9_shadow_us,(unsigned)r9_world_shadow.capacity);
    fclose(f);logged=1;
#endif
}
