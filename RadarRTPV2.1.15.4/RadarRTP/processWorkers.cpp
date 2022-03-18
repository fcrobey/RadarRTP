// This module performs the heavy duty processing of the signals received by the radars
// A main processing thread waits on data to be available in the circular buffers and then
// unpacks the channels to reorganize the data and dispatches tasks to the processing threads.
// Multiple processing threads are used to process multiple CPI in parallel, generally with 
// overlapped processing.
// A single thread gathers the results of the parallel computations, saves the data, calls for the 
// display to be updated, and calculates the pendulum swing parameters.
// Written by: Frank Robey
// MIT Lincoln Laboratory
// Dec 2013-Jan 2014, initial implementation. Initial design using a single processing thread could not
//	keep up under Windows when other activities were happening.  (Explored other alternatives, python, matlab, etc.
//  None had the desired maturity/attributes/cost for what is being developed as a hobby project.)
// Jun-Dec 2014, Restructured thread design to use scatter-gather 
// April 2017 to use ini file and to make more modular in terms of number of radars.  
//				Removed hard coding of number of radars.
// Jan 2018 to use C++11 threads rather than windows threads
// Mar 2018 to correct frequency estimator coding error. Saving processed data now works correctly.
//          Worker threads removed from main control thread file  and put in this file
// 
// This has been tested to support 4 radars (8 ADC channels). There is no reason it shouldn't be able
// to do more.
//
// The routines in this module should be easily modifiable to convert to SONAR processing
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
#include <complex>
#include <stdio.h>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <cinttypes>
#include <iostream>

#ifndef _RTP_Headless
extern HWND hWnd;
#endif

// Flag to stop output worker function.
bool OThreadStopRequest = FALSE;

// To stop contention for non-thread-safe fft_plan
std::mutex fftwPlanLock;

void hamming(float *vector, int nsamp)
{
	float a = 0.53836f, b = 0.46164f;
	int i;
	if (nsamp == 1)
		vector[0] = 1.0;
	else {
		for (i = 0; i < nsamp; i++) {
			vector[i] = a - b * (float)cos(TWOPI*(double)i / ((double)(nsamp - 1)));
		}
	}
}


void fftshift(float* datarray, const int nrow, const int ncol)
{
	float *ptemp;
	int j;
	if ((nrow > 1) && (ncol > 1)) {
		ptemp = (float*)malloc(nrow * sizeof(float));
		if (ptemp == NULL) {
			log_message("Error %d: Unable to perform fftshift of data", GetLastError());
			return;
		}
		for (j = 0; j < ncol / 2; j++) {

			// Using pointer arithmetic to point at location for copy
			memcpy(ptemp, datarray + (j * nrow + nrow / 2), nrow / 2 * sizeof(float)); // first half line
			memcpy(ptemp + nrow / 2, datarray + (j * nrow), nrow / 2 * sizeof(float));  // second half line
			memcpy(datarray + j * nrow, datarray + ((j + ncol / 2) * nrow + nrow / 2), nrow / 2 * sizeof(float));
			memcpy(datarray + j * nrow + nrow / 2, datarray + ((j + ncol / 2) * nrow), nrow / 2 * sizeof(float));
			memcpy(datarray + (j + ncol / 2) * nrow, ptemp, nrow * sizeof(float));
		}
		free(ptemp);
		return;
	}
	if (ncol == 1)
	{
		ptemp = (float*)malloc(nrow/2 * sizeof(float));
		if (ptemp == NULL) {
			log_message("Error %d: Unable to perform fftshift of data", GetLastError());
			return;
		}
		// it is a vector, so swap the vector
		memcpy(ptemp, datarray + (nrow / 2), nrow / 2 * sizeof(float)); // first half line		
		memcpy(datarray + (nrow / 2), datarray , nrow / 2 * sizeof(float));
		memcpy(datarray  , ptemp, nrow/2 * sizeof(float));
		free(ptemp);
		return;
	}
	else {
		ptemp = (float*)malloc(ncol / 2 * sizeof(float));
		if (ptemp == NULL) {
			log_message("Error %d: Unable to perform fftshift of data", GetLastError());
			return;
		}
		memcpy(ptemp, datarray + (ncol / 2), ncol / 2 * sizeof(float)); // first half line
		memcpy(datarray+(ncol/2), datarray , ncol / 2 * sizeof(float));
		memcpy(datarray , ptemp, ncol / 2 * sizeof(float));
		free(ptemp);
		return;
	}
}


