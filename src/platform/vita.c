#include "vita_title_layout.h"
#include "vita_button_edges.h"
#include "r89_stone_cycle.h"
#include "r13_capture.h"
#include "r12_waits.h"
#include "r11_diag.h"
#include "r10_scene.h"
 #include "../h8_remote.h"
#include "../ba_timing.h"
#include "../vita_dialogue.h"
#if defined(__vita__) && (!defined(SDL) || SDL == 0)
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>
#include <psp2/power.h>

#include "../client.h"
#include "../custom.h"
#include "../gameshell.h"
#include "../inputtracking.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../vita_wide_layout.h"
#include "../vita_enhanced.h"
#include "../gl11.h"
#include "../r27_cursor.h"

#include "../thirdparty/bzip.h"
#define TSF_IMPLEMENTATION
#include "../thirdparty/tsf.h"
#define TML_IMPLEMENTATION
#include "../thirdparty/tml.h"

int vita_last_rx, vita_last_ry;
extern ClientData _Client;
extern InputTracking _InputTracking;
extern Custom _Custom;
#ifdef GL11
static bool cursor_visible;
static int cursor_want_x,cursor_want_y;
static bool cursor_pixel(int dx, int dy, uint32_t *color) {
    if ((dy == 0 && dx >= -5 && dx <= 5) || (dx == 0 && dy >= -5 && dy <= 5)) {
        *color = 0x00FFFFFF;
        return true;
    }
    if ((dy == 1 && dx >= -4 && dx <= 6) || (dx == 1 && dy >= -4 && dy <= 6)) {
        *color = 0x00000000;
        return true;
    }
    return false;
}

#endif

#ifndef GL11
#define SCREEN_FB_SIZE (2 * 1024 * 1024) // Must be 256KB aligned
static SceUID displayblock;
static void *base; // pointer to the buffer the game draws into
static void *display_base[2] = {NULL, NULL};
static SceUID display_block[2] = {-1, -1};
static int display_idx = 0;
static bool display_ok = false;
static void *ram_base = NULL;
static int mutex;

#define CURSOR_SIZE 13 // covers hotspot -6..+6
static bool cursor_visible = false;
static bool cursor_saved = false; // true while the cursor is drawn in the framebuffer
static int cursor_fx = 0, cursor_fy = 0;
static int cursor_want_x = 0, cursor_want_y = 0;
static uint32_t cursor_under[CURSOR_SIZE * CURSOR_SIZE];

static void cursor_restore(void) {
    if (!cursor_saved || !base) {
        return;
    }
    uint32_t *fb = (uint32_t *)base;
    for (int j = 0; j < CURSOR_SIZE; j++) {
        for (int i = 0; i < CURSOR_SIZE; i++) {
            int px = cursor_fx - 6 + i;
            int py = cursor_fy - 6 + j;
            if (px < 0 || py < 0 || px >= SCREEN_FB_WIDTH || py >= SCREEN_FB_HEIGHT) {
                continue;
            }
            fb[py * SCREEN_FB_WIDTH + px] = cursor_under[j * CURSOR_SIZE + i];
        }
    }
    cursor_saved = false;
}

static void cursor_put(uint32_t *fb, int px, int py, uint32_t color) {
    if (px < 0 || py < 0 || px >= SCREEN_FB_WIDTH || py >= SCREEN_FB_HEIGHT) {
        return;
    }
    fb[py * SCREEN_FB_WIDTH + px] = color;
}

static uint32_t cursor_swap_rb(uint32_t p) {
    return ((p >> 16) & 0xff) | (p & 0xff00) | ((p & 0xff) << 16);
}

// dx, dy are relative to the hotspot; returns true if the crosshair or its shadow covers that pixel
static bool cursor_pixel(int dx, int dy, uint32_t *color) {
    if ((dy == 0 && dx >= -5 && dx <= 5) || (dx == 0 && dy >= -5 && dy <= 5)) {
        *color = 0x00FFFFFF;
        return true;
    }
    if ((dy == 1 && dx >= -4 && dx <= 6) || (dx == 1 && dy >= -4 && dy <= 6)) {
        *color = 0x00000000;
        return true;
    }
    return false;
}

static void cursor_draw(int fx, int fy) {
    if (!base) {
        return;
    }
    uint32_t *fb = (uint32_t *)base;
    cursor_fx = fx;
    cursor_fy = fy;

    for (int j = 0; j < CURSOR_SIZE; j++) {
        for (int i = 0; i < CURSOR_SIZE; i++) {
            int px = fx - 6 + i;
            int py = fy - 6 + j;
            if (px < 0 || py < 0 || px >= SCREEN_FB_WIDTH || py >= SCREEN_FB_HEIGHT) {
                cursor_under[j * CURSOR_SIZE + i] = 0;
                continue;
            }
            cursor_under[j * CURSOR_SIZE + i] = fb[py * SCREEN_FB_WIDTH + px];
        }
    }
    cursor_saved = true;

    for (int d = -5; d <= 5; d++) { // black shadow
        cursor_put(fb, fx + 1 + d, fy + 1, 0x00000000);
        cursor_put(fb, fx + 1, fy + 1 + d, 0x00000000);
    }
    for (int d = -5; d <= 5; d++) { // white crosshair
        cursor_put(fb, fx + d, fy, 0x00FFFFFF);
        cursor_put(fb, fx, fy + d, 0x00FFFFFF);
    }
}
#endif

static uint64_t systemtime_start = 0;

#define VITA_YOFF ((SCREEN_FB_HEIGHT - SCREEN_HEIGHT) / 2)

#ifndef VITA_ASPECT_FIT
#ifdef VITA_NATIVE_WIDESCREEN
#define VITA_ASPECT_FIT 0
#else
#define VITA_ASPECT_FIT 1
#endif
#endif
#if VITA_ASPECT_FIT && !defined(GL11)
#include "vita_aspect.h"
static VitaAspectViewport aspect_viewport;
static int aspect_columns[SCREEN_FB_WIDTH];
static int aspect_rows[SCREEN_FB_HEIGHT];
static void aspect_init(void) {
    aspect_viewport = vita_aspect_viewport(SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    for (int x = 0; x < SCREEN_FB_WIDTH; x++)
        aspect_columns[x] = vita_aspect_axis(x, aspect_viewport.x, aspect_viewport.width, SCREEN_WIDTH, aspect_viewport.scale);
    for (int y = 0; y < SCREEN_FB_HEIGHT; y++)
        aspect_rows[y] = vita_aspect_axis(y, aspect_viewport.y, aspect_viewport.height, SCREEN_HEIGHT, aspect_viewport.scale);
}
#endif

static SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM], touch_old[SCE_TOUCH_PORT_MAX_NUM];
static SceCtrlData ctrl, ctrl_old;
#include "../r60_latest_analog.h"

#define AUDIO_PORT_SAMPLES 1024
static int g_AudioPort = -1;
#define AUDIO_THREAD_STACK (16 * 1024)
static SceUID g_AudioThread = -1;

#define WAVE_FREQ 22050
static SceUID g_WaveMutex = -1;
static bool g_WaveActive = false;
static _Atomic unsigned r108_wave_attempts,r108_wave_started,r108_wave_busy,r108_wave_failed,r108_wave_completed,r108_wave_nonzero;
void platform_wave_status(unsigned out[6]){out[0]=r108_wave_attempts;out[1]=r108_wave_started;out[2]=r108_wave_busy;out[3]=r108_wave_failed;out[4]=r108_wave_completed;out[5]=r108_wave_nonzero;}

static int16_t *g_WaveSamples = NULL;
static int g_WaveSampleCount = 0;
static int g_WaveSamplePos = 0;
static int g_WaveVolume = 128;

#define MIDI_FREQ 22050
static SceUID g_MidiMutex = -1;
static bool g_MidiActive = false;
static tml_message *TinyMidiLoader = NULL;
static char midi_current_track[64];
static bool g_MidiLoop;
static _Atomic unsigned midi_loop_count;
static unsigned midi_track_duration_ms;
void platform_midi_loop_status(unsigned out[3]){out[0]=midi_loop_count;out[1]=g_MidiLoop;out[2]=midi_track_duration_ms;}

const char*platform_midi_current_track(void){return midi_current_track;}

static tsf *g_TinySoundFont = NULL;
static double g_Msec = 0.0;
static tml_message *g_MidiMessage = NULL;
static float g_MidiVolume = 1.0f;

static int16_t *decode_wave_to_s16_mono(const int8_t *src, int length, int *out_sample_count) {
    if (!src || length <= 44 || !out_sample_count) {
        return NULL;
    }

    const uint8_t *in_data = (const uint8_t *)src + 44;
    int sample_count = length - 44;
    int16_t *decoded = malloc((size_t)sample_count * sizeof(int16_t));
    if (!decoded) {
        return NULL;
    }

    for (int i = 0; i < sample_count; i++) {
        decoded[i] = (int16_t)(((int)in_data[i] - 128) << 8);
    }

    *out_sample_count = sample_count;
    return decoded;
}

