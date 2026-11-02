#include "vita_title_layout.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "classic_ui.h"
#include "pix2d.h"
extern Pix2D _Pix2D;
#include "vita_wide_layout.h"

int classic_repeat_axis(int position, int source, int target, int cap) {
    if (source == target || position < cap) return position;
    if (position >= target - cap) return source - (target - position);
    return cap + (position - cap) % (source - 2 * cap);
}
void classic_extend_pix8(Pix8 *image, int width, int height, int cap_x, int cap_y) {
    if (width == image->width && height == image->height) return;
    int8_t *pixels = malloc(width * height);
    if (!pixels) abort();
    for (int y=0;y<height;y++) for (int x=0;x<width;x++)
        pixels[y*width+x]=image->pixels[classic_repeat_axis(y,image->height,height,cap_y)*image->width+classic_repeat_axis(x,image->width,width,cap_x)];
    free(image->pixels); image->pixels=pixels;
    image->width=image->crop_w=width; image->height=image->crop_h=height;
}
// backbase1 contains all four lighter stone sockets, not separate sprites.
// Copy their caps at original size and repeat only the plain lighter interior.
void classic_extend_chat_controls(Pix8 *image) {
#ifdef VITA_NATIVE_WIDESCREEN
    const int centers[4] = {57, 186, 326, 462};
    const int cell_width = CHAT_CONTROLS_WIDTH / 4;
    int8_t *pixels = malloc(CHAT_CONTROLS_WIDTH * image->height);
    if (!pixels) abort();
    for (int y = 0; y < image->height; y++) {
        for (int button = 0; button < 4; button++) {
            for (int x = 0; x < cell_width; x++) {
                int local = classic_repeat_axis(x, 114, cell_width, 24);
                int source = centers[button] - 57 + local;
                // The report socket ends at the original canvas edge. Complete
                // its right cap with the intact original public-chat stone cap.
                if (button == 3 && local >= 90) source = local;
                pixels[y * CHAT_CONTROLS_WIDTH + button * cell_width + x] =
                    image->pixels[y * image->width + source];
            }
        }
    }
    free(image->pixels); image->pixels = pixels;
    image->width = image->crop_w = CHAT_CONTROLS_WIDTH;
#else
    (void)image;
#endif
}
// backhmid1: the complete button/socket artwork starts on original row 29.
// Insert repeatable stone above it; move every socket by the button offset.
void classic_extend_tab_backing(Pix8 *image, int height) {
    if (height == image->height) return;
    const int sockets_y = 29, tile_y = 16, tile_height = 12;
    int added = height - image->height;
    if (added < 0 || image->height <= sockets_y) abort();
    int8_t *pixels = malloc(image->width * height);
    if (!pixels) abort();
    for (int y = 0; y < height; y++) {
        int source_y = y < sockets_y ? y :
            y < sockets_y + added ? tile_y + (y - sockets_y) % tile_height : y - added;
        memcpy(pixels + y * image->width, image->pixels + source_y * image->width, image->width);
    }
    free(image->pixels);
    image->pixels = pixels;
    image->height = image->crop_h = height;
}
void classic_extend_pix24(Pix24 *image, int width, int height, int cap_x, int cap_y) {
    if (width == image->width && height == image->height) return;
    int *pixels = malloc(width * height * sizeof(int));
    if (!pixels) abort();
    for (int y=0;y<height;y++) for (int x=0;x<width;x++)
        pixels[y*width+x]=image->pixels[classic_repeat_axis(y,image->height,height,cap_y)*image->width+classic_repeat_axis(x,image->width,width,cap_x)];
    free(image->pixels); image->pixels=pixels;
    image->width=image->crop_w=width; image->height=image->crop_h=height;
}
// Quilt only the backhmid2 repeat boundary using original, unblended pixels.
// A connected minimum-error cut avoids slicing straight through stone shading.
void classic_extend_chat_border(Pix24 *image, int width) {
#ifdef VITA_NATIVE_WIDESCREEN
    if (image->width == 562 && image->height == 30 && width == 733) {
        enum { OVERLAP = 32, ROWS = 30, LEFT = 482, RIGHT = 48 };
        int original[ROWS][OVERLAP], repeated[ROWS][OVERLAP];
        long long cost[ROWS][OVERLAP];
        int previous[ROWS][OVERLAP], cut[ROWS];
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < OVERLAP; x++) {
                int a = original[y][x] = image->pixels[y * image->width + LEFT + x];
                int b = repeated[y][x] = image->pixels[y * image->width + RIGHT + x];
                long long error = 0;
                for (int shift = 0; shift <= 16; shift += 8) {
                    int delta = ((a >> shift) & 255) - ((b >> shift) & 255);
                    error += delta * delta;
                }
                int best = x;
                if (y) {
                    best = x > 0 ? x - 1 : x;
                    for (int q = best + 1; q <= x + 1 && q < OVERLAP; q++)
                        if (cost[y-1][q] < cost[y-1][best]) best = q;
                    error += cost[y-1][best];
                }
                cost[y][x] = error;
                previous[y][x] = best;
            }
        }
        int best = 0;
        for (int x = 1; x < OVERLAP; x++)
            if (cost[ROWS-1][x] < cost[ROWS-1][best]) best = x;
        cut[ROWS-1] = best;
        for (int y = ROWS-1; y > 0; y--) cut[y-1] = previous[y][cut[y]];
        classic_extend_pix24(image, width, ROWS, 64, 0);
        for (int y = 0; y < ROWS; y++)
            for (int x = 0; x < OVERLAP; x++)
                image->pixels[y * width + LEFT + x] =
                    x <= cut[y] ? original[y][x] : repeated[y][x];
        return;
    }
