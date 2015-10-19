#include "PaletteView.h"

#include <random>
#include <set>
#include <algorithm>

#include "..\..\..\debug.h"

extern std::unordered_map<int, HWND> openedWindows;

// Event handlers
LRESULT msgWM_CREATE(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgWM_PAINT(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgWM_TIMER(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgWM_CLOSE(HWND hwnd, WPARAM wParam, LPARAM lParam);

//----------------------------------------------------------------------------------------
//Member window procedure
//----------------------------------------------------------------------------------------
LRESULT CALLBACK ExodusVdpPalWndProcWindow(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_CREATE:
		return msgWM_CREATE(hwnd, wparam, lparam);
	case WM_PAINT:
		return msgWM_PAINT(hwnd, wparam, lparam);
	case WM_TIMER:
		return msgWM_TIMER(hwnd, wparam, lparam);
	case WM_CLOSE:
		return msgWM_CLOSE(hwnd, wparam, lparam);
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//----------------------------------------------------------------------------------------
//Event handlers
//----------------------------------------------------------------------------------------
LRESULT msgWM_CREATE(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	SetTimer(hwnd, 1, 1000 / 25, NULL);
	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgWM_PAINT(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	if (!ptrCRAM)
		return ExodusVdpPalWndProcWindow(hwnd, WM_TIMER, wparam, lparam);
	
	//Begin the paint operation
	PAINTSTRUCT paintInfo;
	HDC hdc = BeginPaint(hwnd, &paintInfo);

	//Obtain the current width and height of the client region of the window
	RECT rect;
	GetClientRect(hwnd, &rect);
	int clientWidth = rect.right;
	int clientHeight = rect.bottom;

	//Build a set of palette columns with an extra pixel in their width
	static const unsigned int paletteColumns = 16;
	std::minstd_rand widthExtraPixelDistributionGenerator;
	std::set<unsigned int> widthExtraPixelFoundEntrySet;
	for (unsigned int paletteEntry = 0; paletteEntry < (clientWidth % paletteColumns); ++paletteEntry)
	{
		unsigned int nextEntry;
		do
		{
			nextEntry = (unsigned int)widthExtraPixelDistributionGenerator() % paletteColumns;
		} while (widthExtraPixelFoundEntrySet.find(nextEntry) != widthExtraPixelFoundEntrySet.end());
		widthExtraPixelFoundEntrySet.insert(nextEntry);
	}

	//Build a set of palette rows with an extra pixel in their height
	static const unsigned int paletteRows = 4;
	std::minstd_rand heightExtraPixelDistributionGenerator;
	std::set<unsigned int> heightExtraPixelFoundEntrySet;
	for (unsigned int paletteLine = 0; paletteLine < (clientHeight % paletteRows); ++paletteLine)
	{
		unsigned int nextEntry;
		do
		{
			nextEntry = (unsigned int)heightExtraPixelDistributionGenerator() % paletteRows;
		} while (heightExtraPixelFoundEntrySet.find(nextEntry) != heightExtraPixelFoundEntrySet.end());
		heightExtraPixelFoundEntrySet.insert(nextEntry);
	}

	//Draw each entry in the palette
	unsigned int nextCellPosY = 0;
	for (unsigned int paletteLine = 0; paletteLine < paletteRows; ++paletteLine)
	{
		//Calculate the height of this palette row in the window
		unsigned int cellHeight = (clientHeight / paletteRows) + ((heightExtraPixelFoundEntrySet.find(paletteLine) != heightExtraPixelFoundEntrySet.end()) ? 1 : 0);

		unsigned int nextCellPosX = 0;
		for (unsigned int paletteEntry = 0; paletteEntry < paletteColumns; ++paletteEntry)
		{
			//Calculate the width of this palette entry in the window
			unsigned int cellWidth = (clientWidth / paletteColumns) + ((widthExtraPixelFoundEntrySet.find(paletteEntry) != widthExtraPixelFoundEntrySet.end()) ? 1 : 0);

			//Populate the rectangle with the coordinates of this palette entry in the
			//window
			rect.left = (int)nextCellPosX;
			rect.right = rect.left + (int)cellWidth;
			rect.top = (int)nextCellPosY;
			rect.bottom = rect.top + (int)cellHeight;

			//Retrieve the colour of this palette entry
			COLORREF rgb = get_color(ptrCRAM, paletteEntry + paletteLine * paletteColumns);

			//Draw the rectangle for this palette entry
			HBRUSH brush = CreateSolidBrush(rgb);
			FillRect(hdc, &rect, brush);
			DeleteObject(brush);

			//Advance to the next horizontal cell position
			nextCellPosX += cellWidth;
		}

		//Advance to the next vertical cell position
		nextCellPosY += cellHeight;
	}

	//Complete the paint operation
	EndPaint(hwnd, &paintInfo);
	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgWM_TIMER(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	// Dump current palette state
	dump_cram();

	//Invalidate the window to trigger a redraw operation
	InvalidateRect(hwnd, NULL, FALSE);
	return 0;
}

LRESULT msgWM_CLOSE(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	KillTimer(hwnd, 1);
	DestroyWindow(hwnd);

	std::unordered_map<int, HWND>::const_iterator pair;
	check_window_opened(EXODUS_VDP_PALETTE_ID, &pair);
	openedWindows.erase(pair);

	return 0;
}
