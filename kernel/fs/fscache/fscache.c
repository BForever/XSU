#include "fscache.h"
#include <xsu/fs/fat.h>

extern struct fs_info fat_info;

uint32_t fs_victim_4k(BUF_4K* buf, uint32_t* clock_head, uint32_t size)
{
    uint32_t i;
    uint32_t index = *clock_head;

    /* sweep 1 */
    for (i = 0; i < size; i++) {
        // If reference bit is zero.
        if (((buf[*clock_head].state) & 0x01) == 0) {
            // If dirty bit is also zero, it is the victim.
            if (((buf[*clock_head].state) & 0x02) == 0) {
                index = *clock_head;
                goto fs_victim_4k_ok;
            }
        }
        // Otherwise, clean reference bit.
        else
            buf[*clock_head].state &= 0xfe;

        if ((++(*clock_head)) >= size)
            *clock_head = 0;
    }

    /* sweep 2 */
    for (i = 0; i < size; i++) {
        // Since reference bit has cleaned in sweep 1, only check dirty bit.
        if (((buf[*clock_head].state) & 0x02) == 0) {
            index = *clock_head;
            goto fs_victim_4k_ok;
        }

        if ((++(*clock_head)) >= size)
            *clock_head = 0;
    }

    // If all blocks are dirty, just use clock head.
    index = *clock_head;

fs_victim_4k_ok:

    if ((++(*clock_head)) >= size)
        *clock_head = 0;

    return index;
}

// Write current 4k buffer.
uint32_t fs_write_4k(BUF_4K* f)
{
    if ((f->cur != 0xffffffff) && (((f->state) & 0x02) != 0)) {
        if (write_block(f->buf, f->cur, fat_info.BPB.attr.sectors_per_cluster) == 1)
            goto fs_write_4k_err;

        f->state &= 0x01;
    }

    return 0;

fs_write_4k_err:
    return 1;
}

// Read 4k cluster.
uint32_t fs_read_4k(BUF_4K* f, uint32_t FirstSectorOfCluster, uint32_t* clock_head, uint32_t size)
{
    uint32_t index;
    uint32_t FirstSecWithOfs = FirstSectorOfCluster + fat_info.base_addr;
    // Try to find in buffer.
    for (index = 0; (index < size) && (f[index].cur != FirstSecWithOfs); index++) {
    }

    // If not in buffer, find victim & replace, otherwise set reference bit.
    if (index == size) {
        index = fs_victim_4k(f, clock_head, size);

        if (fs_write_4k(f + index) == 1)
            goto fs_read_4k_err;

        if (read_block(f[index].buf, FirstSecWithOfs, fat_info.BPB.attr.sectors_per_cluster) == 1)
            goto fs_read_4k_err;

        f[index].cur = FirstSecWithOfs;
        f[index].state = 1;
    } else
        f[index].state |= 0x01;

    return index;
fs_read_4k_err:
    return 0xffffffff;
}

// Clear a buffer block, used to avoid reading a new erased block from sd.
uint32_t fs_clr_4k(BUF_4K* buf, uint32_t* clock_head, uint32_t size, uint32_t cur)
{
    uint32_t index;
    uint32_t i;

    index = fs_victim_4k(buf, clock_head, size);

    if (fs_write_4k(buf + index) == 1)
        goto fs_clr_4k_err;

    for (i = 0; i < (fat_info.BPB.attr.sectors_per_cluster << 9); i++)
        buf[index].buf[i] = 0;

    buf[index].cur = cur;
    buf[index].state = 0;

    return 0;

fs_clr_4k_err:
    return 1;
}

// Find victim in 512-byte/4k buffer.
uint32_t fs_victim_512(BUF_512* buf, uint32_t* clock_head, uint32_t size)
{
    uint32_t i;
    uint32_t index = *clock_head;

    /* sweep 1 */
    for (i = 0; i < size; i++) {
        // If reference bit is zero.
        if (((buf[*clock_head].state) & 0x01) == 0) {
            // If dirty bit is also zero, it is the victim.
            if (((buf[*clock_head].state) & 0x02) == 0) {
                index = *clock_head;
                goto fs_victim_512_ok;
            }
        }
        // Otherwise, clean reference bit.
        else
            buf[*clock_head].state &= 0xfe;

        if ((++(*clock_head)) >= size)
            *clock_head = 0;
    }

    /* sweep 2 */
    for (i = 0; i < size; i++) {
        // Since reference bit has cleaned in sweep 1, only check dirty bit.
        if (((buf[*clock_head].state) & 0x02) == 0) {
            index = *clock_head;
            goto fs_victim_512_ok;
        }

        if ((++(*clock_head)) >= size)
            *clock_head = 0;
    }

    // If all blocks are dirty, just use clock head.
    index = *clock_head;

fs_victim_512_ok:

    if ((++(*clock_head)) >= size)
        *clock_head = 0;

    return index;
}

// Write current 512 buffer.
uint32_t fs_write_512(BUF_512* f)
{
    if ((f->cur != 0xffffffff) && (((f->state) & 0x02) != 0)) {
        if (write_block(f->buf, f->cur, 1) == 1)
            goto fs_write_512_err;

        f->state &= 0x01;
    }

    return 0;

fs_write_512_err:
    return 1;
}

// Read 512 sector.
uint32_t fs_read_512(BUF_512* f, uint32_t FirstSectorOfCluster, uint32_t* clock_head, uint32_t size)
{
    uint32_t index;
    uint32_t FirstSecWithOfs = FirstSectorOfCluster + fat_info.base_addr;
    // Try to find in buffer.
    for (index = 0; (index < size) && (f[index].cur != FirstSecWithOfs); index++) {
    }

    // If not in buffer, find victim & replace, otherwise set reference bit.
    if (index == size) {
        index = fs_victim_512(f, clock_head, size);

        if (fs_write_512(f + index) == 1)
            goto fs_read_512_err;

        if (read_block(f[index].buf, FirstSecWithOfs, 1) == 1)
            goto fs_read_512_err;

        f[index].cur = FirstSecWithOfs;
        f[index].state = 1;
    } else
        f[index].state |= 0x01;

    return index;
fs_read_512_err:
    return 0xffffffff;
}

uint32_t fs_clr_512(BUF_512* buf, uint32_t* clock_head, uint32_t size, uint32_t cur)
{
    uint32_t index;
    uint32_t i;

    index = fs_victim_512(buf, clock_head, size);

    if (fs_write_512(buf + index) == 1)
        goto fs_clr_512_err;

    for (i = 0; i < 512; i++)
        buf[index].buf[i] = 0;

    buf[index].cur = cur;
    buf[index].state = 0;

    return 0;

fs_clr_512_err:
    return 1;
}
