#include "oled.h"
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/delay.h>

struct oled_info {
    uint8_t *buf;
    size_t idx;
    struct i2c_client *client;
    struct fb_info *finfo;
};

static struct oled_info* oled_info_list[MAX_CLIENT_NO];

static struct oled_info* oled_find_info_by_fb_info(struct fb_info* info){
    size_t i;
    for (i = 0; i < MAX_CLIENT_NO; ++i){
        if (oled_info_list[i]->finfo == info){
            return oled_info_list[i];
        }
    }
    return NULL;
}

static struct oled_info* oled_find_info_by_i2c_client(struct i2c_client* client){
    size_t i;
    for (i = 0; i < MAX_CLIENT_NO; ++i){
        if (oled_info_list[i]->client == client){
            return oled_info_list[i];
        }
    }
    return NULL;
}

static size_t oled_get_free_index(void){
    size_t i;
    for (i = 0; i < MAX_CLIENT_NO; ++i){
        if (oled_info_list[i] == NULL)
            break;
    }
    return i;
}

static size_t oled_find_index(struct oled_info* info){
    size_t i;
    for (i = 0; i < MAX_CLIENT_NO; ++i){
        if (oled_info_list[i] == info)
            break;
    }
    return i;
}

static int oled_send_message(struct i2c_client *client, const uint8_t *cmd, const size_t sz){
    return i2c_master_send(client, (char*)cmd, sz);
}

static void oled_update_screen(struct oled_info *info){
    size_t px, page, idx;
    uint8_t switch_page_cmd[2];
    uint8_t buf[17];
    buf[0] = DC_DATA | CO_DATA_ONLY;
    switch_page_cmd[0] = DC_CMD | CO_DATA_ONLY;

    oled_send_message(info->client, CMD_SET_START_COL, SIZE_SET_START_COL);
    oled_send_message(info->client, CMD_SET_END_COL, SIZE_SET_END_COL);

    for (page = 0; page < NUMBER_OF_PAGES; ++page ){
        switch_page_cmd[1] = PAGE_0 + page;
        oled_send_message(info->client, switch_page_cmd, 2);
        for (px = 0; px < PAGE_PIXEL_SIZE; px += 16){
            idx = px + page * PAGE_PIXEL_SIZE;
            memcpy(buf + 1, info->buf + idx, 16);
            oled_send_message(info->client,
                              buf, 17);
        }
    }
}

static int oled_fb_open(struct fb_info *info, int user){
    try_module_get(THIS_MODULE);
    return 0;
}

static ssize_t oled_fb_write(struct fb_info *info, const char __user *buf,
                             size_t count, loff_t *ppos){
    struct oled_info *oinfo = oled_find_info_by_fb_info(info);
    int ret;
    size_t bytes_to_copy = count;
    if (count + *ppos > 128 * 8){
        pr_err("Received data doesn't fit buffer. Truncating.\n");
        bytes_to_copy = 128 * 8 - *ppos;
    }

    ret = copy_from_user(oinfo->buf + *ppos, buf, bytes_to_copy);
    if (ret < 0){
        pr_err("couldn't copy from user: %d\n", ret);
    }
    oled_update_screen(oinfo);
    return bytes_to_copy;
}