// From "Fast, Accurate Frequency Estimators" IEEE SP mag May 2007, 
// Cal value for the following windows is: Hanning 0.55, Hamming 0.60, Blackman-Harris 0.56
// Default window is Hamming. Saved windows are Chebychev, so should be similar to BH
// Curve fit and then interpolate to find more accurate Doppler frequency for peak
// The equation is delta = Re { (y1 - y2)/(2 y2 - y1 -y3) }
float PeakEstimate(RTPComplex *a, RTPComplex *b, RTPComplex *c, float cal_val)
{
	float index_frac1;
	RTPComplex ntemp, dtemp;
	ntemp = (*a) - (*c);
	dtemp = 2.0f * (*b) + (*a) + (*c);
	index_frac1 = cal_val* (ntemp / dtemp).real();
	
	return(index_frac1);
}

void stopWorkerThreads(pRadar_Data_Flowing  *pRadarDataArray,
	std::thread  hWorkerThreads[],
	int numThreads)
{
	int threadcnt;
	char msg[64];

	for (threadcnt = 0; threadcnt < numThreads; threadcnt++) {
		// Stop threads - signal to stop and then wake it
		if (pRadarDataArray[threadcnt]->OwnBuffers.try_lock()) {
			pRadarDataArray[threadcnt]->StopRequested = TRUE;
			pRadarDataArray[threadcnt]->DataHere.notify_all();
			snprintf(msg, sizeof(msg), "Inform worker thread %d to stop.", threadcnt);
			log_message((const char *)msg);
			pRadarDataArray[threadcnt]->OwnBuffers.unlock();
		}
		else {
			log_message("Force stop");
			pRadarDataArray[threadcnt]->StopRequested = TRUE;
		}
	}
	log_message("Waiting for SP worker threads to exit");
	// Wait until the threads actually stop.  
	for (threadcnt = 0; threadcnt < numThreads; threadcnt++) {
		hWorkerThreads[threadcnt].join();
	}
	log_message("SP worker threads exited and joined.");
	// Memory is now freed in the destructur
	for (threadcnt = 0; threadcnt < numThreads; threadcnt++) {

	}

	if (pRadarDataArray[0]->pPRI_WGT) {
		free(pRadarDataArray[0]->pPRI_WGT); 
		pRadarDataArray[0]->pPRI_WGT = NULL;
	}
	if (pRadarDataArray[0]->pWRI_WGT) { 
		free(pRadarDataArray[0]->pWRI_WGT); 
		pRadarDataArray[0]->pWRI_WGT = NULL;
	}

	return;
}


// This routine does the range-Doppler processing on a block of data for a single radar in each invocation
// The data is spread across different threads
// The routine uses the metadata to determine how to process the data, but it assumes the metadata is 
// static over the life of the program.  It does not check to see if the parameters have changed during program execution.

void RadarWorkerFunction(pRadar_Data_Flowing MyRadarData,
	float* win_cpi,
	float* win_wri,
	CPI_Params InitParams)

