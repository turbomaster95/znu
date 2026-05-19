#include <ff.h>
#include <diskio.h>    
#include <disk.h>      
#include <ahci.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    BYTE pd;    
    BYTE pt;    
} FATFS_PARTITION_MAP;


const FATFS_PARTITION_MAP VolToPart[FF_VOLUMES] = {
    {0, 0}  
};

#define DEV_AHCI0   0

DSTATUS disk_status (BYTE pdrv) {
    if (pdrv != DEV_AHCI0) {
        return STA_NOINIT;
    }

    disk_t* d = disk_get_by_name("ahci0");
    if (!d || !d->present) {
        return STA_NODISK;
    }

    return 0; 
}

DSTATUS disk_initialize (
    BYTE pdrv       /* Physical drive number to identify the drive */
)
{
    if (pdrv != DEV_AHCI0) {
        return STA_NOINIT;
    }

    disk_t* d = disk_get_by_name("ahci0");
    if (!d || !d->present) {
        return STA_NODISK;
    }

    return 0; 
}

DRESULT disk_read (
    BYTE pdrv,      /* Physical drive number to identify the drive */
    BYTE* buff,     /* Data buffer to store read data */
    LBA_t sector,   /* Start sector in LBA */
    UINT count      /* Number of sectors to read */
)
{
    if (pdrv != DEV_AHCI0 || !count) {
        return RES_PARERR;
    }

    disk_t* d = disk_get_by_name("ahci0");
    if (!d || !d->present) {
        return RES_NOTRDY;
    }

    void* dma_phys = palloc_zero();
    if (!dma_phys) {
        return RES_ERROR;
    }

    uint8_t* bounce_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_phys);

    for (UINT i = 0; i < count; i++) {
        if (!disk_read_sector(d, sector + i, bounce_buf)) {
            pfree(dma_phys);
            return RES_ERROR;
        }

        memcpy(buff + (i * 512), bounce_buf, 512);

        if (sector + i == 0) {
            debugln("[diskio] Sector 0 Signature: 0x%02X 0x%02X", 
                    buff[(i * 512) + 510], buff[(i * 512) + 511]);
        }
    }

    pfree(dma_phys);
    return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT disk_write (
    BYTE pdrv,          
    const BYTE* buff,   
    LBA_t sector,       
    UINT count          
)
{
    if (pdrv != DEV_AHCI0 || !count) {
        return RES_PARERR;
    }

    disk_t* d = disk_get_by_name("ahci0");
    if (!d || !d->present) {
        return RES_NOTRDY;
    }

    void* dma_phys = palloc_zero();
    if (!dma_phys) return RES_ERROR;
    uint8_t* bounce_buf = (uint8_t*)phys_to_virt((uintptr_t)dma_phys);

    for (UINT i = 0; i < count; i++) {
        /* Copy data out from FatFs BSS into the DMA-safe frame */
        memcpy(bounce_buf, buff + (i * 512), 512);

        if (!disk_write_sector(d, sector + i, bounce_buf)) {
            pfree(dma_phys);
            return RES_ERROR;
        }
    }

    pfree(dma_phys);
    return RES_OK;
}

#endif

DRESULT disk_ioctl (
    BYTE pdrv,      /* Physical drive number (0..) */
    BYTE cmd,       /* Control code */
    void* buff      /* Buffer to send/receive control data */
)
{
    if (pdrv != DEV_AHCI0) {
        return RES_PARERR;
    }

    disk_t* d = disk_get_by_name("ahci0");
    if (!d || !d->present) {
        return RES_NOTRDY;
    }

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            if (!buff) return RES_PARERR;
            *(LBA_t*)buff = 204800; 
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (!buff) return RES_PARERR;
            *(WORD*)buff = d->sector_size; 
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (!buff) return RES_PARERR;
            *(DWORD*)buff = 1; 
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

uint32_t get_fattime(void) {
    uint32_t year   = 2026 - 1980;
    uint32_t month  = 5;
    uint32_t day    = 19;
    uint32_t hour   = 12;
    uint32_t minute = 0;
    uint32_t second = 0;

    return (year << 25) | (month << 21) | (day << 16) |
           (hour << 11) | (minute << 5)  | (second >> 1);
}
