/* This module generates and displays the image windows along with the colorbar */
/* Originally included in main windows interface module.  Moved here since the file size became too large */

// Written by: Frank Robey
// MIT Lincoln Laboratory
// Jan-Dec 2014
// Sept 2016 Restructured display logic. Now allows for larger number of radars rather than just 2.
// Apr 2017          This put data into arrays pointed to by a vector of pointers
// Mar 2018 Removed unused code

/*
RadarRTP - Radar Real time Program (RTP)

© 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License, Version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt

*/

#include "stdafx.h"  // This will include radarc.h, which defines MaxRadars
#include "ImageDisplay.h"
#include "winGUI.h"

#ifndef _RTP_Headless


bool dthreadSyncFlag;  // display thread sync flag - pay attention and stop what you are doing

// The following are storage for pointers to the bitmap images 
unsigned char* lpDTIBits[MaxRadars], * lpRDIBits[MaxRadars];
unsigned char* lpColorBarBits;  //Pointer to bitmap of characters for the color bar.
std::thread tFormatDisplayThread;

#ifdef _WINGUI
// Win32 structures
extern HWND hWndApp;
// Bitmap information - 3 bitmaps needed, Colorbar, DTI2 and RDI
BITMAPINFO bmColorBar, bmDTI, bmRDI;
HBITMAP hColorBar, hDTIBitMap[MaxRadars], hRDIBitMap[MaxRadars];

HWND hwndDopLabel[MaxRadars], hwndDopLabelMin[MaxRadars], hwndDopLabelMax[MaxRadars];
HWND hwndTimeLabel, hwndRangeLabel, hwndCBLabel;
HWND hlblCB1, hlblCB2, hlblCB3;

 void CreateImageWindows(HWND hwnd)
{
	//HDC hDC = 0;

	PAINTSTRUCT ps;
	ZeroMemory(&ps, sizeof(PAINTSTRUCT));
	HPEN hpenCreate = 0;
	RECT rectWind, rectImage;
	int myBorderLeft = 40, myBorderBottom=20;
	int ImWidth, ImHeight;
	int CB_XPos;
	int textWid;
	int BorderX = 10, BorderY = 10;
	wchar_t buf[10];

	GetClientRect(hwnd, &rectWind);
		// Calculate my bounding box
		rectImage.left = rectWind.left + iControlPanelWidth+BorderX+10;
		rectImage.right = rectWind.right- iCBPanelWidth- BorderX*2;
		rectImage.top = rectWind.top;
		rectImage.bottom = rectWind.bottom - iOutPanelHeight;
		ImWidth = rectImage.right - rectImage.left - myBorderLeft;
		ImHeight = rectImage.bottom - rectImage.top - myBorderBottom ;

		textWid = ImWidth / 4;

	// Colorbar text
		CB_XPos = rectImage.right +  BorderX;

	hwndCBLabel = CreateWindowW(L"Static", L"Scale (dB)",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		CB_XPos, rectImage.top + BorderY ,
		iCBPanelWidth, 20, hwnd, (HMENU)1, NULL, NULL);
	// Colorbar is at BorderY + 80
	// These go wherever the label needs to be updated (near the colorbar)
	swprintf(buf, 10, L"%d", (int)gRadarState.DispRange);
	hlblCB1 = CreateWindowW(L"Static", buf, WS_CHILD | WS_VISIBLE | SS_CENTER,
		CB_XPos, rectImage.top + BorderY + 55, iCBPanelWidth, 20, hwnd, (HMENU)3, NULL, NULL);  // This is scale
	hlblCB2 = CreateWindowW(L"Static", L"0", WS_CHILD | WS_VISIBLE | SS_CENTER,
		CB_XPos, rectImage.top + 2 * BorderY + 45+ImHeight/2 +BorderY, iCBPanelWidth, 20, hwnd, (HMENU)3, NULL, NULL);  // This is scale
	swprintf(buf, 10, L"Ref: %d", (int)gRadarState.RefLeveldB); 
	hlblCB3 = CreateWindowW(L"Static", buf, WS_CHILD | WS_VISIBLE | SS_CENTER,
		CB_XPos, rectImage.top +2* BorderY + 90 + ImHeight / 2, iCBPanelWidth, 20, hwnd, (HMENU)3, NULL, NULL);  // This is Offset

	// Label the bottom according to the number of radars there are	
	int ImWidX, LabPosX;
	ImWidX= ImWidth / gRadarState.NumSensorsSet;
	
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		LabPosX = rectImage.left + myBorderLeft + ImWidX / 2 + rindex*ImWidX - textWid / 2;

		hwndDopLabel[rindex]= CreateWindowW(L"Static", L"Speed (m/s)",
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			LabPosX, rectImage.bottom + myBorderBottom / 2  ,
			textWid, myBorderBottom, hwnd, (HMENU)1, NULL, NULL);

		LabPosX = rectImage.left + myBorderLeft + ImWidX + ImWidX*rindex - textWid / 4 - 10;
		hwndDopLabelMax[rindex]= CreateWindowW(L"Static", L"a",
			WS_CHILD | WS_VISIBLE | SS_RIGHT,
			LabPosX, rectImage.bottom + myBorderBottom / 2  - 5,
			textWid / 4, myBorderBottom, hwnd, (HMENU)1, NULL, NULL);

		LabPosX = rectImage.left + myBorderLeft + 10 + ImWidX*rindex;
		hwndDopLabelMin[rindex] = CreateWindowW(L"Static", L"a",
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			LabPosX, rectImage.bottom + myBorderBottom / 2   - 5,
			textWid / 4, myBorderBottom, hwnd, (HMENU)1, NULL, NULL);

		wchar_t buf[10];
		swprintf(buf, 10, L"%3.1lf", gRadarConfig.UAmbDoppler);
		SetWindowTextW(hwndDopLabelMax[rindex], buf);
		swprintf(buf, 10, L"%3.1lf", -gRadarConfig.UAmbDoppler);
		SetWindowTextW(hwndDopLabelMin[rindex], buf);
	}
}

