#include "ViewBase.h"

//----------------------------------------------------------------------------------------
//Window procedure helper functions
//----------------------------------------------------------------------------------------
void WndProcDialogImplementSaveFieldWhenLostFocus(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		//Make sure no textbox is selected on startup, and remove focus from textboxes when
		//the user clicks an unused area of the window.
	case WM_LBUTTONDOWN:
	case WM_SHOWWINDOW:
		SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(0, EN_SETFOCUS), NULL);
		SetFocus(NULL);
		break;
	}
}