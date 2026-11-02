#include <stdlib.h>
#include <string.h>
#include "../platform.h"

#include "../packet.h"
#include "wave.h"
#include <stdbool.h>

WaveData _Wave = {0};
static Packet *original_effect_pcm[1000] = {0};
extern ToneData _Tone;

static int generate(Wave *wave, int loopCount);
static void wave_free(Wave *wave);

void wave_free_global(void) {
    for (int i = 0; i < 1000; i++) {
        if (original_effect_pcm[i]) { packet_free(original_effect_pcm[i]); original_effect_pcm[i] = NULL; }
        if (_Wave.tracks[i]) {
            wave_free(_Wave.tracks[i]);
        }
    }
    if (_Wave.waveBuffer) { // it's low detail
        packet_free(_Wave.waveBuffer);
    }
}

static void wave_free(Wave *wave) {
    for (int i = 0; i < 10; i++) {
        if (wave->tones[i]) {
            tone_free(wave->tones[i]);
        }
    }
    free(wave);
}

void wave_unpack(Packet *dat) {
    _Wave.waveBytes = calloc(441000, sizeof(int8_t));
    _Wave.waveBuffer = packet_new(_Wave.waveBytes, 441000);
    tone_init_global();

    while (true) {
        int id = g2(dat);
        if (id == 65535) {
            return;
        }

        _Wave.tracks[id] = calloc(1, sizeof(Wave));
        wave_read(_Wave.tracks[id], dat);
        _Wave.delays[id] = wave_trim(_Wave.tracks[id]);
    }
}

int wave_available(int id) {
    return id >= 0 && id < 1000 && (original_effect_pcm[id] || _Wave.tracks[id]);
}

Packet *wave_generate(int id, int loopCount) {
    if (!wave_available(id)) return NULL;
    if (original_effect_pcm[id]) return original_effect_pcm[id];
    if (!_Wave.tracks[id]) {
        return NULL;
    }

    Wave *track = _Wave.tracks[id];
    return wave_get_wave(track, loopCount);
}

void wave_read(Wave *wave, Packet *dat) {
    for (int tone = 0; tone < 10; tone++) {
        if (g1(dat) != 0) {
            dat->pos--;
            wave->tones[tone] = tone_new();
            tone_read(wave->tones[tone], dat);
        }
    }

    wave->loopBegin = g2(dat);
    wave->loopEnd = g2(dat);
}

int wave_trim(Wave *wave) {
    int start = 9999999;
    for (int tone = 0; tone < 10; tone++) {
        if (wave->tones[tone] && wave->tones[tone]->start / 20 < start) {
            start = wave->tones[tone]->start / 20;
        }
    }

    if (wave->loopBegin < wave->loopEnd && wave->loopBegin / 20 < start) {
        start = wave->loopBegin / 20;
    }

    if (start == 9999999 || start == 0) {
        return 0;
    }

    for (int tone = 0; tone < 10; tone++) {
        if (wave->tones[tone]) {
            wave->tones[tone]->start -= start * 20;
        }
    }

    if (wave->loopBegin < wave->loopEnd) {
        wave->loopBegin -= start * 20;
        wave->loopEnd -= start * 20;
    }

    return start;
}

Packet *wave_get_wave(Wave *wave, int loopCount) {
    if (!wave || !_Wave.waveBuffer || !_Wave.waveBytes) return NULL;
    int length = generate(wave, loopCount);
    _Wave.waveBuffer->pos = 0;
    p4(_Wave.waveBuffer, 0x52494646);   // "RIFF" ChunkID
    ip4(_Wave.waveBuffer, length + 36); // ChunkSize
    p4(_Wave.waveBuffer, 0x57415645);   // "WAVE" format
    p4(_Wave.waveBuffer, 0x666d7420);   // "fmt " chunk id
    ip4(_Wave.waveBuffer, 16);          // chunk size
    ip2(_Wave.waveBuffer, 1);           // audio format
    ip2(_Wave.waveBuffer, 1);           // num channels
    ip4(_Wave.waveBuffer, 22050);       // sample rate
    ip4(_Wave.waveBuffer, 22050);       // byte rate
    ip2(_Wave.waveBuffer, 1);           // block align
    ip2(_Wave.waveBuffer, 8);           // bits per sample
    p4(_Wave.waveBuffer, 0x64617461);   // "data"
    ip4(_Wave.waveBuffer, length);
    _Wave.waveBuffer->pos += length;
    return _Wave.waveBuffer;
}