void UpdateImageWindows(HWND hwnd)
{
//	HDC hDC = 0;
	RECT rectWind, rectImage;
	int myBorderLeft = 40, myBorderBottom = 20;
	int ImWidth, ImHeight;
	int textWid;
	int BorderX = 10, BorderY = 10;
	int CB_XPos;

	GetClientRect(hwnd, &rectWind);
	// Calculate my bounding box
	rectImage.left = rectWind.left + iControlPanelWidth + BorderX + 10;
	rectImage.right = rectWind.right- iCBPanelWidth- BorderX*2;
	rectImage.top = rectWind.top;
	rectImage.bottom = rectWind.bottom - iOutPanelHeight;
	ImWidth = rectImage.right - rectImage.left - myBorderLeft;
	ImHeight = rectImage.bottom - rectImage.top - myBorderBottom;

	//std::cout << "Rectangles are Wind, Image" << rectWind << rectImage << endl;
	textWid = ImWidth / 4;
	CB_XPos = rectImage.right + BorderX;
	SetWindowPos(hwndCBLabel, NULL, CB_XPos, rectImage.top + 20, iCBPanelWidth, 20, NULL);
	SetWindowPos(hlblCB1, NULL, CB_XPos, rectImage.top + BorderY + 55, iCBPanelWidth, 20, NULL);
	SetWindowPos(hlblCB2 , NULL, CB_XPos, rectImage.top + 2 * BorderY + 45 + ImHeight / 2 + BorderY, iCBPanelWidth, 20, NULL);  // This is scale
	SetWindowPos(hlblCB3, NULL, CB_XPos, rectImage.top + 2 * BorderY + 90 + ImHeight / 2, iCBPanelWidth, 20, NULL);
	//textWidr = ImHeight / 4;

	// Label the bottom according to the number of radars 
	int ImWidX, LabPosX;
	ImWidX = ImWidth / gRadarState.NumSensorsSet;

	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		LabPosX = rectImage.left + myBorderLeft + ImWidX / 2 + rindex*ImWidX - textWid / 2;
	
		SetWindowPos(hwndDopLabel[rindex], NULL, LabPosX, rectImage.bottom + myBorderBottom / 2 ,
			textWid, myBorderBottom, NULL);

		LabPosX = rectImage.left + myBorderLeft + ImWidX + ImWidX*rindex - textWid / 4 - 10;
		SetWindowPos(hwndDopLabelMax[rindex], NULL,
			LabPosX, rectImage.bottom + myBorderBottom / 2 - 5,
			textWid / 4, myBorderBottom,  NULL);

		LabPosX = rectImage.left + myBorderLeft + 10 + ImWidX*rindex;
		SetWindowPos(hwndDopLabelMin[rindex], NULL,
			LabPosX, rectImage.bottom + myBorderBottom / 2  - 5,
			textWid / 4, myBorderBottom, NULL);
	}
}