static void wave_audio_init(void) {
    g_WaveMutex = sceKernelCreateMutex("wave_mutex", 0, 1, NULL);
    if (g_WaveMutex < 0) {
        return;
    }

    g_WaveActive = false;
    g_WaveSamplePos = 0;
}

static void wave_audio_shutdown(void) {
    if (g_WaveMutex >= 0) {
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
    }
    g_WaveActive = false;
    if (g_WaveSamples) {
        free(g_WaveSamples);
        g_WaveSamples = NULL;
    }
    g_WaveSampleCount = 0;
    g_WaveSamplePos = 0;
    if (g_WaveMutex >= 0) {
        sceKernelUnlockMutex(g_WaveMutex, 1);
    }
    if (g_WaveMutex >= 0) {
        sceKernelDeleteMutex(g_WaveMutex);
        g_WaveMutex = -1;
    }
}

static void midi_reset_state_locked(void) {
    g_MidiMessage = NULL;
    g_Msec = 0.0;
    if (g_TinySoundFont) {
        tsf_reset(g_TinySoundFont);
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
}

static int16_t clamp_s16(int sample) {
    if (sample < -32768) {
        return -32768;
    }
    if (sample > 32767) {
        return 32767;
    }
    return (int16_t)sample;
}

static void midi_rewind_song_locked(void) {
    /* tsf_reset frees its channel array. Preserve that array at repeat
     * boundaries and reset only already-created channels in place. */
    if(g_TinySoundFont&&g_TinySoundFont->channels){
        int count=g_TinySoundFont->channels->channelNum;
        for(int channel=0;channel<count;channel++){
            tsf_channel_midi_control(g_TinySoundFont,channel,120,0);
            tsf_channel_midi_control(g_TinySoundFont,channel,121,0);
            tsf_channel_set_pitchwheel(g_TinySoundFont,channel,8192);
            tsf_channel_set_presetnumber(g_TinySoundFont,channel,0,channel==9);
        }
    }
    g_Msec=0.0;g_MidiMessage=TinyMidiLoader;midi_loop_count++;
}

static void midi_advance_locked(double elapsed_ms) {
    for(g_Msec+=elapsed_ms;g_MidiMessage&&g_Msec>=g_MidiMessage->time;g_MidiMessage=g_MidiMessage->next){
                    switch (g_MidiMessage->type) {
                    case TML_PROGRAM_CHANGE:
                        tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
                        break;
                    case TML_NOTE_ON:
                        tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
                        break;
                    case TML_NOTE_OFF:
                        tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
                        break;
                    case TML_PITCH_BEND:
                        tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
                        break;
                    case TML_CONTROL_CHANGE:
                        tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
                        break;
                    }
    }
    /* Reuse the decoded song and SoundFont. One-shot jingles have no repeat
     * flag; an explicit stop clears it before this audio-thread callback. */
    if(g_MidiLoop&&TinyMidiLoader&&!g_MidiMessage){
        midi_rewind_song_locked();
    }
}

static int audio_thread_main(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    int16_t mix[AUDIO_PORT_SAMPLES * 2];
    while (g_MidiActive) {
        memset(mix, 0, sizeof(mix));

        bool midi_locked = false;
        if (g_MidiMutex >= 0) {
            int midi_lock_result = sceKernelTryLockMutex(g_MidiMutex, 1);
            midi_locked = midi_lock_result >= 0;
        }
        if (midi_locked && g_TinySoundFont) {
            int16_t *stream = mix;
            int SampleBlock, SampleCount = AUDIO_PORT_SAMPLES;
            for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount;
                 SampleCount -= SampleBlock, stream += (SampleBlock * 2)) {
                if (SampleBlock > SampleCount) {
                    SampleBlock = SampleCount;
                }

                midi_advance_locked(SampleBlock * (1000.0 / MIDI_FREQ));

                tsf_render_short(g_TinySoundFont, stream, SampleBlock, 0);
            }
        }
        if (midi_locked) {
            sceKernelUnlockMutex(g_MidiMutex, 1);
        }

        bool wave_locked = false;
        if (g_WaveMutex >= 0) {
            int wave_lock_result = sceKernelTryLockMutex(g_WaveMutex, 1);
            wave_locked = wave_lock_result >= 0;
        }
        if (wave_locked && g_WaveSamples && g_WaveSamplePos < g_WaveSampleCount) {
            int wavevol = g_WaveVolume;
            if (wavevol < 0) {
                wavevol = 0;
            } else if (wavevol > 128) {
                wavevol = 128;
            }

            int remaining = g_WaveSampleCount - g_WaveSamplePos;
            int count = remaining > AUDIO_PORT_SAMPLES ? AUDIO_PORT_SAMPLES : remaining;
            for (int i = 0; i < count; i++) {
                int wave = ((int)g_WaveSamples[g_WaveSamplePos + i] * wavevol) / 128;
                if(wave)r108_wave_nonzero++;
                int left = (int)mix[i * 2] + wave;
                int right = (int)mix[i * 2 + 1] + wave;
                mix[i * 2] = clamp_s16(left);
                mix[i * 2 + 1] = clamp_s16(right);
            }

            g_WaveSamplePos += count;
            if (g_WaveSamplePos >= g_WaveSampleCount) {
                r108_wave_completed++;
                free(g_WaveSamples);
                g_WaveSamples = NULL;
                g_WaveSampleCount = 0;
                g_WaveSamplePos = 0;
                g_WaveActive = false;
            }
        }
        if (wave_locked) {
            sceKernelUnlockMutex(g_WaveMutex, 1);
        }

        if (g_AudioPort >= 0) {
            sceAudioOutOutput(g_AudioPort, mix);
        } else {
            sceKernelDelayThread(4000);
        }
    }

    if (g_AudioPort >= 0) {
        sceAudioOutReleasePort(g_AudioPort);
        g_AudioPort = -1;
    }

    sceKernelExitDeleteThread(0);
    return 0;
}

static void midi_audio_init(void) {
    g_MidiMutex = sceKernelCreateMutex("midi_mutex", 0, 1, NULL);
    if (g_MidiMutex < 0) {
        return;
    }

    if (!_Client.lowmem) {
        g_TinySoundFont = tsf_load_filename("SCC1_Florestan.sf2");
        if (!g_TinySoundFont) {
            g_TinySoundFont = tsf_load_filename("rom/SCC1_Florestan.sf2");
        }

        if (!g_TinySoundFont) {
            rs2_error("Could not load SoundFont\n");
            return;
        } else {
            /* Reserve voice storage before playback. Avoid audio-thread heap
             * growth and keep polyphony bounded during repeated songs. */
            if (!tsf_set_max_voices(g_TinySoundFont,128)) {
                rs2_error("Could not reserve MIDI voices\n");
                tsf_close(g_TinySoundFont);g_TinySoundFont=NULL;return;
            }
            tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, MIDI_FREQ, 0.0f);
            tsf_set_volume(g_TinySoundFont, g_MidiVolume);
            tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        }
    }

    g_AudioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_PORT_SAMPLES, MIDI_FREQ, SCE_AUDIO_OUT_MODE_STEREO);
    if (g_AudioPort >= 0) {
        int vols[2] = {SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL};
        sceAudioOutSetVolume(g_AudioPort, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols);
    } else {
        rs2_error("Could not open the audio hardware\n");
        return;
    }

    g_MidiActive = true;
    g_AudioThread = sceKernelCreateThread("audio", audio_thread_main, 0x10000100, AUDIO_THREAD_STACK, 0, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, NULL);
    int thread_start_result = g_AudioThread >= 0 ? sceKernelStartThread(g_AudioThread, 0, NULL) : -1;
    if (g_AudioThread < 0 || thread_start_result < 0) {
        g_MidiActive = false;
        if (g_AudioThread >= 0) {
            sceKernelDeleteThread(g_AudioThread);
            g_AudioThread = -1;
        }
        if (g_AudioPort >= 0) {
            sceAudioOutReleasePort(g_AudioPort);
            g_AudioPort = -1;
        }
        if (g_TinySoundFont) {
            tsf_close(g_TinySoundFont);
            g_TinySoundFont = NULL;
        }
        sceKernelDeleteMutex(g_MidiMutex);
        g_MidiMutex = -1;
    }
}

