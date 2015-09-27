#include <Windows.h>
#include "resource.h"

#include <ida.hpp>
#include <dbg.hpp>
#include <diskio.hpp>

#include "dialog_utils.h"
#include "vdp_ram.h"

//extern HWND HWnd;
extern HWND VDPRamHWnd;
extern int DialogsOpen;

segment_t *vram = NULL, *cram = NULL;
UINT8 *VRam = NULL, *CRam = NULL;

HDC VDPRamMemDC;
HBITMAP VDPRamMemBMP;
HBITMAP VDPRamLastBMP;
BITMAPINFO MemBMPi;
COLORREF *MemBMPBits;
int VDPRamPal;
#define VDP_RAM_VCOUNT 20

static asize_t get_vram_size()
{
    if (vram != NULL)
        return vram->size();
    return 0;
}

static void dump_vram(FILE *fp)
{
    asize_t size = vram->size();
    UINT8 *buf = (UINT8 *)malloc(size);
    dbg->read_memory(vram->startEA, buf, size);
    ewrite(fp, buf, size);
    free(buf);
}

static void dump_vram()
{
    asize_t size = vram->size();
    if (VRam == NULL)
        VRam = (UINT8*)malloc(size);
    size = dbg->read_memory(vram->startEA, &VRam[0], size);
}

static void load_vram(FILE *fp)
{
    asize_t size = vram->size();
    UINT8 *buf = (UINT8 *)malloc(size);
    eread(fp, buf, size);
    dbg->write_memory(vram->startEA, buf, size);
    free(buf);
}

static void dump_cram(FILE *fp)
{
    asize_t size = cram->size();
    UINT8 *buf = (UINT8 *)malloc(size);
    dbg->read_memory(cram->startEA, buf, size);
    ewrite(fp, buf, size);
    free(buf);
}

static void dump_cram()
{
    asize_t size = cram->size();
    if (CRam == NULL)
        CRam = (UINT8*)malloc(size);
    size  = dbg->read_memory(cram->startEA, CRam, size);
}

static void load_cram(FILE *fp)
{
    asize_t size = cram->size();
    UINT8 *buf = (UINT8 *)malloc(size);
    eread(fp, buf, size);
    dbg->write_memory(cram->startEA, buf, size);
    free(buf);
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
    static int watchIndex = 0;

    switch (uMsg)
    {
    case WM_INITDIALOG: {
        vram = get_segm_by_name("VDP_VRAM");
        cram = get_segm_by_name("VDP_CRAM");
        
        asize_t vram_size = get_vram_size();
        
        HDC hdc = GetDC(hDlg);
        VDPRamMemDC = CreateCompatibleDC(hdc);
        MemBMPi.bmiHeader.biSize = sizeof(MemBMPi.bmiHeader);
        MemBMPi.bmiHeader.biWidth = 8 * 16;
        MemBMPi.bmiHeader.biHeight = (vram_size / 32 / 16) * 8 + 8;
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
        VDPRamPal = 0;
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
        SetScrollRange(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, 0, vram_size / 0x200 - VDP_RAM_VCOUNT, TRUE);
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
        case SB_LEFT:      // Scroll to far left.
            CurPos = 0;
            break;

        case SB_RIGHT:      // Scroll to far right.
            CurPos = get_vram_size() / 0x200 - VDP_RAM_VCOUNT;
            break;

        case SB_ENDSCROLL:   // End scroll.
            break;

        case SB_LINELEFT:      // Scroll left.
            if (CurPos > 0)
                CurPos--;
            break;

        case SB_LINERIGHT:   // Scroll right.
            if (CurPos < get_vram_size() / 0x200 - VDP_RAM_VCOUNT)
                CurPos++;
            break;

        case SB_PAGELEFT:    // Scroll one page left.
            CurPos -= VDP_RAM_VCOUNT;
            if (CurPos < 0)
                CurPos = 0;
            break;

        case SB_PAGERIGHT:      // Scroll one page righ
        {
            asize_t vram_size = get_vram_size();

            CurPos += VDP_RAM_VCOUNT;
            if (CurPos >= vram_size / 0x200 - VDP_RAM_VCOUNT)
                CurPos = vram_size / 0x200 - VDP_RAM_VCOUNT - 1;
        } break;

        case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
        case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
        {
            SCROLLINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;

            // Call GetScrollInfo to get current tracking
            //    position in si.nTrackPos

            if (!GetScrollInfo(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, &si))
                return 1; // GetScrollInfo failed
            CurPos = si.nTrackPos;
        }	break;
        }
        SetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL, CurPos, TRUE);
    }	break;
    case WM_PAINT:
    {
        asize_t vram_size = get_vram_size();

        dump_vram();
        dump_cram();
        
        PAINTSTRUCT ps;
        BeginPaint(hDlg, &ps);
        SelectObject(VDPRamMemDC, VDPRamLastBMP);
        int i, j, x, y, xx;
        for (i = 0; i < vram_size; ++i)
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
        StretchBlt(ps.hdc, 5, 5 + 4 * 16 + 5, 8 * 16 * 2, VDP_RAM_VCOUNT * 8 * 2, VDPRamMemDC, 0, scroll * 8, 8 * 16, VDP_RAM_VCOUNT * 8, SRCCOPY);
        StretchBlt(ps.hdc, 5, 5, 16 * 16, 4 * 16, VDPRamMemDC, 0, MemBMPi.bmiHeader.biHeight - 8, 16, 4, SRCCOPY);
        r.left = 5;
        r.right = 5 + 16 * 16;
        r.top = 5 + VDPRamPal;
        r.bottom = 5 + 16 + VDPRamPal;
        DrawFocusRect(ps.hdc, &r);
        EndPaint(hDlg, &ps);
        return true;
    }	break;

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam) - 5;
        int y = HIWORD(lParam) - 5;
        if (x >= 0 &&
            x < 16 * 16 &&
            y >= 0 &&
            y <= 16 * 4)
        {
            VDPRamPal = y & 0x30;
        }
    }	break;

    case WM_MOUSEMOVE:
    {
        int x = LOWORD(lParam) - 5;
        int y = HIWORD(lParam) - (5 + 4 * 16 + 5);
        if (x >= 0 &&
            x < 16 * 16 &&
            y >= 0 &&
            y <= VDP_RAM_VCOUNT * 8 * 2)
        {
            HDC dc = GetDC(hDlg);
            int scroll = GetScrollPos(GetDlgItem(hDlg, IDC_SCROLLBAR1), SB_CTL);
            StretchBlt(dc, 5 + 16 * 16 + 16 + 8, 295, 64, 64, VDPRamMemDC, (x >> 4) << 3, ((y >> 4) + scroll) << 3, 8, 8, SRCCOPY);
            ReleaseDC(hDlg, dc);
            char buff[30];
            qsnprintf(buff, sizeof(buff), "Offset: %04X\r\nId: %03X", (((y >> 4) + scroll) << 9) + ((x >> 4) << 5), (((y >> 4) + scroll) << 4) + (x >> 4));
            SetDlgItemText(hDlg, IDC_EDIT1, buff);
        }
    }	break;

    case WM_CLOSE:
        free(VRam);
        free(CRam);
        VRam = CRam = NULL;

        vram = cram = NULL;
        
        SelectObject(VDPRamMemDC, VDPRamLastBMP);
        DeleteObject(VDPRamMemBMP);
        DeleteObject(VDPRamMemDC);
        DialogsOpen--;
        VDPRamHWnd = NULL;
        EndDialog(hDlg, true);
        return true;
    }

    return false;
}