// Update the labels on the colorbar
void UpdateCBLabel(void)
{
	LRESULT pos;
	wchar_t buf[10];
	 	
	pos = SendMessageW(hTrack1, TBM_GETPOS, 0, 0);
	gRadarState.DispRange = 10.0f + 10.0f * pos;
	swprintf(buf, 10, L"%d", (int)gRadarState.DispRange);
	SetWindowTextW(hlblCB1, buf);

	pos = SendMessageW(hTrack2, TBM_GETPOS, 0, 0);
	gRadarState.RefLeveldB= gRadarConfig.MinRefLevel + 10.0f * pos;

	swprintf(buf, 10, L"Ref %d", (int)gRadarState.RefLeveldB);
	SetWindowTextW(hlblCB3, buf);
}
void UpdateDisplay(int count)
{
	PostMessage(hWndApp, WM_USER + 0, 0, count);  // This tells the display to update
}

void UpdateImages(HWND hwnd, HDC hdc, RECT rectImage)
{

	HDC hmemdc;
	HGDIOBJ oldbmp;
	static HBRUSH hBrush;
	static HPEN   hPen;
	int ImWidX; // = (DispSize.right - DispSize.left - iControlPanelWidth - iControlPanelWidth) / gRadarState.NumSensorsSet;
	int ImXPos;
	int BorderX = 10;
	int BorderY = 10;
	int ImHeight;
	int ColorBarImWidth = 50;
	int LabelSize = 50;
	int CB_XPos;
	//CString msgText;
	COLORREF  textcol = RGB(0, 0, 0);
	int myBorderLeft = 40, myBorderBottom = 20;

	hPen = CreatePen(PS_SOLID, 1, RGB(0, 128, 0));
	SelectObject(hdc, hPen);
	MoveToEx(hdc, rectImage.left, rectImage.top, 0);
	LineTo(hdc, rectImage.right - BorderX, rectImage.top);
	LineTo(hdc, rectImage.right - BorderX, rectImage.bottom - BorderY);
	LineTo(hdc, rectImage.left, rectImage.bottom - BorderY);
	LineTo(hdc, rectImage.left, rectImage.top);
	DeleteObject(hPen);

	hmemdc = CreateCompatibleDC(hdc);
	hPen = CreatePen(PS_SOLID, 2, RGB(128, 0, 255));

	SelectObject(hmemdc, hPen);      // select into the DC

	SetBkColor(hmemdc, RGB(0, 0, 255));

	// Continue on and update the display
	SetBkColor(hmemdc, RGB(0, 0, 255));

	oldbmp = ::SelectObject(hmemdc, hRDIBitMap[0]);  // Select/save so we don't create a resource memory leak later
	RECT rectWind;
	int ImWidth, ImHgtY;
	GetClientRect(hwnd, &rectWind);
	LabelSize = 50;
	rectImage.left = rectWind.left + iControlPanelWidth + BorderX + 10;
	rectImage.right = rectWind.right - iCBPanelWidth - BorderX * 2;
	rectImage.top = rectWind.top;
	rectImage.bottom = rectWind.bottom - iOutPanelHeight;
	ImWidth = rectImage.right - rectImage.left - myBorderLeft;
	ImHeight = rectImage.bottom - rectImage.top - myBorderBottom;

	ImWidX = ImWidth / gRadarState.NumSensorsSet;
	ImHgtY = ImHeight / 2 - BorderY;
	SetTextAlign(hdc, TA_CENTER);
	SetTextColor(hdc, textcol);
	// Now select and update the display


	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		ImXPos = iControlPanelWidth + rindex * ImWidX + BorderX + LabelSize;
		// Radar DTI
		SelectObject(hmemdc, hDTIBitMap[rindex]);
		StretchBlt(hdc, ImXPos, BorderY, ImWidX - 2 * BorderX, ImHgtY, hmemdc, 0, 0, gRadarConfig.NWRIPerCPI, gRadarConfig.DTI_Height, SRCCOPY);

		SelectObject(hmemdc, hRDIBitMap[rindex]);
		StretchBlt(hdc, ImXPos, ImHgtY + 2 * BorderY, ImWidX - 2 * BorderX, ImHgtY, hmemdc, 0, 0, gRadarConfig.NWRIPerCPI, gRadarConfig.NSamplesPerWRI, SRCCOPY);
		// now label the x axis

	}

	// Put up the colorbar and notation - don't really need to do this every time
	SelectObject(hmemdc, hColorBar);

	CB_XPos = rectWind.right - iCBPanelWidth / 2 - ColorBarImWidth / 2 - BorderX;

	StretchBlt(hdc, CB_XPos, BorderY + 80, ColorBarImWidth, ImHgtY - 20, hmemdc, 0, 0, 1, CMAP_SIZE, SRCCOPY);

	// Put a box around the colorbar
	hPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 128));
	SelectObject(hdc, hPen);
	MoveToEx(hdc, CB_XPos, BorderY + 80, 0);
	LineTo(hdc, CB_XPos + ColorBarImWidth, BorderY + 80);
	LineTo(hdc, CB_XPos + ColorBarImWidth, BorderY + 80 + ImHgtY - 20);
	LineTo(hdc, CB_XPos, BorderY + 80 + ImHgtY - 20);
	LineTo(hdc, CB_XPos, BorderY + 80);
	DeleteObject(hPen);

	// Cleanup
	SelectObject(hmemdc, oldbmp);


	DeleteDC(hmemdc);

	DeleteObject(hPen);

}

