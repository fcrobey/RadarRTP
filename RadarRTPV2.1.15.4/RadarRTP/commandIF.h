#pragma once
// This file contains the defines for the command interface. The expecation is that the operator interface
// Will use this to set operating parameters.
// Previous to this, the command interface was performed by writing to shared memory.
// Written by Frank Robey
//		Initial version 7/28/2019
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

#include "colormap.h"

typedef enum {
	SetDispScale ,
	SetDispColormap,
	SetDispMarkPeak,
	SetRecordRaw,
	SetRecordProc,
	SetSimOn,
	SetSimAmp,
	SetCharCMD,
	SetExit,
} cmdtype;

typedef struct {
	cmdtype cmd;
	union {
		float fparams[2];
		int iparams[2];
		bool onoff;
		cmaptype cmap;
		char charcmd[4];

	};
} cmdbuffer;

typedef struct {
	bool statSimOn;
	bool statRecRaw;
	bool statRecProc;
	bool statMarkPeak;
	float statSimAmp;
	float statDispRange;
	float statDispRef;
	cmaptype statCMAP;
	float statCentFreq;
	float statBW;
	float statSampRate;
	unsigned int statNWRI;
	unsigned int statNSampPerWRI;
} returnStat;

// Command interpreter
int CommandParse(cmdbuffer Command);