static void midi_audio_shutdown(void) {
    if (g_MidiMutex >= 0) {
        sceKernelLockMutex(g_MidiMutex, 1, NULL);
    }
    g_MidiActive = false;
    if (TinyMidiLoader) {
        tml_free(TinyMidiLoader);
        TinyMidiLoader = NULL;
    }
    g_MidiMessage = NULL;
    g_Msec = 0.0;
    if (g_MidiMutex >= 0) {
        sceKernelUnlockMutex(g_MidiMutex, 1);
    }

    if (g_AudioThread >= 0) {
        sceKernelWaitThreadEnd(g_AudioThread, NULL, NULL);
        g_AudioThread = -1;
    }
    if (g_TinySoundFont) {
        tsf_close(g_TinySoundFont);
        g_TinySoundFont = NULL;
    }
    if (g_AudioPort >= 0) {
        sceAudioOutReleasePort(g_AudioPort);
        g_AudioPort = -1;
    }
    if (g_MidiMutex >= 0) {
        sceKernelDeleteMutex(g_MidiMutex);
        g_MidiMutex = -1;
    }
}

static void midi_set_loader(tml_message *loader, bool loop) {
    if (!loader || g_MidiMutex < 0) {
        if (loader) {
            tml_free(loader);
        }
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    if (TinyMidiLoader) {
        tml_free(TinyMidiLoader);
    }
    TinyMidiLoader = loader;
    g_MidiLoop = loop;
    midi_track_duration_ms=0;for(tml_message*q=loader;q;q=q->next)midi_track_duration_ms=q->time;
    g_MidiMessage = loader;
    g_Msec = 0.0;
    if (g_TinySoundFont) {
        tsf_reset(g_TinySoundFont);
        tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

static void audio_init(void) {
    wave_audio_init();
    midi_audio_init();
}

static void audio_shutdown(void) {
    midi_audio_shutdown();
    wave_audio_shutdown();
}

bool platform_init(void) {
    return true;
}

void platform_new(GameShell *shell) {
    (void)shell;

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    systemtime_start = sceKernelGetSystemTimeWide();
#if VITA_ASPECT_FIT && !defined(GL11)
    aspect_init();
#endif

#ifdef GL11
    /* HW-R2 controlled CPU-write buffer placement experiment. */
    int hw_ring_mib=h6_ring_mib();
    vglSetCircularPoolSize(hw_ring_mib * 1024 * 1024);
    if(!h5_no_init()){h5_mark("graphics_init_begin");if(h5_pool_experiment()==2)vglUseCachedMem(GL_TRUE);
    SceGxmMultisampleMode r6_msaa=h6_msaa()==0?SCE_GXM_MULTISAMPLE_NONE:SCE_GXM_MULTISAMPLE_4X;
    if(h5_pool_experiment()==1)vglInitWithCustomThreshold(h6_immediate_mib()*1024*1024,960,544,0x1000000,0,0x7fffffff,0x7fffffff,r6_msaa);
    else vglInitExtended(h6_immediate_mib()*1024*1024,960,544,0x1000000,r6_msaa);
    h5_mark("graphics_init_end");}
    if(!h5_no_init()){
        vglWaitVblankStart(GL_TRUE);
        /* The first scene reset joins/stops the initialization splash renderer.
           It belongs to initialization, never the measured idle interval. */
        if(h5_silent()){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);vglSwapBuffers(GL_FALSE);h5_mark("splash_handoff_complete");}
    }
#else
    mutex = sceKernelCreateMutex("fb_mutex", 0, 0, NULL);
    displayblock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
    if (displayblock < 0)
        return;
    sceKernelGetMemBlockBase(displayblock, (void **)&display_base[0]);
    display_block[0] = displayblock;
    display_block[1] = sceKernelAllocMemBlock("display2", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
    if (display_block[1] >= 0) {
        sceKernelGetMemBlockBase(display_block[1], (void **)&display_base[1]);
        ram_base = memalign(256, SCREEN_FB_SIZE);
    }
    if (display_block[1] >= 0 && display_base[1] && ram_base) {
        memset(ram_base, 0, SCREEN_FB_SIZE);
        memset(display_base[0], 0, SCREEN_FB_SIZE);
        memset(display_base[1], 0, SCREEN_FB_SIZE);
        base = ram_base;
        display_ok = true;
    } else {
        base = display_base[0]; // fall back to drawing straight into the screen
    }
    SceDisplayFrameBuf frame = {sizeof(frame), display_base[0], (SCREEN_FB_WIDTH), 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT};
    sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
#endif

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    // sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    // sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    audio_init();
}

void platform_free(void) {
    r10_upload_flush();
    r10_shutdown();
    h8_shutdown();
    audio_shutdown();
#ifndef GL11
    sceKernelDeleteMutex(mutex);
    sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_SETBUF_IMMEDIATE);
    if (display_block[1] >= 0) {
        sceKernelFreeMemBlock(display_block[1]);
    }
    sceKernelFreeMemBlock(displayblock);
    if (ram_base) {
        free(ram_base);
        ram_base = NULL;
    }
#endif
}

void platform_set_wave_volume(int wavevol) {
    if (g_WaveMutex >= 0) {
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
    }
    g_WaveVolume = wavevol;
    if (g_WaveMutex >= 0) {
        sceKernelUnlockMutex(g_WaveMutex, 1);
    }
}

void platform_play_wave(int8_t *src, int length) {
    r108_wave_attempts++;
    if (!src || length <= 0 || g_WaveMutex < 0) {
        return;
    }

    bool can_queue = false;
    sceKernelLockMutex(g_WaveMutex, 1, NULL);
    if (!g_WaveActive) {
        g_WaveActive = true;
        can_queue = true;
    }
    sceKernelUnlockMutex(g_WaveMutex, 1);
    if (!can_queue) {
        r108_wave_busy++;return;
    }

    int sample_count = 0;
    int16_t *decoded = decode_wave_to_s16_mono(src, length, &sample_count);
    if (!decoded || sample_count <= 0) {
        r108_wave_failed++;
        if (decoded) {
            free(decoded);
        }
        sceKernelLockMutex(g_WaveMutex, 1, NULL);
        g_WaveActive = false;
        sceKernelUnlockMutex(g_WaveMutex, 1);
        return;
    }

    sceKernelLockMutex(g_WaveMutex, 1, NULL);
    if (!g_WaveSamples) {
        g_WaveSamples = decoded;
        r108_wave_started++;
        g_WaveSampleCount = sample_count;
        g_WaveSamplePos = 0;
        sceKernelUnlockMutex(g_WaveMutex, 1);
    } else {
        g_WaveActive = false;
        sceKernelUnlockMutex(g_WaveMutex, 1);
        free(decoded);
    }
}

void platform_set_midi_volume(float midivol) {
    if (_Client.lowmem || g_MidiMutex < 0) {
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    g_MidiVolume = midivol;
    if (g_TinySoundFont) {
        tsf_set_volume(g_TinySoundFont, g_MidiVolume);
    }
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

void platform_set_jingle(int8_t *src, int len) {
    tml_message *loader = tml_load_memory(src, len);
    free(src);

    if (!loader) {
        return;
    }
    midi_set_loader(loader,false);
}

void platform_set_midi(const char *name, int crc, int len) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "rom/cache/client/songs/%s.mid", name);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        rs2_error("Error loading midi file %s: %s (NOTE: authentic if empty when relogging?)\n", filename, strerror(errno));
        return;
    }

    int8_t *data = malloc(len);
    const size_t data_len = fread(data, 1, len, file);
    fclose(file);
    if (data && crc != 12345678) {
        int data_crc = rs_crc32(data, len);
        if (data_crc != crc) {
            rs2_log("%s midi CRC check failed\n", name);
            free(data);
            data = NULL;
        }
    }

    Packet *packet = packet_new(data, 4);
    const int uncompressed_length = g4(packet);
    int8_t *uncompressed = malloc(uncompressed_length);
    bzip_decompress(uncompressed, data, (int)data_len - 4, 4);

    tml_message *loader = tml_load_memory(uncompressed, uncompressed_length);
    packet_free(packet);
    free(uncompressed);

    if (!loader) {
        return;
    }
    midi_set_loader(loader,true);
    if(g_MidiMutex>=0)snprintf(midi_current_track,sizeof(midi_current_track),"%s",name);
}

void platform_stop_midi(void) {
    if (_Client.lowmem || g_MidiMutex < 0) {
        return;
    }

    sceKernelLockMutex(g_MidiMutex, 1, NULL);
    midi_reset_state_locked();
    midi_current_track[0]='\0';
    g_MidiLoop=false;
    midi_track_duration_ms=0;
    sceKernelUnlockMutex(g_MidiMutex, 1);
}

// maps a raw stick byte (0-255, centered ~128) to -1.0..1.0 with a dead zone and a curve
static float vita_stick_axis(int raw) {
    int v = raw - 128;
    int a = v < 0 ? -v : v;
    if (a < 25) {
        return 0.0f;
    }
    float n = (float)(a - 25) / (127.0f - 25.0f);
    if (n > 1.0f) {
        n = 1.0f;
    }
    n = n * n; // finer control near the center
    return v < 0 ? -n : n;
}

static void vita_cam_key(Client *c, bool *state, bool want, int key) {
    if (want == *state) {
        return;
    }
    *state = want;
    if (want) {
        key_pressed(c->shell, key, -1);
    } else {
        key_released(c->shell, key, -1);
    }
}

// ---- on-screen keyboard (software renderer) ----
#define VKB_COLS 10
#define VKB_ROWS 5
#define VKB_KW 48
#define VKB_KH 36
#include "../r63_keyboard_layout.h"
#include "../r63_search_icon.h"
#define VKB_LABELS 0 // set to 1 to draw key labels with the game font
#define VKB_TEXT_Y 23 // baseline offset inside a key, tune if labels look off

enum { VK_NONE = 0, VK_CHAR, VK_BKSP, VK_SHIFT, VK_SYM, VK_SPACE, VK_ENTER, VK_DONE };

// padding so vita-elf-create has room for its tables (layout workaround)
static const unsigned char vita_layout_pad[16384] __attribute__((used)) = {1};
static bool vkb_open = false;
int platform_keyboard_is_open(void){return vkb_open;}
static bool vkb_amount_active = false;
static bool vkb_large = false;
static bool vkb_login = false;  // true: username/password boxes, false: in-game chat
static bool vkb_ingame = false;
static bool vkb_shift = false;
static bool vkb_sym = false;
static int vkb_focus = -1;
static int vkb_x = 0, vkb_y = 0;

static const char *vkb_alpha[4] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm,."};
static const char *vkb_symbols[4] = {"!@#$%^&*()", "-_=+[]{}\\|", ";:'\",./<>", "?~`"};

static int vkb_key(int row, int col, char *ch) {
    *ch = 0;
    if (row == 4) {
        if (col == 0) return VK_SYM;
        if (col <= 5) return VK_SPACE;
        if (col <= 7) return VK_ENTER;
        return VK_DONE;
    }
    if (row == 2 && col == 9) return VK_BKSP;
    if (row == 3 && col == 0) return vkb_sym ? VK_NONE : VK_SHIFT;
    const char *s = vkb_sym ? vkb_symbols[row] : vkb_alpha[row];
    int idx = (row == 3) ? col - 1 : col;
    if (idx < 0 || idx >= (int)strlen(s)) return VK_NONE;
    char c = s[idx];
    if (!vkb_sym && vkb_shift && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    *ch = c;
    return VK_CHAR;
}

static R63KeyboardBox vkb_box(void){
    return r63_keyboard_box(SCREEN_WIDTH,SCREEN_HEIGHT,vkb_ingame,vkb_large);
}
static void vkb_layout(void) {
#ifdef GL11
    R63KeyboardBox b=vkb_box();vkb_x=b.x+4;vkb_y=b.y+4;
#else
    vkb_x=vkb_ingame?24:(SCREEN_WIDTH-VKB_COLS*VKB_KW)/2;
    vkb_y=vkb_ingame?14:SCREEN_HEIGHT-VKB_ROWS*VKB_KH-8;
#endif
}

static void vkb_show(bool login, bool ingame) {
    vkb_open = true;
    vkb_login = login;
    vkb_ingame = ingame;
    vkb_shift = false;
    vkb_sym = false;
    vkb_layout();
}

static bool vkb_hit(int mx, int my, int *row, int *col) {
#ifdef GL11
    return r63_keyboard_key(vkb_box(),mx,my,row,col)!=0;
#else
    int rx=mx-vkb_x,ry=my-vkb_y;
    if(rx<0||ry<0||rx>=480||ry>=180)return false;
    *row=ry/36;*col=rx/48;return true;
#endif
}

static void vkb_send_char(Client *c, int ch) {
    key_pressed(c->shell, -1, ch);
    key_released(c->shell, -1, ch);
}

static void vkb_send_code(Client *c, int code) {
    key_pressed(c->shell, code, -1);
    key_released(c->shell, code, -1);
}

static void vkb_clear(Client *c) {
    for (int i = 0; i < 24; i++) {
        vkb_send_code(c, 8);
    }
}

#ifndef GL11
// fill a rectangle (game coordinates) directly in the framebuffer, no allocation
static void vkb_fill(int x, int y, int w, int h, int rgb) {
    if (!base) return;
    uint32_t *fb = (uint32_t *)base;
    uint32_t col = cursor_swap_rb((uint32_t)rgb);
    int px = x + SCREEN_CENTER_XOFF;
    int py = y + VITA_YOFF;
    int x0 = px < 0 ? 0 : px;
    int y0 = py < 0 ? 0 : py;
    int x1 = px + w > SCREEN_FB_WIDTH ? SCREEN_FB_WIDTH : px + w;
    int y1 = py + h > SCREEN_FB_HEIGHT ? SCREEN_FB_HEIGHT : py + h;
    sceKernelLockMutex(mutex, 1, NULL);
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            fb[yy * SCREEN_FB_WIDTH + xx] = col;
        }
    }
    sceKernelUnlockMutex(mutex, 1);
}
#endif

