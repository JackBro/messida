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
void UpdateDlgItemString(HWND hwnd, int controlID, const std::string& data);
std::string GetDlgItemString(HWND hwnd, int controlID);

//DPI functions
void DPIGetScreenSettings(int& dpiX, int& dpiY);
void DPIGetScreenScaleFactors(float& dpiScaleX, float& dpiScaleY);
template<class T> T DPIScaleWidth(T pixelWidth);
template<class T> T DPIScaleHeight(T pixelHeight);
template<class T> T DPIReverseScaleWidth(T pixelWidth);
template<class T> T DPIReverseScaleHeight(T pixelHeight);

#include "WindowFunctions.inl"

#endif