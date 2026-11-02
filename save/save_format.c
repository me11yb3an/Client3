#include "save_format.h"

typedef struct {
    const uint8_t *p;
    size_t n;
    size_t i;
    int ok;
} Cur;

static int take(Cur *c, size_t n) {
    if (!c->ok || n > c->n - c->i) {
        c->ok = 0;
        return 0;
    }
    c->i += n;
    return 1;
}

static unsigned ru8(Cur *c) {
    unsigned v;
    if (!take(c, 1)) return 0;
    v = c->p[c->i - 1];
    return v;
}

static unsigned ru16(Cur *c) {
    unsigned v;
    if (!take(c, 2)) return 0;
    v = ((unsigned)c->p[c->i - 2] << 8) | c->p[c->i - 1];
    return v;
}

uint32_t save_crc32(const uint8_t *data, size_t length) {
    static uint32_t table[256];
    static int ready;
    uint32_t crc = 0xFFFFFFFFu;
    size_t n;
    if (!ready) {
        int i, bit;
        for (i = 0; i < 256; i++) {
            uint32_t rem = (uint32_t)i;
            for (bit = 0; bit < 8; bit++)
                rem = (rem & 1u) ? ((rem >> 1) ^ 0xEDB88320u) : (rem >> 1);
            table[i] = rem;
        }
        ready = 1;
    }
    for (n = 0; n < length; n++)
        crc = (crc >> 8) ^ table[(crc ^ data[n]) & 255u];
    return ~crc;
}

static int payload_ok(const uint8_t *payload, size_t n) {
    Cur c;
    unsigned version, varps, invs, slot, raw, count, afk;
    size_t minimum;
    if (n < 4) return 0;
    if (payload[0] != 0x20 || payload[1] != 0x04) return 0;
    version = ((unsigned)payload[2] << 8) | payload[3];
    if (version > SAVE_VERSION_MAX) return 0;
    minimum = 24u + (version >= 2 ? 4u : 2u) + 105u + 2u;
    if (n < minimum) return 0;
    if (version < 5) return 1;
    c.p = payload;
    c.n = n;
    c.i = 0;
    c.ok = 1;
    ru16(&c); /* magic */
    ru16(&c); /* version */
    ru16(&c); /* x */
    ru16(&c); /* z */
    ru8(&c);  /* level */
    take(&c, 7);
    take(&c, 5);
    ru8(&c);  /* gender */
    ru16(&c); /* run energy */
    take(&c, 4); /* playtime, version >= 2 and we are in version >= 5 */
    take(&c, 105);
    varps = ru16(&c);
    if (varps > c.n) c.ok = 0;
    take(&c, (size_t)varps * 4u);
    invs = ru8(&c);
    for (slot = 0; c.ok && slot < invs; slot++) {
        unsigned size, s;
        ru16(&c);
        size = ru16(&c);
        if (size > c.n - c.i) c.ok = 0;
        for (s = 0; c.ok && s < size; s++) {
            raw = ru16(&c);
            if (raw == 0) continue;
            count = ru8(&c);
            if (count == 255) take(&c, 4);
        }
    }
    if (version >= 3) {
        afk = ru8(&c);
        if (afk > c.n) c.ok = 0;
        take(&c, (size_t)afk * 4u);
        ru16(&c);
    }
    if (version >= 4) ru8(&c);
    if (version >= 6) take(&c, 8);
    return c.ok && c.i == c.n;
}

int save_find_image(const uint8_t *data, size_t length, size_t *image_len) {
    size_t end;
    if (!data || length < 8) return 0;
    for (end = length;; end--) {
        uint32_t got, expect;
        if (end >= 8 &&
            data[0] == 0x20 && data[1] == 0x04 &&
            payload_ok(data, end - 4)) {
            got = ((uint32_t)data[end - 4] << 24) | ((uint32_t)data[end - 3] << 16) |
                  ((uint32_t)data[end - 2] << 8) | data[end - 1];
            expect = save_crc32(data, end - 4);
            if (got == expect) {
                if (image_len) *image_len = end;
                return 1;
            }
        }
        if (end <= 8) break;
    }
    return 0;
}
