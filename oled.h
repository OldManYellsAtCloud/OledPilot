#ifndef OLED_H
#define OLED_H

#include <linux/of.h>

#define MAX_CLIENT_NO  10

#define CO_DATA_ONLY 0x0 << 7
#define CO_NOT_DATA_ONLY 0x1 << 7

#define DC_CMD 0x0 << 6
#define DC_DATA 0x1 << 6

#define NUMBER_OF_PAGES 8
#define PAGE_PIXEL_SIZE 128
#define PAGE_0  0xb0

const uint8_t CMD_CHARGE_PUMP_SETTINGS[] = {DC_CMD | CO_DATA_ONLY, 0x8d, 0x8d};
const size_t SIZE_CHARGE_PUMP_SETTINGS = 3;

const uint8_t CMD_DISPLAY_ON[] = {DC_CMD | CO_DATA_ONLY, 0xaf};
const size_t SIZE_DISPLAY_ON = 2;

const uint8_t CMD_DISPLAY_OFF[] = {DC_CMD | CO_DATA_ONLY, 0xae};
const size_t SIZE_DISPLAY_OFF = 2;

const uint8_t CMD_SET_START_COL[] = {DC_CMD | CO_NOT_DATA_ONLY, 0x00};
const size_t SIZE_SET_START_COL = 2;

const uint8_t CMD_SET_END_COL[] =  {DC_CMD | CO_NOT_DATA_ONLY, 0x10};
const size_t SIZE_SET_END_COL = 2;


#endif // OLED_H
