#ifndef _XSU_FSCACHE_H
#define _XSU_FSCACHE_H

#include <xsu/types.h>

/* 4k byte buffer */
typedef struct buf_4k {
    unsigned char buf[4096];
    unsigned long cur;
    unsigned long state;
} BUF_4K;

/* 512 byte buffer */
typedef struct buf_512 {
    unsigned char buf[512];
    unsigned long cur;
    unsigned long state;
} BUF_512;

uint32_t fs_victim_4k(BUF_4K* buf, uint32_t* clock_head, uint32_t size);
uint32_t fs_write_4k(BUF_4K* f);
uint32_t fs_read_4k(BUF_4K* f, uint32_t FirstSectorOfCluster, uint32_t* clock_head, uint32_t size);
uint32_t fs_clr_4k(BUF_4K* buf, uint32_t* clock_head, uint32_t size, uint32_t cur);

uint32_t fs_victim_512(BUF_512* buf, uint32_t* clock_head, uint32_t size);
uint32_t fs_write_512(BUF_512* f);
uint32_t fs_read_512(BUF_512* f, uint32_t FirstSectorOfCluster, uint32_t* clock_head, uint32_t size);
uint32_t fs_clr_512(BUF_512* buf, uint32_t* clock_head, uint32_t size, uint32_t cur);

#endif
