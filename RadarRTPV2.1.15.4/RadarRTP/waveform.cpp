// This module is the waveform generation subroutine
// Focused on generating a ramp, but can generate a chirp for sonar
//
// Written by Frank Robey
// 

// Apr 2018 Split this file (makewaveform) out of radar_io.cpp
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

// Create the waveform that will be output by the soundcard
// Primarily intended to create a ramp that will chirp the radar.
// Sonar waveform and processing added over several months in late 2017-Apr 2018
// Only limited utility in chirping the radar at this time since equalization and calibration needed
// to make chirp processing work well has not been completed.
int create_waveform()

{
	int k, dindex, p;
	float wave_val;

	if (NUMOUTCHAN == 0) {
		return(0);
	}
	if (WaveData != NULL) {
		free(WaveData);
		WaveData = NULL;
	}

	WaveData = (float*)malloc(gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock * NUMOUTCHAN * sizeof(WaveData[0]));
	if (WaveData == NULL)
	{
		log_message("Error %d: Malloc of output data array failed, exiting", errno);
		exit(1);  /* Should exit gracefully rather than just quitting */
	}
	if (gRadarConfig.Sonar) {

		// Allocate memory for pulse compression replica and down conversion
		if (WaveformReplica != NULL)
			free(WaveformReplica);
		WaveformReplica = (RTPComplex*)malloc(gRadarConfig.NSamplesPerWRI * sizeof(WaveformReplica[0]));
		if (DownConvertWF != NULL)
			free(DownConvertWF);
		DownConvertWF = (RTPComplex*)malloc(gRadarConfig.NSamplesPerWRI * sizeof(DownConvertWF[0]));
		if ((WaveformReplica == NULL) || (DownConvertWF == NULL))
		{
			log_message("Error %d: Malloc of waveform data arrays failed, exiting..", errno);
			exit(1);  /* Should exit gracefully rather than just quitting */
		}
	}

	double WFAmp;
	if (gRadarState.TransmitModOn) {
		WFAmp = gRadarState.TransmitModAmp;
	}
	else {
		WFAmp = 0.0;
	}

	/* Initialize the transmit waveform */
	if (!gRadarConfig.Sonar) {  // If not sonar, then it is the radar VCO control
		if (!gRadarState.TransmitLongSweep) {
			/* Note that this is only used when ranging.   */
			for (k = 0, dindex = 0; k < (gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock); k++) {
				wave_val = (float)(WFAmp*(float)(
					((k + ((int) gRadarConfig.TxRxSampleOffset)) % gRadarConfig.NSamplesPerWRI)
					- gRadarConfig.NSamplesPerWRI / 2))
					/ (float)gRadarConfig.NSamplesPerWRI;  /* This generates a sawtooth from -1 to (1-1/gRadarConfig.NSamplesPerWRI) */
				WaveData[dindex++] = wave_val;
				if (NUMOUTCHAN > 1) {
					for (p = 1; p < NUMOUTCHAN; p++) {
						WaveData[dindex++] = 0.0f;	// Currently setting all other channels to zero
					}
				}
			}
		}
		else {
			for (k = 0, dindex = 0; k < (gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock); k++) {
				wave_val = (float)(WFAmp* (
					((k + ((int)gRadarConfig.TxRxSampleOffset)) % (gRadarConfig.NSamplesPerWRI*gRadarConfig.NWRIPerBlock))
					- (gRadarConfig.NSamplesPerWRI* gRadarConfig.NWRIPerBlock) / 2))
					/ (float)(gRadarConfig.NSamplesPerWRI* gRadarConfig.NWRIPerBlock);  /* This generates a sawtooth from -1 to (1-1/gRadarConfig.NSamplesPerWRI) */
				WaveData[dindex++] = wave_val;
				if (NUMOUTCHAN > 1) {
					for (p = 1; p < NUMOUTCHAN; p++) {
						WaveData[dindex++] = 0.0f;	// Currently setting all other channels to zero
					}
				}
			}

		}
		log_message("Created VCO tuning voltage sweep for RF Waveform.");
	}
	else { // Create a sonar waveform on one channel

			// Create the analytic sonar waveform and the downconversion mixer waveform
		double argument;
		for (k = 0; k < gRadarConfig.NSamplesPerWRI; k++) {
			argument = TWOPI * ((gRadarConfig.SonarFreq - gRadarConfig.SonarBandwidth / 2.0) +
				gRadarConfig.SonarBandwidth * ((double)k) / (2.0*((double)gRadarConfig.NSamplesPerWRI)))
				* ((double)k);
			WaveformReplica[k] = (RTPComplex)exp(std::complex<double>(0.0, argument));
			DownConvertWF[k] = (RTPComplex)exp(std::complex<double>(0.0, -TWOPI*((double)k)*gRadarConfig.SonarFreq / gRadarConfig.SampleRate));

		}
		for (k = 0, dindex = 0; k < (gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock); k++) {
			wave_val = (float)WaveformReplica[(k + ((int)gRadarConfig.TxRxSampleOffset)) % gRadarConfig.NSamplesPerWRI].real();  /* This generates a sawtooth from -1 to (1-1/gRadarConfig.NSamplesPerWRI) */
			WaveData[dindex++] = wave_val;
			// Store the complex replica
			if (NUMOUTCHAN > 1) {
				for (p = 1; p < NUMOUTCHAN; p++) {
					WaveData[dindex++] = 0.0f;	// Currently setting all other channels to zero
				}
			}
		}
		if (gRadarState.TransmitLongSweep) {
			log_message("Warning: Long sweep is currently not implemented for sonar. Ignoring");
		}
		// Now downconvert and convert the replica to the frequency domain (FFT)
		for (k = 0; k < gRadarConfig.NSamplesPerWRI; k++) {
			WaveformReplica[k] *= DownConvertWF[k];  // Note, not doing any waveform tapering/windowing
		}
		fftwf_plan fftwfReplicaPlan;
		std::unique_lock<std::mutex> fftwPlanLockU(fftwPlanLock);
		fftwfReplicaPlan =
			fftwf_plan_dft_1d(gRadarConfig.NSamplesPerWRI, (fftwf_complex*)WaveformReplica, (fftwf_complex*)WaveformReplica, FFTW_FORWARD, FFTW_ESTIMATE);
		if ((fftwfReplicaPlan == NULL)) {
			log_message("Error %d: Creating fftw plan failed in waveform creation %s : line %d , exiting...",  GetLastError(), __FILE__, __LINE__ );
			exit(4);
		}
		else {
			
		}
		
		fftwf_execute(fftwfReplicaPlan);
		fftwf_destroy_plan(fftwfReplicaPlan);
		fftwPlanLockU.unlock();

		log_message("Created sonar waveform replica");

	}
	
	return(0);
}

