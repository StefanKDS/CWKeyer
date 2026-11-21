/**
 * @file    text_source.c
 */
#include "text_source.h"
#include "fat_ro.h"
#include "morse.h"
#include "port.h"
#include <string.h>

typedef struct
{
    char     name[FAT_NAME_MAX];
    uint32_t size;
    uint32_t cluster;
} text_file_t;

static fat_fs_t              s_fs;
static text_file_t           s_files[TEXT_MAX_FILES];
static int                   s_count;
static text_source_status_t  s_status = TS_NO_CARD;
static char                  s_text[TEXT_BUF_SIZE + 1];
static char                  s_raw[TEXT_BUF_SIZE];
static uint32_t              s_len;
static int                   s_loaded = -1;

static bool ends_with_txt(const char *n)
{
    size_t l = strlen(n);
    if (l < 4) return false;
    const char *e = n + l - 4;
    return e[0] == '.' && (e[1] | 0x20) == 't' && (e[2] | 0x20) == 'x' && (e[3] | 0x20) == 't';
}

static bool list_cb(const fat_dirent_t *e, void *ctx)
{
    (void)ctx;
    if (e->is_dir || !ends_with_txt(e->name) || s_count >= TEXT_MAX_FILES)
        return s_count < TEXT_MAX_FILES;
    text_file_t *f = &s_files[s_count++];
    strncpy(f->name, e->name, sizeof(f->name) - 1);
    f->name[sizeof(f->name) - 1] = '\0';
    f->size    = e->size;
    f->cluster = e->cluster;
    return true;
}

static int cmp_name(const text_file_t *a, const text_file_t *b)
{
    return strcmp(a->name, b->name);
}

text_source_status_t text_source_scan(void)
{
    s_count  = 0;
    s_loaded = -1;
    s_len    = 0;

    if (!port_sd_present())
        return s_status = TS_NO_CARD;
    if (!port_sd_init() || fat_mount(&s_fs, port_sd_read) != FAT_OK)
        return s_status = TS_ERROR;

    (void)fat_list_dir(&s_fs, 0, list_cb, NULL);

    fat_dirent_t cw;
    if (fat_find(&s_fs, 0, "CW", &cw) == FAT_OK && cw.is_dir)
        (void)fat_list_dir(&s_fs, cw.cluster, list_cb, NULL);

    /* simple insertion sort by name */
    for (int i = 1; i < s_count; i++)
    {
        text_file_t t = s_files[i];
        int j = i - 1;
        while (j >= 0 && cmp_name(&s_files[j], &t) > 0)
        {
            s_files[j + 1] = s_files[j];
            j--;
        }
        s_files[j + 1] = t;
    }
    return s_status = TS_READY;
}

text_source_status_t text_source_status(void) { return s_status; }
int         text_source_count(void)          { return s_count; }
const char *text_source_name(int i)          { return (i >= 0 && i < s_count) ? s_files[i].name : ""; }
uint32_t    text_source_size(int i)          { return (i >= 0 && i < s_count) ? s_files[i].size : 0; }
const char *text_source_text(void)           { return s_text; }
uint32_t    text_source_len(void)            { return s_len; }
int         text_source_loaded_index(void)   { return s_loaded; }

bool text_source_load(int idx)
{
    if (idx < 0 || idx >= s_count)
        return false;

    fat_dirent_t e;
    memset(&e, 0, sizeof(e));
    e.size    = s_files[idx].size;
    e.cluster = s_files[idx].cluster;

    int32_t n = fat_read_file(&s_fs, &e, (uint8_t *)s_raw, TEXT_BUF_SIZE);
    if (n < 0)
    {
        s_len = 0;
        s_loaded = -1;
        return false;
    }
    s_len = text_normalize(s_raw, (uint32_t)n, s_text, TEXT_BUF_SIZE);
    s_text[s_len] = '\0';
    s_loaded = idx;
    return true;
}

uint32_t text_normalize(const char *buf, uint32_t len, char *out, uint32_t max)
{
    uint32_t w = 0;
    bool last_space = true;

    for (uint32_t r = 0; r < len; r++)
    {
        uint8_t c = (uint8_t)buf[r];
        const char *rep = NULL;
        char one[2] = { 0, 0 };

        if (c == 0xC3 && r + 1 < len)            /* UTF-8 umlauts */
        {
            uint8_t d = (uint8_t)buf[++r];
            switch (d)
            {
            case 0x84: case 0xA4: rep = "AE"; break;
            case 0x96: case 0xB6: rep = "OE"; break;
            case 0x9C: case 0xBC: rep = "UE"; break;
            case 0x9F:            rep = "SS"; break;
            default:              rep = NULL; break;
            }
        }
        else if (c == 0xC4 || c == 0xE4) rep = "AE";   /* Latin-1 */
        else if (c == 0xD6 || c == 0xF6) rep = "OE";
        else if (c == 0xDC || c == 0xFC) rep = "UE";
        else if (c == 0xDF)              rep = "SS";
        else if (c == '\r' || c == '\n' || c == '\t' || c == ' ')
        {
            one[0] = ' ';
            rep = one;
        }
        else if (c < 0x80)
        {
            char u = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
            if (morse_encode(u) != NULL)
            {
                one[0] = u;
                rep = one;
            }
        }

        if (rep == NULL)
            continue;
        for (; *rep; rep++)
        {
            if (*rep == ' ')
            {
                if (last_space) continue;
                last_space = true;
            }
            else
            {
                last_space = false;
            }
            if (w < max)
                out[w++] = *rep;
        }
    }
    while (w > 0 && out[w - 1] == ' ')
        w--;
    return w;
}