int init_bitmaps()
{
	int i, j, y, index;
	HDC hdc, hmemdc;
	int pitch = 4 * gRadarConfig.NWRIPerCPI;

	// Create the display image RGB colormap
	colormap(CMAP_SIZE, gColormapRed, gColormapGreen, gColormapBlue, gRadarState.ColorMapSelected);

	// Initialize the array of bitmap structures
	hdc = GetDC(hWndApp);
	hmemdc = CreateCompatibleDC(hdc);

	ZeroMemory(&bmRDI, sizeof(BITMAPINFO));
	bmRDI.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmRDI.bmiHeader.biWidth = gRadarConfig.NWRIPerCPI;
	bmRDI.bmiHeader.biHeight = -gRadarConfig.NSamplesPerWRI;
	bmRDI.bmiHeader.biPlanes = 1;
	bmRDI.bmiHeader.biBitCount = 32;
	for (index = 0; index < MaxRadars; index++) {
		hRDIBitMap[index] = ::CreateDIBSection(hmemdc, &bmRDI, DIB_RGB_COLORS, (VOID**)&lpRDIBits[index], NULL, 0);
		if (!hRDIBitMap[index]) {
			log_message("Error %d: RDI bitmap creation error, not handled correctly", GetLastError());
			return(-1);
		}
		else {
			log_message("Created good RDI bitmap %d", index);
			ZeroMemory(lpRDIBits[index], 4 * gRadarConfig.NWRIPerCPI * gRadarConfig.NSamplesPerWRI);
		}
	}
	ZeroMemory(&bmDTI, sizeof(BITMAPINFO));
	bmDTI.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmDTI.bmiHeader.biWidth = gRadarConfig.NWRIPerCPI;
	bmDTI.bmiHeader.biHeight = -gRadarConfig.DTI_Height;
	bmDTI.bmiHeader.biPlanes = 1;
	bmDTI.bmiHeader.biBitCount = 32;
	for (index = 0; index < MaxRadars; index++) {
		hDTIBitMap[index] = ::CreateDIBSection(hmemdc, &bmDTI, DIB_RGB_COLORS, (VOID**)&lpDTIBits[index], NULL, 0);
		if (!hDTIBitMap[index]) {
			log_message("Error %d: DTI bitmap creation error, not handled correctly", GetLastError());
			return(-1);
		}
		else {
			log_message("Created good DTI bitmap %d", index);
			ZeroMemory(lpDTIBits[index], 4 * gRadarConfig.NWRIPerCPI * gRadarConfig.DTI_Height);
		}

	}
	// The colorbar is currently a static bitmap
	ZeroMemory(&bmColorBar, sizeof(BITMAPINFO));
	bmColorBar.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmColorBar.bmiHeader.biWidth = 1;
	bmColorBar.bmiHeader.biHeight = CMAP_SIZE;
	bmColorBar.bmiHeader.biPlanes = 1;
	bmColorBar.bmiHeader.biBitCount = 32;
	hColorBar = ::CreateDIBSection(hmemdc, &bmColorBar, DIB_RGB_COLORS, (VOID**)&lpColorBarBits, NULL, 0);
	if (!hColorBar) {
		log_error_message("Colorbar bitmap creation error, not handled correctly. ", GetLastError());
		return(-1);
	}
	else {
		log_message("Created good Colorbar bitmap");
	}
	DeleteDC(hmemdc);
	ReleaseDC(hWndApp, hdc);

	// Now initialize the images
	for (i = 0; i < gRadarConfig.NWRIPerCPI; i++) {
		for (y = 0; y < gRadarConfig.DTI_Height / 4; y++) {
			index = y * pitch + i * 4;
			j = (i * CMAP_SIZE / gRadarConfig.NWRIPerCPI);
			for (int bmap = 0; bmap < gRadarConfig.NumRadars; bmap++) {
				lpDTIBits[bmap][index + 0] = (char)gColormapBlue[j];
				lpDTIBits[bmap][index + 1] = (char)gColormapGreen[j];
				lpDTIBits[bmap][index + 2] = (char)gColormapRed[j];
			}
		}
		for (; y < gRadarConfig.DTI_Height / 2; y++) {  // just for interest, switch in the middle to a different image
			index = y * pitch + i * 4;
			j = (i * CMAP_SIZE / gRadarConfig.NWRIPerCPI);
			for (int bmap = 0; bmap < gRadarConfig.NumRadars; bmap++) {
				lpDTIBits[bmap][index + 0] = (char)gColormapRed[j];
				lpDTIBits[bmap][index + 1] = (char)gColormapGreen[j];
				lpDTIBits[bmap][index + 2] = (char)gColormapBlue[j];
			}
		}
		for (; y < gRadarConfig.DTI_Height; y++) {  // and, switch again to a different image
			index = y * pitch + i * 4;
			j = (i * CMAP_SIZE / gRadarConfig.NWRIPerCPI);
			for (int bmap = 0; bmap < gRadarConfig.NumRadars; bmap++) {
				lpDTIBits[bmap][index + 0] = (char)gColormapBlue[j];
				lpDTIBits[bmap][index + 1] = (char)gColormapRed[j];
				lpDTIBits[bmap][index + 2] = (char)gColormapGreen[j];

			}
		}
	}
	SetColorBarImage();

	// We should also have a function that deletes bitmaps using DeleteObject( )
	return 0;

}
#endif  // End of Windows-specific gui routines

