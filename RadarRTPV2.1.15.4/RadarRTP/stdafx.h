// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
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

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#ifndef _RTP_Headless
#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#endif
#endif

// C RunTime Header Files
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#ifdef _WIN32
#include <strsafe.h>
#include <tchar.h>
#endif


// TODO: reference additional headers your program requires here
#include "RadarRTP.h"
#include "radarc.h"
#include "radarConfig.h"
#include "calibration.h"
#include "colormap.h"
#include "logMessages.h"
#include "ImageDisplay.h"