#endif
    classic_extend_pix24(image, width, image->height, 64, 0);
}
// Preserve both original half-backgrounds at 1:1, including complete
// columns, brazier bases and wall patterns. Extend only their plain center
// fog/stone join; mirrored tile turns keep adjacent source pixels continuous.
Pix24 *classic_wide_title_background(Pix24 *original) {
    Pix24 *wide = pix24_new(960, 544, false);
    for (int y = 0; y < 544; y++) {
        int source_y = y - TITLE_Y(0);
        if (source_y < 0) source_y = -source_y - 1;
        if (source_y >= original->height) source_y = 2 * original->height - source_y - 1;
        for (int x = 0; x < 960; x++) {
            int source_x;
            if (x < original->width) source_x = x;
            else if (x >= 960 - original->width) source_x = 959 - x;
            else {
                int left_distance = x - original->width;
                int right_distance = 959 - original->width - x;
                int distance = left_distance < right_distance ? left_distance : right_distance;
                int phase = distance % 30;
                source_x = original->width - 1 - (phase < 16 ? phase : 30 - phase);
            }
            wide->pixels[y * 960 + x] = original->pixels[source_y * original->width + source_x];
        }
    }
    return wide;
}
// Center the visible original artwork in the currently bound title surface.
// Sprite crop offsets are applied by pix24_draw, so subtract them here.
void classic_draw_title_logo(Pix24 *logo, int y) {
    int left = logo->width, right = -1;
    for (int row = 0; row < logo->height; row++) {
        for (int col = 0; col < logo->width; col++) {
            if (logo->pixels[row * logo->width + col]) {
                if (col < left) left = col;
                if (col > right) right = col;
            }
        }
    }
    if (right < left) return;
    int artwork_width = right - left + 1;
    if (artwork_width > _Pix2D.width || y + logo->crop_y + logo->height > _Pix2D.height) abort();
    int x = (_Pix2D.width - artwork_width) / 2 - left - logo->crop_x;
    pix24_draw(logo, x, y);
}
int classic_chat_wrap(const char *text, PixFont *font, int width, ClassicChatLine *lines, int capacity) {
    int length=strlen(text), start=0, count=0;
    while (start < length && count < capacity) {
        int end=start, space=-1;
        char sample[CLASSIC_CHAT_CAPACITY];
        while (end < length) {
            int token=(text[end]=='@' && end+4<length && text[end+4]=='@') ? 5 : 1;
            int n=end-start+token;
            if (n >= CLASSIC_CHAT_CAPACITY) break;
            memcpy(sample,text+start,n); sample[n]=0;
            if (stringWidth(font,sample)>width && end>start) break;
            if (text[end]==' ') space=end;
            end+=token;
        }
        int next=end;
        if (end<length && space>start) {end=space;next=space+1;}
        if (end==start) {end++;next=end;}
        lines[count++]=(ClassicChatLine){start,end};
        start=next;
        while (text[start]==' ') start++;
    }
    return count;
}
static bool chat_message(Client *c, int i, char *text, int *prefix, int *prefix_color, int *body_color) {
    int type=c->message_type[i];
    bool friend=client_is_friend(c,c->message_sender[i]);
    *prefix=0;*prefix_color=BLACK;*body_color=BLACK;
    if (!c->message_text[i][0]) return false;
    if (type==2 && !(c->public_chat_setting==0 || (c->public_chat_setting==1 && friend))) return false;
    if ((type==3 || type==7) && (c->split_private_chat || !(type==7 || c->private_chat_setting==0 || (c->private_chat_setting==1 && friend)))) return false;
    if ((type==5 || type==6) && (c->split_private_chat || c->private_chat_setting>=2)) return false;
    if ((type==4 || type==8) && !(c->trade_chat_setting==0 || (c->trade_chat_setting==1 && friend))) return false;
    text[0]=0;
    if (type==1 || type==2) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"%s: ",c->message_sender[i]);
        *prefix_color=type==1 ? WHITE : BLACK;*body_color=BLUE;
    } else if (type==3 || type==7) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"From %s: ",c->message_sender[i]);*body_color=DARKRED;
    } else if (type==6) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"To %s: ",c->message_sender[i]);*body_color=DARKRED;
    } else if (type==4 || type==8) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"%s ",c->message_sender[i]);*prefix_color=*body_color=type==4 ? TRADE_MESSAGE : DUEL_MESSAGE;
    } else if (type==5) *body_color=DARKRED;
    else if (type!=0) return false;
    *prefix=strlen(text);
    snprintf(text+*prefix,CLASSIC_CHAT_CAPACITY-*prefix,"%s",c->message_text[i]);
    return true;
}
int classic_chat_rows(Client *c, int index, PixFont *font) {
    char text[CLASSIC_CHAT_CAPACITY];int prefix,pc,bc;
    if (!chat_message(c,index,text,&prefix,&pc,&bc)) return 0;
    ClassicChatLine lines[CLASSIC_CHAT_CAPACITY];
    return classic_chat_wrap(text,font,CHAT_TEXT_WIDTH,lines,CLASSIC_CHAT_CAPACITY);
}
void classic_draw_chat(Client *c, PixFont *font) {
    int row=0;
    for (int i=0;i<100;i++) {
        char text[CLASSIC_CHAT_CAPACITY];int prefix,pc,bc;
        if (!chat_message(c,i,text,&prefix,&pc,&bc)) continue;
        ClassicChatLine lines[CLASSIC_CHAT_CAPACITY];
        int count=classic_chat_wrap(text,font,CHAT_TEXT_WIDTH,lines,CLASSIC_CHAT_CAPACITY);
        for (int j=0;j<count;j++) {
            int y=c->chat_scroll_offset+70-(row+count-1-j)*14;
            if (y<=0 || y>=110) continue;
            ClassicChatLine line=lines[j];char part[CLASSIC_CHAT_CAPACITY];int x=4;
            int split=line.end<prefix ? line.end : prefix;
            if (split>line.start) {
                int n=split-line.start;memcpy(part,text+line.start,n);part[n]=0;
                drawString(font,x,y,part,pc);x+=stringWidth(font,part);
            }
            int start=line.start>prefix ? line.start : prefix;
            if (line.end>start) {
                int n=line.end-start;memcpy(part,text+start,n);part[n]=0;
                drawString(font,x,y,part,bc);
            }
        }
        row+=count;
    }
    c->chat_scroll_height=row*14+7;
    if(c->chat_scroll_height<78)c->chat_scroll_height=78;
}

