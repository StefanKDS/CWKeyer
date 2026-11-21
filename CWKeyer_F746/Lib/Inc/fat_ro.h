/**
 * @file    fat_ro.h
 * @brief   Minimal read-only FAT16/FAT32 reader with long file names.
 *
 * Small and dependency free (no FatFs needed). Only what the keyer needs:
 * list a directory and read a file sequentially.
 */
#ifndef FAT_RO_H
#define FAT_RO_H

#include <stdbool.h>
#include <stdint.h>

#define FAT_NAME_MAX  48

typedef bool (*fat_read_fn)(uint32_t lba, uint8_t *buf, uint32_t count);

typedef struct
{
    fat_read_fn read;
    uint8_t     type;            /**< 16 or 32                       */
    uint8_t     sec_per_clus;
    uint32_t    fat_start;       /**< LBA of first FAT               */
    uint32_t    root_start;      /**< FAT16: LBA of root directory   */
    uint32_t    root_sectors;    /**< FAT16: sectors of root dir     */
    uint32_t    root_cluster;    /**< FAT32: first root cluster      */
    uint32_t    data_start;      /**< LBA of cluster 2               */
    uint32_t    cluster_count;
    uint32_t    cached_lba;
    uint8_t     sec[512] __attribute__((aligned(4)));
} fat_fs_t;

typedef struct
{
    char     name[FAT_NAME_MAX];
    uint32_t size;
    uint32_t cluster;            /**< first cluster                   */
    bool     is_dir;
} fat_dirent_t;

typedef enum
{
    FAT_OK = 0,
    FAT_ERR_IO = -1,
    FAT_ERR_NOFS = -2,
    FAT_ERR_NOTFOUND = -3,
} fat_result_t;

/** callback for fat_list_dir(), return false to stop */
typedef bool (*fat_dir_cb)(const fat_dirent_t *e, void *ctx);

int     fat_mount(fat_fs_t *fs, fat_read_fn read);
/** dir_cluster = 0 -> root directory */
int     fat_list_dir(fat_fs_t *fs, uint32_t dir_cluster, fat_dir_cb cb, void *ctx);
int     fat_find(fat_fs_t *fs, uint32_t dir_cluster, const char *name, fat_dirent_t *out);
/** reads up to max bytes from the beginning of the file, returns bytes or <0 */
int32_t fat_read_file(fat_fs_t *fs, const fat_dirent_t *e, uint8_t *buf, uint32_t max);

#endif
