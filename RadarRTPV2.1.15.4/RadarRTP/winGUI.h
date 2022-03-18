// This is the header file for a windows-specific GUI
// This header was originally part of RadarRTP.h, and split out here in 2018
// Written by F. Robey
// The first part of this file was auto-generated by Visual Studio
/*
RadarRTP - Radar Real time Program (RTP)

� 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt
*/

#pragma once
#include "stdafx.h"
#include "RadarRTP.h"

#ifndef _RTP_Headless

#ifdef _WINGUI 
extern HINSTANCE hInst;

const int iControlPanelWidth = 210;
const int iConfigPanPosX = 10, iConfigPanPosY = 5, iConfigPanPosHeight = 115;
const int iRecordPanPosX = 10, iRecordPanPosY = 325;
const int iDispControlPanPosX = 10, iDispControlPanPosY = 125;
const int iSimControlPanPosX = 10, iSimControlPanPosY = 450;
const int iCBPanelWidth = 100;  // colorbar panel on the right
const int iOutPanelHeight = 50;

static int BORDER = 7;
static int BUTWIDTH = 120; // 80
static int BUTHEIGHT = 35; //25

extern HGDIOBJ hfntRotate;
extern HWND hTrack1, hTrack2;

// Forward declarations of functions included in this code module:

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, std::string);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void	CreateDisplayControls(HWND hwnd);
void	CreateConfigControls(HWND hWnd);
void	CreateRecordControls(HWND hWnd);
void	CreateSimControls(HWND hWnd);
void 	UpdateConfigText(void);
void	CreateRotatedFont(void);
int		init_bitmaps(void);
void updateButtonStatus();
#endif // End of _WINGUI (windows specifc GUI)



#endif