void classic_draw_chat_input(PixFont *font, int x, int y, const char *text, int color) {
#ifdef VITA_NATIVE_WIDESCREEN
    while (*text && stringWidth(font,text)>CHAT_TEXT_RIGHT-x) text++;
#endif
    drawString(font,x,y,text,color);
}

// Optional lossless native-screen background; legacy cache remains untouched.
Pix24 *classic_load_restored_title(void){
    FILE*f=fopen("rom/login-background.rgb","rb");if(!f)return NULL;
    unsigned char h[16];if(fread(h,1,16,f)!=16 || memcmp(h,"RS2BG001",8) || h[8] || h[9] || h[10]!=3 || h[11]!=192 || h[12] || h[13] || h[14]!=2 || h[15]!=32){fclose(f);return NULL;}
    Pix24*p=pix24_new(960,544,false);unsigned char row[960*3];
    for(int y=0;y<544;y++){if(fread(row,1,sizeof(row),f)!=sizeof(row)){pix24_free(p);fclose(f);return NULL;}for(int x=0;x<960;x++)p->pixels[y*960+x]=(row[x*3]<<16)|(row[x*3+1]<<8)|row[x*3+2];}
    if(fgetc(f)!=EOF){pix24_free(p);fclose(f);return NULL;}fclose(f);return p;
}

