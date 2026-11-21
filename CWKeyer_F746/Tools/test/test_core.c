/**
 * Host unit tests for the hardware independent modules.
 * Build: see Tools/test/run_tests.sh
 */
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cw_decoder.h"
#include "event_queue.h"
#include "fat_ro.h"
#include "keyer.h"
#include "morse.h"
#include "morse_timing.h"
#include "player.h"
#include "port.h"
#include "settings.h"
#include "text_source.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- port stubs -------------------------------------------------------- */
static uint32_t ms;
static bool dit, dah;
static FILE *img;
uint32_t port_millis(void) { return ms; }
uint32_t port_random(void) { return (uint32_t)rand(); }
void port_irq_lock(void) {}
void port_irq_unlock(void) {}
bool port_paddle_dit(void) { return dit; }
bool port_paddle_dah(void) { return dah; }
bool port_straight_key(void) { return false; }
void port_key_out(bool on) { (void)on; }
void port_audio_set_input_gain(int8_t db) { (void)db; }
bool port_settings_load(void *b, size_t l) { (void)b; (void)l; return false; }
bool port_settings_save(const void *b, size_t l) { (void)b; (void)l; return true; }
bool port_sd_present(void) { return img != NULL; }
bool port_sd_init(void) { return true; }
bool port_sd_read(uint32_t lba, uint8_t *buf, uint32_t n)
{
    fseek(img, (long)lba * 512, SEEK_SET);
    return fread(buf, 512, n, img) == n;
}

/* ---- helpers ----------------------------------------------------------- */
static void collect(event_queue_t *q, char *out, size_t max, int type1, int type2)
{
    event_t e;
    size_t n = strlen(out);
    while (evq_pop(q, &e))
        if ((e.type == type1 || e.type == type2) && n + 1 < max) { out[n++] = e.arg; out[n] = 0; }
}

static void test_morse(void)
{
    const char *s = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?.,/=+";
    for (const char *p = s; *p; p++)
        CHECK(morse_decode(morse_encode(*p)) == *p, "roundtrip %c", *p);
    CHECK(morse_decode("........") == MORSE_UNKNOWN, "unknown");
    CHECK(morse_encode('a') && strcmp(morse_encode('a'), ".-") == 0, "lower");
}

static void test_timing(void)
{
    morse_timing_t t;
    morse_timing_calc(&t, 20, 20);
    CHECK(t.dit == 60 && t.dah == 180 && t.letter_extra == 120 && t.word_extra == 240, "20wpm");
    morse_timing_calc(&t, 20, 10);
    CHECK(t.letter_extra == 3 * 120 - 60 && t.word_extra == 480, "farnsworth");
    morse_timing_calc(&t, 10, 30);
    CHECK(t.dit == 120 && t.letter_extra == 240, "fwpm clamp");
}

/* hold a paddle for 'hold' ms then release and run 'after' ms */
static void run(int n) { while (n--) { keyer_tick(); ms++; } }

static void test_keyer(void)
{
    event_queue_t q; evq_init(&q);
    morse_timing_t t; morse_timing_calc(&t, 20, 20);
    keyer_init(&q);
    keyer_configure(&t, KEYER_IAMBIC_MEMORY, false, 5);
    char out[64] = "";

    /* 'S': hold dit for ~ 2.5 dits periods -> 3 dits */
    dit = true; run(5 + 120 * 2 + 30); dit = false; run(600);
    collect(&q, out, sizeof out, EV_KEY_CHAR, EV_KEY_SPACE);
    CHECK(strcmp(out, "S ") == 0, "keyer S -> '%s'", out);

    /* 'N': tap dah then tap dit during dah (memory) */
    out[0] = 0;
    dah = true; run(30); dah = false; run(30);
    dit = true; run(20); dit = false; run(800);
    collect(&q, out, sizeof out, EV_KEY_CHAR, EV_KEY_SPACE);
    CHECK(strcmp(out, "N ") == 0, "keyer N (memory) -> '%s'", out);

    /* squeeze: dah first then both -> -.-. = C */
    out[0] = 0;
    dah = true; run(10); dit = true; run(5 + 180 + 60 + 60 + 60 + 180 + 30); dah = false; run(30); dit = false; run(800);
    collect(&q, out, sizeof out, EV_KEY_CHAR, EV_KEY_SPACE);
    CHECK(out[0] == 'C' || out[0] == 'K', "keyer squeeze -> '%s'", out);
}

/* player -> audio -> decoder */
/* the decoder adapts to speed/spacing: 'warmup' is sent first, only the
   decoded text after it is compared */