static void vkb_draw(void) {
#ifndef GL11
    if (!vkb_open) return;

    vkb_fill(vkb_x - 4, vkb_y - 4, VKB_COLS * VKB_KW + 8, VKB_ROWS * VKB_KH + 8, 0x000000);

    for (int row = 0; row < VKB_ROWS; row++) {
        for (int col = 0; col < VKB_COLS; col++) {
            char ch = 0;
            int kind = vkb_key(row, col, &ch);
            if (kind == VK_NONE) continue;
            int span = 1;
            if (kind == VK_SPACE) { if (col != 1) continue; span = 5; }
            else if (kind == VK_ENTER) { if (col != 6) continue; span = 2; }
            else if (kind == VK_DONE) { if (col != 8) continue; span = 2; }

            int kx = vkb_x + col * VKB_KW + 1;
            int ky = vkb_y + row * VKB_KH + 1;
            int kw = span * VKB_KW - 2;
            int kh = VKB_KH - 2;

            bool lit = (kind == VK_SHIFT && vkb_shift) || (kind == VK_SYM && vkb_sym);
            vkb_fill(kx, ky, kw, kh, 0xB0B0B0);
            vkb_fill(kx + 1, ky + 1, kw - 2, kh - 2, lit ? 0x806000 : 0x303030);

            char buf[2] = {ch, 0};
            const char *label = buf;
            if (kind == VK_BKSP) label = "Del";
            else if (kind == VK_SHIFT) label = "Shift";
            else if (kind == VK_SYM) label = vkb_sym ? "abc" : "#+=";
            else if (kind == VK_SPACE) label = "Space";
            else if (kind == VK_ENTER) label = "Enter";
            else if (kind == VK_DONE) label = "Done";

#if VKB_LABELS
            int tw = platform_string_width(label);
            platform_set_color(0xFFFFFF);
            platform_draw_string(label, kx + (kw - tw) / 2, ky + VKB_TEXT_Y);
#else
            (void)label;
#endif
        }
    }
#endif
}

static void vkb_update(Client *c, bool press) {
    int mx = c->shell->mouse_x;
    int my = c->shell->mouse_y;
    bool on_login = !c->ingame && c->title_screen_state == 2;
    /* Amount dialogs arrive from a server packet, without a touch on the
     * chat input. Open once per dialog; Done may still dismiss the keyboard
     * and touching the input can reopen it. Submission closes it cleanly. */
    bool amount = c->ingame && c->chatback_input_open;
    if (amount && !vkb_amount_active) vkb_show(false, true);
    if (!amount && vkb_amount_active && !vkb_login) vkb_open = false;
    vkb_amount_active = amount;

    if (vkb_open) {
        if ((vkb_login && !on_login) || (!vkb_login && (!c->ingame || (ve_enabled() && ve_chat_hidden(c))))) {
            vkb_open = false;
        } else if (vkb_login && c->title_login_field != vkb_focus) {
            // focus moved (Enter/Tab), clear the new field so typing replaces it
            vkb_focus = c->title_login_field;
            vkb_clear(c);
        }
    }

    if (!press) return;

#ifdef GL11
    if(vkb_open&&r63_keyboard_tab(vkb_box(),mx,my)){
        update_touch=false;c->shell->mouse_button=0;
        vkb_large=!vkb_large;vkb_layout();return;
    }
#endif
    int row = 0, col = 0;
    if (vkb_open && vkb_hit(mx, my, &row, &col)) {
        // the press belongs to the keyboard, hide it from the game
        update_touch = false;
        c->shell->mouse_button = 0;

        char ch = 0;
        switch (vkb_key(row, col, &ch)) {
        case VK_CHAR: vkb_send_char(c, ch); vkb_shift = false; break;
        case VK_SPACE: vkb_send_char(c, ' '); break;
        case VK_BKSP: vkb_send_code(c, 8); break;
        case VK_ENTER: vkb_send_code(c, 10); break;
        case VK_SHIFT: vkb_shift = !vkb_shift; break;
        case VK_SYM: vkb_sym = !vkb_sym; break;
        case VK_DONE: vkb_open = false; break;
        default: break;
        }
        return;
    }

#ifdef VITA_NATIVE_WIDESCREEN
    int login_field=c->title_restored?approved_title_field_at(mx,my):title_login_field_at(mx,my);
    bool in_user = !c->ingame && c->title_screen_state == 2 && login_field == 0;
    bool in_pass = !c->ingame && c->title_screen_state == 2 && login_field == 1;
#else
    bool in_user = !c->ingame && c->title_screen_state == 2 && mx >= 301 && mx <= 562 && my >= 262 && my < 279;
    bool in_pass = !c->ingame && c->title_screen_state == 2 && mx >= 301 && mx <= 562 && my >= 279 && my <= 296;
#endif

    if (in_user || in_pass) {
        int field = in_pass ? 1 : 0;
        c->title_login_field = field;
        if (!vkb_open || !vkb_login || vkb_focus != field) {
            vkb_show(true, false);
            vkb_focus = field;
            vkb_clear(c);
        }
    } else if (c->ingame && insideMobileInputArea(c)) {
        if (!vkb_open) {
            vkb_show(false, true);
        }
    }
}

