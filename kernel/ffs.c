#include <ff.h>      /* Path to standard FatFs declarations */
#include <diskio.h>  /* Path to FatFs disk io definitions */
#include <disk.h>                    /* Your kernel/disk.h layout */
#include <string.h>

/* * Helper function to map FatFs drive numbers (0, 1, 2...) 
 * directly to your kernel's internal disk array tracking.
 */
static disk_t* get_kernel_disk(BYTE pdrv) {
    extern disk_t g_disks[];
    extern int g_disk_count;

    if (pdrv >= (BYTE)g_disk_count) {
        return NULL;
    }
    return &g_disks[pdrv];
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(
    BYTE pdrv        /* Physical drive number to identify the drive */
) {
    disk_t* d = get_kernel_disk(pdrv);
    if (!d || !d->present) {
        return STA_NOINIT;
    }
    return 0; /* Drive is initialized and present */
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(
    BYTE pdrv        /* Physical drive number to identify the drive */
) {
    disk_t* d = get_kernel_disk(pdrv);
    if (!d || !d->present) {
        return STA_NOINIT;
    }
    /* * Your kernel manages AHCI controller layout via disk_init(), 
     * so if the disk exists in g_disks, it's already spun up.
     */
    return 0; 
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(
    BYTE pdrv,       /* Physical drive number to identify the drive */
    BYTE* buff,      /* Data buffer to store read data */
    LBA_t sector,    /* Start sector address (LBA) */
    UINT count       /* Number of sectors to read */
) {
    disk_t* d = get_kernel_disk(pdrv);
    if (!d || !d->present) return RES_PARERR;

    /* Loop through and read each sector sequentially */
    for (UINT i = 0; i < count; i++) {
        uint64_t current_lba = (uint64_t)sector + i;
        void* current_buffer = (void*)(buff + (i * d->sector_size));

        if (!disk_read_sector(d, current_lba, current_buffer)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write(
    BYTE pdrv,       /* Physical drive number to identify the drive */
    const BYTE* buff,/* Data to be written */
    LBA_t sector,    /* Start sector address (LBA) */
    UINT count       /* Number of sectors to write */
) {
    disk_t* d = get_kernel_disk(pdrv);
    if (!d || !d->present) return RES_PARERR;

    /* Loop through and write each sector sequentially */
    for (UINT i = 0; i < count; i++) {
        uint64_t current_lba = (uint64_t)sector + i;
        /* Cast away const safely as the AHCI layer consumes the direct address pointer */
        void* current_buffer = (void*)(buff + (i * d->sector_size));

        if (!disk_write_sector(d, current_lba, current_buffer)) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Controls                                                */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(
    BYTE pdrv,       /* Physical drive number */
    BYTE cmd,        /* Control code */
    void* buff       /* Buffer to send/receive control data */
) {
    disk_t* d = get_kernel_disk(pdrv);
    if (!d || !d->present) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            /* * Your ahci_read_sector/write_sector functions are fully synchronous.
             * They issue commands via PxCI and spin execution until wait_slot_done completes,
             * meaning data is safely committed instantly when the function exits.
             */
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = (WORD)d->sector_size; /* 512 */
            return RES_OK;

        case GET_BLOCK_SIZE:
            /* Erase block size in units of sectors. 1 means non-flash layout or unknown. */
            *(DWORD*)buff = 1; 
            return RES_OK;

        case GET_SECTOR_COUNT:
            /* * If your disk_t structure doesn't track total drive sectors yet, 
             * hardcode your test volume capacity size (e.g., 40MB image = 81920 sectors).
             */
            *(LBA_t*)buff = 204800; 
            return RES_OK;
    }

    return RES_PARERR;
}

/*-----------------------------------------------------------------------*/
/* Time Stamp Generation Function                                        */
/*-----------------------------------------------------------------------*/
DWORD get_fattime(void) {
    /* * FatFs packs date/time configurations directly into a single bitfield DWORD:
     * Bits 31:25 -> Year origin from 1980 (e.g. 2026 - 1980 = 46)
     * Bits 24:21 -> Month (1-12)
     * Bits 20:16 -> Day (1-31)
     * Bits 15:11 -> Hour (0-23)
     * Bits 10:5  -> Minute (0-59)
     * Bits 4:0   -> Second / 2 (0-29)
     */
    return ((DWORD)(2026 - 1980) << 25) 
         | ((DWORD)5 << 21) 
         | ((DWORD)17 << 16) 
         | ((DWORD)12 << 11) 
         | ((DWORD)0 << 5) 
         | ((DWORD)0 >> 1);
}
