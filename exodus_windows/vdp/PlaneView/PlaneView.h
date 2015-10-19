#ifndef __PLANEVIEW_H__
#define __PLANEVIEW_H__

#include <Windows.h>

#define EXODUS_VDP_PLANE_VIEWER_ID 2
INT_PTR CALLBACK ExodusVdpPlaneViewWndProcDialog(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

//Enumerations
enum SelectedLayer
{
	SELECTEDLAYER_LAYERA,
	SELECTEDLAYER_LAYERB,
	SELECTEDLAYER_WINDOW,
	SELECTEDLAYER_SPRITES
};

//Structures
struct ScreenBoundaryPrimitive
{
	ScreenBoundaryPrimitive(unsigned int apixelPosXBegin, unsigned int apixelPosXEnd, unsigned int apixelPosYBegin, unsigned int apixelPosYEnd, bool aprimitiveIsPolygon = false, bool aprimitiveIsScreenBoundary = true)
		:pixelPosXBegin(apixelPosXBegin), pixelPosXEnd(apixelPosXEnd), pixelPosYBegin(apixelPosYBegin), pixelPosYEnd(apixelPosYEnd), primitiveIsPolygon(aprimitiveIsPolygon), primitiveIsScreenBoundary(aprimitiveIsScreenBoundary)
	{}

	bool primitiveIsPolygon;
	bool primitiveIsScreenBoundary;
	unsigned int pixelPosXBegin;
	unsigned int pixelPosXEnd;
	unsigned int pixelPosYBegin;
	unsigned int pixelPosYEnd;
};

//----------------------------------------------------------------------------------------
struct SpriteMappingTableEntry
{
	SpriteMappingTableEntry()
		:rawDataWord0(16), rawDataWord1(16), rawDataWord2(16), rawDataWord3(16)
	{}

	UINT16 rawDataWord0;
	UINT16 rawDataWord1;
	UINT16 rawDataWord2;
	UINT16 rawDataWord3;

	unsigned int blockNumber;
	unsigned int paletteLine;
	unsigned int xpos;
	unsigned int ypos;
	unsigned int width;
	unsigned int height;
	unsigned int link;
	bool priority;
	bool vflip;
	bool hflip;
};

static size_t real_size;
static UINT16 *ptrRegs;

#endif
