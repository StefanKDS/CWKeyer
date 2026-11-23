/**
 * PC simulator: renders all screens into PPM images and runs a few
 * touch interaction checks. Uses the real App/ and Gui/ code.
 * Build + run: sh Tools/sim/run_sim.sh <out_dir> [fat_image]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "cw_decoder.h"
#include "gui.h"
#include "player.h"
#include "port.h"
#include "screens.h"
#include "settings.h"
#include "text_source.h"
#include "trainer.h"

static uint32_t s_ms;
static FILE    *s_img;
static bool     s_dit;

uint32_t port_millis(void) { return s_ms; }
uint32_t port_random(void) { return (uint32_t)rand(); }
void port_irq_lock(void) {}
void port_irq_unlock(void) {}
bool port_paddle_dit(void) { return s_dit; }
bool port_paddle_dah(void) { return false; }
bool port_straight_key(void) { return false; }
void port_key_out(bool on) { (void)on; }
void port_audio_set_input_gain(int8_t db) { (void)db; }
bool port_settings_load(void *b, size_t l) { (void)b; (void)l; return false; }
bool port_settings_save(const void *b, size_t l) { (void)b; (void)l; return true; }
bool port_sd_present(void) { return s_img != NULL; }
bool port_sd_init(void) { return true; }
static bool s_usb;
void port_usb_drive(bool on) { s_usb = on; }
bool port_usb_active(void) { return s_usb; }
bool port_usb_host_connected(void) { return s_usb; }
void port_usb_stats(uint32_t *r, uint32_t *w, uint32_t *e) { *r = 1234; *w = 56; *e = 0; }
bool port_sd_read(uint32_t lba, uint8_t *buf, uint32_t n)
{
    fseek(s_img, (long)lba * 512, SEEK_SET);
    return fread(buf, 512, n, s_img) == n;
}

static uint16_t fb[LCD_W * LCD_H];
static const char *s_out = ".";
static int s_fails;

static void frame(int x, int y, bool down)
{
    touch_t t = { down, (int16_t)x, (int16_t)y };
    for (int i = 0; i < 33; i++) { app_tick_1ms(); s_ms++; }
    app_poll();
    gui_render(fb, &t, s_ms);
}

static void idle(int n) { while (n--) frame(0, 0, false); }

static void tap(int x, int y)
{
    frame(x, y, true);
    frame(x, y, true);
    frame(x, y, false);
    frame(0, 0, false);
}

static void shot(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s.ppm", s_out, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", LCD_W, LCD_H);
    for (int i = 0; i < LCD_W * LCD_H; i++)
    {
        uint16_t c = fb[i];
        uint8_t rgb[3] = { (uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 0x3F) << 2), (uint8_t)((c & 0x1F) << 3) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

#define EXPECT(cond, msg) do { if (!(cond)) { s_fails++; printf("FAIL: %s\n", msg); } else printf("ok: %s\n", msg); } while (0)

int main(int argc, char **argv)
{
    if (argc > 1) s_out = argv[1];
    if (argc > 2) s_img = fopen(argv[2], "rb");
    srand(3);

    app_init();
    g_settings.wpm = 18; g_settings.fwpm = 12; app_apply_settings();
    text_source_scan();
    gui_init();
    idle(2);
    shot("01_home");

    /* tap the "Keyer" tile */
    tap(80, 100);
    EXPECT(screen_current() == SCR_KEYER, "tap tile Keyer opens keyer screen");
    textlog_t *kl = app_keyer_log();
    strcpy(kl->buf, "CQ CQ DE DL1ABC DL1ABC K"); kl->len = (uint16_t)strlen(kl->buf);
    s_dit = true; idle(2);
    shot("02_keyer");
    s_dit = false;
    /* + button */
    int w0 = g_settings.wpm;
    tap(420, 80);
    EXPECT(g_settings.wpm == w0 + 1, "plus button increments WPM");
    tap(20, 15);
    EXPECT(screen_current() == SCR_HOME, "back arrow returns home");

    tap(240, 100);
    EXPECT(screen_current() == SCR_MONITOR, "tap tile Monitor");
    textlog_t *ml = app_monitor_log();
    strcpy(ml->buf, "TEST DE DK2XY DK2XY PSE K   R UR RST 599 5NN QTH MUNICH"); ml->len = (uint16_t)strlen(ml->buf);
    /* feed a tone so the meter shows something */
    {
        int16_t in[2 * 256], out[2 * 256];
        for (int k = 0; k < 40; k++)
        {
            for (int i = 0; i < 256; i++) { int16_t v = (int16_t)(6000 * __builtin_sinf(2 * 3.14159f * 700.f * (float)(k * 256 + i) / 16000.f)); in[2*i] = in[2*i+1] = v; }
            app_audio_block(in, out, 256);
        }
        ml->len = (uint16_t)strlen(ml->buf);
    }
    idle(1);
    shot("03_monitor");
    tap(20, 15);

    tap(400, 100);
    EXPECT(screen_current() == SCR_TRAINER, "tap tile Trainer");
    shot("04_trainer");
    tap(240, 100);                      /* Geben zufällig */
    EXPECT(screen_current() == SCR_DRILL, "open drill");
    idle(2);
    shot("05_drill");
    trainer_on_keyed_char(trainer_state()->target[0]);
    idle(2);
    shot("06_drill_correct");
    idle(40);
    trainer_on_keyed_char('E');
    idle(2);
    shot("07_drill_wrong");
    tap(20, 15);
    tap(400, 200);                      /* Zeichenauswahl */
    EXPECT(screen_current() == SCR_LETTERS, "open letters");
    tap(20, 50);                        /* toggle A off */
    idle(1);
    shot("08_letters");
    tap(20, 15);
    EXPECT(strchr(g_settings.letters, 'A') == NULL, "letter A removed and saved");
    tap(20, 15);                        /* trainer -> home */

    tap(80, 200);
    EXPECT(screen_current() == SCR_SDTEXT, "tap tile SD-Texte");
    tap(80, 60);                        /* first file */
    tap(290, 250);                      /* play */
    idle(60);
    shot("09_sdtext");
    tap(160, 244);                      /* USB drive on */
    EXPECT(port_usb_active() && text_source_status() == TS_USB, "USB drive switched on");
    idle(1);
    shot("09b_sdtext_usb");
    tap(90, 178);                       /* Disconnect */
    EXPECT(!port_usb_active() && text_source_status() == TS_READY, "USB drive switched off, card rescanned");
    tap(20, 15);

    tap(240, 200);
    EXPECT(screen_current() == SCR_MEMORY, "tap tile Speicher");
    tap(470, 250);                      /* OK */
    tap(240, 200);
    idle(1);
    shot("10_memory");
    tap(20, 15);

    tap(400, 200);
    EXPECT(screen_current() == SCR_SETTINGS, "tap tile Einstellungen");
    shot("11_settings_speed");
    for (int t = 1; t < SETTINGS_TAB_COUNT; t++)
    {
        tap(60, 30 + t * (242 / SETTINGS_TAB_COUNT) + 20);
        char n[32]; snprintf(n, sizeof n, "%02d_settings_tab%d", 11 + t, t);
        shot(n);
    }
    /* drag the speed slider */
    tap(60, 50);
    frame(160, 75, true); frame(250, 75, true); frame(250, 75, false);
    printf("wpm after drag: %u\n", g_settings.wpm);
    EXPECT(g_settings.wpm > 5 && g_settings.wpm < 30, "slider drag changes WPM");

    printf(s_fails ? "%d FAILURES\n" : "SIM OK\n", s_fails);
    return s_fails != 0;
}
