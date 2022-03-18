// Some simple colormaps
// By Frank Robey
// 1988 Original implementation
// Dec 2014, minor changes to integrate with the radar RTP
// Oct 2017 Reorganized.  Added jet colormap (which I dislike, but others like)
// 
// Grey scale and heated object colormaps originally written while a graduate student
// 
// 
/*
RadarRTP - Radar Real time Program (RTP)

© 1988, Frank Robey, © 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License, Version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt
*/
#include "colormap.h"
#include "stdafx.h"

int gColormapRed[CMAP_SIZE], gColormapGreen[CMAP_SIZE], gColormapBlue[CMAP_SIZE];


// Generate the colormap tables
// Storage is allocated elsewhere
// Numcolors is the number of entries in the table
// red, green and blue are the tables.
// cmap is the type of colormap. Three are implemented, hot, grey and jet.
int colormap(int NumColors, int *red, int *green, int *blue, cmaptype cmap)
// This creates a heated object colormap
{
	int count, band, step;
	double scale = 255.0;
	if (NumColors < 1) {
		return(-1); // error
	}

	switch (cmap)
	{
	case cmaptype::COLORMAP_HOT:
	{
		log_message("Set colormap to HOT.");
		band = NumColors * 3 / 8;
		for (count = 0; count < band; count++) {
			red[count] = (int)(scale*(count + 1) / (double)band);
			green[count] = 0;
			blue[count] = 0;
		}
		for (step = 1; count < (2 * band); count++, step++) {
			red[count] = (int)scale;
			green[count] = (int)scale*step / band;
			blue[count] = 0;
		}
		for (step = 1; count < NumColors; count++, step++) {
			red[count] = (int)scale;
			green[count] = (int)scale;
			blue[count] = (int)(scale*step / (double)(NumColors - 2 * band));
		}
	}
	break;
	case cmaptype::COLORMAP_JET:
	{
		log_message("Set colormap to JET.");
#define LINTERP(val, alow, blow, ahigh, bhigh)   ( (ahigh)+ double((val) - (alow)) * ((bhigh)-(ahigh)) / ((blow) - (alow)) )
		int band1 = NumColors / 4;
		band = NumColors / 8;
		for (count = 0; count < band; count++) {  // 0 to 1/8
			red[count] = 0;
			green[count] = 0;
			blue[count] = (int)(scale*(LINTERP(count, 0, band, 0.5, 1.0)));
		}
		for (; count < (band1 + band); count++) {  // 1/8 to 3/8
			red[count] = 0;
			green[count] = (int)(scale*(LINTERP(count, band, band + band1, 0.0, 1.0)));
			blue[count] = (int)scale;
		}
		for (; count < (band1 + band1 + band); count++) {  // 3/8 to 5/8
			red[count] = (int)(scale*(LINTERP(count, band + band1, 2 * band1 + band, 0.0, 1.0)));	// 0 to 1
			green[count] = (int)scale;
			blue[count] = (int)(scale*(LINTERP(count, band + band1, 2 * band1 + band, 1.0, 0.0)));  // 1 to 0
		}
		for (; count < (NumColors - band); count++) {  // 5/8 to 7/8
			red[count] = (int)scale;
			green[count] = (int)(scale*(LINTERP(count, 2 * band1 + band, NumColors - band, 1.0, 0.0))); // 1 to 0
			blue[count] = (int)0;
		}
		for (; count < NumColors; count++) {  // 7/8 to 1
			red[count] = (int)(scale*(LINTERP(count, NumColors - band, NumColors, 1.0, 0.5)));  // 1 to 0.5
			green[count] = 0;
			blue[count] = 0;
		}
	}
	break;
	case cmaptype::COLORMAP_GRAY:
	{
		log_message("Set colormap to GRAYSCALE.");
		for (count = 0; count < NumColors; count++) {
			red[count] = (int)(scale*(count ) / (double)(NumColors - 1));
			green[count] = (int)(scale*(count ) / (double)(NumColors - 1));
			blue[count] = (int)(scale*(count ) / (double)(NumColors - 1));
		}
	}
	break;
	default:
		log_message("Invalid colormap selected or not implemented");
	}
	return(0);
}

