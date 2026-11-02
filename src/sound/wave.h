#pragma once

#include <stdint.h>

#include "../packet.h"
#include "tone.h"

// name and packaging confirmed 100% in rs2/mapview applet strings
typedef struct {
    Tone *tones[10];
    int loopBegin;
    int loopEnd;
} Wave;

typedef struct {
    Wave *tracks[1000];
    int delays[1000];
    int8_t *waveBytes;
    Packet *waveBuffer;
} WaveData;

void wave_free_global(void);
void wave_unpack(Packet *dat);
int wave_available(int id);
Packet *wave_generate(int id, int loopCount);
void wave_read(Wave *wave, Packet *dat);
int wave_trim(Wave *wave);
Packet *wave_get_wave(Wave *wave, int loopCount);

// Optional, original reference-rendered one-shot effects; existing synthesis is unchanged.
void wave_unpack_pcm(Packet *dat);
