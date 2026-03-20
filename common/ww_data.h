#ifndef WW_DATA_H
#define WW_DATA_H

#include "espnow.h"

static const espnow_addr_t DEV_1_MAC = {0x7C, 0x9e, 0xBD, 0xF9, 0xD0, 0x58};
static const espnow_addr_t DEV_2_MAC = {0x10, 0x52, 0x1C, 0x69, 0x08, 0xD8};
static const espnow_addr_t DEV_3_MAC = {0x7C, 0x9E, 0xBD, 0xf4, 0x4E, 0xB4};
// TODO: Add ID-4 MAC addr
// static const espnow_addr_t DEV_4_MAC = {};

#define MOTHERSHIP_MAC DEV_2_MAC

#endif