static int generate(Wave *wave, int loopCount) {
    int duration = 0;
    for (int tone = 0; tone < 10; tone++) {
        if (wave->tones[tone] && wave->tones[tone]->length + wave->tones[tone]->start > duration) {
            duration = wave->tones[tone]->length + wave->tones[tone]->start;
        }
    }

    if (duration == 0) {
        return 0;
    }

    int sampleCount = duration * 22050 / 1000;
    int loopStart = wave->loopBegin * 22050 / 1000;
    int loopStop = wave->loopEnd * 22050 / 1000;
    if (loopStart < 0 || loopStop < 0 || loopStop > sampleCount || loopStart >= loopStop) {
        loopCount = 0;
    }

    int totalSampleCount = sampleCount + (loopStop - loopStart) * (loopCount - 1);
    for (int sample = 44; sample < totalSampleCount + 44; sample++) {
        _Wave.waveBytes[sample] = -128;
    }

    for (int tone = 0; tone < 10; tone++) {
        if (wave->tones[tone]) {
            int toneSampleCount = wave->tones[tone]->length * 22050 / 1000;
            int start = wave->tones[tone]->start * 22050 / 1000;
            int *samples = tone_generate(wave->tones[tone], toneSampleCount, wave->tones[tone]->length);

            for (int sample = 0; sample < toneSampleCount; sample++) {
                _Wave.waveBytes[sample + start + 44] += (int8_t)(samples[sample] >> 8);
            }
        }
    }

    if (loopCount > 1) {
        loopStart += 44;
        loopStop += 44;
        sampleCount += 44;
        totalSampleCount += 44;

        int endOffset = totalSampleCount - sampleCount;
        for (int sample = sampleCount - 1; sample >= loopStop; sample--) {
            _Wave.waveBytes[sample + endOffset] = _Wave.waveBytes[sample];
        }

        for (int loop = 1; loop < loopCount; loop++) {
            int offset = (loopStop - loopStart) * loop;

            for (int sample = loopStart; sample < loopStop; sample++) {
                _Wave.waveBytes[sample + offset] = _Wave.waveBytes[sample];
            }
        }

        totalSampleCount -= 44;
    }

    return totalSampleCount;
}

// The three imported effects are verified one-shots (original loopBegin == loopEnd == 0).
// Original JagFX/Tone/Filter code renders their immutable PCM; no filter is discarded.
void wave_unpack_pcm(Packet *dat) {
    if (dat->length < 6 || g4(dat) != 0x4e535031) rs2_error("Invalid original-effect PCM header");
    int count = g2(dat);
    if (count > 1000) rs2_error("Invalid original-effect PCM count");
    for (int i = 0; i < count; i++) {
        if (dat->pos + 8 > dat->length) rs2_error("Truncated original-effect PCM record");
        int id = g2(dat), delay = g2(dat), size = g4(dat);
        if (id < 0 || id >= 1000 || size < 44 || size > 441000 || size > dat->length - dat->pos || _Wave.tracks[id] || original_effect_pcm[id]) rs2_error("Invalid or colliding original-effect PCM record");
        int8_t *bytes = malloc(size);
        if (!bytes) rs2_error("Original-effect PCM allocation failed");
        memcpy(bytes, dat->data + dat->pos, size); dat->pos += size;
        if (memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4)) rs2_error("Invalid original-effect WAV");
        original_effect_pcm[id] = packet_new(bytes, size); original_effect_pcm[id]->pos = size;
        _Wave.delays[id] = delay;
    }
    if (dat->pos != dat->length) rs2_error("Trailing original-effect PCM data");
}
