/**
 * @file    fat_ro.c
 * @brief   Read-only FAT16/FAT32 implementation.
 */
#include "fat_ro.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool load(fat_fs_t *fs, uint32_t lba)
{
    if (fs->cached_lba == lba)
        return true;
    if (!fs->read(lba, fs->sec, 1))
    {
        fs->cached_lba = 0xFFFFFFFFu;
        return false;
    }
    fs->cached_lba = lba;
    return true;
}

static bool is_boot_sector(const uint8_t *s)
{
    return (s[0] == 0xEB || s[0] == 0xE9) && rd16(&s[11]) == 512 &&
           s[13] != 0 && s[16] != 0;
}

int fat_mount(fat_fs_t *fs, fat_read_fn read)
{
    memset(fs, 0, sizeof(*fs));
    fs->read       = read;
    fs->cached_lba = 0xFFFFFFFFu;

    if (!load(fs, 0))
        return FAT_ERR_IO;
    if (rd16(&fs->sec[510]) != 0xAA55)
        return FAT_ERR_NOFS;

    uint32_t vol = 0;
    if (!is_boot_sector(fs->sec))
    {
        /* MBR: use the first partition with a FAT type */
        for (int i = 0; i < 4; i++)
        {
            const uint8_t *pe = &fs->sec[0x1BE + 16 * i];
            uint8_t t = pe[4];
            if (t == 0x04 || t == 0x06 || t == 0x0E || t == 0x0B || t == 0x0C)
            {
                vol = rd32(&pe[8]);
                break;
            }
        }
        if (vol == 0 || !load(fs, vol))
            return FAT_ERR_NOFS;
        if (!is_boot_sector(fs->sec))
            return FAT_ERR_NOFS;
    }

    const uint8_t *b = fs->sec;
    uint32_t rsvd    = rd16(&b[14]);
    uint32_t nfats   = b[16];
    uint32_t rootent = rd16(&b[17]);
    uint32_t tot     = rd16(&b[19]) ? rd16(&b[19]) : rd32(&b[32]);
    uint32_t fatsz   = rd16(&b[22]) ? rd16(&b[22]) : rd32(&b[36]);

    fs->sec_per_clus = b[13];
    fs->fat_start    = vol + rsvd;
    fs->root_start   = fs->fat_start + nfats * fatsz;
    fs->root_sectors = (rootent * 32u + 511u) / 512u;
    fs->data_start   = fs->root_start + fs->root_sectors;
    fs->cluster_count = (tot - (fs->data_start - vol)) / fs->sec_per_clus;

    if (fs->cluster_count < 4085)
        return FAT_ERR_NOFS;                      /* FAT12 not supported */
    if (fs->cluster_count < 65525)
    {
        fs->type = 16;
    }
    else
    {
        fs->type = 32;
        fs->root_cluster = rd32(&b[44]);
    }
    return FAT_OK;
}

static uint32_t next_cluster(fat_fs_t *fs, uint32_t c)
{
    uint32_t off = (fs->type == 16) ? c * 2u : c * 4u;
    if (!load(fs, fs->fat_start + off / 512u))
        return 0;
    if (fs->type == 16)
    {
        uint32_t v = rd16(&fs->sec[off % 512u]);
        return (v >= 0xFFF8u) ? 0 : v;
    }
    uint32_t v = rd32(&fs->sec[off % 512u]) & 0x0FFFFFFFu;
    return (v >= 0x0FFFFFF8u) ? 0 : v;
}

static uint32_t cluster_lba(const fat_fs_t *fs, uint32_t c)
{
    return fs->data_start + (c - 2u) * fs->sec_per_clus;
}

static uint8_t lfn_checksum(const uint8_t *short_name)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1u) << 7) + (sum >> 1) + short_name[i]);
    return sum;
}

typedef struct
{
    char    lfn[FAT_NAME_MAX];
    uint8_t lfn_sum;
    bool    lfn_valid;
} lfn_state_t;

static void lfn_put(lfn_state_t *l, const uint8_t *e)
{
    static const uint8_t pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    uint8_t ord = e[0] & 0x1Fu;
    if (e[0] & 0x40u)
    {
        memset(l->lfn, 0, sizeof(l->lfn));
        l->lfn_valid = true;
        l->lfn_sum   = e[13];
    }
    if (ord == 0 || e[13] != l->lfn_sum)
    {
        l->lfn_valid = false;
        return;
    }
    for (int i = 0; i < 13; i++)
    {
        uint32_t idx = (uint32_t)(ord - 1u) * 13u + (uint32_t)i;
        uint16_t ch  = rd16(&e[pos[i]]);
        if (ch == 0x0000 || ch == 0xFFFF)
            break;
        if (idx < FAT_NAME_MAX - 1)
            l->lfn[idx] = (ch < 0x80) ? (char)ch : '_';
    }
}

