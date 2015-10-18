#ifndef DEBUG_H
#define DEBUG_H

#include "registers.h"

static void dump_cram();
static void dump_vram();

static UINT8 *ptrVRAM = NULL;
static UINT8 *ptrCRAM = NULL;

static UINT32 mask(UINT8 bit_idx, UINT8 bits_cnt = 1);
static UINT32 mask_get(UINT32 data, UINT8 bit_idx, UINT8 bits_cnt = 1);

inline static COLORREF get_color(UINT8 *cram, int index);;
static UINT16 get_vdp_reg_value(register_t idx, UINT16 *region, size_t real_size);
static size_t mess_vdp_read(const char *region, void *buffer, size_t size);

void *find_region(const char *region, char delim, size_t *size);

static HINSTANCE GetHInstance();

#endif