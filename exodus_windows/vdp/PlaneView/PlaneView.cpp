#include "PlaneView.h"
#include "..\resource.h"

#include "..\..\..\debug.h"
#include "..\..\..\exodus_helpers\WindowFunctions.h"

#include <vector>
#include <set>

//Event handlers
INT_PTR msgWM_INITDIALOG(HWND hwnd, WPARAM wParam, LPARAM lParam);
INT_PTR msgWM_CLOSE(HWND hwnd, WPARAM wParam, LPARAM lParam);
INT_PTR msgWM_COMMAND(HWND hwnd, WPARAM wParam, LPARAM lParam);
INT_PTR msgWM_HSCROLL(HWND hwnd, WPARAM wParam, LPARAM lParam);
INT_PTR msgWM_VSCROLL(HWND hwnd, WPARAM wParam, LPARAM lParam);
void UpdateScrollbar(HWND scrollWindow, WPARAM wParam);

//Render window procedure
LRESULT WndProcRender(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK WndProcRenderStatic(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

//Render window event handlers
LRESULT msgRenderWM_CREATE(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgRenderWM_DESTROY(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgRenderWM_TIMER(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgRenderWM_LBUTTONDOWN(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgRenderWM_KEYUP(HWND hwnd, WPARAM wParam, LPARAM lParam);
LRESULT msgRenderWM_KEYDOWN(HWND hwnd, WPARAM wParam, LPARAM lParam);

static SpriteMappingTableEntry GetSpriteMappingTableEntry(unsigned int spriteTableBaseAddress, unsigned int entryNo);
static void GetScrollPlanePaletteInfo(UINT8* vramData, unsigned int mappingBaseAddress, unsigned int patternBaseAddress, unsigned int planeWidth, unsigned int planeHeight, unsigned int xpos, unsigned int ypos, bool interlaceMode2Active, unsigned int& paletteRow, unsigned int& paletteIndex);
static unsigned int CalculatePatternDataRowNumber(unsigned int patternRowNumberNoFlip, bool interlaceMode2Active, UINT16 mappingData);
static unsigned int CalculatePatternDataRowAddress(unsigned int patternRowNumber, unsigned int patternCellOffset, bool interlaceMode2Active, UINT16 mappingData);
static void DigitalRenderReadVscrollData(unsigned int screenColumnNumber, unsigned int layerNumber, bool vscrState, bool interlaceMode2Active, unsigned int& layerVscrollPatternDisplacement, unsigned int& layerVscrollMappingDisplacement, UINT16& vsramReadCache);
static void GetScrollPlaneHScrollData(UINT8* vramData, unsigned int screenRowNumber, unsigned int hscrollDataBase, bool hscrState, bool lscrState, bool layerA, unsigned int& layerHscrollPatternDisplacement, unsigned int& layerHscrollMappingDisplacement);

HGLRC glrc;
HWND hwndRender;
unsigned char* buffer;
bool initializedDialog;
std::string previousText;
unsigned int currentControlFocus;

SelectedLayer selectedLayer;
bool displayScreen;
bool spriteBoundaries;

bool layerAScrollPlaneManual;
unsigned int layerAScrollPlaneWidth;
unsigned int layerAScrollPlaneHeight;
bool layerBScrollPlaneManual;
unsigned int layerBScrollPlaneWidth;
unsigned int layerBScrollPlaneHeight;
bool windowScrollPlaneManual;
unsigned int windowScrollPlaneWidth;
unsigned int windowScrollPlaneHeight;
bool spriteScrollPlaneManual;
unsigned int spriteScrollPlaneWidth;
unsigned int spriteScrollPlaneHeight;

bool layerAMappingBaseManual;
bool layerBMappingBaseManual;
bool windowMappingBaseManual;
bool spriteMappingBaseManual;
unsigned int layerAMappingBase;
unsigned int layerBMappingBase;
unsigned int windowMappingBase;
unsigned int spriteMappingBase;
bool layerAPatternBaseManual;
bool layerBPatternBaseManual;
bool windowPatternBaseManual;
bool spritePatternBaseManual;
unsigned int layerAPatternBase;
unsigned int layerBPatternBase;
unsigned int windowPatternBase;
unsigned int spritePatternBase;

bool needsUpdate;

//----------------------------------------------------------------------------------------
//Member window procedure
//----------------------------------------------------------------------------------------
INT_PTR CALLBACK ExodusVdpPlaneViewWndProcDialog(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		return msgWM_INITDIALOG(hwnd, wparam, lparam);
	case WM_COMMAND:
		return msgWM_COMMAND(hwnd, wparam, lparam);
	case WM_HSCROLL:
		return msgWM_HSCROLL(hwnd, wparam, lparam);
	case WM_VSCROLL:
		return msgWM_VSCROLL(hwnd, wparam, lparam);
	case WM_DESTROY:
		return msgWM_CLOSE(hwnd, wparam, lparam);
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//----------------------------------------------------------------------------------------
//Event handlers
//----------------------------------------------------------------------------------------
INT_PTR msgWM_INITDIALOG(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	glrc = 0;
	buffer = 0;
	hwndRender = NULL;

	selectedLayer = SELECTEDLAYER_LAYERA;
	displayScreen = true;
	spriteBoundaries = true;
	layerAScrollPlaneManual = false;
	layerBScrollPlaneManual = false;
	windowScrollPlaneManual = false;
	spriteScrollPlaneManual = false;
	layerAMappingBaseManual = false;
	layerBMappingBaseManual = false;
	windowMappingBaseManual = false;
	spriteMappingBaseManual = false;
	layerAPatternBaseManual = false;
	layerBPatternBaseManual = false;
	windowPatternBaseManual = false;
	spritePatternBaseManual = false;

	//Retrieve the current width of the vertical scroll bar
	RECT scrollBarVOriginalRect;
	GetClientRect(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_VSCROLL), &scrollBarVOriginalRect);
	int scrollBarVOriginalWidth = scrollBarVOriginalRect.right - scrollBarVOriginalRect.left;
	int scrollBarVOriginalHeight = scrollBarVOriginalRect.bottom - scrollBarVOriginalRect.top;

	//If the current width of the vertical scroll bar is different to the required width,
	//resize the scroll bar to match the required size.
	int scrollBarVRequiredWidth = GetSystemMetrics(SM_CXVSCROLL);
	if(scrollBarVOriginalWidth != scrollBarVRequiredWidth)
	{
		SetWindowPos(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_VSCROLL), NULL, 0, 0, scrollBarVRequiredWidth, scrollBarVOriginalHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	}

	//Retrieve the current height of the horizontal scroll bar
	RECT scrollBarHOriginalRect;
	GetClientRect(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_HSCROLL), &scrollBarHOriginalRect);
	int scrollBarHOriginalWidth = scrollBarHOriginalRect.right - scrollBarHOriginalRect.left;
	int scrollBarHOriginalHeight = scrollBarHOriginalRect.bottom - scrollBarHOriginalRect.top;

	//If the current height of the horizontal scroll bar is different to the required
	//height, resize the scroll bar to match the required size.
	int scrollBarHRequiredHeight = GetSystemMetrics(SM_CYHSCROLL);
	if(scrollBarHOriginalHeight != scrollBarHRequiredHeight)
	{
		SetWindowPos(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_HSCROLL), NULL, 0, 0, scrollBarHOriginalWidth, scrollBarHRequiredHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	}

	//If we resized either the horizontal or vertical scrollbars, resize the main dialog
	//window to match.
	if((scrollBarVOriginalWidth != scrollBarVRequiredWidth) || (scrollBarHOriginalHeight != scrollBarHRequiredHeight))
	{
		//Calculate the current size of the dialog window
		RECT mainDialogRect;
		GetWindowRect(hwnd, &mainDialogRect);
		int currentMainDialogWidth = mainDialogRect.right - mainDialogRect.left;
		int currentMainDialogHeight = mainDialogRect.bottom - mainDialogRect.top;

		//Resize the dialog window to the required size
		int newMainDialogWidth = currentMainDialogWidth + (scrollBarVRequiredWidth - scrollBarVOriginalWidth);
		int newMainDialogHeight = currentMainDialogHeight + (scrollBarHRequiredHeight - scrollBarHOriginalHeight);
		SetWindowPos(hwnd, NULL, 0, 0, newMainDialogWidth, newMainDialogHeight, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE);
	}

	//Create the window class for the render window
	WNDCLASSEX wc;
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = WndProcRenderStatic;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance	 = GetHInstance();
	wc.hIcon         = NULL;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "Plane Render Child";
	wc.hIconSm       = NULL;
	RegisterClassEx(&wc);

	//Calculate the marked target position for the child window inside the dialog
	RECT markerRect;
	GetWindowRect(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MARKER), &markerRect);
	POINT markerPos;
	unsigned int width = DPIScaleWidth(64*8);   //markerRect.right - markerRect.left;
	unsigned int height = DPIScaleHeight(64*8); //markerRect.bottom - markerRect.top;
	markerPos.x = markerRect.left;
	markerPos.y = markerRect.top;
	ScreenToClient(hwnd, &markerPos);

	//Create the window
	hwndRender = CreateWindowEx(0, "Plane Render Child", "Plane Render Child", WS_CHILD, markerPos.x, markerPos.y, width, height, hwnd, NULL, wc.hInstance, NULL);
	ShowWindow(hwndRender, SW_SHOWNORMAL);
	UpdateWindow(hwndRender);

	//Set the window controls to their default state
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_LAYERA, (selectedLayer == SELECTEDLAYER_LAYERA)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_LAYERB, (selectedLayer == SELECTEDLAYER_LAYERB)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_LAYERWINDOW, (selectedLayer == SELECTEDLAYER_WINDOW)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_LAYERSPRITES, (selectedLayer == SELECTEDLAYER_SPRITES)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_DISPLAYSCREEN, (displayScreen)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_DISPLAYSPRITEBOUNDARIES, (spriteBoundaries)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PLANESIZELAYERAMANUAL, (layerAScrollPlaneManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PLANESIZELAYERBMANUAL, (layerBScrollPlaneManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PLANESIZEWINDOWMANUAL, (windowScrollPlaneManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PLANESIZESPRITESMANUAL, (spriteScrollPlaneManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERA, (layerAMappingBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERB, (layerBMappingBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGWINDOW, (windowMappingBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGSPRITES, (spriteMappingBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERA, (layerAPatternBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERB, (layerBPatternBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNWINDOW, (windowPatternBaseManual)? BST_CHECKED: BST_UNCHECKED);
	CheckDlgButton(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNSPRITES, (spritePatternBaseManual)? BST_CHECKED: BST_UNCHECKED);

	//Set the initial enable state for controls
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERA), (layerAScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERA), (layerAScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERB), (layerBScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERB), (layerBScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHWINDOW), (windowScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTWINDOW), (windowScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHSPRITES), (spriteScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTSPRITES), (spriteScrollPlaneManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERA), (layerAMappingBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERB), (layerBMappingBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGWINDOW), (windowMappingBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGSPRITES), (spriteMappingBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERA), (layerAPatternBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERB), (layerBPatternBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNWINDOW), (windowPatternBaseManual)? TRUE: FALSE);
	EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNSPRITES), (spritePatternBaseManual)? TRUE: FALSE);

	return TRUE;
}

//----------------------------------------------------------------------------------------
INT_PTR msgWM_CLOSE(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	DestroyWindow(hwnd);
	//Note that we need to explicitly destroy the child window here, since we share state
	//with the child window, passing in the "this" pointer as its state. Since the
	//destructor for our state may be called anytime after this window is destroyed, and
	//this window is fully destroyed before child windows are destroyed, we need to
	//explicitly destroy the child window here. The child window is fully destroyed before
	//the DestroyWindow() function returns, and our state is still valid until we return
	//from handling this WM_DESTROY message.
	DestroyWindow(hwndRender);
	hwndRender = NULL;

	return FALSE;
}

//----------------------------------------------------------------------------------------
INT_PTR msgWM_COMMAND(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	if(HIWORD(wparam) == BN_CLICKED)
	{
		int controlID = (int)LOWORD(wparam);
		switch(controlID)
		{
		case IDC_S315_5313_PLANEVIEW_LAYERA:
			selectedLayer = SELECTEDLAYER_LAYERA;
			break;
		case IDC_S315_5313_PLANEVIEW_LAYERB:
			selectedLayer = SELECTEDLAYER_LAYERB;
			break;
		case IDC_S315_5313_PLANEVIEW_LAYERSPRITES:
			selectedLayer = SELECTEDLAYER_SPRITES;
			break;
		case IDC_S315_5313_PLANEVIEW_LAYERWINDOW:
			selectedLayer = SELECTEDLAYER_WINDOW;
			break;
		case IDC_S315_5313_PLANEVIEW_DISPLAYSCREEN:
			displayScreen = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			break;
		case IDC_S315_5313_PLANEVIEW_DISPLAYSPRITEBOUNDARIES:
			spriteBoundaries = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			break;
		case IDC_S315_5313_PLANEVIEW_PLANESIZELAYERAMANUAL:
			layerAScrollPlaneManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERA), (layerAScrollPlaneManual)? TRUE: FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERA), (layerAScrollPlaneManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PLANESIZELAYERBMANUAL:
			layerBScrollPlaneManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERB), (layerBScrollPlaneManual)? TRUE: FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERB), (layerBScrollPlaneManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PLANESIZEWINDOWMANUAL:
			windowScrollPlaneManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHWINDOW), (windowScrollPlaneManual)? TRUE: FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTWINDOW), (windowScrollPlaneManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PLANESIZESPRITESMANUAL:
			spriteScrollPlaneManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEWIDTHSPRITES), (spriteScrollPlaneManual)? TRUE: FALSE);
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PLANEHEIGHTSPRITES), (spriteScrollPlaneManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_MAPPINGLAYERAMANUAL:
			layerAMappingBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERA), (layerAMappingBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_MAPPINGLAYERBMANUAL:
			layerBMappingBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGLAYERB), (layerBMappingBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_MAPPINGWINDOWMANUAL:
			windowMappingBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGWINDOW), (windowMappingBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_MAPPINGSPRITESMANUAL:
			spriteMappingBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_MAPPINGSPRITES), (spriteMappingBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PATTERNLAYERAMANUAL:
			layerAPatternBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERA), (layerAPatternBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PATTERNLAYERBMANUAL:
			layerBPatternBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNLAYERB), (layerBPatternBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PATTERNWINDOWMANUAL:
			windowPatternBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNWINDOW), (windowPatternBaseManual)? TRUE: FALSE);
			break;
		case IDC_S315_5313_PLANEVIEW_PATTERNSPRITESMANUAL:
			spritePatternBaseManual = IsDlgButtonChecked(hwnd, controlID) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_PATTERNSPRITES), (spritePatternBaseManual)? TRUE: FALSE);
			break;
		}
	}
	else if((HIWORD(wparam) == EN_SETFOCUS) && initializedDialog)
	{
		previousText = GetDlgItemString(hwnd, LOWORD(wparam));
		currentControlFocus = LOWORD(wparam);
	}
	else if((HIWORD(wparam) == EN_KILLFOCUS) && initializedDialog)
	{
		std::string newText = GetDlgItemString(hwnd, LOWORD(wparam));
		if(newText != previousText)
		{
			int controlID = (int)LOWORD(wparam);
			switch(controlID)
			{
			case IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERA:
				layerAScrollPlaneWidth = GetDlgItemBin(hwnd, controlID);
				if(layerAScrollPlaneWidth <= 0)
				{
					layerAScrollPlaneWidth = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, layerAScrollPlaneWidth);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERA:
				layerAScrollPlaneHeight = GetDlgItemBin(hwnd, controlID);
				if(layerAScrollPlaneHeight <= 0)
				{
					layerAScrollPlaneHeight = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, layerAScrollPlaneHeight);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERB:
				layerBScrollPlaneWidth = GetDlgItemBin(hwnd, controlID);
				if(layerBScrollPlaneWidth <= 0)
				{
					layerBScrollPlaneWidth = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, layerBScrollPlaneWidth);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERB:
				layerBScrollPlaneHeight = GetDlgItemBin(hwnd, controlID);
				if(layerBScrollPlaneHeight <= 0)
				{
					layerBScrollPlaneHeight = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, layerBScrollPlaneHeight);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEWIDTHWINDOW:
				windowScrollPlaneWidth = GetDlgItemBin(hwnd, controlID);
				if(windowScrollPlaneWidth <= 0)
				{
					windowScrollPlaneWidth = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, windowScrollPlaneWidth);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEHEIGHTWINDOW:
				windowScrollPlaneHeight = GetDlgItemBin(hwnd, controlID);
				if(windowScrollPlaneHeight <= 0)
				{
					windowScrollPlaneHeight = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, windowScrollPlaneHeight);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEWIDTHSPRITES:
				spriteScrollPlaneWidth = GetDlgItemBin(hwnd, controlID);
				if(spriteScrollPlaneWidth <= 0)
				{
					spriteScrollPlaneWidth = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, spriteScrollPlaneWidth);
				break;
			case IDC_S315_5313_PLANEVIEW_PLANEHEIGHTSPRITES:
				spriteScrollPlaneHeight = GetDlgItemBin(hwnd, controlID);
				if(spriteScrollPlaneHeight <= 0)
				{
					spriteScrollPlaneHeight = 1;
				}
				UpdateDlgItemBin(hwnd, controlID, spriteScrollPlaneHeight);
				break;
			case IDC_S315_5313_PLANEVIEW_MAPPINGLAYERA:
				layerAMappingBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, layerAMappingBase);
				break;
			case IDC_S315_5313_PLANEVIEW_MAPPINGLAYERB:
				layerBMappingBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, layerBMappingBase);
				break;
			case IDC_S315_5313_PLANEVIEW_MAPPINGWINDOW:
				windowMappingBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, windowMappingBase);
				break;
			case IDC_S315_5313_PLANEVIEW_MAPPINGSPRITES:
				spriteMappingBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, spriteMappingBase);
				break;
			case IDC_S315_5313_PLANEVIEW_PATTERNLAYERA:
				layerAPatternBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, layerAPatternBase);
				break;
			case IDC_S315_5313_PLANEVIEW_PATTERNLAYERB:
				layerBPatternBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, layerBPatternBase);
				break;
			case IDC_S315_5313_PLANEVIEW_PATTERNWINDOW:
				windowPatternBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, windowPatternBase);
				break;
			case IDC_S315_5313_PLANEVIEW_PATTERNSPRITES:
				spritePatternBase = GetDlgItemHex(hwnd, controlID);
				UpdateDlgItemHex(hwnd, controlID, 5, spritePatternBase);
				break;
			}
		}
	}
	return TRUE;
}

