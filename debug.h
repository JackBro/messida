#ifndef DEBUG_H
#define DEBUG_H

#include <Windows.h>
#include <unordered_map>

#include "registers.h"

void dump_cram();
void dump_vram();
void dump_vsram();

extern size_t cram_size;
extern size_t vram_size;
extern size_t vsram_size;
extern UINT8 *ptrVRAM;
extern UINT8 *ptrCRAM;
extern UINT8 *ptrVSRAM;

UINT32 mask(UINT8 bit_idx, UINT8 bits_cnt = 1);
inline static UINT32 mask_get(UINT32 data, UINT8 bit_idx, UINT8 bits_cnt = 1)
{
	return (data >> bit_idx) & ((((1 << (bits_cnt - 1)) - 1) << 1) | 0x01);
}

COLORREF get_color(UINT8 *cram, int index);
UINT16 get_vdp_reg_value(register_t idx, UINT16 *region, size_t real_size);
size_t mess_vdp_read(const char *region, void* &buffer, size_t &orig_size);

void *find_region(const char *region, char delim, size_t *size);

HINSTANCE GetHInstance();
bool check_window_opened(int id, std::unordered_map<int, HWND>::const_iterator *pair);

#endif