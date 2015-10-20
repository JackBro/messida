#include "WindowFunctions.h"

#include <sstream>
#include <iomanip>

//----------------------------------------------------------------------------------------
//Control text helper functions
//----------------------------------------------------------------------------------------
void UpdateDlgItemBin(HWND hwnd, int controlID, unsigned int data)
{
	const unsigned int maxTextLength = 1024;
	char currentTextTemp[maxTextLength];
	if (GetDlgItemText(hwnd, controlID, currentTextTemp, maxTextLength) == 0)
	{
		currentTextTemp[0] = L'\0';
	}
	std::string currentText = currentTextTemp;
	std::stringstream text;
	text << data;
	if (text.str() != currentText)
	{
		SetDlgItemText(hwnd, controlID, text.str().c_str());
	}
}

//----------------------------------------------------------------------------------------
unsigned int GetDlgItemBin(HWND hwnd, int controlID)
{
	unsigned int value = 0;

	const unsigned int maxTextLength = 1024;
	char currentTextTemp[maxTextLength];
	if (GetDlgItemText(hwnd, controlID, currentTextTemp, maxTextLength) == 0)
	{
		currentTextTemp[0] = L'\0';
	}
	std::stringstream buffer;
	buffer << currentTextTemp;
	buffer >> value;

	return value;
}

//----------------------------------------------------------------------------------------
void UpdateDlgItemHex(HWND hwnd, int controlID, unsigned int width, unsigned int data)
{
	const unsigned int maxTextLength = 1024;
	char currentTextTemp[maxTextLength];
	if (GetDlgItemText(hwnd, controlID, currentTextTemp, maxTextLength) == 0)
	{
		currentTextTemp[0] = L'\0';
	}
	std::string currentText = currentTextTemp;
	std::stringstream text;
	text << std::setw(width) << std::setfill('0') << std::hex << std::uppercase;
	text << data;
	if (text.str() != currentText)
	{
		SetDlgItemText(hwnd, controlID, text.str().c_str());
	}
}

//----------------------------------------------------------------------------------------
unsigned int GetDlgItemHex(HWND hwnd, int controlID)
{
	unsigned int value = 0;

	const unsigned int maxTextLength = 1024;
	char currentTextTemp[maxTextLength];
	if (GetDlgItemText(hwnd, controlID, currentTextTemp, maxTextLength) == 0)
	{
		currentTextTemp[0] = L'\0';
	}
	std::stringstream buffer;
	buffer << std::hex << currentTextTemp;
	buffer >> value;

	return value;
}

//----------------------------------------------------------------------------------------
void UpdateDlgItemString(HWND hwnd, int controlID, const std::string& data)
{
	SetDlgItemText(hwnd, controlID, data.c_str());
}

//----------------------------------------------------------------------------------------
std::string GetDlgItemString(HWND hwnd, int controlID)
{
	std::string result;

	const unsigned int maxTextLength = 1024;
	char currentTextTemp[maxTextLength];
	if(GetDlgItemText(hwnd, controlID, currentTextTemp, maxTextLength) == 0)
	{
		currentTextTemp[0] = L'\0';
	}
	result = currentTextTemp;

	return result;
}

//----------------------------------------------------------------------------------------
HGLRC CreateOpenGLWindow(HWND hwnd)
{
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cAlphaBits = 0;
	pfd.cAccumBits = 0;
	pfd.cDepthBits = 32;
	pfd.cStencilBits = 0;
	pfd.cAuxBuffers = 0;
	pfd.iLayerType = PFD_MAIN_PLANE;

	HDC deviceContext = GetDC(hwnd);
	if (deviceContext == NULL)
	{
		return NULL;
	}

	GLuint pixelFormat = ChoosePixelFormat(deviceContext, &pfd);
	if (pixelFormat == 0)
	{
		ReleaseDC(hwnd, deviceContext);
		return NULL;
	}

	if (SetPixelFormat(deviceContext, pixelFormat, &pfd) == FALSE)
	{
		ReleaseDC(hwnd, deviceContext);
		return NULL;
	}

	HGLRC renderingContext = wglCreateContext(deviceContext);
	if (renderingContext == NULL)
	{
		ReleaseDC(hwnd, deviceContext);
		return NULL;
	}

	if (wglMakeCurrent(deviceContext, renderingContext) == FALSE)
	{
		wglDeleteContext(renderingContext);
		ReleaseDC(hwnd, deviceContext);
		return NULL;
	}

	ReleaseDC(hwnd, deviceContext);
	return renderingContext;
}

//----------------------------------------------------------------------------------------
void DPIGetScreenSettings(int& dpiX, int& dpiY)
{
	//Obtain the current screen DPI settings
	dpiX = 96;
	dpiY = 96;
	HDC hdc = GetDC(NULL);
	if (hdc)
	{
		dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
		dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(NULL, hdc);
	}
}

//----------------------------------------------------------------------------------------
void DPIGetScreenScaleFactors(float& dpiScaleX, float& dpiScaleY)
{
	//Obtain the current screen DPI settings
	int dpiX;
	int dpiY;
	DPIGetScreenSettings(dpiX, dpiY);

	//Calculate a scale value for pixel values based on the current screen DPI settings
	dpiScaleX = (float)dpiX / 96.0f;
	dpiScaleY = (float)dpiY / 96.0f;
}