static ssize_t oled_fb_read(struct fb_info *info, char __user *buf,
                            size_t count, loff_t *ppos){
    struct oled_info *oinfo;
    char* tempstr;
    int ret;
    if (*ppos >= 64)
        return 0;

    oinfo = oled_find_info_by_fb_info(info);
    tempstr = kzalloc(100, GFP_KERNEL);

    sprintf(tempstr, "page %d: 0x%02x 0x%02x 0x%02x 0x%02x "
                              "0x%02x 0x%02x 0x%02x 0x%02x "
                              "0x%02x 0x%02x 0x%02x 0x%02x "
                              "0x%02x 0x%02x 0x%02x 0x%02x\n",
            (int)(*ppos / 8),
            oinfo->buf[*ppos * 16],
            oinfo->buf[*ppos * 16 + 1],
            oinfo->buf[*ppos * 16 + 2],
            oinfo->buf[*ppos * 16 + 3],
            oinfo->buf[*ppos * 16 + 4],
            oinfo->buf[*ppos * 16 + 5],
            oinfo->buf[*ppos * 16 + 6],
            oinfo->buf[*ppos * 16 + 7],
            oinfo->buf[*ppos * 16 + 8],
            oinfo->buf[*ppos * 16 + 9],
            oinfo->buf[*ppos * 16 + 10],
            oinfo->buf[*ppos * 16 + 11],
            oinfo->buf[*ppos * 16 + 12],
            oinfo->buf[*ppos * 16 + 13],
            oinfo->buf[*ppos * 16 + 14],
            oinfo->buf[*ppos * 16 + 15]);

    ++*ppos;
    ret = copy_to_user(buf, tempstr, strlen(tempstr));
    if (ret < 0){
        pr_err("Could not copy data to user! Error: %d\n", ret);
    }

    ret = strnlen(tempstr, 100);
    kfree(tempstr);

    return ret;
}

static int oled_fb_release(struct fb_info *info, int user){
    module_put(THIS_MODULE);
    return 0;
}

static int oled_fb_blank(int blank, struct fb_info *info){
    struct oled_info *oinfo;
    oinfo = oled_find_info_by_fb_info(info);

    switch (blank){
    case FB_BLANK_NORMAL:
        oled_send_message(oinfo->client, CMD_DISPLAY_OFF, SIZE_DISPLAY_OFF);
        break;
    case FB_BLANK_UNBLANK:
        oled_send_message(oinfo->client, CMD_DISPLAY_ON, SIZE_DISPLAY_ON);
        break;
    default:
        return -ENOSYS;
    }

    return 0;
}

static void oled_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect){
    struct oled_info *oinfo;
    size_t x, y;
    uint8_t color;
    if (rect->dx + rect->width > 128){
        pr_err("Width is too big!\n");
    }

    if (rect->dy + rect->height > 64){
        pr_err("Height is too big!\n");
    }

    oinfo = oled_find_info_by_fb_info(info);
    color = rect->color ? 0x1 : 0x0;

    for (x = rect->dx; x < rect->dx + rect->width; ++x){
        for (y = rect->dy; y < rect->dy + rect->height; ++y){
            oinfo->buf[(int)(y / 8) * 128 + x] &= ~color << (y % 8);
        }
    }

    oled_update_screen(oinfo);
}

static int oled_get_pixel_at(struct oled_info *info, size_t x, size_t y){
    return (info->buf[(int)(y / 8) * 128 + x] >> (y % 8)) & 0x1;
}

static void oled_fb_copyarea(struct fb_info *info, const struct fb_copyarea *region){
    struct oled_info *oinfo;
    size_t x, y;
    uint8_t *buf;
    oinfo = oled_find_info_by_fb_info(info);
    buf = kzalloc((region->height * region->width) / 8 + 1, GFP_KERNEL);
    // note: should break up pixels to bytes (instead of bits), and go with memcpy/memmove?
    for (x = 0; x < region->width; x++){
        for (y = 0; y < region->height; ++y){
            buf[(int)(y / 8) * 128 + x] |= (oled_get_pixel_at(oinfo, x + region->sx, y +region->sy) << (y % 8));
        }
    }


    // TODO: thinking of it, this can't be right. test it, and fix it.
    for (x = 0; x < region->width; ++x){
        for (y = 0; y < region->height; ++y){
            oinfo->buf[(int)((y + region->dy) / 8) * 128 + x + region->dx] =
                buf[(int)(y / 8) * 128 + x];
        }
    }
    kfree(buf);
    oled_update_screen(oinfo);
}

static void oled_fb_imageblit (struct fb_info *info, const struct fb_image *image){
    struct oled_info *oinfo;
    oinfo = oled_find_info_by_fb_info(info);
    pr_info("Image x: %d, y: %d, w: %d, h: %d, depth: %d\n", image->dx, image->dy, image->width, image->height, image->depth);
}

static const struct of_device_id oled_of_match[] = {
    {
        .compatible = "monochrome,oled"
    },
    {}
};