// Approved 95% logo presentation; original archive sprite remains immutable.
void classic_draw_title_logo_scaled(Pix24 *logo, int y) {
 int left=logo->width,right=-1,top=logo->height,bottom=-1;
 for(int row=0;row<logo->height;row++)for(int col=0;col<logo->width;col++)if(logo->pixels[row*logo->width+col]){
  if(col<left)left=col;if(col>right)right=col;if(row<top)top=row;if(row>bottom)bottom=row;
 }
 if(right<left)return;
 int sw=right-left+1,sh=bottom-top+1,dw=(sw*95+50)/100,dh=(sh*95+50)/100;
 int ox=(_Pix2D.width-dw)/2,oy=y+logo->crop_y+top+(sh-dh+1)/2;
 for(int dy=0;dy<dh;dy++)for(int dx=0;dx<dw;dx++){
  int sx=left+(sw==444&&dw==422?approved_logo_x[dx]:(2*dx+1)*sw/(2*dw)),sy=top+(sh==142&&dh==135?approved_logo_y[dy]:(2*dy+1)*sh/(2*dh)),color=logo->pixels[sy*logo->width+sx];
  if(color&&ox+dx>=0&&ox+dx<_Pix2D.width&&oy+dy>=0&&oy+dy<_Pix2D.height)_Pix2D.pixels[(oy+dy)*_Pix2D.width+ox+dx]=color;
 }
}

/* Approved in-game corner cleanups only. Original archives and title sprites stay untouched.
 * Source pixels are neighboring backvmid3 stone; masks reproduce the reviewed previews. */
void classic_repair_lower_corner(Pix8 *corner, const Pix8 *neighbor) {
    if (!corner || !neighbor || corner->width != 288 || corner->height != 40 ||
        neighbor->width != 61 || neighbor->height != 117 ||
        corner->palette_count < 1 || corner->palette_count > 256 ||
        neighbor->palette_count < 1 || neighbor->palette_count > 256) return;
    static const unsigned int first[14] = {0xffffu, 0xffffu, 0x1ffffu, 0x1ffffu, 0x3ffffu, 0x3ffffu, 0x3ffffu, 0x3ffffu, 0x3fffcu, 0x3fff0u, 0x3ffc0u, 0x3ff00u, 0x7e00u, 0x800u};
    static const unsigned int second[29] = {0x1fu, 0x1fu, 0x1fu, 0x3fu, 0x3fu, 0x7fu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0x1ffu, 0x1ffu, 0x3ffu, 0x3ffu, 0x3ffu, 0x3ffu, 0x7ffu, 0x7ffu, 0x7ffu, 0x7ffu, 0x3ffu, 0x3ffu, 0x1ffu, 0x1ffu, 0x7fu, 0x1fu, 0x7u, 0x1u};
    int capacity = corner->palette_count + neighbor->palette_count;
    if (capacity > 256) capacity = 256;
    int *palette = malloc((size_t)capacity * sizeof(*palette));
    if (!palette) return; /* Failed load-time repair preserves the original sprite. */
    memcpy(palette, corner->palette, (size_t)corner->palette_count * sizeof(*palette));
    int count = corner->palette_count;
    unsigned char map[256] = {0};
    for (int pass = 0; pass < 2; pass++) {
        int w = pass ? 12 : 18, h = pass ? 29 : 14;
        int sx = pass ? 22 : 15, sy = pass ? 69 : 15;
        const unsigned int *mask = pass ? second : first;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            if (!(mask[y] & (1u << x))) continue;
            unsigned char index = (unsigned char)neighbor->pixels[(sy+y)*neighbor->width+sx+x];
            if (!index || index >= neighbor->palette_count) { free(palette); return; }
            int color = neighbor->palette[index], target = 1;
            while (target < count && palette[target] != color) target++;
            if (target == count) {
                if (count == capacity) { free(palette); return; }
                palette[count++] = color;
            }
            map[index] = (unsigned char)target;
        }
    }
    for (int pass = 0; pass < 2; pass++) {
        int w = pass ? 12 : 18, h = pass ? 29 : 14;
        int dx = pass ? 0 : 16, dy = pass ? 0 : 16;
        int sx = pass ? 22 : 15, sy = pass ? 69 : 15;
        const unsigned int *mask = pass ? second : first;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (mask[y] & (1u << x)) {
            unsigned char index = (unsigned char)neighbor->pixels[(sy+y)*neighbor->width+sx+x];
            corner->pixels[(dy+y)*corner->width+dx+x] = (int8_t)map[index];
        }
    }
    free(corner->palette);
    corner->palette = palette;
    corner->palette_count = count;
}
