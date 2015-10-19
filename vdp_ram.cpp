#define NOMINMAX

#include <Windows.h>
#include <algorithm>
#include "resource.h"

#include <ida.hpp>
#include <dbg.hpp>
#include <diskio.hpp>

#include "dialog_utils.h"
#include "vdp_ram.h"
#include "debug.h"

//extern HWND HWnd;
extern HWND VDPRamHWnd;

UINT8 *VRam = NULL, *CRam = NULL;

HDC VDPRamMemDC, hDC;
HBITMAP VDPRamMemBMP;
HBITMAP VDPRamLastBMP;
BITMAPINFO MemBMPi;
COLORREF *MemBMPBits;
int VDPRamPal, VDPRamTile;
#define VDP_RAM_VCOUNT 20

const asize_t vram_size_ = 0x10000;
const asize_t cram_size_ = 0x80;

static size_t mess_vdp_read(const char *region, void *buffer, size_t size)
{
	size_t real_size = 0;
	UINT8 *ptr = (UINT8 *)find_region(region, '/', &real_size);

	if (!(ptr || real_size))
		return 0;

	real_size = std::min(real_size, size);
	for (size_t i = 0; i < real_size; i += 2)
	{
		((UINT8*)buffer)[i] = ptr[i + 1];
		((UINT8*)buffer)[i + 1] = ptr[i];
	}
	return real_size;
}

static size_t mess_vdp_write(const char *region, const void *buffer, size_t size)
{
	size_t real_size = 0;
	UINT8 *ptr = (UINT8 *)find_region(region, '/', &real_size);

	if (!(ptr || real_size))
		return 0;

	real_size = std::min(real_size, size);
	for (size_t i = 0; i < real_size; i += 2)
	{
		ptr[i + 1] = ((UINT8*)buffer)[i];
		ptr[i] = ((UINT8*)buffer)[i + 1];
	}
	return real_size;
}

static void dump_vram(FILE *fp)
{
	UINT8 *buf = (UINT8 *)malloc(vram_size_);
	mess_vdp_read("m_vram", buf, vram_size_);
	ewrite(fp, buf, vram_size_);
	free(buf);
}

static void dump_vram()
{
	if (VRam == NULL)
		VRam = (UINT8*)malloc(vram_size_);
	mess_vdp_read("m_vram", VRam, vram_size_);
}

static void load_vram(FILE *fp)
{
	UINT8 *buf = (UINT8 *)malloc(vram_size_);
	eread(fp, buf, vram_size_);
	mess_vdp_write("m_vram", buf, vram_size);
	free(buf);
}

static void dump_cram(FILE *fp)
{
	/*UINT8 *buf = (UINT8 *)malloc(cram_size);
	mess_vdp_read("m_cram", buf, cram_size);
	ewrite(fp, buf, cram_size);
	free(buf);*/
}

static void dump_cram()
{
	/*if (CRam == NULL)
		CRam = (UINT8*)malloc(cram_size);
	mess_vdp_read("m_cram", CRam, cram_size);*/
}

static void load_cram(FILE *fp)
{
	/*UINT8 *buf = (UINT8 *)malloc(cram_size);
	eread(fp, buf, cram_size);
	mess_vdp_write("m_cram", buf, cram_size);
	free(buf);*/
}

inline static COLORREF get_color(UINT8 *cram, int index)
{
	UINT16 word = ((cram[index * 2] << 8) | cram[index * 2 + 1]);

	UINT8 r = (UINT8)(((word >> 0) & 0xE) << 4);
	UINT8 g = (UINT8)(((word >> 4) & 0xE) << 4);
	UINT8 b = (UINT8)(((word >> 8) & 0xE) << 4);

	return RGB(b, g, r);
}

void Update_VDP_RAM()
{
	if (VDPRamHWnd)
		InvalidateRect(VDPRamHWnd, NULL, FALSE);
}

