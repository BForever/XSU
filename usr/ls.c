#include <driver/vga.h>
#include <xsu/fs/fat.h>
#include <xsu/fs/vfs.h>
#include <xsu/fs/vnode.h>
#include <xsu/slab.h>
#include <xsu/utils.h>

void handle_path(char* path, char* newpath)
{
    struct vnode* vn;
    char* name;
    int result;

    name = kmalloc(kernel_strlen(path) + 1);
    result = vfs_lookparent(path, &vn, name, sizeof(name));
    if (result) {
        return;
    }
    kernel_memcpy(newpath, name, kernel_strlen(name) + 1);
    kfree(name);
}

void get_month_name(int month, char* name)
{
    switch (month) {
    case 1:
        name = "Jan";
        break;
    case 2:
        name = "Feb";
        break;
    case 3:
        name = "Mar";
        break;
    case 4:
        name = "Apr";
        break;
    case 5:
        name = "May";
        break;
    case 6:
        name = "Jun";
        break;
    case 7:
        name = "Jul";
        break;
    case 8:
        name = "Aug";
        break;
    case 9:
        name = "Sep";
        break;
    case 10:
        name = "Oct";
        break;
    case 11:
        name = "Nov";
        break;
    case 12:
        name = "Dec";
        break;
    default:
        name = "Nam"; // Not a month
    }
}

int ls(char* path, char* options)
{
    char* newpath;
    newpath = kmalloc(kernel_strlen(path) + 1);
    handle_path(path, newpath);
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
                uint32_t file_size = get_entry_filesize(entry);
                itoa(file_size, size, 10);
                uint16_t date = entry.date;
                int month = (date & 0x00f0) >> 4;
                int day = date & 0x000f;
                char month_disp[4] = "   ";
                get_month_name(month, month_name);
                char day_disp[3] = "  ";
                itoa(day, day_disp, 2);
                uint16_t time = entry.time;
                int hour = (time & 0xf800) >> 10;
                int minute = (time & 0x07e0) >> 5;
                char hour_disp[3] = "  ";
                char minute_disp[3] = "  ";
                itoa(hour, hour_disp, 2);
                itoa(minute, minute_disp, 2);

                // Display.
                if (name[0] != '.') {
                    if (entry.attr = 0x10) {
                        kernel_printf("%s %s %s %s:%s ", size, month_disp, day_disp, hour_disp, minute_disp);
                        kernel_puts(name, VGA_GREEN, VGA_BLACK);
                    } else {
                        kernel_printf("%s %s %s %s:%s %s", size, month_disp, day_disp, hour_disp, minute_disp, name);
                    }
                    kernel_printf("\n");
                }
            } else if (!kernel_strcmp(options, "-al")) {
                char size[11] = "          ";
                uint32_t file_size = get_entry_filesize(entry);
                itoa(file_size, size, 10);
                uint16_t date = entry.date;
                int month = (date & 0x00f0) >> 4;
                int day = date & 0x000f;
                char month_disp[4] = "   ";
                get_month_name(month, month_name);
                char day_disp[3] = "  ";
                itoa(day, day_disp, 2);
                uint16_t time = entry.time;
                int hour = (time & 0xf800) >> 10;
                int minute = (time & 0x07e0) >> 5;
                char hour_disp[3] = "  ";
                char minute_disp[3] = "  ";
                itoa(hour, hour_disp, 2);
                itoa(minute, minute_disp, 2);

                if (entry.attr = 0x10) {
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
            goto readdir;
        }
    } else {
        kfree(newpath);
        return 1;
    }
    kfree(newpath);
    return 0;
}