#include "vita_font.inc"

#define VKB_GW 10
#define VKB_GH 12
#define VKB_TW (VKB_COLS * VKB_KW + 8)
#define VKB_TH (VKB_ROWS * VKB_KH + 8)

static uint32_t vkb_saved[VKB_TW * VKB_TH];
static bool vkb_composited = false;

static const char *vkb_label_str(int kind, char ch, char *buf) {
    buf[0] = ch;
    buf[1] = 0;
    switch (kind) {
    case VK_BKSP: return "Del";
    case VK_SHIFT: return "Shft";
    case VK_SYM: return vkb_sym ? "abc" : "#+=";
    case VK_SPACE: return "Space";
    case VK_ENTER: return "Enter";
    case VK_DONE: return "Done";
    default: return buf;
    }
}

// color (0xRRGGBB) of the keyboard at game coordinates, false if outside it
static bool vkb_pixel(int gx, int gy, uint32_t *out) {
    int px = gx - vkb_x, py = gy - vkb_y;
    if (px < -4 || py < -4 || px >= VKB_COLS * VKB_KW + 4 || py >= VKB_ROWS * VKB_KH + 4) return false;
    // Brown rim with a dark inset, following the original options-menu frame.
    *out = OPTIONS_MENU;
    if (px == -4 || py == -4 || px == VKB_COLS * VKB_KW + 3 || py == VKB_ROWS * VKB_KH + 3) *out = 0x302B23;
    if (px < 0 || py < 0 || px >= VKB_COLS * VKB_KW || py >= VKB_ROWS * VKB_KH) return true;
    int row = py / VKB_KH, col = px / VKB_KW;
    char ch = 0;
    int kind = vkb_key(row, col, &ch);
    if (kind == VK_NONE) return true;
    int start = col, span = 1;
    if (kind == VK_SPACE) { start = 1; span = 5; }
    else if (kind == VK_ENTER) { start = 6; span = 2; }
    else if (kind == VK_DONE) { start = 8; span = 2; }
    int kx = start * VKB_KW + 1, ky = row * VKB_KH + 1;
    int kw = span * VKB_KW - 2, kh = VKB_KH - 2;
    int lx = px - kx, ly = py - ky;
    if (lx < 0 || ly < 0 || lx >= kw || ly >= kh) { *out = 0x4B4338; return true; }
    if (lx == 0 || ly == 0 || lx == kw - 1 || ly == kh - 1) { *out = 0x302B23; return true; }
    bool lit = (kind == VK_SHIFT && vkb_shift) || (kind == VK_SYM && vkb_sym);
    // Classic menu palette; fixed subtle grain is evaluated only on cache refresh.
    unsigned int grain = ((unsigned int)px * 1973u) ^ ((unsigned int)py * 9277u);
    grain ^= grain >> 7;
    int shade = (int)(grain & 3u) - 1;
    int backing = lit ? 0x302B23 : OPTIONS_MENU;
    *out = (uint32_t)(backing + shade * 0x010101);
    // One-pixel muted inner light; keep the low-resolution edge deliberately simple.
    if (!lit && (lx == 1 || ly == 1)) *out += 0x060606;
    char buf[2];
    const char *label = vkb_label_str(kind, ch, buf);
    int n = (int)strlen(label);
    int tx = (kw - n * VKB_GW) / 2, ty = (kh - VKB_GH) / 2;
    int gxl = lx - tx, gyl = ly - ty;
    if (gyl >= 0 && gyl < VKB_GH && gxl >= 0 && gxl < n * VKB_GW) {
        int c = (unsigned char)label[gxl / VKB_GW];
        if (c >= 32 && c < 127 && ((vkb_font[c - 32][gyl] >> (VKB_GW - 1 - gxl % VKB_GW)) & 1)) *out = lit ? 0xFFFF00 : 0xFFFFFF;
    }
    return true;
}

static uint32_t vkb_cache[VKB_TW * VKB_TH];
static int vkb_cache_sig = -1;

static void vkb_ensure_cache(void) {
    int sig = (vkb_shift ? 1 : 0) | (vkb_sym ? 2 : 0) | (vkb_ingame ? 4 : 0) | (vkb_x << 4) | (vkb_y << 16);
    if (sig == vkb_cache_sig) {
        return;
    }
    vkb_cache_sig = sig;
    for (int j = 0; j < VKB_TH; j++) {
        for (int i = 0; i < VKB_TW; i++) {
            uint32_t col = 0;
            vkb_pixel(vkb_x - 4 + i, vkb_y - 4 + j, &col);
            vkb_cache[j * VKB_TW + i] = col;
        }
    }
}

static bool vkb_lookup(int gx, int gy, uint32_t *out) {
    int i = gx - (vkb_x - 4), j = gy - (vkb_y - 4);
    if (i < 0 || j < 0 || i >= VKB_TW || j >= VKB_TH) {
        return false;
    }
    *out = vkb_cache[j * VKB_TW + i];
    return true;
}