LRESULT CALLBACK VDPRamProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;

	switch (uMsg)
	{
	case WM_INITDIALOG: {
		hDC = GetDC(hDlg);
		VDPRamMemDC = CreateCompatibleDC(hDC);
		MemBMPi.bmiHeader.biSize = sizeof(MemBMPi.bmiHeader);
		MemBMPi.bmiHeader.biWidth = 8 * 16;
		MemBMPi.bmiHeader.biHeight = (vram_size_ / 32 / 16) * 8 + 8;
		MemBMPi.bmiHeader.biBitCount = 32;
		MemBMPi.bmiHeader.biPlanes = 1;
		MemBMPi.bmiHeader.biCompression = BI_RGB;
		MemBMPi.bmiHeader.biClrImportant = 0;
		MemBMPi.bmiHeader.biClrUsed = 0;
		MemBMPi.bmiHeader.biSizeImage = (MemBMPi.bmiHeader.biWidth*MemBMPi.bmiHeader.biHeight) * 4;
		MemBMPi.bmiHeader.biXPelsPerMeter = 0;
		MemBMPi.bmiHeader.biYPelsPerMeter = 0;
		//VDPRamMemBMP=CreateCompatibleBitmap(hdc,8*16,(sizeof(VRam)/32/16)*8+8);
		VDPRamMemBMP = CreateDIBSection(VDPRamMemDC, &MemBMPi, DIB_RGB_COLORS, (void **)&MemBMPBits, NULL, NULL);
		VDPRamLastBMP = (HBITMAP)SelectObject(VDPRamMemDC, VDPRamMemBMP);
		VDPRamPal = VDPRamTile = 0;
		//memset(&MemBMPi,0,sizeof(MemBMPi));
		//MemBMPi.bmiHeader.biSize=sizeof(MemBMPi.bmiHeader);
		//GetDIBits(VDPRamMemDC,VDPRamMemBMP,0,0,NULL,&MemBMPi,DIB_RGB_COLORS);
		//MemBMPBits = new COLORREF[MemBMPi.bmiHeader.biSizeImage/4+1];

		VDPRamHWnd = hDlg;

		GetWindowRect(hDlg, &r);
		dx1 = (r.right - r.left) / 2;
		dy1 = (r.bottom - r.top) / 2;

		GetWindowRect(hDlg, &r2);
		dx2 = (r2.right - r2.left) / 2;
		dy2 = (r2.bottom - r2.top) / 2;

		// push it away from the main window if we can
		const int width = (r.right - r.left);
		const int width2 = (r2.right - r2.left);
		if (r.left + width2 + width < GetSystemMetrics(SM_CXSCREEN))
		{
			r.right += width;
			r.left += width;
		}
		else if ((int)r.left - (int)width2 > 0)
		{
			r.right -= width2;
			r.left -= width2;
		}

		SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
		SetWindowPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), NULL, 5 + 16 * 16, 5 + 16 * 4 + 5, 16, 16 * VDP_RAM_VCOUNT, SWP_NOZORDER | SWP_SHOWWINDOW);
		SetScrollRange(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, 0, vram_size_ / 0x200 - VDP_RAM_VCOUNT, TRUE);
		return true;
	}	break;

	case WM_COMMAND:
	{
		switch (wParam)
		{
		case IDC_DUMP_PAL:
		{
			char fname[2048];
			qstrncpy(fname, "pal.bin", sizeof(fname));
			if ((getSaveFileName("Save Dump Pal As...", fname, sizeof(fname), "All Files\0*.*\0\0", ".", hDlg)))
			{
				FILE *out = fopenWB(fname);
				dump_cram(out);
				eclose(out);
			}
		}
		break;
		case IDC_LOAD_PAL:
		{
			char fname[2048];
			qstrncpy(fname, "pal.bin", sizeof(fname));
			if (getOpenFileName("Load Dump Pal As...", fname, sizeof(fname), "All Files\0*.*\0\0", ".", hDlg))
			{
				FILE *in = fopenRB(fname);
				load_cram(in);
				eclose(in);
			}
		}
		break;
		case IDC_DUMP_VRAM:
		{
			char fname[2048];
			qstrncpy(fname, "vram.bin", sizeof(fname));
			if (getSaveFileName("Save Dump VRAM As...", fname, sizeof(fname), "All Files\0 * .*\0\0", ".", hDlg))
			{
				FILE *out = fopenWB(fname);
				dump_vram(out);
				eclose(out);
			}
		}
		break;
		case IDC_LOAD_VRAM:
		{
			char fname[2048];
			qstrncpy(fname, "vram.bin", sizeof(fname));
			if (getOpenFileName("Load Dump VRAM As...", fname, sizeof(fname), "All Files\0*.*\0\0", ".", hDlg))
			{
				FILE *in = fopenRB(fname);
				load_vram(in);
				eclose(in);
			}
		}
		break;
		}
	}	break;

	case WM_VSCROLL:
	{
		int CurPos = GetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL);
		int nSBCode = LOWORD(wParam);
		int nPos = HIWORD(wParam);
		switch (nSBCode)
		{
		case SB_LEFT:	  // Scroll to far left.
			CurPos = 0;
			break;

		case SB_RIGHT:	  // Scroll to far right.
			CurPos = vram_size_ / 0x200 - VDP_RAM_VCOUNT;
			break;

		case SB_ENDSCROLL:   // End scroll.
			break;

		case SB_LINELEFT:	  // Scroll left.
			if (CurPos > 0)
				CurPos--;
			break;

		case SB_LINERIGHT:   // Scroll right.
			if (CurPos < vram_size_ / 0x200 - VDP_RAM_VCOUNT)
				CurPos++;
			break;

		case SB_PAGELEFT:	// Scroll one page left.
			CurPos -= VDP_RAM_VCOUNT;
			if (CurPos < 0)
				CurPos = 0;
			break;

		case SB_PAGERIGHT:	  // Scroll one page righ
		{
			CurPos += VDP_RAM_VCOUNT;
			if (CurPos >= vram_size_ / 0x200 - VDP_RAM_VCOUNT)
				CurPos = vram_size_ / 0x200 - VDP_RAM_VCOUNT - 1;
		} break;

		case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		{
			SCROLLINFO si;
			ZeroMemory(&si, sizeof(si));
			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS;

			// Call GetScrollInfo to get current tracking
			//	position in si.nTrackPos

			if (!GetScrollInfo(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, &si))
				return 1; // GetScrollInfo failed
			CurPos = si.nTrackPos;
		}	break;
		}
		SetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, CurPos, TRUE);
		Update_VDP_RAM();
	}	break;
	case WM_PAINT:
	{
		dump_vram();
		dump_cram();
		
		PAINTSTRUCT ps;
		BeginPaint(hDlg, &ps);
		SelectObject(VDPRamMemDC, VDPRamLastBMP);
		int i, j, x, y, xx;
		for (i = 0; i < vram_size_; ++i)
		{
			x = ((i >> 5) & 0xf) << 3;
			y = ((i >> 9) << 3);
			xx = (MemBMPi.bmiHeader.biHeight - 8 - y + 7 - ((i >> 2) & 7))*MemBMPi.bmiHeader.biWidth + (x + (i & 3 ^ 1) * 2);
			MemBMPBits[xx] = get_color(CRam, (VRam[i^1] >> 4) + VDPRamPal);
			MemBMPBits[xx + 1] = get_color(CRam, (VRam[i^1] & 0xf) + VDPRamPal);
		}
		for (j = 0; j < 4; ++j)
			for (i = 0; i < 16; ++i)
				MemBMPBits[(7 - j)*MemBMPi.bmiHeader.biWidth + i] = get_color(CRam, i + j * 16);

		SelectObject(VDPRamMemDC, VDPRamMemBMP);
		int scroll = GetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL);
		StretchBlt(ps.hdc, 5, 5 + 4 * 16 + 5, 8 * 16 * 2, VDP_RAM_VCOUNT * 8 * 2, VDPRamMemDC, 0, scroll * 8, 8 * 16, VDP_RAM_VCOUNT * 8, SRCCOPY); // VRAM
		StretchBlt(ps.hdc, 5, 5, 16 * 16, 4 * 16, VDPRamMemDC, 0, MemBMPi.bmiHeader.biHeight - 8, 16, 4, SRCCOPY); // CRAM
		StretchBlt(ps.hdc, 5 + 16 * 16 + 16 + 8, 295, 64, 64, VDPRamMemDC, (VDPRamTile % 16) << 3, (VDPRamTile >> 4) << 3, 8, 8, SRCCOPY); // Selected Tile
		
		r.left = 5;
		r.right = r.left + 16 * 16;
		r.top = 5 + VDPRamPal;
		r.bottom = r.top + 16;
		DrawFocusRect(ps.hdc, &r);

		r.left = 5 + ((VDPRamTile % 16) << 4);
		r.right = r.left + 16;
		r.top = 5 + 4 * 16 + 5 + ((VDPRamTile >> 4) << 4);
		r.bottom = r.top + 16;
		DrawFocusRect(ps.hdc, &r);

		EndPaint(hDlg, &ps);
		return true;
	}	break;

	case WM_LBUTTONDOWN:
	{
		int x = LOWORD(lParam) - 5;
		int y = HIWORD(lParam) - 5;
		if (x >= 0 && x < 16 * 16 &&
			y >= 0 && y <= 16 * 4)
		{
			VDPRamPal = y & 0x30;
			Update_VDP_RAM();
		}
		else
		{
			x = LOWORD(lParam) - 5;
			y = HIWORD(lParam) - (5 + 4 * 16 + 5);

			if (x >= 0 &&
				x < 16 * 16 &&
				y >= 0 &&
				y <= VDP_RAM_VCOUNT * 8 * 2)
			{
				int scroll = GetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL);
				
				char buff[0x20 * 2 + 1];
				int offset = (((y >> 4) + scroll) << 9) + ((x >> 4) << 5);
				int id = VDPRamTile = (((y >> 4) + scroll) << 4) + (x >> 4);

				for (int i = 0; i < 0x20; i++)
					qsnprintf(&buff[i * 2], 3, "%02X", VRam[offset + i]);
				SetDlgItemText(hDlg, IDC_TILE_INFO2, buff);

				qsnprintf(buff, sizeof(buff), "Offset: %04X\r\nId: %03X", offset, id);
				SetDlgItemText(hDlg, IDC_TILE_INFO, buff);

				Update_VDP_RAM();
			}
		}
	}	break;

	case WM_CLOSE:
		free(VRam);
		free(CRam);
		VRam = CRam = NULL;
		
		SelectObject(VDPRamMemDC, VDPRamLastBMP);
		DeleteObject(VDPRamMemBMP);
		DeleteObject(VDPRamMemDC);
		DeleteObject(VDPRamLastBMP);
		ReleaseDC(hDlg, hDC);
		DestroyWindow(VDPRamHWnd);
		VDPRamHWnd = NULL;
		return true;
	}

	return false;
}