// The following should convert a float array into a character image array. It doesn't work.
void convertDB2CM_Image(unsigned char * pRDIBits, float* pRDIPower,
	double ScaleData, double RefLeveldB, double DispRange,
	int Samp_Per_WRI,  int Num_WRI,  int Bytes_per_pixel)
{
	int i, ind1, ind2, im_index, cindex;
	float tmp;
	//log_message("COnvert %lf, %lf, %lf, %d, %d, %d", 
	//	ScaleData, RefLeveldB, DispRange, Samp_Per_WRI, Num_WRI, Bytes_per_pixel);
	// Does the next corner turn here while converting.
	for (i = 0, ind1 = 0; ind1 < Num_WRI; ind1++) {
		for (ind2 = 0, im_index = ind1 * Bytes_per_pixel;
			ind2 < Samp_Per_WRI; ind2++)
		{
			// Get the most recent data
			tmp = pRDIPower[i] + (float)ScaleData;
			
			// convert to an image based on the current settings
			cindex = (int)(256.0f * ((tmp - RefLeveldB) / DispRange));
			cindex = max(cindex, 0);
			cindex = min(cindex, 255);
			pRDIBits[im_index + 0] = (char)gColormapBlue[cindex];
			pRDIBits[im_index + 1] = (char)gColormapGreen[cindex];
			pRDIBits[im_index + 2] = (char)gColormapRed[cindex];
			im_index += Num_WRI * Bytes_per_pixel;
		}
	}
	
}

// The following routine converts the power spectrum produced by the processing routines into a format
// for display as either a RDI or scrolling DTI