{
	float max_val;
	unsigned int index_max_d;
	char msg[64];
	
	// Initialize this worker thread
	snprintf(msg, sizeof(msg), "Worker thread, %d starting.", MyRadarData->MyID);
	log_message((const char *)msg);

	MyRadarData->initialize(InitParams, win_cpi, win_wri);

	while (!MyRadarData->StopRequested)
	{

		// Wait until data is put into buffer.
		std::unique_lock<std::mutex> bufferlock(MyRadarData->OwnBuffers);
		while (!MyRadarData->InBufferFull && !MyRadarData->StopRequested) {
			std::cv_status cvstat = MyRadarData->DataHere.wait_for(bufferlock, std::chrono::milliseconds(5000));
			if ((cvstat == std::cv_status::timeout) && !MyRadarData->StopRequested)
				log_message("Worker thread waiting on data timeout");
		}
		MyRadarData->InBufferFull = FALSE;
		if (MyRadarData->StopRequested)
		{
			snprintf(msg, sizeof(msg), "Worker thread, %d stop requested.", MyRadarData->MyID);
			log_message((const char *)msg);
			bufferlock.unlock();
			break;
		}

		// Now have data, process it.
		MyRadarData->calibrate();

		// Data is now calibrated.  Continue processing, but check for stop first.
		if (MyRadarData->StopRequested == TRUE)
		{
			bufferlock.unlock();
			break;
		}
		fftwf_execute(MyRadarData->fftwfPlan);

		// Copy frequency domain data into buffer  */
		// The first for loop section grabs data for Range-Doppler Image(s) 
		// Then converts it to dB
		// It puts it into the range-Doppler image with a corner turn and an FFTshift
		//float maxpval = -2000.0;
		max_val = -2000.0;
		unsigned int maxpIndx = 0;
		for (unsigned int i = 0; i < MyRadarData->Params.Samp_Per_WRI*MyRadarData->Params.Num_WRI; i++) {
			MyRadarData->pRDIPower[i] = 10.0f*(float)log10((MyRadarData->pData[i][0] * MyRadarData->pData[i][0] +
				MyRadarData->pData[i][1] * MyRadarData->pData[i][1]) + 1e-15f);
			if (MyRadarData->pRDIPower[i] > max_val) {
				max_val = MyRadarData->pRDIPower[i];
				maxpIndx = i;
			}
		}

		//fftshift(MyRadarData->pRDIPower, MyRadarData->Params.Samp_Per_WRI, MyRadarData->Params.Num_WRI);
		// The following is the range index with no corner turn
		int index_max_r = maxpIndx% MyRadarData->Params.Samp_Per_WRI;
		// The folliwing is the Doppler index with no corner turn
		index_max_d = maxpIndx / MyRadarData->Params.Samp_Per_WRI;
		if (index_max_d >= MyRadarData->Params.Num_WRI) {
			log_message("Index error");
			index_max_d = MyRadarData->Params.Num_WRI - 1;
		}
		// After corner turn
		//index_max_d = (index_max_d + MyRadarData->Params.Num_WRI /2 ) % MyRadarData->Params.Num_WRI;
		//int tmp = ((MyRadarData->Params.Num_WRI / 2) % MyRadarData->Params.Num_WRI)
		//	* MyRadarData->Params.Samp_Per_WRI
		//	+ index_max_r;
	//	log_message("Index %d, range %d, Dopp %d, idxd %d", maxpIndx, index_max_r, index_max_d, tmp);
		
		//std::cout << index_max_d1 << "\n";
		
		MyRadarData->index_max_d = index_max_d;
		MyRadarData->index_max_r = index_max_r;
		MyRadarData->peakAmplitude = max_val;
		
		// Find the centroid of the peak (in 1-D)
		float cal_val = 0.60f; // Cal value for the particular window used.  

		// TODO: fix this so it wraps around the edges rather than truncating
		if ((index_max_d > 0) && (index_max_d < (MyRadarData->Params.Num_WRI - 1))) { // from 1 to NumWRI-2 for this calculation
			int itmp = index_max_d * MyRadarData->Params.Samp_Per_WRI + index_max_r;
			MyRadarData->index_frac_d =
				PeakEstimate((RTPComplex *) &MyRadarData->pData[itmp- MyRadarData->Params.Samp_Per_WRI][0],
				(RTPComplex *)& MyRadarData->pData[itmp ][0],
				(RTPComplex *)& MyRadarData->pData[itmp+ MyRadarData->Params.Samp_Per_WRI][0], cal_val);
		}
		else {
			MyRadarData->index_frac_d = 0.0;  // We can't do curve fitting at the edges (but we actually could wrap around with more work)
		}

		// Todo: calculate fractional portion of range bin
		
		// Results are available.  If a consumer is waiting, wake it.
		MyRadarData->OutBufferFull = TRUE;
		bufferlock.unlock();  // Finished with the buffer
		MyRadarData->DataHere.notify_all();

	}
	
	if (!(MyRadarData->fftwfPlan == NULL)) {
		std::unique_lock<std::mutex> fftwPlanLockU(fftwPlanLock);
		fftwf_destroy_plan(MyRadarData->fftwfPlan);  // Destroy plan is also not thread safe
		snprintf(msg, sizeof(msg), "FFTW plan deleted, thread %d.", MyRadarData->MyID);
		log_message((const char *)msg);
		fftwPlanLockU.unlock();
	}
	else {
		log_message("Warning: fftwfPlan already deleted in thread: %d ", MyRadarData->MyID);
	}
	log_message("Worker thread %d exiting.", MyRadarData->MyID);

	// The following should be moved into the destructor for the radar data
	if (!(MyRadarData->pData == NULL)) {
		fftwf_free(MyRadarData->pData);
		MyRadarData->pData = NULL;
	}
	else {
		snprintf(msg, sizeof(msg), "pData already deleted in thread %d.", MyRadarData->MyID);
		log_message((const char *)msg);
	}

	/*
	if (!(MyRadarData->pTargetLine == NULL)) {
		fftwf_free(MyRadarData->pTargetLine);
		MyRadarData->pTargetLine = NULL;
	}
	else {
		snprintf(msg, sizeof(msg), "pTargetLine already deleted in thread %d.", MyRadarData->MyID);
		log_message((const char *)msg);
	}
	*/
	free(MyRadarData->DCOffsetArr);

	return;
}