MODULE_DEVICE_TABLE(of, oled_of_match);


static struct fb_ops fops = {
    .owner = THIS_MODULE,
    .fb_open = oled_fb_open,
    .fb_write = oled_fb_write,
    .fb_read = oled_fb_read,
    .fb_release = oled_fb_release,
    .fb_fillrect = oled_fb_fillrect,
    .fb_copyarea = oled_fb_copyarea,
    .fb_imageblit = oled_fb_imageblit,
    .fb_blank = oled_fb_blank,
};

static const struct fb_var_screeninfo var_screeninfo = {
    .xres = 64,
    .yres = 128,
    .bits_per_pixel = 1,
    .grayscale = 1,
    .width = 128,
    .height = 64,
    .blue = {
        .offset = 0,
        .length = 1
    },
    .red = {
        .offset = 0,
        .length = 1
    },
    .green = {
        .offset = 0,
        .length = 1
    },
    .transp = {
        .offset = 0,
        .length = 1
    }
};

static int oled_probe(struct i2c_client *client){
    int ret, idx;
    struct fb_info *finfo;
    struct fb_fix_screeninfo *fix_screeninfo;
    struct oled_info *oinfo;

    idx = oled_get_free_index();
    if (idx == MAX_CLIENT_NO){
        pr_err("Too many clients connected!\n");
        return -ENOMEM;
    }

    ret = oled_send_message(client, CMD_CHARGE_PUMP_SETTINGS, SIZE_CHARGE_PUMP_SETTINGS);
    if (ret < 0){
        pr_err("Could not turn on oled charge pump!\n");
        return ret;
    }

    ret = oled_send_message(client, CMD_DISPLAY_ON, SIZE_DISPLAY_ON);
    if (ret < 0){
        pr_err("Could not turn on display!\n");
        return ret;
    }
    msleep(100);
    finfo = kzalloc(sizeof(struct fb_info), GFP_KERNEL);
    fix_screeninfo = kzalloc(sizeof(struct fb_fix_screeninfo), GFP_KERNEL);

    oinfo = kzalloc(sizeof(struct oled_info), GFP_KERNEL);
    oinfo->finfo = finfo;
    oinfo->client = client;
    oinfo->buf = kzalloc(128 * 8, GFP_KERNEL);

    strncpy(fix_screeninfo->id, client->name, strnlen(client->name, 16));
    fix_screeninfo->visual = FB_VISUAL_MONO10;
    fix_screeninfo->type = FB_TYPE_PACKED_PIXELS;
    fix_screeninfo->smem_start = *oinfo->buf;
    fix_screeninfo->smem_len = 128 * 8;

    finfo->fbops = &fops;
    finfo->var = var_screeninfo;
    finfo->fix = *fix_screeninfo;

    register_framebuffer(finfo);

    oled_info_list[idx] = oinfo;
    oled_update_screen(oinfo);

    return 0;
}

static void oled_remove(struct i2c_client *client){
    int ret;
    size_t idx;
    struct oled_info *oinfo;
    ret = oled_send_message(client, CMD_DISPLAY_OFF, SIZE_DISPLAY_OFF);
    if (ret < 0)
        pr_err("Could not turn off display: %d\n", ret);

    oinfo = oled_find_info_by_i2c_client(client);
    unregister_framebuffer(oinfo->finfo);
    kfree(oinfo->buf);
    kfree(oinfo->finfo);
    idx = oled_find_index(oinfo);
    if (idx != MAX_CLIENT_NO)
        oled_info_list[idx] = NULL;
}

static struct i2c_driver driver = {
    .driver = {
        .owner = THIS_MODULE,
        .of_match_table = oled_of_match,
        .name = "oled_driver"
    },
    .probe = oled_probe,
    .remove = oled_remove
};

static int __init oled_init(void){
    int res;
    res = i2c_register_driver(THIS_MODULE, &driver);
    return res;
}

static void __exit oled_exit(void){
    i2c_del_driver(&driver);
}


module_init(oled_init);
module_exit(oled_exit);

MODULE_AUTHOR("Gyorgy Sarvari");
MODULE_LICENSE("GPL");