void PowerSpecDispThread()
{
	unsigned int  i;
	int pitch = NBYTES_PER_PIXEL*gRadarConfig.NWRIPerCPI;

	int count = 0;
	int ind1, ind2, im_index;  // These are array indices. Should be unsigned.
	float tmp;
	int cindex;
	uint32_t *targetLines[MaxRadars];
	log_message("Thread that converts results to display format and tells display to update has started.");

	// initialize the database
#ifndef __WithoutDataBase__
		dBInitialize();
#endif

	// Reserve my output memory
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		gProcessedData.pDTI[rindex] = (float*)
			malloc(gRadarConfig.DTI_Height*gRadarConfig.NWRIPerCPI*NBYTES_PER_PIXEL);
		gProcessedData.target_line[rindex] = (char*)malloc(gRadarConfig.NWRIPerCPI*NBYTES_PER_PIXEL);
		targetLines[rindex] = (unsigned int *)malloc(gRadarConfig.NWRIPerCPI*sizeof(unsigned int));
		if ((gProcessedData.pDTI[rindex] == NULL) || 
			(gProcessedData.target_line[rindex] == NULL) ||
			(targetLines[rindex]==NULL) )
		{
			log_error_message("Allocation of DTI memory blocks failed.  Exiting", GetLastError());
			// If the array allocation fails, the system is out of memory so exit
			exit(5);
		}
	}
	dthreadSyncFlag = FALSE;  // If starting thread is listening, let them know we completed initialization

	while (!dthreadSyncFlag)  // Loop until stop is requested
	{
		// Grab the display data buffer
		std::unique_lock<std::mutex> bufferlock(gProcessedData.OwnBuffers);
		// Indefinite wait for results to be ready		
		while (!gProcessedData.InBufferFull && !dthreadSyncFlag) {
			std::cv_status cvstat = gProcessedData.DataHere.wait_for(bufferlock, std::chrono::milliseconds(2000));
			if (cvstat == std::cv_status::timeout) 	log_message("Output format process waiting on data timeout");
		}
		
		if (dthreadSyncFlag) { // If flag was set while waiting then break out of loop
			bufferlock.unlock();
			break;
		}
		
		// First make room in the scrolling DTI image bitmaps to update the displays

		for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
			// Scroll the bitmaps up by one line
			memcpy(lpDTIBits[rindex], &lpDTIBits[rindex][pitch], pitch*(gRadarConfig.DTI_Height - 1));

			// Convert the image dB values to a colormap
			// Using the following function gives incorrect answers, but it works when code is copied directly here. 
			// TODO: check for something going out of scope?
			//convertDB2CM_Image(&lpRDIBits[rindex][0], &gProcessedData.pRDIPower[rindex][0],
			//	gRadarConfig.ScaleData, gRadarState.RefLeveldB, gRadarState.DispRange,
			//	gProcessedData.Params.Samp_Per_WRI, gProcessedData.Params.Num_WRI, (int) NBYTES_PER_PIXEL);
			{
				unsigned char* pRDIBits;
				float* pRDIPower;
				float ScaleData, RefLeveldB, DispRange;
				int Samp_Per_WRI, Num_WRI, Bytes_per_pixel;
				Bytes_per_pixel = NBYTES_PER_PIXEL;
				Num_WRI = gProcessedData.Params.Num_WRI;
				Samp_Per_WRI = gProcessedData.Params.Samp_Per_WRI;
				ScaleData = (float)gRadarConfig.ScaleData;
				RefLeveldB = (float) gRadarState.RefLeveldB;
				DispRange = (float) gRadarState.DispRange;
				pRDIBits = lpRDIBits[rindex];
				pRDIPower = gProcessedData.pRDIPower[rindex];
				// Do the next corner turn here!
				for (i = 0, ind1 = 0; ind1 < Num_WRI; ind1++) {
					for (ind2 = 0, im_index = ind1 * Bytes_per_pixel;
						ind2 < Samp_Per_WRI; ind2++)
					{
						// Get the most recent data
						tmp = pRDIPower[i] + ScaleData;
						i++;

						// convert to an image based on the current settings
						cindex = (int)(256.0f * ((tmp - RefLeveldB) / DispRange));
						cindex = max(cindex, 0);
						cindex = min(cindex, 255);
						pRDIBits[im_index + 0] = (char)gColormapBlue[cindex];
						pRDIBits[im_index + 1] = (char)gColormapGreen[cindex];
						pRDIBits[im_index + 2] = (char)gColormapRed[cindex];
						im_index += Num_WRI * Bytes_per_pixel;
					}
				}

				// Now do FFTShift on only the Doppler axis
				float* ptemp;
				int j;
				if ((Num_WRI > 1)) {
					ptemp = (float*)malloc(Num_WRI * sizeof(char)* Bytes_per_pixel);
					if (ptemp == NULL) {
						log_message("Error %d: Unable to perform fftshift of data", GetLastError());
						return;
					}
					int txsize=Num_WRI / 2 * sizeof(char) * Bytes_per_pixel;
					for (j = 0; j < Samp_Per_WRI ; j++) {
						// Using pointer arithmetic to point at location for copy
						memcpy(ptemp, pRDIBits + (j * Num_WRI + Num_WRI / 2) * Bytes_per_pixel, txsize); // first half line
						memcpy(pRDIBits + (j * Num_WRI + Num_WRI / 2)* Bytes_per_pixel, pRDIBits + (j  * Num_WRI ) * Bytes_per_pixel, txsize);
						memcpy(pRDIBits + j * Num_WRI * Bytes_per_pixel, ptemp, txsize);
					}
					free(ptemp);
				}
			}
					
			// Copy out the target line
			memcpy(gProcessedData.target_line[rindex],
				lpRDIBits[rindex] + gProcessedData.Params.Num_WRI* gProcessedData.index_max_r[rindex] * NBYTES_PER_PIXEL,
				gProcessedData.Params.Num_WRI* NBYTES_PER_PIXEL);
			memcpy(targetLines[rindex], gProcessedData.target_line[rindex], NBYTES_PER_PIXEL * gProcessedData.Params.Num_WRI);
			// If the peak overlay is on, then set the blue value of the pixel full on
			if (gRadarState.PeakOverlay) {
				int dopidx = NBYTES_PER_PIXEL * (( gProcessedData.index_max_d[rindex] + gProcessedData.Params.Num_WRI/2)% gProcessedData.Params.Num_WRI);
				
				gProcessedData.target_line[rindex][dopidx] = (char)255;
			}
			// Copy out just a single line to the scrolling DTI
			memcpy(&lpDTIBits[rindex][pitch*(gRadarConfig.DTI_Height - 1)],
				gProcessedData.target_line[rindex], pitch);  // Insert the line at the bottom
		}
		gProcessedData.InBufferFull = FALSE;  // We took the data we needed
		//log_message("Display thread unlock next");
		bufferlock.unlock();