/*
Radar_Data_Flowing::~Radar_Data_Flowing()
{

}
*/

int Radar_Data_Flowing::calibrate()
{
	float rsamp, isamp, tmpr, tmpi, ar, ai;
	float window_pt;

	for (unsigned int i = 0; i < Params.Samp_Per_WRI* Params.Num_WRI; i++) {
		rsamp = pData[i][0];
		isamp = pData[i][1];
	
		// correct the data for IQ mismatch and DC offset */						
		if (gRadarConfig.DC_CalOnly) {  // Subtract constant DC offset 
			tmpr = rsamp - DCOffset.real();
			tmpi = isamp - DCOffset.imag();
		}
		else {  				// Subtract time-dependent DC offset 
			tmpr = rsamp - DCOffsetArr[i % Params.Samp_Per_WRI].real();
			tmpi = isamp - DCOffsetArr[i % Params.Samp_Per_WRI].imag();
			//tmpr = rsamp - DCOffset.real();
			//tmpi = isamp - DCOffset.imag();
		}
		
		ar = CalTransform.value[0] * tmpr + CalTransform.value[1] * tmpi;    // apply calibration coefficients 
		ai = CalTransform.value[3] * tmpi + CalTransform.value[2] * tmpr;

		// Now prepare for a 2-D FFT like what would be done for stretch range-Doppler processing 
		window_pt = pPRI_WGT[i % Params.Samp_Per_WRI] * pWRI_WGT[i / Params.Samp_Per_WRI];

		pData[i][0] = (float)window_pt*ar;
		pData[i][1] = (float)window_pt*ai;
	}
	if (FALSE && !gRadarConfig.DC_CalOnly)
		if ((Params.block_id > 0) && (Params.block_id % 50 == 0)) {
			log_message("Using per sample DC offset");
			std::cout << "Block ID: " << Params.block_id << " DC_OffsetArr = ";
			for (unsigned int nnn = 0; nnn < Params.Samp_Per_WRI; nnn++)
				std::cout << DCOffsetArr[nnn];
			std::cout << std::endl;
		}
	return 0;
}

// The following really should be part of the constructor for the Radar_Data_Flowing struct/class

int Radar_Data_Flowing::initialize(CPI_Params InitParams, float* win_cpi, float* win_wri)