static void decode_test(uint8_t wpm, uint8_t fwpm, float noise, const char *warmup, const char *expect)
{
    char text[128];
    snprintf(text, sizeof text, "%s%s", warmup, expect);
    event_queue_t pq, dq; evq_init(&pq); evq_init(&dq);
    morse_timing_t t; morse_timing_calc(&t, wpm, fwpm);
    player_init(&pq); player_set_timing(&t);
    cw_decoder_init(&dq, 16000); cw_decoder_configure(700, 20); cw_decoder_enable(true);

    player_start(text, (uint32_t)strlen(text));
    char out[256] = "";
    double ph = 0; float env = 0;
    int16_t buf[16];
    int tail = 3000;
    while (tail > 0)
    {
        player_tick();
        if (!player_busy()) tail--;
        for (int i = 0; i < 16; i++)
        {
            float target = player_tone() ? 1.f : 0.f;
            env += (target - env) * 0.05f;
            ph += 2 * M_PI * 700.0 / 16000.0;
            float n = noise * ((float)rand() / RAND_MAX * 2.f - 1.f);
            buf[i] = (int16_t)(8000.f * (env * sinf((float)ph) + n));
        }
        cw_decoder_process(buf, 16);
        collect(&dq, out, sizeof out, EV_DEC_CHAR, EV_DEC_SPACE);
    }
    /* trim */
    size_t l = strlen(out); while (l && out[l - 1] == ' ') out[--l] = 0;
    cw_decoder_status_t st; cw_decoder_get_status(&st);
    const char *tail_ = (l >= strlen(expect)) ? out + l - strlen(expect) : out;
    CHECK(strcmp(tail_, expect) == 0, "decoder %u/%u wpm noise %.2f: '%s' (est %u wpm)", wpm, fwpm, noise, out, st.wpm);
    if (strcmp(tail_, expect) == 0) printf("  ok decoder %2u/%2u wpm noise %.2f est %u wpm\n", wpm, fwpm, noise, st.wpm);
}

static bool print_cb(const fat_dirent_t *e, void *ctx)
{
    (void)ctx; printf("    %-28s %6u %s\n", e->name, e->size, e->is_dir ? "<DIR>" : "");
    return true;
}

static void test_fat(const char *path)
{
    img = fopen(path, "rb");
    CHECK(img != NULL, "open %s", path);
    if (!img) return;
    fat_fs_t fs;
    int r = fat_mount(&fs, port_sd_read);
    CHECK(r == FAT_OK, "mount %s: %d", path, r);
    printf("  %s: FAT%u\n", path, fs.type);
    fat_list_dir(&fs, 0, print_cb, NULL);

    text_source_status_t st = text_source_scan();
    CHECK(st == TS_READY && text_source_count() == 3, "scan count %d", text_source_count());
    for (int i = 0; i < text_source_count(); i++)
    {
        CHECK(text_source_load(i), "load %d", i);
        printf("    [%d] %s -> %u chars: %.40s\n", i, text_source_name(i), text_source_len(), text_source_text());
    }
    int u = -1;
    for (int i = 0; i < text_source_count(); i++)
        if (strstr(text_source_name(i), "bung_lange")) u = i;
    CHECK(u >= 0, "LFN name");
    if (u >= 0) { text_source_load(u); CHECK(strcmp(text_source_text(), "HALLO WELT, AEOEUE SS! ZEILE 2") == 0, "normalize '%s'", text_source_text()); }
    fclose(img); img = NULL;
}

int main(int argc, char **argv)
{
    settings_defaults(&g_settings);
    test_morse();
    test_timing();
    test_keyer();
    decode_test(12, 12, 0.0f, "", "CQ CQ DE DL1ABC K");
    decode_test(20, 20, 0.1f, "", "PARIS PARIS 599 TU");
    decode_test(30, 30, 0.1f, "VVV ", "TEST DE DK2XY 73");
    decode_test(25, 25, 0.25f, "VVV ", "NOISY SIGNAL 5NN");
    decode_test(20, 10, 0.05f, "VVV VVV ", "DE DL0ABC = QRU?");
    decode_test(40, 40, 0.05f, "VVV ", "QRL? QSY UP 2");
    decode_test(8, 5, 0.05f, "VVV ", "SLOW 73");
    decode_test(35, 35, 0.15f, "VVV ", "FAST CQ TEST");
    for (int i = 1; i < argc; i++) test_fat(argv[i]);
    printf(fails ? "%d FAILURES\n" : "ALL TESTS PASSED\n", fails);
    return fails != 0;
}
