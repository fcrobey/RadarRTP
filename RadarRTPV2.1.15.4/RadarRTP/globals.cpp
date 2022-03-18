//  globals.cpp
// This file includes the global variables and data structures.
// by Frank Robey
// Dec 2013-Jan 2014 Initial implementation
// Sep 2016 Modified to use more structures removing individual variables out of global (NSAMP_PER WRI, NWRI, etc.
// Apr 2017 Change to use array of pointers to data blocks allowing support to more than 2 radar channels
// Feb-Mar 2018 Timing switched to use C++11 chrono functions.
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


#include "stdafx.h"
#include "RadarRTP.h"
#include "radarc.h"
 
/* Recording parameters */
FILE * filedat ;		// handle for processed data file
time_t tfiledat;	// Time data file opened.
SNDFILE * fileraw ;  /* I'm using soundfiles to record data since I had that library */
time_t tfileraw;

/* Global variables */
// The following holds the transmitted waveform. VCO modulation for radar, samples for sonar
float * WaveData;  /* Waveform data.  Memory allocated elsewhere, this is the pointer to the data */
RTPComplex *  WaveformReplica; // Storage for sonar time series -> freq domain
RTPComplex *  DownConvertWF;  // Complex exponential to downconvert the sonar to baseband

RadarConfig gRadarConfig; /* Radar configuration information */
RadarState gRadarState;		/* Radar state information */
ProcessedRadarData gProcessedData; /* Data to communicate between processing and display */

// The following is for the ring buffer between the data input thread and the radar processing dispatch thread
float *Buff_Data[NBUFF];					// storage for ringbuffer data
PaStreamCallbackTimeInfo Buff_Time[NBUFF];  // structure with callback time, ADC time, 
DataTics Buff_Time_ticks[NBUFF];		// time of validity of data in 1 usec time ticks
int Buff_count[NBUFF];						//ADC frame count

// The following are to convert different times 
PaTime gStreamPATimeRef; // Reference time for stream (PaTime) with no specific epoch
std::chrono::steady_clock::time_point gStreamTPTimeRef; // Reference time epoch for gStreamPATimeRef in c++ standard time_point
std::chrono::system_clock::time_point gStreamSysTimeRef;
