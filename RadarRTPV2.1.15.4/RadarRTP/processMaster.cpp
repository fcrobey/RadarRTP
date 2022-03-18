// This module coordinates the processing of the signals received by the radars
// The main processing thread waits on data to be available in the circular buffers and then
// unpacks the channels to reorganize the data and dispatches tasks to the processing threads.
// Multiple processing threads are used to process multiple CPI in parallel.
// A single thread gathers the results of the parallel computations, saves the data, calls for the 
// display to be updated, .
// Written by: Frank Robey
// MIT Lincoln Laboratory
// Dec 2011-Jan 2014, initial implementation. Initial design using a single processing thread could not
//	keep up under Windows when other activities were happening.  (Explored other alternatives, python, matlab, etc.
//  None had the desired maturity/attributes/cost for what is being developed as a hobby project.)
// Jun-Dec 2014, Restructured thread design to use scatter-gather 
// April 2017 to use ini file and to make more modular in terms of number of radars.  
//			Removed hard coding of number of radars (except there is still a hard limit)
// Jan 2018 to use C++11 threads rather than windows threads
// Mar 2018 to correct frequency estimator coding error. Saving processed data now works correctly.
//			Moved processing worker threads into a separate file.  Moved initializer code into a member function. 
// Mar 2018, revised the simulation code to add in the simulated target as a complex number
//
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

#include "stdafx.h"
#include <complex>
#include <stdio.h>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <cinttypes>
#include <iostream>
#include "MyRawDataBuffer.h"

std::thread tproc_thread_id;

/* Thread synchronization flags */
bool threadSyncFlag = FALSE;
extern bool OThreadStopRequest;

int startProcessingThread(void)
// Create the main processing thread and boost the priority to a higher level.
{
	log_message("Starting main processing thread");
	// The following starts the main processing loop that manages distribution of data to other threads
	tproc_thread_id = std::thread(Process_data);

	/* Set thread to higher priority than normal */
	// This doesn't seem to be needed, even for low end windows computer
#ifdef _WIN32
	SetThreadPriority(tproc_thread_id.native_handle(), THREAD_PRIORITY_HIGHEST);
	log_message("Priority of processing thread has been increased");
	/* Start it up */
	threadSyncFlag = TRUE;
	ResumeThread(tproc_thread_id.native_handle());
#else /* Unix version is not implemented.  Does not seem to be needed.  */

#endif

	//Note: It would have been better to use a condition variable rather than threadSyncFlag to indicate startup

	/* Wait for thread to startup */
	log_message("Waiting for processing thread startup.");
	while (threadSyncFlag) {
		Pa_Sleep(5);  /* Sleep while waiting for startup */
	}
	log_message("Processing thread started successfully");
	return(0);
}


/* The following is called by the main routine to stop the processing thread */
int stopProcessingThread(void)
{
	log_message("Telling main signal processing thread to stop.");
	threadSyncFlag = TRUE;

	/* Wait for processing thread to stop */
	tproc_thread_id.join();
	log_message("Main signal processing thread has stopped.");
	return(0);
}


// This is the routine that coordinates the signal processing work

// TODO: If I were doing this now, I would use vectors of threads and vectors of smart data structures rather than fixing the number at compile time. 
// Since this is working I haven't felt the need to update the code.