{
	Params = InitParams;
	pPRI_WGT = win_wri;	// Pointers to window used to control sidelobes
	pWRI_WGT = win_cpi; // Need to be careful this is not freed while threads are running
	DCOffset = RTPComplex((float)gRadarConfig.CalDCVal[0], (float)gRadarConfig.CalDCVal[0]);  // Default DC offset cal constants 
	CalTransform = gRadarConfig.CalTransForm[0];
	DCOnly = gRadarConfig.DC_CalOnly;
	RadarChan = 0;
	InBufferFull = false;		// Tell thread that input data is in buffer
	OutBufferFull = false;		// Tell following thread that output data is in output buffers
	StopRequested = false;

	/* allocate memory for buffers.  Using the fftw library function forces block alignment as needed for SIMD instructions*/
	pData = (fftwf_complex*)
		fftwf_malloc(sizeof(fftwf_complex) * Params.Samp_Per_WRI*Params.Num_WRI); /* single radar 2-D */
//	pTargetLine = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * Params.Num_WRI);

	pRDIPower = (float*)malloc(Params.Samp_Per_WRI*Params.Num_WRI * sizeof(pRDIPower[0]));
	DCOffsetArr=(RTPComplex*)malloc(Params.Samp_Per_WRI * sizeof( DCOffsetArr[0] ));

	if ((pData == NULL)
		|| (pRDIPower == NULL)
		|| (DCOffsetArr==NULL) )
	{
		log_message("Error %d: Allocation of radar data memory blocks failed.  Exiting", GetLastError());
		// If the array allocation fails, the system is out of memory so exit
		exit(6);
	}

	std::unique_lock<std::mutex> fftwPlanLockU(fftwPlanLock);
	fftwfPlan =
		fftwf_plan_dft_2d(Params.Num_WRI, Params.Samp_Per_WRI, pData, pData, FFTW_FORWARD, FFTW_MEASURE);
	if ((fftwfPlan == NULL)) {
		log_message("Error %d: Creating fftw plan failed in %s : line %d . exiting...", GetLastError(), __FILE__, __LINE__);
		exit(4);
	}
	fftwPlanLockU.unlock();
	return(0);
}

int StartWorkerThreads(pRadar_Data_Flowing * pRadarDataArray, std::thread hWorkerThreads[], int numThreads, CPI_Params InitParams)
{
	int threadcnt;
//	char msg[64];
	float*win_cpi, *win_wri;

	// load processing window for all threads
	// First create storage for windows to weight the data prior to the FFT
	win_cpi = (float*)malloc(InitParams.Num_WRI * sizeof(float));
	win_wri = (float*)malloc(InitParams.Samp_Per_WRI * sizeof(float));
	if ((win_cpi == NULL) || (win_wri == NULL)) {
		log_message("Error %d: Can't allocate memory for window functions in processing thread. Exiting", GetLastError());
		exit(1L);
	}

	// Then load the windows.  This routine will default to a hamming window end emit warning if parameter file is not found 
	load_window(win_cpi, InitParams.Num_WRI, 80);  // 60dB sidelobes still results in sidelobes raising the noise level.  So, use 80dB.
	load_window(win_wri, InitParams.Samp_Per_WRI, 80);
	
	// Start the threads
	for (threadcnt = 0; threadcnt < numThreads; threadcnt++) {
		pRadarDataArray[threadcnt]->MyID = threadcnt;
		hWorkerThreads[threadcnt] = std::thread(RadarWorkerFunction, pRadarDataArray[threadcnt], win_cpi, win_wri, InitParams);
	}

	log_message("Started Worker Threads");
	return(0);
}


