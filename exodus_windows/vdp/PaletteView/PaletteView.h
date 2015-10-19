#ifndef __PALETTEVIEW_H__
#define __PALETTEVIEW_H__

#define NOMINMAX

#include <Windows.h>

#define EXODUS_VDP_PALETTE_ID 1
LRESULT CALLBACK ExodusVdpPalWndProcWindow(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#endif
