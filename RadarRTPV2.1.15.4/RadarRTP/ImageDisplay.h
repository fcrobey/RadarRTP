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
#pragma once

#include "RadarRTP.h"

#ifndef _RTP_Headless

#ifdef _WINGUI
void UpdateImages(HWND hWnd, HDC dc, RECT rectImage);
void CreateImageWindows(HWND hWnd);
void UpdateImageWindows(HWND hwnd);
void UpdateCBLabel(void);
void	SetColorBarImage(void);
//void SetImageRect(RECT rectIn, RECT &rectOut);
#endif

void PowerSpecDispThread();
void StartRadarDisp();
void StopRadarDisp();

#endif