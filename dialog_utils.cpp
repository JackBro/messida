#include <Windows.h>
#include <commdlg.h>
#include "dialog_utils.h"

int getOpenFileName(const char *title, char *fileName, int nameLen, const char *filter, char *initDir, HWND hWnd) {
    OPENFILENAME ofn;
    if (fileName == NULL || nameLen == 0) {
        return NULL;
    }
    memset(&ofn, 0, sizeof(ofn));

    // Initialize OPENFILENAME
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = fileName;
    //
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
    // use the contents of szFile to initialize itself.
    //
    //   *fileName = '\0';
    ofn.nMaxFile = nameLen;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileName(&ofn)) {
        return 1;
    }
    return 0;
}

int getSaveFileName(const char *title, char *fileName, int nameSize, const char *filter, char *initDir, HWND hWnd) {
    OPENFILENAME ofn;
    if (fileName == NULL || nameSize == 0) {
        return NULL;
    }
    memset(&ofn, 0, sizeof(ofn));

    // Initialize OPENFILENAME
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = fileName;
    //
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
    // use the contents of szFile to initialize itself.
    //
    //*fileName = '\0';
    ofn.nMaxFile = nameSize;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileName(&ofn)) {
        return 1;
    }
    return 0;
}