void platform_poll_events(Client *c) {
    static bool right_click = false;

    static SceTouchPanelInfo info;
    // Panel dimensions are fixed; avoid a kernel query on every frame.
    if (info.maxAaX == 0 || info.maxAaY == 0) {
        if (sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &info) < 0 ||
            info.maxAaX == 0 || info.maxAaY == 0) {
            return;
        }
    }

    memcpy(touch_old, touch, sizeof(touch_old));

    for (int port = 0; port < 1 /* SCE_TOUCH_PORT_MAX_NUM */; port++) {
        sceTouchPeek(port, &touch[port], 1);
        for (int i = 0; i < 1 /* SCE_TOUCH_MAX_REPORT */; i++) {
            static bool touch_down = false;
            if (touch[port].reportNum > 0) {
                int screen_x = touch[port].report[i].x * SCREEN_FB_WIDTH / info.maxAaX;
                int screen_y = touch[port].report[i].y * SCREEN_FB_HEIGHT / info.maxAaY;
#if VITA_ASPECT_FIT && !defined(GL11)
                int x = vita_aspect_axis(screen_x, aspect_viewport.x, aspect_viewport.width, SCREEN_WIDTH, aspect_viewport.scale);
                int y = vita_aspect_axis(screen_y, aspect_viewport.y, aspect_viewport.height, SCREEN_HEIGHT, aspect_viewport.scale);
                if (x < 0 || y < 0) {
                    // A touch in a side bar cannot click a game component.
                    if (touch_down) {
                        touch_down = false;
                        c->shell->mouse_button = 0;
                        if (_InputTracking.enabled) inputtracking_mouse_released(&_InputTracking, right_click ? 1 : 0);
                    }
                    continue;
                }
#else
                int x = screen_x - SCREEN_CENTER_XOFF;
                int y = screen_y - VITA_YOFF;
#endif

                c->shell->idle_cycles = 0;
                c->shell->mouse_x = x;
                c->shell->mouse_y = y;

                if (_InputTracking.enabled) {
                    inputtracking_mouse_moved(&_InputTracking, x, y);
                }

                if (!touch_down) {
                    touch_down = true;
                    update_touch = true;

                    last_touch_x = x;
                    last_touch_y = y;

                    if (insideMobileInputArea(c)) {
                        // SDL_StartTextInput();
                    }

                    if (right_click) {
                        last_touch_button = 2;
                        c->shell->mouse_button = 2;
                    } else {
                        last_touch_button = 1;
                        c->shell->mouse_button = 1;
                    }

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_pressed(&_InputTracking, x, y, right_click ? 1 : 0);
                    }
                }
            } else {
                if (touch_down) {
                    touch_down = false;

                    c->shell->idle_cycles = 0;
                    c->shell->mouse_button = 0;

                    if (_InputTracking.enabled) {
                        inputtracking_mouse_released(&_InputTracking, right_click ? 1 : 0);
                    }
                }
            }
        }
    }

    ctrl_old = ctrl;
    /* Keep short Square presses that occur between game polls. Peeked
     * history overlaps polls, so timestamps deduplicate each physical edge. */
    SceCtrlData samples[64];
    static VitaButtonEdges square_edges;
    static VitaButtonEdges stone_left_edges,stone_right_edges;
    int square_toggle=0;
    int stone_steps=0;
    int sample_count=sceCtrlPeekBufferPositive(0,samples,64);
    if(sample_count>0) {
        for(int i=1;i<sample_count;i++) {
            SceCtrlData value=samples[i];int j=i;
            while(j>0&&samples[j-1].timeStamp>value.timeStamp){samples[j]=samples[j-1];j--;}
            samples[j]=value;
        }
        ctrl=samples[sample_count-1];
        if(!square_edges.ready)vita_button_edge(&square_edges,ctrl.timeStamp,ctrl.buttons,SCE_CTRL_SQUARE);
        else for(int i=0;i<sample_count;i++)square_toggle^=vita_button_edge(&square_edges,samples[i].timeStamp,samples[i].buttons,SCE_CTRL_SQUARE);
        if(!stone_left_edges.ready||!stone_right_edges.ready)r89_stone_edge(&stone_left_edges,&stone_right_edges,ctrl.timeStamp,ctrl.buttons,SCE_CTRL_LEFT,SCE_CTRL_RIGHT);
        else for(int i=0;i<sample_count;i++)stone_steps+=r89_stone_edge(&stone_left_edges,&stone_right_edges,samples[i].timeStamp,samples[i].buttons,SCE_CTRL_LEFT,SCE_CTRL_RIGHT);
    }
    vita_latest_analog(&ctrl);
    ctrl.buttons=h8_filter_buttons(ctrl.buttons);
    if(h8_is_armed()){ctrl.lx=ctrl.ly=ctrl.rx=ctrl.ry=128;}

    int pressed = ctrl.buttons & ~ctrl_old.buttons;
    int released = ~ctrl.buttons & ctrl_old.buttons;
    const unsigned int r_buttons = SCE_CTRL_R1 | SCE_CTRL_RTRIGGER;
    const unsigned int confirm_buttons = SCE_CTRL_CROSS | r_buttons;
    static bool r_menu_consumed = false;
    bool menu_r_pressed = c->menu_visible && (pressed & r_buttons);
    bool r_primary_pressed = !c->menu_visible && !r_menu_consumed && (pressed & r_buttons);
    bool menu_confirm_pressed = c->menu_visible &&
        (ctrl.buttons & confirm_buttons) && !(ctrl_old.buttons & confirm_buttons);
    if (menu_r_pressed) r_menu_consumed = true;

    // Edge-triggered dialogue input shares the existing poll; higher-priority UIs win.
    bool dialogue_confirm=vita_dialogue_input(c,vkb_open || c->menu_visible,
        (pressed & (c->ingame?SCE_CTRL_CROSS:confirm_buttons))!=0,
        (pressed & SCE_CTRL_DOWN)?1:(pressed & SCE_CTRL_UP)?-1:0,
        update_touch && last_touch_button==1);
    if(dialogue_confirm) {
        pressed &= ~SCE_CTRL_CROSS;
        r_primary_pressed=false; // simultaneous confirm inputs produce one action
        update_touch=false;
        c->shell->mouse_click_button=c->shell->mouse_button=0;
    }

    /* Direct panel selection never dispatches a stone/button action: Run
     * and its authoritative varp are untouched. Higher-priority inputs win;
     * the persistent tutorial guide remains compatible with tab selection. */
    if(stone_steps&&c->ingame&&!vkb_open&&!h8_is_armed()&&!c->menu_visible&&
       c->viewport_interface_id<0&&c->sidebar_interface_id<0&&c->chat_interface_id<0&&
       !c->obj_drag_area&&!c->show_social_input&&!c->chatback_input_open){
        int next=c->selected_tab;
        while(stone_steps){int direction=stone_steps>0?1:-1;next=r89_next_stone(c->tab_interface_id,next,direction);stone_steps-=direction;}
        if(next!=c->selected_tab){c->selected_tab=next;c->redraw_sidebar=c->redraw_sideicons=true;c->shell->idle_cycles=0;}
    }

    if (pressed & SCE_CTRL_TRIANGLE) {
        key_pressed(c->shell, K_CONTROL, -1);
    }

    if (pressed & SCE_CTRL_CIRCLE) {
    }

    if (c->menu_visible ? menu_confirm_pressed : (pressed & SCE_CTRL_CROSS)) {
        if (c->menu_visible) c->vita_menu_confirm = true;
        else if (!dialogue_confirm) {
            // Cross is the primary pointer action; L opens the option menu.
            r_primary_pressed = true;
        }
    }

    if (square_toggle && !h8_is_armed()) {
        _Custom.show_performance = !_Custom.show_performance;
    }

    if (pressed & SCE_CTRL_DOWN) {
        if (c->menu_visible && c->menu_size > 0) {
            if (c->vita_menu_selected > 0) c->vita_menu_selected--;
            if(!ve_enabled())c->redraw_sidebar = c->redraw_chatback = true;
            c->shell->idle_cycles = 0;
        }
    }

    if (pressed & SCE_CTRL_UP) {
        if (c->menu_visible && c->menu_size > 0) {
            if (c->vita_menu_selected < c->menu_size - 1) c->vita_menu_selected++;
            if(!ve_enabled())c->redraw_sidebar = c->redraw_chatback = true;
            c->shell->idle_cycles = 0;
        }
    }

    if (pressed & SCE_CTRL_SELECT) {
        _Custom.show_performance = !_Custom.show_performance;
    }
    if (pressed & SCE_CTRL_START) {
    }
    if (pressed & (SCE_CTRL_L1 | SCE_CTRL_LTRIGGER)) {
        if (c->menu_visible) {
            c->vita_menu_cancel = true;
            update_touch = false;
            c->shell->mouse_click_button = 0;
        } else {
            c->shell->idle_cycles = 0;
            update_touch = true;
            last_touch_x = c->shell->mouse_x;
            last_touch_y = c->shell->mouse_y;
            last_touch_button = 2;
            c->shell->mouse_button = 2;
            if (_InputTracking.enabled) {
                inputtracking_mouse_pressed(&_InputTracking, c->shell->mouse_x, c->shell->mouse_y, 1);
            }
        }
    }
    if (r_primary_pressed) {
        c->shell->idle_cycles = 0;
        update_touch = true;
        last_touch_x = c->shell->mouse_x;
        last_touch_y = c->shell->mouse_y;
        last_touch_button = 1;
        c->shell->mouse_button = 1;
        if (_InputTracking.enabled) {
            inputtracking_mouse_pressed(&_InputTracking, c->shell->mouse_x, c->shell->mouse_y, 0);
        }
    }

    if (released & SCE_CTRL_TRIANGLE) {
        key_released(c->shell, K_CONTROL, -1);
    }

    if (released & SCE_CTRL_CIRCLE) {
    }

    if (released & SCE_CTRL_CROSS) {
        right_click = false;
        c->shell->mouse_button = 0;
        if (_InputTracking.enabled) inputtracking_mouse_released(&_InputTracking, 0);
    }

    if (released & SCE_CTRL_SQUARE) {
    }

    if (released & SCE_CTRL_SELECT) {
    }
    if (released & SCE_CTRL_START) {
    }
    if (released & (SCE_CTRL_L1 | SCE_CTRL_LTRIGGER)) {
        c->shell->idle_cycles = 0;
        c->shell->mouse_button = 0;
        if (_InputTracking.enabled) {
            inputtracking_mouse_released(&_InputTracking, 1);
        }
    }
    if (released & r_buttons) {
        // A menu confirm is not a mouse click, including its later release.
        if (!r_menu_consumed) {
            c->shell->idle_cycles = 0;
            c->shell->mouse_button = 0;
            if (_InputTracking.enabled) {
                inputtracking_mouse_released(&_InputTracking, 0);
            }
        }
        if (!(ctrl.buttons & r_buttons)) r_menu_consumed = false;
    }

    // rs2_log("\e[m Stick:[%3i:%3i][%3i:%3i]\r", ctrl.lx,ctrl.ly, ctrl.rx,ctrl.ry);

    // left stick = cursor, right stick = camera
    {
        // Match the original 12 px at 50 Hz, independent of render speed.
        static uint64_t last_cursor_time = 0;
        uint64_t now = rs2_now();
        uint64_t elapsed = last_cursor_time ? now - last_cursor_time : 20;
        last_cursor_time = now;
        if (elapsed > 50) elapsed = 50; // avoid jumps after loading stalls
        const float cursor_max_speed = 0.6f * (float)elapsed;

        static float cursor_x = 0.0f, cursor_y = 0.0f;
        static int last_mouse_x = -1, last_mouse_y = -1;

        if (c->shell->mouse_x != last_mouse_x || c->shell->mouse_y != last_mouse_y) {
            // touch moved the mouse, continue from there
            cursor_x = (float)c->shell->mouse_x;
            cursor_y = (float)c->shell->mouse_y;
        }

        float sx = vita_stick_axis(ctrl.lx);
        float sy = vita_stick_axis(ctrl.ly);

        if (sx != 0.0f || sy != 0.0f) {
            const float max_x = (float)(SCREEN_WIDTH - 1);
            const float max_y = (float)(SCREEN_HEIGHT - 1);

            cursor_x += sx * cursor_max_speed;
            cursor_y += sy * cursor_max_speed;

            if (cursor_x < 0.0f) cursor_x = 0.0f;
            if (cursor_y < 0.0f) cursor_y = 0.0f;
            if (cursor_x > max_x) cursor_x = max_x;
            if (cursor_y > max_y) cursor_y = max_y;

            c->shell->mouse_x = (int)cursor_x;
            c->shell->mouse_y = (int)cursor_y;
            c->shell->idle_cycles = 0;

            if (_InputTracking.enabled) {
                inputtracking_mouse_moved(&_InputTracking, c->shell->mouse_x, c->shell->mouse_y);
            }
        }

        last_mouse_x = c->shell->mouse_x;
        last_mouse_y = c->shell->mouse_y;

        if (sx != 0.0f || sy != 0.0f) cursor_visible=true;
        cursor_want_x=c->shell->mouse_x+SCREEN_CENTER_XOFF;
        cursor_want_y=c->shell->mouse_y+VITA_YOFF;

        static bool cam_left = false, cam_right = false, cam_up = false, cam_down = false;
        int rx = (int)ctrl.rx - 128;
        int ry = (int)ctrl.ry - 128;
        vita_last_rx = rx;
        vita_last_ry = ry;

        vita_cam_key(c, &cam_left, rx < -60, K_LEFT);
        vita_cam_key(c, &cam_right, rx > 60, K_RIGHT);
        vita_cam_key(c, &cam_up, ry < -60, K_UP);
        vita_cam_key(c, &cam_down, ry > 60, K_DOWN);
    }

    {
        bool touch_edge = touch[0].reportNum > 0 && touch_old[0].reportNum == 0;
        vkb_update(c, !dialogue_confirm && (touch_edge || (pressed & (SCE_CTRL_L1 | SCE_CTRL_LTRIGGER)) || r_primary_pressed));
    }
}

