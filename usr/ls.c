#include <driver/vga.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>
#include <xsu/utils.h>

void get_month_name(int month, char* name)
{
    char* tmp = kmalloc(4);
    switch (month) {
    case 1:
        tmp = "Jan";
        break;
    case 2:
        tmp = "Feb";
        break;
    case 3:
        tmp = "Mar";
        break;
    case 4:
        tmp = "Apr";
        break;
    case 5:
        tmp = "May";
        break;
    case 6:
        tmp = "Jun";
        break;
    case 7:
        tmp = "Jul";
        break;
    case 8:
        tmp = "Aug";
        break;
    case 9:
        tmp = "Sep";
        break;
    case 10:
        tmp = "Oct";
        break;
    case 11:
        tmp = "Nov";
        break;
    case 12:
        tmp = "Dec";
        break;
    default:
        tmp = "Nam"; // Not a month
    }
    kernel_memcpy(name, tmp, 4);
    kfree(tmp);
}

int ls(char* path, char* options)
{
    char newpath[256];
    kernel_memcpy(newpath, path + 3, kernel_strlen(path) - 2);
    assert(kernel_strlen(newpath) != 0, "path handle error.");
#ifdef VFS_DEBUG
    kernel_printf("ls path: %s\n", newpath);
#endif
    FS_FAT_DIR dir;
    struct dir_entry_attr entry;
    char name[32];
    unsigned int r;

    if (fs_open_dir(&dir, newpath)) {
        kernel_printf("open dir(%s) failed : No such directory!\n", path);
        return 1;
    }

readdir:
    r = fs_read_dir(&dir, (unsigned char*)&entry);
    if (1 != r) {
        if (-1 == r) {
            kernel_printf("\n");
        } else {
            get_filename((unsigned char*)&entry, name);
            if (options) {
                if (!kernel_strcmp(options, "-a")) {
                    // Include directory entries whose names begin with a dot (.).
                    if (entry.attr == 0x10) // sub dir
                        kernel_puts(name, VGA_GREEN, VGA_BLACK);
                    else
                        kernel_printf("%s", name);
                    kernel_printf("\t");
                } else if (!kernel_strcmp(options, "-l")) {
                    // List in long format.

                    // Change data to a fix-length string.
                    // https://www.wikiwand.com/en/Design_of_the_FAT_file_system
                    // 4 * 1024 * 1024 * 1024 = 4264967396B = 4GB
                    char size[11] = "          ";
                    uint32_t file_size = entry.size;
                    itoa(file_size, size, 10);
                    uint16_t date = entry.date;
                    int month = (date & 0x01e0) >> 5;
                    int day = date & 0x001f;
                    char month_disp[4] = "   ";
                    get_month_name(month, month_disp);
                    char day_disp[3] = "  ";
                    itoa(day, day_disp, 2);
                    uint16_t time = entry.time;
                    int hour = (time & 0xf800) >> 11;
                    int minute = (time & 0x07e0) >> 5;
                    char hour_disp[3] = "  ";
                    char minute_disp[3] = "  ";
                    zitoa(hour, hour_disp, 2);
                    zitoa(minute, minute_disp, 2);

                    // Display.
                    if (name[0] != '.') {
                        if (entry.attr == 0x10) {
                            kernel_printf("%s %s %s %s:%s ", size, month_disp, day_disp, hour_disp, minute_disp);
                            kernel_puts(name, VGA_GREEN, VGA_BLACK);
                        } else {
                            kernel_printf("%s %s %s %s:%s %s", size, month_disp, day_disp, hour_disp, minute_disp, name);
                        }
                        kernel_printf("\n");
                    }
                } else if (!kernel_strcmp(options, "-al")) {
                    char size[11] = "          ";
                    uint32_t file_size = entry.size;
                    itoa(file_size, size, 10);
                    uint16_t date = entry.date;
                    int month = (date & 0x01e0) >> 5;
                    int day = date & 0x001f;
                    char month_disp[4] = "   ";
                    get_month_name(month, month_disp);
                    char day_disp[3] = "  ";
                    itoa(day, day_disp, 2);
                    uint16_t time = entry.time;
                    int hour = (time & 0xf800) >> 11;
                    int minute = (time & 0x07e0) >> 5;
                    char hour_disp[3] = "  ";
                    char minute_disp[3] = "  ";
                    zitoa(hour, hour_disp, 2);
                    zitoa(minute, minute_disp, 2);

                    if (entry.attr == 0x10) {
                        kernel_printf("%s %s %s %s:%s ", size, month_disp, day_disp, hour_disp, minute_disp);
                        kernel_puts(name, VGA_GREEN, VGA_BLACK);
                    } else {
                        kernel_printf("%s %s %s %s:%s %s", size, month_disp, day_disp, hour_disp, minute_disp, name);
                    }
                    kernel_printf("\n");
                } else {
                    if (name[0] != '.') {
                        if (entry.attr == 0x10) // sub dir
                            kernel_puts(name, VGA_GREEN, VGA_BLACK);
                        else
                            kernel_printf("%s", name);
                        kernel_printf("\t");
                    }
                }
            } else {
                if (name[0] != '.') {
                    if (entry.attr == 0x10) // sub dir
                        kernel_puts(name, VGA_GREEN, VGA_BLACK);
                    else
                        kernel_printf("%s", name);
                    kernel_printf("\t");
                }
            }
            goto readdir;
        }
    } else {
        return 1;
    }
    return 0;
}
