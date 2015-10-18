#ifndef WINDOWFUNCTIONS_H
#define WINDOWFUNCTIONS_H

#include <Windows.h>
#include <string>
#include <gl\GL.h>

//Window creation helpers
HGLRC CreateOpenGLWindow(HWND hwnd);

//Control text helper functions
void UpdateDlgItemBin(HWND hwnd, int controlID, unsigned int data);
unsigned int GetDlgItemBin(HWND hwnd, int controlID);
void UpdateDlgItemHex(HWND hwnd, int controlID, unsigned int width, unsigned int data);
unsigned int GetDlgItemHex(HWND hwnd, int controlID);
std::string GetDlgItemString(HWND hwnd, int controlID);

//DPI functions
void DPIGetScreenSettings(int& dpiX, int& dpiY);
void DPIGetScreenScaleFactors(float& dpiScaleX, float& dpiScaleY);
template<class T> T DPIScaleWidth(T pixelWidth);
template<class T> T DPIScaleHeight(T pixelHeight);
template<class T> T DPIReverseScaleWidth(T pixelWidth);
template<class T> T DPIReverseScaleHeight(T pixelHeight);

//----------------------------------------------------------------------------------------
//DPI functions
//----------------------------------------------------------------------------------------
template<class T> T DPIScaleWidth(T pixelWidth)
{
	//Calculate a scale value for pixel values based on the current screen DPI settings
	float dpiScaleX;
	float dpiScaleY;
	DPIGetScreenScaleFactors(dpiScaleX, dpiScaleY);

	//Scale the supplied values based on the DPI scale values
	pixelWidth = (T)(((float)pixelWidth * dpiScaleX) + 0.5f);

	//Return the scaled value
	return pixelWidth;
}

//----------------------------------------------------------------------------------------
template<class T> T DPIScaleHeight(T pixelHeight)
{
	//Calculate a scale value for pixel values based on the current screen DPI settings
	float dpiScaleX;
	float dpiScaleY;
	DPIGetScreenScaleFactors(dpiScaleX, dpiScaleY);

	//Scale the supplied values based on the DPI scale values
	pixelHeight = (T)(((float)pixelHeight * dpiScaleY) + 0.5f);

	//Return the scaled value
	return pixelHeight;
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

#endif