void platform_blit_surface(Surface *surface, int x, int y) {
#ifdef GL11
    (void)surface, (void)x, (void)y;
#else
    x += SCREEN_CENTER_XOFF;
    y += VITA_YOFF;

    sceKernelLockMutex(mutex, 1, NULL);
    // mix the keyboard into the surface so it is never missing between frames
    bool kb_hit = false;
    int kax = 0, kay = 0, kfx0 = 0, kfy0 = 0, kfx1 = 0, kfy1 = 0;
    uint32_t *kb_src = (uint32_t *)surface->pixels;
    if (vkb_open) {
        kax = vkb_x - 4 + SCREEN_CENTER_XOFF;
        kay = vkb_y - 4 + VITA_YOFF;
        int kbx = kax + VKB_TW, kby = kay + VKB_TH;
        kfx0 = kax > x ? kax : x;
        kfy0 = kay > y ? kay : y;
        kfx1 = kbx < x + surface->w ? kbx : x + surface->w;
        kfy1 = kby < y + surface->h ? kby : y + surface->h;
        kb_hit = kfx0 < kfx1 && kfy0 < kfy1;
    }
    if (kb_hit) {
        vkb_ensure_cache();
        vkb_composited = true;
        for (int fy = kfy0; fy < kfy1; fy++) {
            for (int fx = kfx0; fx < kfx1; fx++) {
                int si = (fy - y) * surface->w + (fx - x);
                vkb_saved[(fy - kay) * VKB_TW + (fx - kax)] = kb_src[si];
                uint32_t col;
                if (vkb_lookup(fx - SCREEN_CENTER_XOFF, fy - VITA_YOFF, &col)) {
                    kb_src[si] = col;
                }
            }
        }
    }

    // Restore once before scene/UI blits; composite the cursor at presentation.
    cursor_restore();
    platform_set_pixels(base, surface, x, y, true);

    if (kb_hit) {
        for (int fy = kfy0; fy < kfy1; fy++) {
            for (int fx = kfx0; fx < kfx1; fx++) {
                kb_src[(fy - y) * surface->w + (fx - x)] = vkb_saved[(fy - kay) * VKB_TW + (fx - kax)];
            }
        }
    }

    sceKernelUnlockMutex(mutex, 1);
#endif
}

#ifndef GL11
static void present_frame(void) {
    if (!display_ok) {
        return;
    }
#ifdef BUILD_A_EMBEDDED
    static bool had_pending=false;
    uint64_t wait_started=ba_timing_now();
    int wait_result=sceDisplayWaitSetFrameBuf();
    if(wait_result<0)ba_timing_record(BA_DISPLAY_WAIT_FAIL,0);
    SceDisplayFrameBuf observed={0};observed.size=sizeof(observed);
    int get_result=sceDisplayGetFrameBuf(&observed,SCE_DISPLAY_SETBUF_IMMEDIATE);
    if(get_result<0)ba_timing_record(BA_DISPLAY_GET_FAIL,0);
    if(had_pending&&wait_result==0&&get_result==0){
        if(observed.base==display_base[display_idx])ba_timing_record(BA_DISPLAY_ACK,ba_timing_now()-wait_started);
        else ba_timing_record(BA_DISPLAY_MISMATCH,0);
    }
#else
    sceDisplayWaitSetFrameBuf(); // the previous flip is done, so the other buffer is free
#endif
    int next = display_idx ^ 1;
    sceKernelLockMutex(mutex, 1, NULL);
#if VITA_ASPECT_FIT
    const uint32_t *source = (const uint32_t *)ram_base;
    uint32_t *output = (uint32_t *)display_base[next];
    for (int y = 0; y < SCREEN_FB_HEIGHT; y++) {
        uint32_t *row = output + y * SCREEN_FB_WIDTH;
        memset(row, 0, SCREEN_FB_WIDTH * sizeof(*row));
        if (aspect_rows[y] < 0) continue;
        const uint32_t *input = source + (aspect_rows[y] + VITA_YOFF) * SCREEN_FB_WIDTH + SCREEN_CENTER_XOFF;
        for (int x = aspect_viewport.x; x < aspect_viewport.x + aspect_viewport.width; x++) {
            row[x] = input[aspect_columns[x]];
        }
    }
#else
    memcpy(display_base[next], ram_base, SCREEN_FB_WIDTH * SCREEN_FB_HEIGHT * 4);
#endif
    sceKernelUnlockMutex(mutex, 1);
    SceDisplayFrameBuf frame = {sizeof(frame), display_base[next], SCREEN_FB_WIDTH, 0, SCREEN_FB_WIDTH, SCREEN_FB_HEIGHT};
#ifdef BUILD_A_EMBEDDED
    int set_result=sceDisplaySetFrameBuf(&frame,SCE_DISPLAY_SETBUF_NEXTFRAME);
    if(set_result<0)ba_timing_record(BA_DISPLAY_SET_FAIL,0);
    had_pending=set_result==0;
#else
    sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
#endif
    display_idx = next;
}

static uint32_t vkb_bg[VKB_TW * VKB_TH];
static bool vkb_bg_valid = false;
static int vkb_bg_x = 0, vkb_bg_y = 0;

