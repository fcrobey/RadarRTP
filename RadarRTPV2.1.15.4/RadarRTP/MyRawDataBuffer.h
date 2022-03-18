/*Initial version by Frank Robey
Aug 2019 - functions moved out of processMaster

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
#include "radarc.h"


struct RawDataBuffer {
private:
	std::vector<RTPComplex *> data; // array of data
	CPI_Params SetupParams;

public:
	CPI_Params Params;	// Parameters to use in setting up data buffers
	const unsigned NWRIPerBlock;
	const bool RealOnly;
	RawDataBuffer(CPI_Params InParams, int NWRIPerBlockIn, const bool RealOnlyIn) :
		Params(InParams), RealOnly(RealOnlyIn), NWRIPerBlock(NWRIPerBlockIn)
	{
		log_message("Initializing Raw Data Buffer");
		data.resize(Params.NumSensorsSet);
		for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
			data[rindex] = (RTPComplex*)fftwf_malloc(sizeof(RTPComplex) * Params.Samp_Per_WRI*Params.Num_WRI);
			if (data[rindex] == NULL) {
				log_error_message("Can't allocate raw data memory for processing thread", GetLastError());
				exit(1L);
			}
		}
	};
	void MoveUp() {
		if (Params.Num_WRI > NWRIPerBlock) {
			for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
				memcpy(data[rindex], &data[rindex][Params.Samp_Per_WRI*(NWRIPerBlock)],
					(Params.Samp_Per_WRI*(Params.Num_WRI - NWRIPerBlock)) * sizeof(RTPComplex));
			}
		}
	};
	int LoadData(float* InData) {
		int offset; //The index where the new data was placed
		if (!RealOnly) {
			switch (Params.NumSensorsSet)
			{
			case (1):  /* single radar operation */

				// Copy input data out of circular buffer 
				memcpy(&data[0][Params.Samp_Per_WRI*(Params.Num_WRI - NWRIPerBlock)],
					InData, (Params.Samp_Per_WRI*NWRIPerBlock) * sizeof(RTPComplex));
				offset = Params.Samp_Per_WRI*(Params.Num_WRI - NWRIPerBlock);
				break;
			default:
				// two or more radar channels.  Need to do de-interleaving (cornerturn) of the data
				offset = Params.Samp_Per_WRI*(Params.Num_WRI - NWRIPerBlock);
				// Here we have to deinterleave the data so create an explicit loop to do it
				float * dptr;
				unsigned int i;
				for (i = offset, dptr = InData;
					i < (Params.Samp_Per_WRI*NWRIPerBlock + offset); i++) {
					for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
						data[rindex][i] = RTPComplex(*dptr, *(++dptr));
						dptr++;
					}
				}
				break;

			}
		}
		else { // Real only.  Need to deinterleave the data
			float * dptr;
			unsigned int i;
			offset = Params.Samp_Per_WRI*(Params.Num_WRI - NWRIPerBlock);
			// Here we have to deinterleave the data so create an explicit loop to do it
			for (i = offset, dptr = InData;
				i < (Params.Samp_Per_WRI* NWRIPerBlock + offset); i++) {
				for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
					data[rindex][i] = RTPComplex(*dptr++, 0.0f);

				}
			}
		}
		return(offset);
	};
	void AddSimData(sensordata <RTPComplex> *SimData, int offset) {
		for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
			unsigned int wri, i;

			if (!RealOnly) {
				for ( wri = 0, i = offset; wri < ((unsigned int)NWRIPerBlock); wri++)
					for (unsigned int times = 0; times < (unsigned int)Params.Samp_Per_WRI; times++, i++)
						data[rindex][i] += (*SimData)(wri, times, rindex);
						
			}
			else {

				for (unsigned int wri = 0, i = offset; wri < ((unsigned int)NWRIPerBlock); wri++)
					for (unsigned int times = 0; times < (unsigned int)Params.Samp_Per_WRI; times++, i++)
						data[rindex][i] += RTPComplex((*SimData)(wri, times, rindex).real(), 0.0f);
			}
		}

	};
	void CopyOut(RTPComplex * OutBuffer, unsigned int Sensor) {
		memcpy(OutBuffer, data[Sensor], (Params.Samp_Per_WRI*Params.Num_WRI) * sizeof(RTPComplex));
	};
	~RawDataBuffer() {
		for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
			if (data[rindex] != NULL) { fftwf_free(data[rindex]);  data[rindex] = NULL; }
		}
	};
};

//MyRawDataBuffer