#ifndef DIALOG_UTILS_H
#define DIALOG_UTILS_H

int getOpenFileName(const char *title, char *fileName, int nameLen, const char *filter, char *initDir, HWND hWnd);
int getSaveFileName(const char *title, char *fileName, int nameSize, const char *filter, char *initDir, HWND hWnd);

#endif


