#include "fscache.h"
#include <xsu/fs/fat.h>

extern struct fs_info fat_info;

u32 fs_victim_4k(BUF_4K* buf, u32* clock_head, u32 size)
{
    u32 i;
    u32 index = *clock_head;

    // sweep 1.
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

    // sweep 2.
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
u32 fs_write_4k(BUF_4K* f)
{
    // If the dirty bit is 1.
    if ((f->cur != 0xffffffff) && (((f->state) & 0x02) != 0)) {
        if (write_block(f->buf, f->cur, fat_info.BPB.attr.sectors_per_cluster) == 1)
            goto fs_write_4k_error;
        // Reset the dirty bit and set the reference bit.
        f->state &= 0x01;
    }
    return 0;

fs_write_4k_error:
    return 1;
}

// Read 4k cluster.
u32 fs_read_4k(BUF_4K* f, u32 FirstSectorOfCluster, u32* clock_head, u32 size)
{
    u32 index;
    u32 FirstSecWithOfs = FirstSectorOfCluster + fat_info.base_addr;
    // Try to find in buffer.
    for (index = 0; (index < size) && (f[index].cur != FirstSecWithOfs); index++) {
    }

    // If not in buffer, find victim & replace, otherwise set reference bit.
    if (index == size) {
        index = fs_victim_4k(f, clock_head, size);

        if (fs_write_4k(f + index) == 1)
            goto fs_read_4k_error;

        if (read_block(f[index].buf, FirstSecWithOfs, fat_info.BPB.attr.sectors_per_cluster) == 1)
            goto fs_read_4k_error;

        f[index].cur = FirstSecWithOfs;
        f[index].state = 1;
    } else
        f[index].state |= 0x01;

    return index;

fs_read_4k_error:
    return 0xffffffff;
}

// Clear a buffer block, used to avoid reading a new erased block from sd.
u32 fs_clear_4k(BUF_4K* buf, u32* clock_head, u32 size, u32 cur)
{
    u32 index;
    u32 i;

    index = fs_victim_4k(buf, clock_head, size);

    if (fs_write_4k(buf + index) == 1)
        goto fs_clear_4k_error;

    for (i = 0; i < (fat_info.BPB.attr.sectors_per_cluster << 9); i++)
        buf[index].buf[i] = 0;

    buf[index].cur = cur;
    buf[index].state = 0;

    return 0;

fs_clear_4k_error:
    return 1;
}

// Find victim in 512-byte buffer.
u32 fs_victim_512(BUF_512* buf, u32* clock_head, u32 size)
{
    u32 i;
    u32 index = *clock_head;

    // sweep 1.
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

    // sweep 2.
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
u32 fs_write_512(BUF_512* f)
{
    // If the dirty bit is 1.
    if ((f->cur != 0xffffffff) && (((f->state) & 0x02) != 0)) {
        if (write_block(f->buf, f->cur, 1) == 1)
            goto fs_write_512_error;
        // Reset the dirty bit and set the reference bit.
        f->state &= 0x01;
    }
    return 0;

fs_write_512_error:
    return 1;
}

// Read 512 sector.
u32 fs_read_512(BUF_512* f, u32 FirstSectorOfCluster, u32* clock_head, u32 size)
{
    u32 index;
    u32 FirstSecWithOfs = FirstSectorOfCluster + fat_info.base_addr;
    // Try to find in buffer.
    for (index = 0; (index < size) && (f[index].cur != FirstSecWithOfs); index++) {
    }

    // If not in buffer, find victim & replace, otherwise set reference bit.
    if (index == size) {
        index = fs_victim_512(f, clock_head, size);

        if (fs_write_512(f + index) == 1)
            goto fs_read_512_error;

        if (read_block(f[index].buf, FirstSecWithOfs, 1) == 1)
            goto fs_read_512_error;

        f[index].cur = FirstSecWithOfs;
        f[index].state = 1;
    } else
        f[index].state |= 0x01;

    return index;

fs_read_512_error:
    return 0xffffffff;
}

u32 fs_clear_512(BUF_512* buf, u32* clock_head, u32 size, u32 cur)
{
    u32 index;
    u32 i;

    index = fs_victim_512(buf, clock_head, size);

    if (fs_write_512(buf + index) == 1)
        goto fs_clear_512_error;

    for (i = 0; i < 512; i++)
        buf[index].buf[i] = 0;

    buf[index].cur = cur;
    buf[index].state = 0;

    return 0;

fs_clear_512_error:
    return 1;
}