static void short_name(const uint8_t *e, char *out)
{
    int n = 0;
    for (int i = 0; i < 8 && e[i] != ' '; i++) out[n++] = (char)e[i];
    if (e[8] != ' ')
    {
        out[n++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++) out[n++] = (char)e[i];
    }
    out[n] = '\0';
}

/* returns 1 = stop requested, 0 = end of directory, <0 = error */
static int scan_sector(fat_fs_t *fs, uint32_t lba, lfn_state_t *l, fat_dir_cb cb, void *ctx)
{
    if (!load(fs, lba))
        return FAT_ERR_IO;

    for (int i = 0; i < 16; i++)
    {
        uint8_t e[32];
        memcpy(e, &fs->sec[i * 32], 32);   /* callback may reload the cache */

        if (e[0] == 0x00)
            return 2;                     /* end of directory */
        if (e[0] == 0xE5)
        {
            l->lfn_valid = false;
            continue;
        }
        if (e[11] == 0x0F)
        {
            lfn_put(l, e);
            continue;
        }
        if (e[11] & 0x08)                 /* volume label */
        {
            l->lfn_valid = false;
            continue;
        }

        fat_dirent_t d;
        if (l->lfn_valid && l->lfn_sum == lfn_checksum(e))
            strcpy(d.name, l->lfn);
        else
            short_name(e, d.name);
        l->lfn_valid = false;

        d.is_dir  = (e[11] & 0x10) != 0;
        d.size    = rd32(&e[28]);
        d.cluster = ((uint32_t)rd16(&e[20]) << 16) | rd16(&e[26]);

        if (d.name[0] == '.')
            continue;                     /* "." and ".." */
        if (!cb(&d, ctx))
            return 1;
        if (!load(fs, lba))
            return FAT_ERR_IO;
    }
    return 0;
}

int fat_list_dir(fat_fs_t *fs, uint32_t dir_cluster, fat_dir_cb cb, void *ctx)
{
    lfn_state_t l = { .lfn_valid = false };
    int r;

    if (dir_cluster == 0 && fs->type == 16)
    {
        for (uint32_t s = 0; s < fs->root_sectors; s++)
        {
            r = scan_sector(fs, fs->root_start + s, &l, cb, ctx);
            if (r != 0) return r < 0 ? r : FAT_OK;
        }
        return FAT_OK;
    }

    uint32_t c = dir_cluster ? dir_cluster : fs->root_cluster;
    uint32_t guard = 0;
    while (c >= 2 && c < fs->cluster_count + 2u && guard++ < 65536u)
    {
        for (uint32_t s = 0; s < fs->sec_per_clus; s++)
        {
            r = scan_sector(fs, cluster_lba(fs, c) + s, &l, cb, ctx);
            if (r != 0) return r < 0 ? r : FAT_OK;
        }
        c = next_cluster(fs, c);
    }
    return FAT_OK;
}

typedef struct
{
    const char   *name;
    fat_dirent_t *out;
    bool          found;
} find_ctx_t;

static bool name_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        char x = *a++, y = *b++;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y) return false;
    }
    return *a == *b;
}

static bool find_cb(const fat_dirent_t *e, void *ctx)
{
    find_ctx_t *f = (find_ctx_t *)ctx;
    if (name_eq(e->name, f->name))
    {
        *f->out  = *e;
        f->found = true;
        return false;
    }
    return true;
}

int fat_find(fat_fs_t *fs, uint32_t dir_cluster, const char *name, fat_dirent_t *out)
{
    find_ctx_t f = { name, out, false };
    int r = fat_list_dir(fs, dir_cluster, find_cb, &f);
    if (r < 0) return r;
    return f.found ? FAT_OK : FAT_ERR_NOTFOUND;
}

int32_t fat_read_file(fat_fs_t *fs, const fat_dirent_t *e, uint8_t *buf, uint32_t max)
{
    uint32_t remaining = e->size < max ? e->size : max;
    uint32_t done = 0;
    uint32_t c = e->cluster;

    while (remaining > 0 && c >= 2 && c < fs->cluster_count + 2u)
    {
        for (uint32_t s = 0; s < fs->sec_per_clus && remaining > 0; s++)
        {
            if (!load(fs, cluster_lba(fs, c) + s))
                return FAT_ERR_IO;
            uint32_t n = remaining < 512u ? remaining : 512u;
            memcpy(&buf[done], fs->sec, n);
            done      += n;
            remaining -= n;
        }
        if (remaining > 0)
            c = next_cluster(fs, c);
    }
    return (int32_t)done;
}
