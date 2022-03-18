#pragma once
// colormap header file
/*
RadarRTP - Radar Real time Program (RTP)

© 1988 Frank Robey, © 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License, Version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt
*/


enum class cmaptype {
	COLORMAP_HOT,
	COLORMAP_JET,
	COLORMAP_GRAY
} ;


// Colormap information
#define CMAP_SIZE 256  /* Number of colors in colormap */
extern int gColormapRed[CMAP_SIZE], gColormapGreen[CMAP_SIZE], gColormapBlue[CMAP_SIZE];

int colormap(int nmap, int *red, int *green, int *blue, cmaptype cmap);