#ifndef _RTP_Headless
#ifdef _WINGUI
		UpdateDisplay(count);
#endif
#endif
		
#ifndef __WithoutDataBase__
		dBOutput(targetLines, gProcessedData.Params.Num_WRI);
#endif
	}
	log_message("Display data formatting routine exiting.");
	Sleep(10);
	// Delete memory that I allocated
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		free(gProcessedData.pDTI[rindex]);
		free(gProcessedData.target_line[rindex]);
		free(targetLines[rindex]);
		gProcessedData.pDTI[rindex] = NULL;
		gProcessedData.target_line[rindex] = NULL;
		targetLines[rindex] = NULL;
	}
	// Is this sufficient to close the database? Should it be reset on close?
#ifndef __WithoutDataBase__
	dBClose();
#endif
	return;
}



void SetColorBarImage()
{
	for (int i = 0; i < CMAP_SIZE; i++) {
		lpColorBarBits[4 * i] = (char)gColormapBlue[i];
		lpColorBarBits[4 * i + 1] = (char)gColormapGreen[i];
		lpColorBarBits[4 * i + 2] = (char)gColormapRed[i];
	}
	return;
}

void StartRadarDisp()
{
	dthreadSyncFlag = TRUE;
	tFormatDisplayThread = std::thread(PowerSpecDispThread);
	Sleep(1000);
}

void StopRadarDisp()
{
	dthreadSyncFlag = TRUE;
	log_message("Stopping display data formatting routine.");
	if (tFormatDisplayThread.joinable())
		tFormatDisplayThread.join();
	log_message("Display data formatting routine ended.");

}
#endif