#pragma once
#include "stdafx.h"
// Calibration header file
// This was originally part of the process module
// by Frank Robey
// Mar 2018 Created when calibration routines were split into a separate file
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
#include"sensordata.h"
typedef struct CalData {
	CPI_Params Params;

	std::vector < RTPComplex *> pCalData;		// Working data provided to cal worker thread.  Can be changed by worker
	std::vector < RTPComplex *>  pOffsetsdat;	// DC offset that varies with range sample
	std::vector <RTPComplex> DCOffset;
									// C++11 constructs
	std::condition_variable DataHere; // Data ready for processing
	std::mutex OwnBuffers;			// Mutex for the data buffers
	bool InBufferFull = false;		// Tell thread that input data is in buffer
	
	std::vector<floatdim4> CalXform;// Transform

	bool Cal_ready=FALSE;					// Flag to indicate cal values are ready.  Used as non-blocking semaphore.
	float FadeMemVal=0.95f;					// Fading memory update coefficient
	bool CalDCOnly=TRUE;					// Flag to say how to cal data
	int CalCount = 0;					// How many calibration blocks have been processed
	bool StopRequested=FALSE;

	CalData(CPI_Params InParams) :Params(InParams), NumSensorsSet(InParams.NumSensorsSet),
		DCOffset(InParams.NumSensorsSet), DCOffsetHist(InParams.NumSensorsSet), 
		DCOffsetHistN(InParams.NumSensorsSet*Nhist), // only use this with particular DC offset cal
		DCOffsetWRI(InParams.NumSensorsSet), DCOffsetWRIHist(InParams.NumSensorsSet), 
		R(InParams.NumSensorsSet), Rhist(InParams.NumSensorsSet), CalXform(InParams.NumSensorsSet), 
		Cal_ready(FALSE),FadeMemVal(0.95f), CalDCOnly(TRUE),StopRequested(FALSE)
	{
		for (unsigned int i = 0; i < InParams.NumSensorsSet; i++) {
			DCOffsetWRI[i] = new RTPComplex[InParams.Samp_Per_WRI];
			DCOffsetWRIHist[i] = new RTPComplex[InParams.Samp_Per_WRI];
		}
		//initializeCal();
	}	;
	int initializeCal(CPI_Params Params);
	//int initializeCal(CPI_Params Params, RTPComplex *DCOffsetVals[], RTPComplex DCOffset[]);
	void CalibrateFunction(int bob);
	int whitenTransformCholesky();
	int whitenTransformEigen();
	void complexMean();
	void covarianceSingleMean();
	void covarianceRangeDepMean();
	void complexMeanColumns3D();
	void FadeMemAveDCWRI();
	void FadeMemAveDCSingle();
	void DCAveSingle();

	~CalData() {
		for (unsigned int i = 0; i < Params.NumSensorsSet; i++) {
			delete  DCOffsetWRI[i];
			delete DCOffsetWRIHist[i];
		}
	};
private:
	int Nhist = 50;
	int NumSensorsSet;
//	std::vector<RTPComplex>  DCOffset;  // Allocate temp storage for the single DC offset per radar
	std::vector<RTPComplex>  DCOffsetHist;  // Allocate storage for the single DC offset per radar
	std::vector<RTPComplex>  DCOffsetHistN;  // Allocate storage for the single DC offset per radar
	std::vector<RTPComplex*>  DCOffsetWRI;  // Allocate storage for the range-dependent DC offset per radar
	std::vector<RTPComplex*>  DCOffsetWRIHist;  // Allocate storage for the range-dependent DC offset per radar

	std::vector<floatdim4> R;
	std::vector<floatdim4> Rhist;		// Estimated covariance matrix used to calculate transform
//	std::vector<floatdim4> CalXform;// Transform
}  CalData, *pCalData;


// In calibration.cpp
//int initializeCal(pCalData pRadarCalData, CPI_Params Params, RTPComplex *DCOffsetVals[], RTPComplex DCOffset[]);

int cleanupCalThread(pCalData pRadarCalData, RTPComplex *DCOffsetVals[], RTPComplex DCOffset[]);
//void CalibrateFunction(pCalData pRadarCalData);

/*
template <typename T> class CalDataC :public sensordata<T> 
{ public:
	std:vector <float> *Rest[4];		// Estimated covariance matrix used to calculate transform
	std:vector <float> *CalXform[4];	// Transform
	bool Cal_ready;					// Flag to indicate cal values are ready.  Used as non-blocking semaphore.
	float FadeMemVal;				// Fading memory update coefficient
	bool CalDCOnly;					// Flag to say how to cal data
	int CalCount = 0;				// How many calibration blocks have been processed
	bool StopRequested;
};

int initializeCal(CalDataC< RTPComplex>   * pRadarCalData, CPI_Params Params, std::vector<RTPComplex> * pDCOffset);
*/