/* A sequencer-style note stream (short gates, notes every 62 ms, all at block starts like the wrapper delivers them) steals voices that are
 * still sounding. They must fade out, not be cut: no sample-to-sample spike at the note-ons beyond what the sound itself has elsewhere. */
#include <stdio.h>
#include <stdlib.h>
#include "engine.h"
static int fails;
#define CHECK(c, m) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", m); fails += !ok_; } while (0)
static void run(const char *assign, const char *alloc, int *near_max, int *far_max) {
    const mpc_engine_t *e = mpc_engine(); void *h = e->create(NULL);
    e->set_param(h, "program", "0"); e->set_param(h, "assign", assign); e->set_param(h, "alloc", alloc);
    static const int notes[8] = { 48, 51, 55, 58, 60, 63, 48, 48 };
    const int step = 2756, gate = step / 4;
    int16_t out[256], y1[2] = { 0 }, y2[2] = { 0 }; long t = 0;
    *near_max = *far_max = 0;
    for (int blk = 0; blk < 44100 * 4 / 128; blk++) {
        int noteon = 0;
        for (int i = 0; i < 128; i++, t++) {
            int k = notes[(t / step) % 8];
            if (t % step == 0) { uint8_t m[3] = { 0x90, (uint8_t)k, 100 }; e->midi(h, m, 3); noteon = 1; }
            if (t % step == gate) { uint8_t m[3] = { 0x80, (uint8_t)k, 0 }; e->midi(h, m, 3); }
        }
        e->render(h, out, 128);
        for (int i = 0; i < 128; i++) for (int c = 0; c < 2; c++) {
            int y = out[2 * i + c], d2 = abs(y - 2 * y1[c] + y2[c]);
            if (blk > 2) { if (noteon && i < 64) { if (d2 > *near_max) *near_max = d2; } else if (d2 > *far_max) *far_max = d2; }
            y2[c] = y1[c]; y1[c] = (int16_t)y;
        }
    }
    e->destroy(h);
}
int main(void) {
    int n, f;
    run("0", "0", &n, &f); printf("  poly:    note-on %d, elsewhere %d\n", n, f); CHECK(n <= 2 * f + 50, "poly: stolen voices fade out");
    run("2", "0", &n, &f); printf("  unison:  note-on %d, elsewhere %d\n", n, f); CHECK(n <= 2 * f + 50, "unison: restarted voices fade out");
    run("0", "1", &n, &f); printf("  mono:    note-on %d, elsewhere %d\n", n, f); CHECK(n <= 2 * f + 50, "mono: the releasing voice fades out");
    printf(fails ? "FAILED\n" : "PASSED\n"); return fails;
}