//----------------------------------------------------------------------------------------
INT_PTR msgWM_HSCROLL(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	UpdateScrollbar(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_HSCROLL), wParam);
	return TRUE;
}

//----------------------------------------------------------------------------------------
INT_PTR msgWM_VSCROLL(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	UpdateScrollbar(GetDlgItem(hwnd, IDC_S315_5313_PLANEVIEW_VSCROLL), wParam);
	return TRUE;
}

//----------------------------------------------------------------------------------------
void UpdateScrollbar(HWND scrollWindow, WPARAM wParam)
{
	//Get the current scrollbar info
	SCROLLINFO currentScrollInfo;
	currentScrollInfo.cbSize = sizeof(currentScrollInfo);
	currentScrollInfo.fMask = SIF_TRACKPOS | SIF_RANGE | SIF_POS | SIF_PAGE;
	GetScrollInfo(scrollWindow, SB_CTL, &currentScrollInfo);

	//Calculate the new scrollbar position
	int newScrollPos = currentScrollInfo.nPos;
	switch(LOWORD(wParam))
	{
	case SB_THUMBTRACK:{
		newScrollPos = currentScrollInfo.nTrackPos;
		break;}
	case SB_TOP:
		newScrollPos = 0;
		break;
	case SB_BOTTOM:
		newScrollPos = currentScrollInfo.nMax;
		break;
	case SB_PAGEUP:
		newScrollPos -= currentScrollInfo.nPage;
		break;
	case SB_PAGEDOWN:
		newScrollPos += currentScrollInfo.nPage;
		break;
	case SB_LINEUP:
		newScrollPos -= 1;
		break;
	case SB_LINEDOWN:
		newScrollPos += 1;
		break;
	}
	newScrollPos = (newScrollPos < 0)? 0: newScrollPos;
	newScrollPos = (newScrollPos > currentScrollInfo.nMax)? currentScrollInfo.nMax: newScrollPos;

	//Apply the new scrollbar position
	SCROLLINFO newScrollInfo;
	newScrollInfo.cbSize = sizeof(newScrollInfo);
	newScrollInfo.nPos = newScrollPos;
	newScrollInfo.fMask = SIF_POS;
	SetScrollInfo(scrollWindow, SB_CTL, &newScrollInfo, TRUE);
}