void OutputWorkerFunction(pRadar_Data_Flowing pRadarData[], CPI_Params InitParams)
{
	int nextThread = 0;
	int CurRadarChan;
	// char msg[1024];


	log_message("Output gather thread has started.");

	// Initialize output storage 
	gProcessedData.Params = InitParams;

	log_message( "Output format will be: Num WRI = %d, Num Samples per WRI = %d", 
		gProcessedData.Params.Samp_Per_WRI, gRadarConfig.NWRIPerCPI);


	// Allocate memory for storing the output Range-Doppler Image (RDI)
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		gProcessedData.pRDIPower[rindex] = (float*)
			malloc(gProcessedData.Params.Samp_Per_WRI*gProcessedData.Params.Num_WRI *
				sizeof(gProcessedData.pRDIPower[0]));
		if (gProcessedData.pRDIPower[rindex] == NULL) {
			log_message("Error %d: Allocation of radar data memory blocks failed in output gather. Exiting", GetLastError());
			exit(7);  // If the array allocation fails, no way to recover, so exit
		}
	}

	log_message("Starting processing threads output accumulation loop");

	while (!OThreadStopRequest)  // Loop until stop is requested
	{
		// Grab the processing worker thread's output buffer
		std::unique_lock<std::mutex> bufferlock(pRadarData[nextThread]->OwnBuffers);
		// Indefinite wait for results to be ready		
		while (!pRadarData[nextThread]->OutBufferFull && !OThreadStopRequest) {
			std::cv_status cvstat = pRadarData[nextThread]->DataHere.wait_for(bufferlock, std::chrono::milliseconds(1000));
			if ((cvstat == std::cv_status::timeout) && !OThreadStopRequest) log_message("Processing thread accumulation loop waiting on data timeout");
		}


		if (OThreadStopRequest)
		{
			bufferlock.unlock();
			break;  // stop flag - don't pay attention to flag from individual worker threads
		}

		CurRadarChan = pRadarData[nextThread]->RadarChan;
		std::unique_lock<std::mutex> bufferlockOutput(gProcessedData.OwnBuffers);
		
		memcpy(gProcessedData.pRDIPower[CurRadarChan], pRadarData[nextThread]->pRDIPower, pRadarData[nextThread]->Params.Num_WRI*
			pRadarData[nextThread]->Params.Samp_Per_WRI * sizeof(pRadarData[nextThread]->pRDIPower[0]));

		gProcessedData.peakDoppler[CurRadarChan] = ((float)gRadarConfig.UAmbDoppler)*((float)pRadarData[nextThread]->index_max_d +
			pRadarData[nextThread]->index_frac_d - ((float)(pRadarData[nextThread]->Params.Num_WRI / 2)))* 2.0F /
			(float)pRadarData[nextThread]->Params.Num_WRI;
		gProcessedData.peakAmplitude[CurRadarChan] = (float) pRadarData[nextThread]->peakAmplitude;

		// Copy time of validity and block counter
		// There should be a check to verify that Params are the same for each radar
		gProcessedData.Params = pRadarData[nextThread]->Params;
	
		// Doppler peak
		gProcessedData.index_max_d[CurRadarChan] = (int) pRadarData[nextThread]->index_max_d;
		gProcessedData.index_frac_d[CurRadarChan] = (float) pRadarData[nextThread]->index_frac_d;
		gProcessedData.index_max_r[CurRadarChan] = (int)pRadarData[nextThread]->index_max_r;
		gProcessedData.index_frac_r[CurRadarChan] = (float)pRadarData[nextThread]->index_frac_r;

		pRadarData[nextThread]->OutBufferFull = FALSE;

		// At this point we have all of the data that is needed and can unlock access to my input buffer
		// pRadarData[nextThread]->Buflock.unlock();
		bufferlock.unlock();
		bufferlockOutput.unlock(); // And output buffer lock can be removed

		if (CurRadarChan == (gRadarState.NumSensorsSet - 1)) {  // Now have the complete, most recent set of processed data
			//log_message( "Finished processing data. Thread %d.", nextThread);

		   // The following tells the display formatter to prepare the RDI images for display
			bufferlockOutput.lock();
			gProcessedData.InBufferFull = TRUE;
			bufferlockOutput.unlock(); // And output buffer lock can be removed
			gProcessedData.DataHere.notify_all();

			// Processed data recording called here
			if (gRadarState.DataRecording == TRUE)  save_processed_data();  // This needs to be more robust
		}
		nextThread++;
		if (nextThread >= gRadarConfig.NumThreads) nextThread = 0;

	}
	//OutputWorkerCleanup: Falls through to here when StopRequested
	log_message("Display Interface cleanup started.");

	// Delete memory allocated here
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		free(gProcessedData.pRDIPower[rindex]);
		gProcessedData.pRDIPower[rindex] = NULL;
	}

	log_message("Display Interface routine exiting.");
	return;
}