static void vkb_direct(void) {
    if (!base) {
        return;
    }
    uint32_t *fb = (uint32_t *)base;

    if (!vkb_open) {
        if (vkb_bg_valid) {
            // closing: put back what was under the keyboard
            sceKernelLockMutex(mutex, 1, NULL);
            bool had_cursor = cursor_saved;
            cursor_restore();
            for (int j = 0; j < VKB_TH; j++) {
                int fy = vkb_bg_y - 4 + j + VITA_YOFF;
                if (fy < 0 || fy >= SCREEN_FB_HEIGHT) continue;
                for (int i = 0; i < VKB_TW; i++) {
                    int fx = vkb_bg_x - 4 + i + SCREEN_CENTER_XOFF;
                    if (fx < 0 || fx >= SCREEN_FB_WIDTH) continue;
                    fb[fy * SCREEN_FB_WIDTH + fx] = vkb_bg[j * VKB_TW + i];
                }
            }
            if (had_cursor) {
                cursor_draw(cursor_fx, cursor_fy);
            }
            sceKernelUnlockMutex(mutex, 1);
            vkb_bg_valid = false;
        }
        return;
    }

    vkb_ensure_cache();
    sceKernelLockMutex(mutex, 1, NULL);
    bool cursor_on = cursor_saved;

    if (!vkb_bg_valid) {
        // first frame: remember the screen under the keyboard, without the crosshair on it
        cursor_restore();
        vkb_bg_x = vkb_x;
        vkb_bg_y = vkb_y;
        for (int j = 0; j < VKB_TH; j++) {
            int fy = vkb_y - 4 + j + VITA_YOFF;
            for (int i = 0; i < VKB_TW; i++) {
                int fx = vkb_x - 4 + i + SCREEN_CENTER_XOFF;
                uint32_t v = 0;
                if (fx >= 0 && fx < SCREEN_FB_WIDTH && fy >= 0 && fy < SCREEN_FB_HEIGHT) {
                    v = fb[fy * SCREEN_FB_WIDTH + fx];
                }
                vkb_bg[j * VKB_TW + i] = v;
            }
        }
        vkb_bg_valid = true;
    }

    for (int j = 0; j < VKB_TH; j++) {
        int fy = vkb_y - 4 + j + VITA_YOFF;
        if (fy < 0 || fy >= SCREEN_FB_HEIGHT) continue;
        for (int i = 0; i < VKB_TW; i++) {
            int fx = vkb_x - 4 + i + SCREEN_CENTER_XOFF;
            if (fx < 0 || fx >= SCREEN_FB_WIDTH) continue;
            fb[fy * SCREEN_FB_WIDTH + fx] = cursor_swap_rb(vkb_cache[j * VKB_TW + i]);
        }
    }

    if (cursor_on) {
        // draw the crosshair over the keys, keeping the saved "under" pixels in step
        int rx0 = vkb_x - 4 + SCREEN_CENTER_XOFF, ry0 = vkb_y - 4 + VITA_YOFF;
        for (int j = 0; j < CURSOR_SIZE; j++) {
            for (int i = 0; i < CURSOR_SIZE; i++) {
                int px = cursor_fx - 6 + i, py = cursor_fy - 6 + j;
                if (px < 0 || py < 0 || px >= SCREEN_FB_WIDTH || py >= SCREEN_FB_HEIGHT) continue;
                if (px >= rx0 && px < rx0 + VKB_TW && py >= ry0 && py < ry0 + VKB_TH) {
                    cursor_under[j * CURSOR_SIZE + i] = fb[py * SCREEN_FB_WIDTH + px];
                }
                uint32_t col;
                if (cursor_pixel(i - 6, j - 6, &col)) {
                    fb[py * SCREEN_FB_WIDTH + px] = col;
                }
            }
        }
        cursor_saved = true;
    }
    sceKernelUnlockMutex(mutex, 1);
}
#endif

void platform_update_surface(void) {
    h8_present();
    if(h5_silent())return;
#ifdef GL11
    static PixMap *gpu_keyboard,*gpu_cursor,*gpu_keyboard_tab;
    static int gpu_keyboard_sig=-1;
    if(vkb_open){
        if(!gpu_keyboard){gpu_keyboard=pixmap_new(VKB_TW,VKB_TH);gpu_keyboard_sig=-1;}
        vkb_ensure_cache();
        if(gpu_keyboard_sig!=vkb_cache_sig){
            memcpy(gpu_keyboard->pixels,vkb_cache,sizeof(vkb_cache));
            gpu_keyboard->dirty=true;gpu_keyboard_sig=vkb_cache_sig;
        }
        R63KeyboardBox b=vkb_box();
        /* Scale just this keyboard quad; all game/UI matrices are restored. */
        glMatrixMode(GL_MODELVIEW);glPushMatrix();
        glTranslatef(b.x+SCREEN_CENTER_XOFF,b.y+VITA_YOFF,0.f);
        glScalef(b.w/(float)VKB_TW,b.h/(float)VKB_TH,1.f);
        glTranslatef(-(b.x+SCREEN_CENTER_XOFF),-(b.y+VITA_YOFF),0.f);
        pixmap_draw(gpu_keyboard,b.x,b.y+VITA_YOFF);glPopMatrix();
        if(!gpu_keyboard_tab){
            gpu_keyboard_tab=pixmap_new(44,36);
            for(int y=0;y<36;y++)for(int x=0;x<44;x++){
                unsigned rgb=(x==0||y==0||x==43||y==35)?0x302B23:OPTIONS_MENU;
                int ix=x-8,iy=y-4;
                if(ix>=0&&ix<28&&iy>=0&&iy<28){
                    unsigned a=r63_search_alpha[iy*28+ix],v=0;
                    for(int k=0;k<3;k++){unsigned c=(rgb>>(k*8))&255;v|=((c*(255-a)+255*a+127)/255)<<(k*8);}rgb=v;
                }
                gpu_keyboard_tab->pixels[y*44+x]=rgb;
            }
            gpu_keyboard_tab->dirty=true;
        }
        pixmap_draw(gpu_keyboard_tab,b.tab_x,b.tab_y+VITA_YOFF);
    }else{
        if(gpu_keyboard){pixmap_free(gpu_keyboard);gpu_keyboard=NULL;gpu_keyboard_sig=-1;}
        if(gpu_keyboard_tab){pixmap_free(gpu_keyboard_tab);gpu_keyboard_tab=NULL;}
    }
    if(cursor_visible){
        static int gpu_cursor_choice=-1;
        int selected=r27_cursor_choice();const R27Cursor*shape=r27_cursor_at(selected);
        if(gpu_cursor&&gpu_cursor_choice!=selected){pixmap_free(gpu_cursor);gpu_cursor=NULL;}
        if(!gpu_cursor){
            gpu_cursor=pixmap_new(shape->width,shape->height);
            if(gpu_cursor){memcpy(gpu_cursor->pixels,shape->pixels,(size_t)shape->width*shape->height*sizeof(uint32_t));gpu_cursor->dirty=true;gpu_cursor_choice=selected;}
        }
        if(gpu_cursor)pixmap_draw(gpu_cursor,cursor_want_x-shape->hotspot_x-SCREEN_CENTER_XOFF,cursor_want_y-shape->hotspot_y);
    }else if(gpu_cursor){pixmap_free(gpu_cursor);gpu_cursor=NULL;}

    static uint64_t gpu_memory_due;
    uint64_t gpu_memory_now=sceKernelGetSystemTimeWide();
    if(gpu_memory_now>=gpu_memory_due){
        gpu_memory_due=gpu_memory_now+10000000;
        extern int r16_diag_verbose(void);
        if(r16_diag_verbose()){
            uint64_t io0=ba_timing_now();
            FILE *gpu_memory=fopen("ux0:/data/RS2Vita/gpu-memory.csv","a");
            if(gpu_memory){fprintf(gpu_memory,"%llu,%u,%u,%u,%u\n",(unsigned long long)gpu_memory_now,
                (unsigned)vglMemFree(VGL_MEM_RAM),(unsigned)vglMemTotal(VGL_MEM_RAM),
                (unsigned)vglMemFree(VGL_MEM_VRAM),(unsigned)vglMemTotal(VGL_MEM_VRAM));fclose(gpu_memory);}
            r16_io_add(4,ba_timing_now()-io0);
        }
    }
    r13_capture_before_present();
    r12_before_present();
    {R11_SCOPE(R11_PRESENT);vglSwapBuffers(GL_FALSE);}
    r12_end();
    r11_end();
#ifdef BUILD_A_EMBEDDED
    /* GPU probe: conservatively count sampled changes of the visible buffer. */
    static const void *previous_visible;
    SceDisplayFrameBuf visible={0};visible.size=sizeof(visible);
    int observed=sceDisplayGetFrameBuf(&visible,SCE_DISPLAY_SETBUF_IMMEDIATE);
    if(observed<0)ba_timing_record(BA_DISPLAY_GET_FAIL,0);
    else {if(previous_visible&&visible.base!=previous_visible)ba_timing_record(BA_DISPLAY_ACK,0);previous_visible=visible.base;}
#endif
#else
    vkb_direct();
    if (cursor_visible) {
        sceKernelLockMutex(mutex, 1, NULL);
        cursor_restore();
        cursor_draw(cursor_want_x, cursor_want_y);
        sceKernelUnlockMutex(mutex, 1);
    }
    vkb_composited = false;
    present_frame();
#endif
}

uint64_t rs2_now(void) {
    return (sceKernelGetSystemTimeWide() - systemtime_start) / 1000; // fixes int to float conversions starting at 0
}

void rs2_sleep(int ms) {
    sceKernelDelayThreadCB(ms * 1000);
}
#endif