int Process_data(void)
{
	unsigned int count = 0;  // This is the count of how many blocks were processed.
	int NextThread;
	CPI_Params Params;

	Radar_Data_Flowing RadarDataArray[MaxThreads];
	pRadar_Data_Flowing  pRadarDataArray[MaxThreads]; // This is an array of pointers to the data blocks.
	for (int it = 0; it < MaxThreads; it++)
		pRadarDataArray[it] = &RadarDataArray[it];

	// Raw data buffers are allocated in this function.  Processed datat buffers are allocated in the WorkerThreads initialization
	// Order of stopping threads is important for thread integrity and memory cleanup

	std::thread  hWorkerThreads[MaxThreads];
	std::thread hGatherThread;
	std::thread hCalibrateThread;
	
	RTPComplex DCOffset[MaxRadars], *DCOffsetVals[MaxRadars];

	std::vector <RTPComplex> DCOffset1(gRadarState.NumSensorsSet* gRadarConfig.NSamplesPerWRI);

	DataTics TOVtt;  // Time of validity in clock ticks

	double 	phaseinc = 140.0*TWOPI / gRadarConfig.SampleRate;
	double pendfreqinc = 0.125*TWOPI / gRadarConfig.SampleRate;
	PendSimParms SimParms;
	SimParms.RadarSimInit();
	
	int thisADCFrameCount;  // Buffer count used for simulating the target

	/* initialize */
	log_message("Process Data thread starting.");
	Params.Num_WRI = gRadarConfig.NWRIPerCPI;
	Params.Samp_Per_WRI = gRadarConfig.NSamplesPerWRI;
	Params.block_id = 0;
	Params.Data_TOVtt = std::chrono::duration<int>(0);
	Params.NumSensorsSet = gRadarState.NumSensorsSet; 
	RawDataBuffer RadarRawDat(Params, gRadarConfig.NWRIPerBlock, gRadarConfig.ReceiveRealOnly);
		
	sensordata< RTPComplex> SimData(Params);
	sensordata< RTPComplex> RadarDat((unsigned int)gRadarConfig.NWRIPerCPI, 
		(unsigned int)gRadarConfig.NSamplesPerWRI,
		(unsigned int)gRadarState.NumSensorsSet);

	SimData(0, 0, 0) = RTPComplex(0.0, 1.0);
	//std::cout << "SimData" << SimData(0, 0, 0)<<std::endl;

	std::vector<floatdim4> CalXform1(gRadarState.NumSensorsSet);
	// load default cal constant to initialize. True values applied later.
	for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
		CalXform1[rindex] = gRadarConfig.CalTransForm[rindex];
	}

	log_message("Setting up worker threads.");

	// Set up and initialize the worker threads
	
	StartWorkerThreads(pRadarDataArray, hWorkerThreads, gRadarConfig.NumThreads, Params);

	// The following delay is just to give the threads time to start, which makes the logging look nicer. It is not needed functionally
	std::this_thread::sleep_for(std::chrono::milliseconds(150));

	// Start up output accumulation thread that merges and aligns the results from the different processing threads
	OThreadStopRequest = FALSE;
	hGatherThread = std::thread(OutputWorkerFunction, pRadarDataArray, Params);

	CalData RadarCalDat(Params);
	RadarCalDat.initializeCal(Params);
	std::unique_lock <std::mutex> CalBufferlock(RadarCalDat.OwnBuffers, std::defer_lock);  // Declare mutex for buffer
	hCalibrateThread = std::thread(&CalData::CalibrateFunction, &RadarCalDat, 5);

	log_message("Finished init of processing and worker threads");

	threadSyncFlag = FALSE;  // This will let main processing thread know we have completed initialization.
	NextThread = 0;
	
	// Loop and dispatch data for processing
	while (TRUE) {
		/* wait for data */
		int dataIndex;	//Index for data available in circular buffer. 
		int offset; // Index into data block- value depends on data type (real v. complex) and number of radars
		// float *dptr;  // Pointer into data blocks - used to iterate

		while ( ((dataIndex = buff_Wait_For_Data()) < 0 )|| (dataIndex>20) ) {  // This will block until data is available or timeout error
			if (threadSyncFlag) { break; }	// if the main thread sets this, then stop processing 
			if (count>0) log_message("Warning: Timeout waiting for ADC data."); // Check to suppress warning on startup
		}
		
		if (threadSyncFlag) {  // Even if have data, if the main thread sets this, then stop processing
			log_message("Got signal interrupting processing thread. Will close workers and exit gracefully.");
			break;
		}

		// if recording, save raw data 

		if (gRadarState.RawRecording) {
			save_raw_data(Buff_Data[dataIndex], 2 * Params.Samp_Per_WRI * gRadarConfig.NWRIPerBlock * gRadarState.NumSensorsSet);
		}

		// Now deinterleave the input data into the raw data buffer(s)
		offset=RadarRawDat.LoadData(Buff_Data[dataIndex]);

		TOVtt = Buff_Time_ticks[dataIndex];
		double nowTime = Buff_Time[dataIndex].currentTime;  // Warning- this is not synchronized to ADC clock

		// The following is to support injecting simulated data.
		// A new timer/counter is needed because the portAudio ADCTime is not regular
		// Ideally the ADCTime would increase regularly with data samples. It doesn't.
		// Basing the simulation on a clock that is not regular caused very high sidelobes
		// and other odd effects.  
		// Using this as the time refererence introduced a different timing problem since pendulum swings too slowly in
		// simulation at high (>1MSPS) sample rates. Since it is not observed normally, then I haven't determined why that is
		thisADCFrameCount = Buff_count[dataIndex];
		
		double ADCSimTime = ((double)thisADCFrameCount)*(Params.Samp_Per_WRI*gRadarConfig.NWRIPerBlock) 
			/ gRadarConfig.SampleRate;
		buff_free();  /* Free this buffer */
		
	// Do some checking on whether the sim is on and whether the amplitude is in bounds
	// Set the simulated signal amplitude to the value from the current configuration
	// The configuration value is in dBfs, so convert to linear amplitude
		float SimAmpLin = 0.01f;
		if (gRadarState.SimOn) {
			//std::cout << " " << thisADCFrameCount << " " << ADCSimTime;
			// Generate the simulated data. (TODO: Verify the simulation amplitude is in bounds)
			SimParms.simTargetData(ADCSimTime, &SimData, gRadarState.SimAmp);

			RadarRawDat.AddSimData(&SimData, offset);
		}

		// This dispatches the data to the processing worker threads.
		// It does it by parsing out one radar block per worker thread for each time step.
		// The data is put back together in order by a data accumulation thread.
		//
		// As originally written, when this routine skipped an update to avoid buffer over-run, it confused the accumulation thread.
		// Corrected by using lock() instead of try_lock().  Seems to help, but the logic should be reviewed.
		// Fill the buffer on the next worker thread and then kick it off.
		for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
			pRadarDataArray[NextThread]->OwnBuffers.lock(); { // Lock the buffer so I own it.
			//if (pRadarDataArray[NextThread]->OwnBuffers.try_lock()) { // Lock the buffer so I own it.
			// Now own the data buffer.  Fill it with data then kickoff processing task.

				RadarRawDat.CopyOut((RTPComplex*) pRadarDataArray[NextThread]->pData, rindex);

				pRadarDataArray[NextThread]->Params.block_id = count;
				pRadarDataArray[NextThread]->Params.Data_TOVtt = TOVtt;

				pRadarDataArray[NextThread]->RadarChan = rindex;

				pRadarDataArray[NextThread]->DCOffset = DCOffset[rindex];
				pRadarDataArray[NextThread]->CalTransform = CalXform1[rindex]; // Gain and correlation cal constants

				pRadarDataArray[NextThread]->InBufferFull = TRUE;
				pRadarDataArray[NextThread]->OwnBuffers.unlock();

				// Now the persample DC offset
				if (!gRadarConfig.DC_CalOnly) {
					for (unsigned int sampIdx = 0; sampIdx < Params.Samp_Per_WRI; sampIdx++)
						pRadarDataArray[NextThread]->DCOffsetArr[sampIdx] = DCOffsetVals[rindex][sampIdx];

				}
			}

			// Wake SP thread item to get started.
			pRadarDataArray[NextThread]->DataHere.notify_all(); // Should only be one process waiting
			NextThread++;
			if (NextThread >= (gRadarConfig.NumThreads)) NextThread = 0;
		}

		count++;  // Increment processed block counter	
		gRadarState.Current_block_id = count; // Update the counter for the radar state

		if (threadSyncFlag == TRUE) break; // signaled to stop, so break out of loop and stop
		if (gRadarState.AutoCalOn) {
			// Kickoff calibration thread 
			if (((count - 20) % 50) == 0) {  // every so often- currently every 50th CPI, start calibration task
											  // Grab the calibration data buffer

				if (CalBufferlock.try_lock()) {
					for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
						//memcpy(&RadarCalDat.pCalData[rindex][0][0], pRadarRawDat[rindex],
						//	(Params.Samp_Per_WRI*Params.Num_WRI) * sizeof(fftwf_complex));
						RadarRawDat.CopyOut((RTPComplex *) &RadarCalDat.pCalData[rindex][0], rindex);
					}

					RadarCalDat.Params = Params;
					RadarCalDat.InBufferFull = TRUE;
					CalBufferlock.unlock();
				}
				else { log_message("Warning: Cal routine not ready for data, try later..."); }

			}
		}
		// Now prepare for next data block - move raw data up in buffer
		
		RadarRawDat.MoveUp();
		// If a previously kicked off cal thread completed the update, then copy and start using it
		if (gRadarState.AutoCalOn) {
			if (CalBufferlock.try_lock()) {
				if (RadarCalDat.Cal_ready) {
					for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
						DCOffset[rindex] = RadarCalDat.DCOffset[rindex];
						CalXform1[rindex] = RadarCalDat.CalXform[rindex];
						
						if (!RadarCalDat.CalDCOnly) {
							// Copy per WRI sample DC offset values
							memcpy(&DCOffsetVals[rindex][0], &RadarCalDat.pOffsetsdat[rindex][0],
								(Params.Samp_Per_WRI * sizeof(fftwf_complex)));
							
						}

					}
					RadarCalDat.Cal_ready = FALSE;
				}
				CalBufferlock.unlock();
			}
		}
	} /* Go back up and keep looping until signaled to stop */

	// We got a signal to stop, so clean up.
	//closeDebugDataFile();
	// Stop output worker thread
	OThreadStopRequest = TRUE;

	// Stop cal thread
	log_message("Stopping calibration thread");
	RadarCalDat.StopRequested = TRUE;
	RadarCalDat.InBufferFull = TRUE;

	hGatherThread.join();
	hCalibrateThread.join();
	cleanupCalThread(&RadarCalDat, &DCOffsetVals[0], &DCOffset[0]);

	char logmsg[128];
	snprintf(logmsg, sizeof(logmsg), "Stopping signal processing. Total data blocks processed = %d", count);
	log_message((const char *)logmsg);

	stopWorkerThreads(pRadarDataArray, hWorkerThreads, 	gRadarConfig.NumThreads);

	log_message("All signal processing threads have been signaled");
	/* stop thread and exit */
	threadSyncFlag = FALSE;  /* let the calling program know we are stopping */

	//for (int rindex = 0; rindex < gRadarState.NumSensorsSet; rindex++) {
	//	if (pRadarRawDat[rindex] != NULL) { fftwf_free(pRadarRawDat[rindex]);  pRadarRawDat[rindex] = NULL; }
	//}

	return(0);
}