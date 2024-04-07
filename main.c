#include <stdio.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include <malloc.h>
#include <stdint.h>
#include <sys/mman.h>

#define SQUARE_SIDE 50
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128
#define TOP_AND_BOTTOM_GAP (SCREEN_HEIGHT - SQUARE_SIDE) / 2
#define PAGE_NR 8

static void get_frame(size_t n, uint8_t **buf){
    size_t x, y;
    for (y = 0; y < SCREEN_HEIGHT; ++y){
        for (x = 0; x < SCREEN_WIDTH; ++x){
            // vertical sides
            if ((x == n || x == n + SQUARE_SIDE) && y > TOP_AND_BOTTOM_GAP
                && y < TOP_AND_BOTTOM_GAP + SQUARE_SIDE){
                (*buf)[(y / PAGE_NR) * SCREEN_WIDTH + x] |= (0x1 << (y % PAGE_NR));
            // horizontal sides
            } else if (x > n && x < n + SQUARE_SIDE &&
                       (y == TOP_AND_BOTTOM_GAP || y == TOP_AND_BOTTOM_GAP + SQUARE_SIDE)){
                (*buf)[(y / PAGE_NR) * SCREEN_WIDTH + x] |= (0x1 << (y % PAGE_NR));
            } else {
                (*buf)[(y / PAGE_NR) * SCREEN_WIDTH + x] &= ~(0x1 << (y % PAGE_NR));
            }

        }
    }
}

int main(int argc, char* argv[])
{
    int ret, i;
    int fd = open("/dev/fb1", O_RDWR);
    uint8_t *buf = malloc(PAGE_NR * SCREEN_WIDTH);
    memset(buf, 0, SCREEN_WIDTH * PAGE_NR);
    struct fb_var_screeninfo *fvs;
    if (fd < 0){
        fprintf(stderr, "Could not open framebuffer! Error: %d - %s\n",
            errno, strerror(errno));
        goto out;
    }

    fvs = malloc(sizeof(struct fb_var_screeninfo));

    ret = ioctl(fd, FBIOGET_VSCREENINFO, fvs);
    if (ret < 0){
        fprintf(stderr, "Could not send ioctl 1: %d - %s\n",
                errno, strerror(errno));
        goto out;
    }

    fprintf(stdout, "h: %d, w: %d, gs: %d, bpp: %d\n", fvs->height, fvs->width,
            fvs->grayscale, fvs->bits_per_pixel);


    while (1) {
        for (i = 0; i < SCREEN_WIDTH - SQUARE_SIDE; ++i){
            get_frame(i, &buf);
            write(fd, buf, 8 * SCREEN_WIDTH);
        }
        for (;i >= 0; --i){
            get_frame(i, &buf);
            write(fd, buf, 8 * SCREEN_WIDTH);
        }
    }

out:
    close(fd);
    free(fvs);
    return 0;
}
