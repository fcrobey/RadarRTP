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
using DataTOV = std::chrono::system_clock::time_point;
using DataTics = std::chrono::duration<int64_t, std::micro>;

enum DataType {
	CW, LFMRangeDoppler, UpDownLFM, ArbRangeDoppler, SonarRangeDoppler 
};

typedef struct CPI_Params {
	DataTics Data_TOVtt;				// Time of validity in time ticks 
	DataTOV Data_TOV;					// Time of validity as a time point
	unsigned int block_id=0;				// Block counter - index of input data block - maintained by processing thread
	int ADCFrameCount=0;					// Count of ADC blocks.  Count maintained by ADC interrupt/callback routine
	float ADCSampleRate = 48000.0f;		// ADC Sample Rate
	unsigned int Samp_Per_WRI=128;			// Samples per waveform repetition interval
	unsigned int Num_WRI=128;				// Number of WRI per coherent processing interval
	unsigned int NumSensorsSet=2;			// Number of sensors
	//DataType myType;
} CPI_Params;