//----------------------------------------------------------------------------------------
//Render window procedure
//----------------------------------------------------------------------------------------
LRESULT CALLBACK WndProcRenderStatic(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//Obtain the object pointer
	bool state = (bool)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	//Process the message
	switch(msg)
	{
	case WM_CREATE:
		//Set the object pointer
		state = true;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, true);

		//Pass this message on to the member window procedure function
		if(state)
		{
			return WndProcRender(hwnd, msg, wparam, lparam);
		}
		break;
	case WM_DESTROY:
		if(state)
		{
			//Pass this message on to the member window procedure function
			LRESULT result = WndProcRender(hwnd, msg, wparam, lparam);

			//Discard the object pointer
			SetWindowLongPtr(hwnd, GWLP_USERDATA, false);

			//Return the result from processing the message
			return result;
		}
		break;
	}

	//Pass this message on to the member window procedure function
	if(state)
	{
		return WndProcRender(hwnd, msg, wparam, lparam);
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//----------------------------------------------------------------------------------------
LRESULT WndProcRender(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
	case WM_CREATE:
		return msgRenderWM_CREATE(hwnd, wparam, lparam);
	case WM_DESTROY:
		return msgRenderWM_DESTROY(hwnd, wparam, lparam);
	case WM_PAINT:
		needsUpdate = true;
		break;
	case WM_TIMER:
		return msgRenderWM_TIMER(hwnd, wparam, lparam);
	case WM_LBUTTONDOWN:
		return msgRenderWM_LBUTTONDOWN(hwnd, wparam, lparam);
	case WM_KEYUP:
		return msgRenderWM_KEYUP(hwnd, wparam, lparam);
	case WM_KEYDOWN:
		return msgRenderWM_KEYDOWN(hwnd, wparam, lparam);
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//----------------------------------------------------------------------------------------
//Render window event handlers
//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_CREATE(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	real_size = 0;
	ptrRegs = (UINT16 *)find_region("gen_vdp/0/m_regs", ':', &real_size);

	//OpenGL Initialization code
	int screenWidth = DPIScaleWidth(64*8);
	int screenHeight = DPIScaleHeight(64*8);
	glrc = CreateOpenGLWindow(hwnd);
	if(glrc != NULL)
	{
		glViewport(0, 0, screenWidth, screenHeight);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, (float)screenWidth, (float)screenHeight, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	//Allocate a memory buffer for the rendered VRAM data
	int bufferWidth = 64*8;
	int bufferHeight = 64*8;
	buffer = new unsigned char[bufferWidth * bufferHeight * 4];

	SetTimer(hwnd, 1, 1000/15, NULL);

	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_DESTROY(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	if(glrc != NULL)
	{
		wglDeleteContext(glrc);
		glrc = NULL;
	}
	if(buffer != 0)
	{
		delete[] buffer;
		buffer = 0;
	}
	KillTimer(hwnd, 1);

	return DefWindowProc(hwnd, WM_DESTROY, wparam, lparam);
}

//----------------------------------------------------------------------------------------
static void CalculateEffectiveCellScrollSize(unsigned int hszState, unsigned int vszState, unsigned int& effectiveScrollWidth, unsigned int& effectiveScrollHeight)
{
	unsigned int screenSizeModeH = hszState;
	unsigned int screenSizeModeV = ((vszState & 0x1) & ((~hszState & 0x02) >> 1)) | ((vszState & 0x02) & ((~hszState & 0x01) << 1));
	effectiveScrollWidth = (screenSizeModeH + 1) * 32;
	effectiveScrollHeight = (screenSizeModeV + 1) * 32;
	if (screenSizeModeH == 2)
	{
		effectiveScrollWidth = 32;
		effectiveScrollHeight = 1;
	}
	else if (screenSizeModeV == 2)
	{
		effectiveScrollWidth = 32;
		effectiveScrollHeight = 32;
	}
}

static UINT16 get_vdp_reg_val(char idx)
{
	return get_vdp_reg_value((register_t)(R_DR00 + idx), ptrRegs, real_size);
}

//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_TIMER(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	initializedDialog = true;

	//Read the current state of all required registers
	bool h40ModeActive = mask_get(get_vdp_reg_val(0x0C), 0);
	bool v30ModeActive = mask_get(get_vdp_reg_val(0x01), 3);
	bool interlacingActive = mask_get(get_vdp_reg_val(0x0C), 1);
	bool interlaceMode2Active = interlacingActive && mask_get(get_vdp_reg_val(0x0C), 2);
	bool vscrState = mask_get(get_vdp_reg_val(0x0B), 2);
	bool hscrState = mask_get(get_vdp_reg_val(0x0B), 1);
	bool lscrState = mask_get(get_vdp_reg_val(0x0B), 0);

	bool extendedVRAMModeEnabled = mask_get(get_vdp_reg_val(0x01), 7);
	unsigned int hscrollDataBase = mask_get(get_vdp_reg_val(0x0D), 0, (extendedVRAMModeEnabled) ? 7 : 6) << 10;

	unsigned int hszState = mask_get(get_vdp_reg_val(0x10), 0, 2);
	unsigned int vszState = mask_get(get_vdp_reg_val(0x10), 4, 2);
	unsigned int paletteRowBackground = mask_get(get_vdp_reg_val(0x07), 4, 2);
	unsigned int paletteIndexBackground = mask_get(get_vdp_reg_val(0x07), 0, 4);

	//Constants
	const unsigned int blockPixelSizeX = 8;
	const unsigned int blockPixelSizeY = (interlaceMode2Active)? 16: 8;
	const unsigned int width = 64*8;
	const unsigned int height = 64*8;
	const unsigned int screenWidthInCells = (h40ModeActive)? 40: 32;
	const unsigned int screenWidthInPixels = screenWidthInCells * blockPixelSizeX;
	const unsigned int screenHeightInCells = (v30ModeActive)? 30: 28;
	const unsigned int screenHeightInPixels = screenHeightInCells * blockPixelSizeY;

	//Calculate the effective width and height of the main scroll planes based on the
	//current register settings
	unsigned int currentRegisterScrollPlaneWidth;
	unsigned int currentRegisterScrollPlaneHeight;
	CalculateEffectiveCellScrollSize(hszState, vszState, currentRegisterScrollPlaneWidth, currentRegisterScrollPlaneHeight);

	//Latch new settings for the layer A plane where required
	if(!layerAScrollPlaneManual)
	{
		layerAScrollPlaneWidth = currentRegisterScrollPlaneWidth;
		layerAScrollPlaneHeight = currentRegisterScrollPlaneHeight;
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERA, layerAScrollPlaneWidth);
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERA, layerAScrollPlaneHeight);
	}
	bool mode4Enabled = !(mask_get(get_vdp_reg_val(0x01), 2));
	if(!layerAMappingBaseManual)
	{
		if (mode4Enabled)
		{
			layerAMappingBase = mask_get(get_vdp_reg_val(0x02), 1, 3) << 11;
		}
		layerAMappingBase = mask_get(get_vdp_reg_val(0x02), 3, (extendedVRAMModeEnabled) ? 4 : 3) << 13;

		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_MAPPINGLAYERA, 5, layerAMappingBase);
	}
	if(!layerAPatternBaseManual)
	{
		if (extendedVRAMModeEnabled)
		{
			layerAPatternBase = mask_get(get_vdp_reg_val(0x0E), 0, 1) << 16;
		}
		layerAPatternBase = 0;

		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PATTERNLAYERA, 5, layerAPatternBase);
	}

	//Latch new settings for the layer B plane where required
	if(!layerBScrollPlaneManual)
	{
		layerBScrollPlaneWidth = currentRegisterScrollPlaneWidth;
		layerBScrollPlaneHeight = currentRegisterScrollPlaneHeight;
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEWIDTHLAYERB, layerBScrollPlaneWidth);
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEHEIGHTLAYERB, layerBScrollPlaneHeight);
	}
	if(!layerBMappingBaseManual)
	{
		layerBMappingBase = mask_get(get_vdp_reg_val(0x04), 0, (extendedVRAMModeEnabled) ? 4 : 3) << 13;
		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_MAPPINGLAYERB, 5, layerBMappingBase);
	}
	if(!layerBPatternBaseManual)
	{
		if (extendedVRAMModeEnabled)
		{
			layerBPatternBase = layerAPatternBase & (mask_get(get_vdp_reg_val(0x0E), 4, 1) << 16);
		}
		layerBPatternBase = 0;

		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PATTERNLAYERB, 5, layerBPatternBase);
	}

	//Latch new settings for the window plane where required
	if(!windowScrollPlaneManual)
	{
		windowScrollPlaneWidth = h40ModeActive? 64: 32;
		windowScrollPlaneHeight = 32;
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEWIDTHWINDOW, windowScrollPlaneWidth);
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEHEIGHTWINDOW, windowScrollPlaneHeight);
	}
	if(!windowMappingBaseManual)
	{
		if (h40ModeActive)
		{
			windowMappingBase = mask_get(get_vdp_reg_val(0x03), 2, (extendedVRAMModeEnabled) ? 5 : 4) << 12;
		}
		windowMappingBase = mask_get(get_vdp_reg_val(0x03),1, (extendedVRAMModeEnabled) ? 6 : 5) << 11;

		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_MAPPINGWINDOW, 5, windowMappingBase);
	}
	if(!windowPatternBaseManual)
	{
		windowPatternBase = layerAPatternBase;
		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PATTERNWINDOW, 5, windowPatternBase);
	}

	//Latch new settings for the sprite plane where required
	if(!spriteScrollPlaneManual)
	{
		const unsigned int spritePosBitCountH = 9;
		const unsigned int spritePosBitCountV = (interlaceMode2Active)? 10: 9;
		spriteScrollPlaneWidth = (1 << spritePosBitCountH) / blockPixelSizeX;
		spriteScrollPlaneHeight = (1 << spritePosBitCountV) / blockPixelSizeY;
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEWIDTHSPRITES, spriteScrollPlaneWidth);
		UpdateDlgItemBin(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PLANEHEIGHTSPRITES, spriteScrollPlaneHeight);
	}
	if(!spriteMappingBaseManual)
	{
		if (mode4Enabled)
		{
			spriteMappingBase = mask_get(get_vdp_reg_val(0x05), 1, 6) << 8;
		}
		else if (h40ModeActive)
		{
			spriteMappingBase = mask_get(get_vdp_reg_val(0x05), 1, (extendedVRAMModeEnabled) ? 7 : 6) << 10;
		}
		spriteMappingBase = mask_get(get_vdp_reg_val(0x05), 0, (extendedVRAMModeEnabled) ? 8 : 7) << 9;
		
		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_MAPPINGSPRITES, 5, spriteMappingBase);
	}
	if(!spritePatternBaseManual)
	{
		if (mode4Enabled)
		{
			spritePatternBase = mask_get(get_vdp_reg_val(0x06), 2, 1) << 13;
		}
		else if (extendedVRAMModeEnabled)
		{
			spritePatternBase = mask_get(get_vdp_reg_val(0x06), 5, 1) << 16;
		}
		spritePatternBase = 0;

		UpdateDlgItemHex(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_PATTERNSPRITES, 5, spritePatternBase);
	}

	//Get the current horizontal scrollbar settings
	SCROLLINFO hscrollInfoCurrent;
	hscrollInfoCurrent.cbSize = sizeof(hscrollInfoCurrent);
	hscrollInfoCurrent.fMask = SIF_POS | SIF_RANGE;
	GetScrollInfo(GetDlgItem(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_HSCROLL), SB_CTL, &hscrollInfoCurrent);
	int currentScrollPosH = hscrollInfoCurrent.nPos;
	int currentScrollMaxH = hscrollInfoCurrent.nMax;

	//Get the current vertical scrollbar settings
	SCROLLINFO vscrollInfoCurrent;
	vscrollInfoCurrent.cbSize = sizeof(vscrollInfoCurrent);
	vscrollInfoCurrent.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
	GetScrollInfo(GetDlgItem(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_VSCROLL), SB_CTL, &vscrollInfoCurrent);
	int currentScrollPosV = vscrollInfoCurrent.nPos;
	int currentScrollMaxV = vscrollInfoCurrent.nMax;
	unsigned int currentScrollPageSizeV = vscrollInfoCurrent.nPage;

	//Retrieve the current layer size for the target layer
	unsigned int selectedPlaneWidth = 0;
	unsigned int selectedPlaneHeight = 0;
	switch(selectedLayer)
	{
	case SELECTEDLAYER_LAYERA:
		selectedPlaneWidth = layerAScrollPlaneWidth;
		selectedPlaneHeight = layerAScrollPlaneHeight;
		break;
	case SELECTEDLAYER_LAYERB:
		selectedPlaneWidth = layerBScrollPlaneWidth;
		selectedPlaneHeight = layerBScrollPlaneHeight;
		break;
	case SELECTEDLAYER_WINDOW:
		selectedPlaneWidth = windowScrollPlaneWidth;
		selectedPlaneHeight = windowScrollPlaneHeight;
		break;
	case SELECTEDLAYER_SPRITES:
		selectedPlaneWidth = spriteScrollPlaneWidth;
		selectedPlaneHeight = spriteScrollPlaneHeight;
		break;
	}

	//Apply the latest horizontal scrollbar settings
	if(currentScrollMaxH != (int)selectedPlaneWidth)
	{
		SCROLLINFO hscrollInfo;
		hscrollInfo.cbSize = sizeof(hscrollInfo);
		hscrollInfo.nMin = 0;
		hscrollInfo.nMax = (int)(selectedPlaneWidth > 0)? selectedPlaneWidth - 1: 0;
		hscrollInfo.nPos = currentScrollPosH;
		hscrollInfo.nPage = 64;
		hscrollInfo.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
		SetScrollInfo(GetDlgItem(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_HSCROLL), SB_CTL, &hscrollInfo, TRUE);
	}

	//Apply the latest vertical scrollbar settings
	unsigned int newScrollPageSizeV = (interlaceMode2Active)? 32: 64;
	if((currentScrollMaxV != (int)selectedPlaneHeight) || (currentScrollPageSizeV != newScrollPageSizeV))
	{
		SCROLLINFO vscrollInfo;
		vscrollInfo.cbSize = sizeof(vscrollInfo);
		vscrollInfo.nMin = 0;
		vscrollInfo.nMax = (int)(selectedPlaneHeight > 0)? selectedPlaneHeight - 1: 0;
		vscrollInfo.nPos = currentScrollPosV;
		vscrollInfo.nPage = newScrollPageSizeV;
		vscrollInfo.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
		SetScrollInfo(GetDlgItem(GetParent(hwnd), IDC_S315_5313_PLANEVIEW_VSCROLL), SB_CTL, &vscrollInfo, TRUE);
	}

	//Calculate the pixel coordinates of the currently visible window region
	unsigned int renderRegionPixelStartX = currentScrollPosH * blockPixelSizeX;
	unsigned int renderRegionPixelStartY = currentScrollPosV * blockPixelSizeY;
	unsigned int renderRegionPixelEndX = (currentScrollPosH * blockPixelSizeX) + width;
	unsigned int renderRegionPixelEndY = (currentScrollPosV * blockPixelSizeY) + height;

	//Obtain a copy of the current VRAM data buffer
	dump_vram();
	dump_cram();
	dump_vsram();

	//Fill the plane render buffer
	for(unsigned int ypos = 0; ypos < height; ++ypos)
	{
		for(unsigned int xpos = 0; xpos < width; ++xpos)
		{
			//Calculate the position of the current pixel in the currently selected layer
			unsigned int layerPixelPosX = renderRegionPixelStartX + xpos;
			unsigned int layerPixelPosY = renderRegionPixelStartY + ypos;

			//Retrieve the pixel value for the selected layer at the current position
			bool outsideSelectedPlane = true;
			unsigned int paletteRow = 0;
			unsigned int paletteIndex = 0;
			switch(selectedLayer)
			{
			case SELECTEDLAYER_LAYERA:
				GetScrollPlanePaletteInfo(ptrVRAM, layerAMappingBase, layerAPatternBase, layerAScrollPlaneWidth, layerAScrollPlaneHeight, layerPixelPosX, layerPixelPosY, interlaceMode2Active, paletteRow, paletteIndex);
				outsideSelectedPlane = (layerPixelPosX >= (layerAScrollPlaneWidth * blockPixelSizeX)) || (layerPixelPosY >= (layerAScrollPlaneHeight * blockPixelSizeY));
				break;
			case SELECTEDLAYER_LAYERB:
				GetScrollPlanePaletteInfo(ptrVRAM, layerBMappingBase, layerBPatternBase, layerBScrollPlaneWidth, layerBScrollPlaneHeight, layerPixelPosX, layerPixelPosY, interlaceMode2Active, paletteRow, paletteIndex);
				outsideSelectedPlane = (layerPixelPosX >= (layerBScrollPlaneWidth * blockPixelSizeX)) || (layerPixelPosY >= (layerBScrollPlaneHeight * blockPixelSizeY));
				break;
			case SELECTEDLAYER_WINDOW:
				GetScrollPlanePaletteInfo(ptrVRAM, windowMappingBase, windowPatternBase, windowScrollPlaneWidth, windowScrollPlaneHeight, layerPixelPosX, layerPixelPosY, interlaceMode2Active, paletteRow, paletteIndex);
				outsideSelectedPlane = (layerPixelPosX >= (windowScrollPlaneWidth * blockPixelSizeX)) || (layerPixelPosY >= (windowScrollPlaneHeight * blockPixelSizeY));
				break;
			case SELECTEDLAYER_SPRITES:
				//If the sprite plane is selected, just fill the buffer with the backdrop
				//colour where appropriate for now. We'll fill in sprite data later.
				paletteRow = 0;
				paletteIndex = 0;
				outsideSelectedPlane = (layerPixelPosX >= (spriteScrollPlaneWidth * blockPixelSizeX)) || (layerPixelPosY >= (spriteScrollPlaneHeight * blockPixelSizeY));
				break;
			}

			//Set the initial state for the output colour for this pixel
			unsigned char colorR = 0;
			unsigned char colorG = 0;
			unsigned char colorB = 0;

			//If this pixel isn't outside the boundaries of the currently selected plane,
			//decode the selected palette colour value for this pixel.
			if(!outsideSelectedPlane)
			{
				//If the pixel for the selected layer is transparent, select the current
				//background colour.
				if(paletteIndex == 0)
				{
					paletteRow = paletteRowBackground;
					paletteIndex = paletteIndexBackground;
				}

				//Decode the colour for the target palette entry
				COLORREF color = get_color(ptrCRAM, paletteRow + paletteIndex * 16);
				colorR = GetRValue(color);
				colorG = GetGValue(color);
				colorB = GetBValue(color);
			}

			//Write this pixel colour value into the data buffer
			buffer[((xpos + (((height-1)-ypos) * width)) * 4) + 0] = colorR;
			buffer[((xpos + (((height-1)-ypos) * width)) * 4) + 1] = colorG;
			buffer[((xpos + (((height-1)-ypos) * width)) * 4) + 2] = colorB;
			buffer[((xpos + (((height-1)-ypos) * width)) * 4) + 3] = 0xFF;
		}
	}

	//If the sprite plane is currently selected, render each sprite to the buffer.
	std::vector<ScreenBoundaryPrimitive> screenBoundaryPrimitives;
	if(selectedLayer == SELECTEDLAYER_SPRITES)
	{
		//Render each sprite to the sprite plane
		unsigned int maxSpriteCount = (h40ModeActive)? 80: 64;
		unsigned int currentSpriteNo = 0;
		std::set<unsigned int> processedSprites;
		do
		{
			//Read the mapping data for this sprite
			SpriteMappingTableEntry spriteMapping = GetSpriteMappingTableEntry(spriteMappingBase, currentSpriteNo);

			//Render this sprite to the buffer
			unsigned int spriteHeightInCells = spriteMapping.height + 1;
			unsigned int spriteWidthInCells = spriteMapping.width + 1;
			for(unsigned int ypos = 0; ypos < (spriteHeightInCells * blockPixelSizeY); ++ypos)
			{
				for(unsigned int xpos = 0; xpos < (spriteWidthInCells * blockPixelSizeX); ++xpos)
				{
					//If this sprite pixel lies outside the visible buffer region, skip
					//it.
					if(((spriteMapping.xpos + xpos) < renderRegionPixelStartX) || ((spriteMapping.xpos + xpos) >= renderRegionPixelEndX)
					|| ((spriteMapping.ypos + ypos) < renderRegionPixelStartY) || ((spriteMapping.ypos + ypos) >= renderRegionPixelEndY))
					{
						continue;
					}

					//Calculate the target pixel row and column number within the sprite
					unsigned int pixelRowNo = (spriteMapping.vflip)? ((spriteHeightInCells * blockPixelSizeY) - 1) - ypos: ypos;
					unsigned int pixelColumnNo = (spriteMapping.hflip)? ((spriteWidthInCells * blockPixelSizeX) - 1) - xpos: xpos;

					//Calculate the row and column numbers for the target block within the
					//sprite, and the target pattern data within that block.
					unsigned int blockRowNo = pixelRowNo / blockPixelSizeY;
					unsigned int blockColumnNo = pixelColumnNo / blockPixelSizeX;
					unsigned int blockOffset = (blockColumnNo * spriteHeightInCells) + blockRowNo;
					unsigned int patternRowNo = pixelRowNo % blockPixelSizeY;
					unsigned int patternColumnNo = pixelColumnNo % blockPixelSizeX;

					//Calculate the VRAM address of the target pattern row data
					const unsigned int patternDataRowByteSize = 4;
					const unsigned int blockPatternByteSize = blockPixelSizeY * patternDataRowByteSize;
					unsigned int patternRowDataAddress = (((spriteMapping.blockNumber + blockOffset) * blockPatternByteSize) + (patternRowNo * patternDataRowByteSize)) % (unsigned int)vram_size;
					patternRowDataAddress = (spritePatternBase + patternRowDataAddress) % (unsigned int)vram_size;

					//Read the pattern data byte for the target pixel in the target block
					const unsigned int pixelsPerPatternByte = 2;
					unsigned int patternByteNo = patternColumnNo / pixelsPerPatternByte;
					bool patternDataUpperHalf = (patternColumnNo % pixelsPerPatternByte) == 0;
					UINT8 patternData = ptrVRAM[patternRowDataAddress + patternByteNo];

					//Return the target palette row and index numbers
					unsigned int paletteRow = spriteMapping.paletteLine;
					unsigned int paletteIndex = mask_get(patternData, (patternDataUpperHalf)? 4: 0, 4);

					//If this pixel is transparent, skip it.
					if(paletteIndex == 0)
					{
						continue;
					}

					//Decode the colour for the target palette entry
					COLORREF color = get_color(ptrCRAM, paletteRow + paletteIndex * 16);
					unsigned char colorR = GetRValue(color);
					unsigned char colorG = GetGValue(color);
					unsigned char colorB = GetBValue(color);

					//Calculate the location of the target pixel within the data buffer
					unsigned int spritePixelPosXInBuffer = (spriteMapping.xpos + xpos) - renderRegionPixelStartX;
					unsigned int spritePixelPosYInBuffer = (spriteMapping.ypos + ypos) - renderRegionPixelStartY;

					//Write this pixel colour value into the data buffer
					buffer[((spritePixelPosXInBuffer + (((height-1)-spritePixelPosYInBuffer) * width)) * 4) + 0] = colorR;
					buffer[((spritePixelPosXInBuffer + (((height-1)-spritePixelPosYInBuffer) * width)) * 4) + 1] = colorG;
					buffer[((spritePixelPosXInBuffer + (((height-1)-spritePixelPosYInBuffer) * width)) * 4) + 2] = colorB;
					buffer[((spritePixelPosXInBuffer + (((height-1)-spritePixelPosYInBuffer) * width)) * 4) + 3] = 0xFF;
				}
			}

			//Calculate the boundaries of this sprite if requested
			if(spriteBoundaries)
			{
				unsigned int spritePosStartX = spriteMapping.xpos;
				unsigned int spritePosStartY = spriteMapping.ypos;
				unsigned int spritePosEndX = spriteMapping.xpos + (spriteWidthInCells * blockPixelSizeX);
				unsigned int spritePosEndY = spriteMapping.ypos + (spriteHeightInCells * blockPixelSizeY);
				screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosStartX, spritePosStartX, spritePosStartY, spritePosEndY, false, false));
				screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosEndX, spritePosEndX, spritePosStartY, spritePosEndY, false, false));
				screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosStartX, spritePosEndX, spritePosStartY, spritePosStartY, false, false));
				screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosStartX, spritePosEndX, spritePosEndY, spritePosEndY, false, false));
			}

			//Advance to the next sprite in the list
			processedSprites.insert(currentSpriteNo);
			currentSpriteNo = spriteMapping.link;
		}
		while((currentSpriteNo > 0) && (currentSpriteNo < maxSpriteCount) && (processedSprites.find(currentSpriteNo) == processedSprites.end()));
	}

	//Calculate the screen boundary region information for the target layer, if requested.
	if(displayScreen)
	{
		if((selectedLayer == SELECTEDLAYER_LAYERA) || (selectedLayer == SELECTEDLAYER_LAYERB))
		{
			for(unsigned int screenRow = 0; screenRow < screenHeightInPixels; ++screenRow)
			{
				unsigned int pixelsPerColumn = 2 * blockPixelSizeX;
				unsigned int screenColumnCount = (screenWidthInCells / 2);
				for(unsigned int screenColumn = 0; screenColumn < screenColumnCount; ++screenColumn)
				{
					//Calculate the properties for the selected layer
					bool inLayerA = (selectedLayer == SELECTEDLAYER_LAYERA);
					unsigned int layerScrollPlaneWidthInPixels = ((selectedLayer == SELECTEDLAYER_LAYERA)? layerAScrollPlaneWidth: layerBScrollPlaneWidth) * blockPixelSizeX;
					unsigned int layerScrollPlaneHeightInPixels = ((selectedLayer == SELECTEDLAYER_LAYERA)? layerAScrollPlaneHeight: layerBScrollPlaneHeight) * blockPixelSizeY;

					//Read the vertical scroll data for this column
					UINT16 vsramDataCache = 0;
					unsigned int layerVScrollPatternDisplacement;
					unsigned int layerVScrollMappingDisplacement;
					DigitalRenderReadVscrollData(screenColumn, (inLayerA)? 0: 1, vscrState, interlaceMode2Active, layerVScrollPatternDisplacement, layerVScrollMappingDisplacement, vsramDataCache);

					//Read the horizontal scroll data for this row
					unsigned int layerHScrollPatternDisplacement;
					unsigned int layerHScrollMappingDisplacement;
					GetScrollPlaneHScrollData(ptrVRAM, screenRow, hscrollDataBase, hscrState, lscrState, inLayerA, layerHScrollPatternDisplacement, layerHScrollMappingDisplacement);

					//Calculate the screen boundaries within the selected layer using the
					//scroll data for the current column and row
					unsigned int layerPosScreenStartX = (layerScrollPlaneWidthInPixels - (((layerHScrollMappingDisplacement * pixelsPerColumn) + layerHScrollPatternDisplacement) % layerScrollPlaneWidthInPixels)) % layerScrollPlaneWidthInPixels;
					unsigned int layerPosScreenEndX = (layerPosScreenStartX + screenWidthInPixels) % layerScrollPlaneWidthInPixels;
					unsigned int layerPosScreenStartY = ((layerVScrollMappingDisplacement * blockPixelSizeY) + layerVScrollPatternDisplacement) % layerScrollPlaneHeightInPixels;
					unsigned int layerPosScreenEndY = (layerPosScreenStartY + screenHeightInPixels) % layerScrollPlaneHeightInPixels;
					unsigned int layerPosScreenLastRow = ((layerPosScreenEndY + layerScrollPlaneHeightInPixels) - 1) % layerScrollPlaneHeightInPixels;

					//Calculate the position of the current row and column within the
					//selected layer
					unsigned int layerPixelPosX = (layerPosScreenStartX + (screenColumn * pixelsPerColumn)) % layerScrollPlaneWidthInPixels;
					unsigned int layerPixelPosY = (layerPosScreenStartY + screenRow) % layerScrollPlaneHeightInPixels;

					//Draw the screen boundary line for the left or right of the screen
					//region, if required.
					if((layerPixelPosX == layerPosScreenStartX)
					&& (((layerPosScreenStartY < layerPosScreenEndY) && (layerPixelPosY >= layerPosScreenStartY) && (layerPixelPosY < layerPosScreenEndY))
					|| ((layerPosScreenStartY > layerPosScreenEndY) && ((layerPixelPosY >= layerPosScreenStartY) || (layerPixelPosY < layerPosScreenEndY)))))
					{
						screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(layerPixelPosX+1, layerPixelPosX+1, layerPixelPosY, layerPixelPosY+1));
					}
					if((((layerPixelPosX + pixelsPerColumn) % layerScrollPlaneWidthInPixels) == layerPosScreenEndX)
					&& (((layerPosScreenStartY < layerPosScreenEndY) && (layerPixelPosY >= layerPosScreenStartY) && (layerPixelPosY < layerPosScreenEndY))
					|| ((layerPosScreenStartY > layerPosScreenEndY) && ((layerPixelPosY >= layerPosScreenStartY) || (layerPixelPosY < layerPosScreenEndY)))))
					{
						unsigned int linePosX = (layerPixelPosX + pixelsPerColumn + 1) % layerScrollPlaneWidthInPixels;
						screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(linePosX, linePosX, layerPixelPosY, layerPixelPosY+1));
					}

					//Draw the screen boundary line for the top or the bottom of the
					//screen region, if required.
					if(((layerPixelPosY == layerPosScreenStartY) || (layerPixelPosY == layerPosScreenLastRow))
					&& (((layerPosScreenStartX < layerPosScreenEndX) && (layerPixelPosX >= layerPosScreenStartX) && (layerPixelPosX < layerPosScreenEndX))
					|| ((layerPosScreenStartX > layerPosScreenEndX) && ((layerPixelPosX >= layerPosScreenStartX) || (layerPixelPosX < layerPosScreenEndX)))))
					{
						//If the current column wraps around to the start of the layer,
						//split the boundary line, otherwise draw a single line to the end
						//of the column.
						if((layerPixelPosX + pixelsPerColumn) > layerScrollPlaneWidthInPixels)
						{
							screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(layerPixelPosX, layerPixelPosX+pixelsPerColumn, layerPixelPosY, layerPixelPosY));
							screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(0, (layerPixelPosX+pixelsPerColumn) % layerScrollPlaneWidthInPixels, layerPixelPosY, layerPixelPosY));
						}
						else
						{
							screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(layerPixelPosX, layerPixelPosX+pixelsPerColumn, layerPixelPosY, layerPixelPosY));
						}
					}

					//Calculate the shaded region within the window for the current row
					//and column. If the column wraps around to the start of the layer,
					//split the shaded region, otherwise draw it as a single block.
					if((layerPixelPosX + pixelsPerColumn) > layerScrollPlaneWidthInPixels)
					{
						screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(layerPixelPosX, layerPixelPosX+pixelsPerColumn, layerPixelPosY, layerPixelPosY+1, true));
						screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(0, (layerPixelPosX+pixelsPerColumn) % layerScrollPlaneWidthInPixels, layerPixelPosY, layerPixelPosY+1, true));
					}
					else
					{
						screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(layerPixelPosX, layerPixelPosX+pixelsPerColumn, layerPixelPosY, layerPixelPosY+1, true));
					}
				}
			}
		}
		else if(selectedLayer == SELECTEDLAYER_WINDOW)
		{
			//Calculate the screen boundary region for the window layer
			unsigned int windowPosScreenStartX = 0;
			unsigned int windowPosScreenStartY = 0;
			unsigned int windowPosScreenEndX = (windowPosScreenStartX + screenWidthInPixels);
			unsigned int windowPosScreenEndY = (windowPosScreenStartY + screenHeightInPixels);
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(windowPosScreenStartX, windowPosScreenStartX, windowPosScreenStartY, windowPosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(windowPosScreenEndX, windowPosScreenEndX, windowPosScreenStartY, windowPosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(windowPosScreenStartX, windowPosScreenEndX, windowPosScreenStartY, windowPosScreenStartY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(windowPosScreenStartX, windowPosScreenEndX, windowPosScreenEndY, windowPosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(windowPosScreenStartX, windowPosScreenEndX, windowPosScreenStartY, windowPosScreenEndY, true));
		}
		else if(selectedLayer == SELECTEDLAYER_SPRITES)
		{
			//Calculate the screen boundary region for the sprite layer
			unsigned int spritePosScreenStartX = 0x80;
			unsigned int spritePosScreenStartY = (interlaceMode2Active)? 0x100: 0x80;
			unsigned int spritePosScreenEndX = (spritePosScreenStartX + screenWidthInPixels);
			unsigned int spritePosScreenEndY = (spritePosScreenStartY + screenHeightInPixels);
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosScreenStartX, spritePosScreenStartX, spritePosScreenStartY, spritePosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosScreenEndX, spritePosScreenEndX, spritePosScreenStartY, spritePosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosScreenStartX, spritePosScreenEndX, spritePosScreenStartY, spritePosScreenStartY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosScreenStartX, spritePosScreenEndX, spritePosScreenEndY, spritePosScreenEndY));
			screenBoundaryPrimitives.push_back(ScreenBoundaryPrimitive(spritePosScreenStartX, spritePosScreenEndX, spritePosScreenStartY, spritePosScreenEndY, true));
		}
	}

	//Adjust all screen boundary primitives to take scrolling and limiting of the render
	//region into account
	std::vector<ScreenBoundaryPrimitive> validScreenBoundaryLines;
	std::vector<ScreenBoundaryPrimitive> validScreenBoundaryQuads;
	for(unsigned int i = 0; i < (unsigned int)screenBoundaryPrimitives.size(); ++i)
	{
		//Take a copy of this screen boundary primitive
		ScreenBoundaryPrimitive primitive = screenBoundaryPrimitives[i];

		//If this primitive is entirely outside the visible window region, skip it.
		if((primitive.pixelPosXBegin >= renderRegionPixelEndX) || (primitive.pixelPosXEnd < renderRegionPixelStartX)
		|| (primitive.pixelPosYBegin >= renderRegionPixelEndY) || (primitive.pixelPosYEnd < renderRegionPixelStartY))
		{
			continue;
		}

		//Clamp the primitive boundaries to constrain them to the visible render window
		//region
		primitive.pixelPosXBegin = (primitive.pixelPosXBegin < renderRegionPixelStartX)? renderRegionPixelStartX: primitive.pixelPosXBegin;
		primitive.pixelPosXEnd = (primitive.pixelPosXEnd > renderRegionPixelEndX)? renderRegionPixelEndX: primitive.pixelPosXEnd;
		primitive.pixelPosYBegin = (primitive.pixelPosYBegin < renderRegionPixelStartY)? renderRegionPixelStartY: primitive.pixelPosYBegin;
		primitive.pixelPosYEnd = (primitive.pixelPosYEnd > renderRegionPixelEndY)? renderRegionPixelEndY: primitive.pixelPosYEnd;

		//Convert this primitive to be relative to the visible render window region
		primitive.pixelPosXBegin -= renderRegionPixelStartX;
		primitive.pixelPosXEnd -= renderRegionPixelStartX;
		primitive.pixelPosYBegin -= renderRegionPixelStartY;
		primitive.pixelPosYEnd -= renderRegionPixelStartY;

		//Save the modified primitive to the appropriate list of valid primitive
		//definitions
		if(primitive.primitiveIsPolygon)
		{
			validScreenBoundaryQuads.push_back(primitive);
		}
		else
		{
			validScreenBoundaryLines.push_back(primitive);
		}
	}

	//Draw the rendered buffer data to the window
	HDC hdc = GetDC(hwnd);
	if (hdc != NULL)
	{
		bool madeCurrent = true;
		if (needsUpdate)
		{
			madeCurrent = (wglMakeCurrent(hdc, glrc) != FALSE);
			needsUpdate = false;
		}

		if ((glrc != NULL) && madeCurrent)
		{
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			//If a nonstandard DPI mode is active, scale the pixel image based on the
			//current DPI settings.
			float dpiScaleX;
			float dpiScaleY;
			DPIGetScreenScaleFactors(dpiScaleX, dpiScaleY);
			glPixelZoom(dpiScaleX, dpiScaleY);

			//Draw the pixel buffer to the screen
			glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

			//Draw our screen boundary lines
			for (unsigned int i = 0; i < (unsigned int)validScreenBoundaryLines.size(); ++i)
			{
				//Set the colour for this line
				if (validScreenBoundaryLines[i].primitiveIsScreenBoundary)
				{
					glColor3d(0.0, 1.0, 0.0);
				}
				else
				{
					glColor3d(1.0, 1.0, 1.0);
				}

				//Draw the line
				glBegin(GL_LINES);
				glVertex2i(validScreenBoundaryLines[i].pixelPosXBegin, validScreenBoundaryLines[i].pixelPosYBegin);
				glVertex2i(validScreenBoundaryLines[i].pixelPosXEnd, validScreenBoundaryLines[i].pixelPosYEnd);
				glEnd();
			}
			glColor3d(1.0, 1.0, 1.0);

			//Draw our screen boundary quads
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glColor4d(0.0, 1.0, 0.0, 0.5);
			for (unsigned int i = 0; i < (unsigned int)validScreenBoundaryQuads.size(); ++i)
			{
				glBegin(GL_QUADS);
				glVertex2i(validScreenBoundaryQuads[i].pixelPosXBegin, validScreenBoundaryQuads[i].pixelPosYBegin);
				glVertex2i(validScreenBoundaryQuads[i].pixelPosXEnd, validScreenBoundaryQuads[i].pixelPosYBegin);
				glVertex2i(validScreenBoundaryQuads[i].pixelPosXEnd, validScreenBoundaryQuads[i].pixelPosYEnd);
				glVertex2i(validScreenBoundaryQuads[i].pixelPosXBegin, validScreenBoundaryQuads[i].pixelPosYEnd);
				glEnd();
			}
			glColor4d(1.0, 1.0, 1.0, 1.0);
			glDisable(GL_BLEND);

			glFlush();
			SwapBuffers(hdc);
		}
		ReleaseDC(hwnd, hdc);
	}

	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_LBUTTONDOWN(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	SetFocus(hwnd);
	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_KEYUP(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	/*ISystemDeviceInterface::KeyCode keyCode;
	if(presenter.GetSystemInterface().TranslateKeyCode((unsigned int)wparam, keyCode))
	{
		presenter.GetSystemInterface().HandleInputKeyUp(keyCode);
	}*/
	return 0;
}

//----------------------------------------------------------------------------------------
LRESULT msgRenderWM_KEYDOWN(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	/*ISystemDeviceInterface::KeyCode keyCode;
	if(presenter.GetSystemInterface().TranslateKeyCode((unsigned int)wparam, keyCode))
	{
		presenter.GetSystemInterface().HandleInputKeyDown(keyCode);
	}*/
	return 0;
}

//----------------------------------------------------------------------------------------
//Render helper methods
//----------------------------------------------------------------------------------------
static void GetScrollPlanePaletteInfo(UINT8* vramData, unsigned int mappingBaseAddress, unsigned int patternBaseAddress, unsigned int planeWidth, unsigned int planeHeight, unsigned int xpos, unsigned int ypos, bool interlaceMode2Active, unsigned int& paletteRow, unsigned int& paletteIndex)
{
	//Constants
	const unsigned int mappingByteSize = 2;
	const unsigned int pixelsPerPatternByte = 2;
	unsigned int blockPixelSizeX = 3;
	unsigned int blockPixelSizeY = (interlaceMode2Active)? 4: 3;

	//Determine the address of the mapping data to use for this layer
	unsigned int mappingIndex = (((ypos >> blockPixelSizeY) % planeHeight) * planeWidth) + ((xpos >> blockPixelSizeX) % planeWidth);
	unsigned int mappingAddress = (mappingBaseAddress + (mappingIndex * mappingByteSize)) % (unsigned int)vram_size;

	//Read the mapping data
	UINT16 mappingData = 0;
	mappingData = (vramData[mappingAddress + 0] << 8) | vramData[mappingAddress + 1];

	//Determine the address of the target row in the target block
	unsigned int patternRowNumberNoFlip = ypos % blockPixelSizeY;
	unsigned int patternRowNumber = CalculatePatternDataRowNumber(patternRowNumberNoFlip, interlaceMode2Active, mappingData);
	unsigned int patternRowDataAddress = CalculatePatternDataRowAddress(patternRowNumber, 0, interlaceMode2Active, mappingData);
	patternRowDataAddress = (patternBaseAddress + patternRowDataAddress) % (unsigned int)vram_size;

	//Read the pattern data byte for the target pixel in the target block
	bool patternHFlip = mask_get(mappingData, 11);
	unsigned int patternColumnNo = (patternHFlip)? (blockPixelSizeX - 1) - (xpos % blockPixelSizeX): (xpos % blockPixelSizeX);
	unsigned int patternByteNo = patternColumnNo / pixelsPerPatternByte;
	bool patternDataUpperHalf = (patternColumnNo % pixelsPerPatternByte) == 0;
	UINT8 patternData = vramData[patternRowDataAddress + patternByteNo];

	//Return the target palette row and index numbers
	paletteRow = mask_get(mappingData, 13, 2);
	paletteIndex = mask_get(patternData, (patternDataUpperHalf) ? 4 : 0, 4);
}

//----------------------------------------------------------------------------------------
static void GetScrollPlaneHScrollData(UINT8* vramData, unsigned int screenRowNumber, unsigned int hscrollDataBase, bool hscrState, bool lscrState, bool layerA, unsigned int& layerHscrollPatternDisplacement, unsigned int& layerHscrollMappingDisplacement)
{
	//Calculate the address of the hscroll data to read for this line
	unsigned int hscrollDataAddress = hscrollDataBase;
	if(hscrState)
	{
		const unsigned int hscrollDataPairSize = 4;
		const unsigned int blockPixelSizeY = 8;
		hscrollDataAddress += lscrState? (screenRowNumber * hscrollDataPairSize): (((screenRowNumber / blockPixelSizeY) * blockPixelSizeY) * hscrollDataPairSize);
	}
	if(!layerA)
	{
		hscrollDataAddress += 2;
	}

	//Read the hscroll data for this line
	unsigned int layerHscrollOffset = ((unsigned int)vramData[(hscrollDataAddress + 0) % (unsigned int)vram_size] << 8) | (unsigned int)vramData[(hscrollDataAddress + 1) % (unsigned int)vram_size];

	//Break the hscroll data into its two component parts. The lower 4 bits represent a
	//displacement into the 2-cell column, or in other words, the displacement of the
	//starting pixel within each column, while the upper 6 bits represent an offset for
	//the column mapping data itself.
	//-----------------------------------------
	//| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//|---------------------------------------|
	//|  Column Shift Value   | Displacement  |
	//-----------------------------------------
	layerHscrollPatternDisplacement = (layerHscrollOffset & 0x00F);
	layerHscrollMappingDisplacement = (layerHscrollOffset & 0x3F0) >> 4;
}

//----------------------------------------------------------------------------------------
static unsigned int CalculatePatternDataRowNumber(unsigned int patternRowNumberNoFlip, bool interlaceMode2Active, UINT16 mappingData)
{
	//Calculate the final number of the pattern row to read, taking into account vertical
	//flip if it is specified in the block mapping.
	//Mapping (Pattern Name) data format:
	//-----------------------------------------------------------------
	//|15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//|---------------------------------------------------------------|
	//|Pri|PalRow |VF |HF |              Pattern Number               |
	//-----------------------------------------------------------------
	//Pri:    Priority Bit
	//PalRow: The palette row number to use when displaying the pattern data
	//VF:     Vertical Flip
	//HF:     Horizontal Flip
	const unsigned int rowsPerTile = (!interlaceMode2Active) ? 8 : 16;
	unsigned int patternRowNumber = mask_get(mappingData, 12) ? (rowsPerTile - 1) - patternRowNumberNoFlip : patternRowNumberNoFlip;
	return patternRowNumber;
}

//----------------------------------------------------------------------------------------
//Sprite list debugging functions
//----------------------------------------------------------------------------------------
static SpriteMappingTableEntry GetSpriteMappingTableEntry(unsigned int spriteTableBaseAddress, unsigned int entryNo)
{
	//Calculate the address in VRAM of this sprite table entry
	static const unsigned int spriteTableEntrySize = 8;
	unsigned int spriteTableEntryAddress = spriteTableBaseAddress + (entryNo * spriteTableEntrySize);
	spriteTableEntryAddress %= vram_size;

	//Read all raw data for the sprite from the sprite attribute table in VRAM
	SpriteMappingTableEntry entry;
	entry.rawDataWord0 = (ptrVRAM[spriteTableEntryAddress + 0] << 8) | ptrVRAM[spriteTableEntryAddress + 1];
	entry.rawDataWord1 = (ptrVRAM[spriteTableEntryAddress + 2] << 8) | ptrVRAM[spriteTableEntryAddress + 3];
	entry.rawDataWord2 = (ptrVRAM[spriteTableEntryAddress + 4] << 8) | ptrVRAM[spriteTableEntryAddress + 5];
	entry.rawDataWord3 = (ptrVRAM[spriteTableEntryAddress + 6] << 8) | ptrVRAM[spriteTableEntryAddress + 7];

	//Decode the sprite mapping data
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word0  |---------------------------------------------------------------|
	//        |                          Vertical Pos                         |
	//        -----------------------------------------------------------------
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word1  |---------------------------------------------------------------|
	//        | /   /   /   / | HSize | VSize | / |         Link Data         |
	//        -----------------------------------------------------------------
	//        HSize:     Horizontal size of the sprite
	//        VSize:     Vertical size of the sprite
	//        Link Data: Next sprite entry to read from table during sprite rendering
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word2  |---------------------------------------------------------------|
	//        |Pri|PalRow |VF |HF |              Pattern Number               |
	//        -----------------------------------------------------------------
	//        Pri:    Priority Bit
	//        PalRow: The palette row number to use when displaying the pattern data
	//        VF:     Vertical Flip
	//        HF:     Horizontal Flip
	//        Mapping (Pattern Name) data format:
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word3  |---------------------------------------------------------------|
	//        |                         Horizontal Pos                        |
	//        -----------------------------------------------------------------
	entry.ypos = entry.rawDataWord0;
	entry.width = mask_get(entry.rawDataWord1, 10, 2);
	entry.height = mask_get(entry.rawDataWord1, 8, 2);
	entry.link = mask_get(entry.rawDataWord1, 0, 7);
	entry.priority = mask_get(entry.rawDataWord2, 15);
	entry.paletteLine = mask_get(entry.rawDataWord2, 13, 2);
	entry.vflip = mask_get(entry.rawDataWord2, 12);
	entry.hflip = mask_get(entry.rawDataWord2, 11);
	entry.blockNumber = mask_get(entry.rawDataWord2, 0, 11);
	entry.xpos = entry.rawDataWord3;

	return entry;
}

//----------------------------------------------------------------------------------------
static unsigned int CalculatePatternDataRowAddress(unsigned int patternRowNumber, unsigned int patternCellOffset, bool interlaceMode2Active, UINT16 mappingData)
{
	//The address of the pattern data to read is determined by combining the number of the
	//pattern (tile) with the row of the pattern to be read. The way the data is combined
	//is different under interlace mode 2, where patterns are 16 pixels high instead of
	//the usual 8 pixels. The format for pattern data address decoding is as follows when
	//interlace mode 2 is not active:
	//-----------------------------------------------------------------
	//|15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//|---------------------------------------------------------------|
	//|              Pattern Number               |Pattern Row| 0 | 0 |
	//-----------------------------------------------------------------
	//When interlace mode 2 is active, the pattern data address decoding is as follows:
	//-----------------------------------------------------------------
	//|15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//|---------------------------------------------------------------|
	//|            Pattern Number             |  Pattern Row  | 0 | 0 |
	//-----------------------------------------------------------------
	//Note that we grab the entire mapping data block as the block number when calculating
	//the address. This is because the resulting address is wrapped to keep it within the
	//VRAM boundaries. Due to this wrapping, in reality only the lower 11 bits of the
	//mapping data are effective when determining the block number, or the lower 10 bits
	//in the case of interlace mode 2.
	//##TODO## Test the above assertion on the TeraDrive with the larger VRAM mode active
	static const unsigned int patternDataRowByteSize = 4;
	const unsigned int rowsPerTile = (!interlaceMode2Active) ? 8 : 16;
	const unsigned int blockPatternByteSize = rowsPerTile * patternDataRowByteSize;
	unsigned int patternDataAddress = (((mappingData + patternCellOffset) * blockPatternByteSize) + (patternRowNumber * patternDataRowByteSize)) % vram_size;
	return patternDataAddress;
}

//----------------------------------------------------------------------------------------
static void DigitalRenderReadVscrollData(unsigned int screenColumnNumber, unsigned int layerNumber, bool vscrState, bool interlaceMode2Active, unsigned int& layerVscrollPatternDisplacement, unsigned int& layerVscrollMappingDisplacement, UINT16& vsramReadCache)
{
	//Calculate the address of the vscroll data to read for this block
	static const unsigned int vscrollDataLayerCount = 2;
	static const unsigned int vscrollDataEntrySize = 2;
	unsigned int vscrollDataAddress = vscrState ? (screenColumnNumber * vscrollDataLayerCount * vscrollDataEntrySize) + (layerNumber * vscrollDataEntrySize) : (layerNumber * vscrollDataEntrySize);

	//##NOTE## This implements what appears to be the correct behaviour for handling reads
	//past the end of the VSRAM buffer during rendering. This can occur when horizontal
	//scrolling is applied along with vertical scrolling, in which case the leftmost
	//column can be reading data from a screen column of -1, wrapping around to the end of
	//the VSRAM buffer. In this case, the last successfully read value from the VSRAM
	//appears to be used as the read value. This also applies when performing manual reads
	//from VSRAM externally using the data port. See data port reads from VSRAM for more
	//info.
	//##TODO## This needs more through hardware tests, to definitively confirm the correct
	//behaviour.
	if (vscrollDataAddress < 0x50)
	{
		//Read the vscroll data for this line. Note only the lower 10 bits are
		//effective, or the lower 11 bits in the case of interlace mode 2, due to the
		//scrolled address being wrapped to lie within the total field boundaries,
		//which never exceed 128 blocks.
		vsramReadCache = (ptrVSRAM[vscrollDataAddress + 0] << 8) | ptrVSRAM[vscrollDataAddress + 1];
	}
	else
	{
		//##FIX## This is a temporary patch until we complete our hardware testing on the
		//behaviour of VSRAM. Hardware tests do seem to confirm that when the VSRAM read
		//process passes into the undefined upper region of VSRAM, the returned value is
		//the ANDed result of the last two entries in VSRAM.
		vsramReadCache = (ptrVSRAM[0x4C + 0] << 8) | ptrVSRAM[0x4C + 1];
		vsramReadCache &= (ptrVSRAM[0x4E + 0] << 8) | ptrVSRAM[0x4E + 1];
	}

	//Break the vscroll data into its two component parts. The format of the vscroll data
	//varies depending on whether interlace mode 2 is active. When interlace mode 2 is not
	//active, the vscroll data is interpreted as a 10-bit value, where the lower 3 bits
	//represent a vertical shift on the pattern line for the selected block mapping, or in
	//other words, the displacement of the starting row within each pattern, while the
	//upper 7 bits represent an offset for the mapping data itself, like so:
	//------------------------------------------
	//| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0  |
	//|----------------------------------------|
	//|    Column Shift Value     |Displacement|
	//------------------------------------------
	//Where interlace mode 2 is active, pattern data is 8x16 pixels, not 8x8 pixels. In
	//this case, the vscroll data is treated as an 11-bit value, where the lower 4 bits
	//give the row offset, and the upper 7 bits give the mapping offset, like so:
	//---------------------------------------------
	//|10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//|-------------------------------------------|
	//|    Column Shift Value     | Displacement  |
	//---------------------------------------------
	//Note that the unused upper bits in the vscroll data are simply discarded, since they
	//fall outside the maximum virtual playfield size for the mapping data. Since the
	//virtual playfield wraps, this means they have no effect.
	layerVscrollPatternDisplacement = interlaceMode2Active ? mask_get(vsramReadCache, 0, 4) : mask_get(vsramReadCache, 0, 3);
	layerVscrollMappingDisplacement = interlaceMode2Active ? mask_get(vsramReadCache, 4, 7) : mask_get(vsramReadCache, 3, 7);
}
