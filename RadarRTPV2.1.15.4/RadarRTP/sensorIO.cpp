/*
This set of functions reads in the ADC data into the circular buffer. 

Initial version authored by Frank Robey
December 2013	Initial implementation
Aug/Sept 2014	Realtime processing for two radars (4 soundcard channels) added rather than just one
Nov		 2014   Add calibration data structure. Offline calibration enabled.
Oct		2017	Edited to improve error checking. PortAudio startup moved to separate function
Mar		2018	Added ADC simulation to replace portaudio. Added modules to radarSim.cpp to support.
Sep		2020	Split these routines out from the radarControl file
*/

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
#include "radarc.h"
#include <random>
//#include <algorithm>

// Forward declare routine local to this module
int init_ADC_data(void);

// A local variable used for streaming ADC data control
PaStream* stream;
int PaADCFrameCount = 0;

int paWaveCallback(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags, void* framecount)
{
	int wr_ind;
	int* fc = (int*)framecount;
	int test = (int)statusFlags & 255; //Portaudio only uses the first 5 bits
	if (test != 0) {
		if ((statusFlags & paInputUnderflow) ||(statusFlags & paInputOverflow))
			log_message("Warning, ADC Input under/overflow %d",test);
		else
			if ((statusFlags & paOutputUnderflow)||(statusFlags & paOutputOverflow))
				log_message("Warning, portaudio callback OutputUnderflow %d",test);
			else
				log_message("Warning, portaudio callback status = %d",test);
	}
	/* Copy waveform data into output buffer */
	if (outputBuffer != NULL) {
		memcpy(outputBuffer, WaveData, framesPerBuffer * sizeof(float) * NUMOUTCHAN);
	}

	if (inputBuffer != NULL)  /* Process the input data */
	{
		/* Get index of next free buffer */
		wr_ind = buff_get_next_free();

		// Keep track of the number of ADC input buffers that have been used
		(*fc)++;  // This increments the buffer count. Interface does not keep track.
		Buff_count[wr_ind] = *fc;

		/* if no free buffer, then return after printing over-run message */
		if (wr_ind < 0) { /* no free buffer */
			log_message("Warning: Receive data input buffer over-run %d", wr_ind);
		}
		else {
			/* copy data to buffer */
			memcpy(Buff_Data[wr_ind], inputBuffer, framesPerBuffer * gRadarState.NumADCChans * sizeof(float));
			/* copy time to buffer */
			Buff_Time[wr_ind].outputBufferDacTime = timeInfo->outputBufferDacTime;
			Buff_Time[wr_ind].currentTime = timeInfo->currentTime;
			// I am finding that the field ->inputBufferAdcTime is not set properly. It is usually 0
			Buff_Time[wr_ind].inputBufferAdcTime = timeInfo->inputBufferAdcTime;
			int64_t mytime = (int64_t)(1e6L * (long double)(timeInfo->inputBufferAdcTime - gStreamPATimeRef));
			Buff_Time_ticks[wr_ind] = std::chrono::microseconds(mytime);
			double mytime1;
			long double mytime2;
			mytime2= (1e6L * (long double)(timeInfo->inputBufferAdcTime - gStreamPATimeRef));
			mytime1 = (double)mytime;
			long ftmp;
			ftmp = (*fc) * gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock;
			buff_mark_used(wr_ind);  /* mark this buffer as used - and cue the processing thread */
			// The following is helpful to debug timing
		/*	debugPrint("%4d:%d %lu %lf, %d: %d, %ld", 
				(*fc), test, framesPerBuffer, 
				timeInfo->currentTime- gStreamPATimeRef,
				gRadarConfig.NSamplesPerWRI, gRadarConfig.NWRIPerBlock,
				ftmp);
				*/
		}

	}
	return 0;
}


#ifdef _WIN32
#if PA_USE_ASIO
// This routine checks whether an ASIO device is present and returns the device number if so.
// Return value is negative if no ASIO
// Unfortunately, this had a seg fault crash in a test on Linux, so can't call there. 
// Putting ifdef's precludes calling from Linux
int IsASIOPresent()
{
	//char msg[256];
	int devnum = -1;
	PaHostApiIndex myPaHostIDX;
	const PaHostApiInfo* myPaHostApi = NULL;
	myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(paASIO);  // Did this get built with ASIO?
	if (myPaHostIDX != paHostApiNotFound) {
		myPaHostApi = Pa_GetHostApiInfo(myPaHostIDX);
		if (myPaHostApi != NULL) {
			devnum = myPaHostApi->defaultInputDevice;
		}
	}
	else {
		devnum = -1;
		log_message("Warning: No ASIO device interface is available. In function IsASIOPresent, : %s ", Pa_GetErrorText(myPaHostIDX));
	}
	return(devnum);

}
#endif
int FindDevice(int NumChan, double srate, PaStreamParameters* inputParameters, PaStreamParameters* outputParameters, PaHostApiTypeId hostAPI)
{
	int numDevices;
	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		log_message("Warning: Pa_CountDevices returned 0x%x\n", numDevices);
		return(-1);
	}

	log_message("Looking for %s audio ADC device %d, Nchan= %d, rate= %lf", Pa_GetHostApiInfo(Pa_HostApiTypeIdToHostApiIndex(hostAPI))->name, hostAPI, NumChan, srate);
	int DevNum, desDev = -1;
	const   PaDeviceInfo* deviceInfo;
	if ((inputParameters == NULL) && (outputParameters == NULL)) return(-1);
	int defaultDev;
	if (inputParameters != NULL) {
		desDev = -1;
		for (DevNum = 0; DevNum < numDevices; ++DevNum)
		{
			deviceInfo = Pa_GetDeviceInfo(DevNum);
			if (DevNum == Pa_GetDefaultInputDevice()) { 
				defaultDev = 1; }
			else { 
				defaultDev = 0; }

			inputParameters->device = DevNum;
			inputParameters->suggestedLatency = NULL;
			inputParameters->channelCount = NumChan;
			inputParameters->hostApiSpecificStreamInfo = NULL; //&AsioInputInfo;
			inputParameters->sampleFormat = paFloat32;
			inputParameters->suggestedLatency = Pa_GetDeviceInfo(DevNum)->defaultLowInputLatency;

			log_message("Device %d, %s, API: %s, Default: %d Max in: %d, Max out: %d, default rate: %lf", DevNum,
				deviceInfo->name, Pa_GetHostApiInfo(deviceInfo->hostApi)->name, defaultDev, deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels, deviceInfo->defaultSampleRate);
			PaError err;

			err = Pa_IsFormatSupported(inputParameters, NULL, srate);

			if ((err == paFormatIsSupported) && (deviceInfo->hostApi == Pa_HostApiTypeIdToHostApiIndex(hostAPI)))

			{
				log_message("Rx device: %d supports requested format, %d, %d, %d, %d", DevNum, deviceInfo->hostApi,
					Pa_HostApiTypeIdToHostApiIndex(hostAPI), hostAPI, paASIO);
				desDev = DevNum;
			}
			else
			{
				//log_message("Devices: %d does not support requested format", DevNum);
			}
		}

		return(desDev);
	}
	if (outputParameters != NULL) {
		desDev = -1;
		for (DevNum = 0; DevNum < numDevices; ++DevNum)
		{
			deviceInfo = Pa_GetDeviceInfo(DevNum);
			if (DevNum == Pa_GetDefaultOutputDevice()) { defaultDev = 1; }
			else { defaultDev = 0; }
			outputParameters->device = DevNum;
			outputParameters->suggestedLatency = NULL;
			outputParameters->channelCount = 2;
			outputParameters->hostApiSpecificStreamInfo = NULL;
			outputParameters->sampleFormat = paFloat32;

			log_message("Device %d, %s, API: %s, Default:%d, Max in: %d, Max out: %d, default rate: %lf", DevNum,
				deviceInfo->name, Pa_GetHostApiInfo(deviceInfo->hostApi)->name, defaultDev, deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels, deviceInfo->defaultSampleRate);
			PaError err;
			err = Pa_IsFormatSupported(NULL, outputParameters, srate);

			if ((err == paFormatIsSupported) && (deviceInfo->hostApi == Pa_HostApiTypeIdToHostApiIndex(hostAPI)))
			{
				log_message("Tx device: %d supports requested format, %d, %d, %d, %d", DevNum, deviceInfo->hostApi,
					Pa_HostApiTypeIdToHostApiIndex(hostAPI), hostAPI, paASIO);
				desDev = DevNum;
			}
			else
			{
				//log_message("Devices: %d does not support requested format", DevNum);
			}
		}

		return(desDev);
	}
	return(-1);
}

#endif




// Initialize the ADC/audio data path.
// A better way to open the port audio stream would have been to first test the default device to see if it had the desired
// sample rate and number of channels. 
// If the default doesn't have what is needed then check the other devices.
// For windows the preferred order is: ASIO, directsound.
// For Linux the rountine only checks for an ALSA interface.
// When opening the interface if the interface fails for the desired number of desired receive channels then it
// fall back to 2 channels.

int init_ADC_data()
{
	if (gRadarConfig.SimADC) {
		log_message("SimADC is true, so the input ADC data path will be simulated");
		gRadarState.NumADCChans = 2 * gRadarConfig.NumRadars;
		startSimADC();
	}
	else {
		PaError err = paNoError;  // Initialize to no error.
		PaStreamParameters InputParameters, OutputParameters;
		int Desired_Rx_Device = -1, Desired_Tx_Device = -1;  // The default device is from the enumeration provided by pa_devs.
		char msg[1024];
		PaHostApiIndex myPaHostIDX = -1;
		PaHostApiTypeId mypaAudioHostAPI = paDirectSound;
		int NChan;

		log_message("Initialize PortAudio as the ADC input method");
		err = Pa_Initialize();
		log_message("Completed Pa_Initialize");
		if (err != paNoError) {
			log_message("Portaudio ADC input method did not initialize. Error: %s", Pa_GetErrorText(err));
			return(-1);
		}

		if (gRadarConfig.ReceiveRealOnly)
			NChan = gRadarState.NumSensorsSet;
		else
			NChan = 2 * gRadarState.NumSensorsSet;

		if ((gRadarConfig.RxADC_Chan >= 0) && (gRadarConfig.TxADC_Chan >= 0)) {
			Desired_Rx_Device = gRadarConfig.RxADC_Chan;
			Desired_Tx_Device = gRadarConfig.TxADC_Chan;
		}
		else {

			// This is an example of how to check for a particular interface
			// bool isAsio = Pa_GetHostApiInfo(Pa_GetDeviceInfo(deviceIndex)->hostApi)->type == paASIO;

#ifdef _WIN32
#if PA_USE_ASIO
			
			if (gRadarConfig.ASIOPriority ||
				(gRadarConfig.ReceiveRealOnly && gRadarConfig.NumRadars > 2) ||
				(!gRadarConfig.ReceiveRealOnly && gRadarConfig.NumRadars > 1)) {
				log_message("Searching for PortAudio ASIO low latency stream device, ASIO priority = %d",
					gRadarConfig.ASIOPriority);
				/* First check if a desired low latency (ASIO) I/O path is available  */
				Desired_Rx_Device = IsASIOPresent();

				if (Desired_Rx_Device >= 0 && (Desired_Rx_Device != paNoDevice)) {  // Have ASIO device
					//Have ASIO device, determine parameters
					log_message("ASIO is present, %d", Desired_Rx_Device);
					mypaAudioHostAPI = paASIO;
					// Find the device number
					log_message("Calling FindDevice, %d, %lf, %d, type %s", NChan, gRadarConfig.SampleRate, paASIO, Pa_GetHostApiInfo(Pa_HostApiTypeIdToHostApiIndex(paASIO))->name);
					Desired_Rx_Device = FindDevice(NChan, gRadarConfig.SampleRate, &InputParameters, NULL, paASIO);
					if (Desired_Rx_Device >= 0) {
						log_message("ASIO Rx Device meeting all goals is: %d", Desired_Rx_Device);
					} else {
						log_message("No ASIO Rx Device with all desired properties");
						// Try again with just 2 channels
						if (NChan > 2) {
							Desired_Rx_Device = FindDevice(2, gRadarConfig.SampleRate, &InputParameters, NULL, paASIO);
							if (Desired_Rx_Device >= 0) {
								log_message("ASIO Device meeting 2-channel reduced goal is: %d", Desired_Rx_Device);
								NChan = 2;
								if (gRadarConfig.ReceiveRealOnly)
									gRadarState.NumSensorsSet = 2;
								else
									gRadarState.NumSensorsSet = 1;
							}
							else {
								Desired_Rx_Device = -1; // Didn't find a device for <=2 channels
							}
						}
						else {
							log_message("Going with default ASIO");
							// The following line is wrong...
							//Desired_Rx_Device = IsASIOPresent();
							Desired_Rx_Device = -1;
						}
					}
					// Find output device
					Desired_Tx_Device = FindDevice(NChan, gRadarConfig.SampleRate, NULL, &OutputParameters, paASIO);
					if (Desired_Tx_Device >= 0) {
						log_message("ASIO Device meeting TX goals is: %d", Desired_Tx_Device);
					}
					else
					{
						log_message("No ASIO Device with all desired properties");
						myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(mypaAudioHostAPI);
						Desired_Tx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultOutputDevice;
					}
				}
			}
			else  // ASIO isn't a priority
			{
				log_message("ASIO device is not a priority or not more than 2 ADC channels needed\nWill try to use default device.");
				mypaAudioHostAPI = paMME; 
				myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(mypaAudioHostAPI);
				Desired_Rx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultInputDevice;
				Desired_Tx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultOutputDevice;
			}
#else // WIN32, but not being built with ASIO
			mypaAudioHostAPI = paMME;
			//Desired_Rx_Device = FindDevice(NChan, gRadarConfig.SampleRate, &InputParameters, NULL, mypaAudioHostAPI);
			
			myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(mypaAudioHostAPI);
			//Desired_Rx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultInputDevice;
			//Desired_Tx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultOutputDevice;
#endif
#else // For Linux
			mypaAudioHostAPI = paALSA;
			//	mypaAudioHostAPI = paJACK;
			myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(mypaAudioHostAPI);
			//Desired_Rx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultInputDevice;
			//Desired_Tx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultOutputDevice;
#endif
			}

		
		
		if (Desired_Rx_Device < 0) { // Try to find a device meeting goals
			if (NChan > 2) NChan = 2;
			Desired_Rx_Device = FindDevice(NChan, gRadarConfig.SampleRate, &InputParameters, &OutputParameters, mypaAudioHostAPI);
			if (Desired_Rx_Device >= 0) {
				log_message("Found Rx device meeting goals: Device %d, nchannels %d", Desired_Rx_Device, NChan);
			}
			else {
				log_message("Error: No Device found. Unable to start ADC interface with desired parameters");
				return(-1);
			}
		}
		//Output
		if (Desired_Tx_Device < 0) {
			if (myPaHostIDX < 0) myPaHostIDX = Pa_HostApiTypeIdToHostApiIndex(mypaAudioHostAPI);
			Desired_Tx_Device = Pa_GetHostApiInfo(myPaHostIDX)->defaultOutputDevice;
		}

		log_message("Will try to open stream with in %d, out %d", Desired_Rx_Device, Desired_Tx_Device);
		//Input
		InputParameters.channelCount = NChan;
		gRadarState.NumADCChans = NChan;
		InputParameters.device = Desired_Rx_Device;
		InputParameters.suggestedLatency = NULL;
		InputParameters.channelCount = NChan;
		InputParameters.hostApiSpecificStreamInfo = NULL; //&AsioInputInfo;
		InputParameters.sampleFormat = paFloat32;
		
		

		OutputParameters.device = Desired_Tx_Device;
		OutputParameters.channelCount = NUMOUTCHAN;
	//	OutputParameters.suggestedLatency = Pa_GetDeviceInfo(OutputParameters.device)->defaultLowOutputLatency;
		OutputParameters.suggestedLatency = NULL;
		OutputParameters.channelCount = 2;
		OutputParameters.hostApiSpecificStreamInfo = NULL;
		OutputParameters.sampleFormat = paFloat32;

		log_message("Trying to open desired audio stream to provide ADC data");

		err = Pa_OpenStream(
			&stream,
			&InputParameters,
			&OutputParameters,
			gRadarConfig.SampleRate,
			gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock,        /* data samples in each channel per buffer */
			paClipOff, // No special processing
			paWaveCallback,
			&PaADCFrameCount);   /* Pass the counter to the callback by reference */
		log_message("Open Stream: error=%d, paNoError= %d, text= %s", err, paNoError, Pa_GetErrorText(err));
		if ((err != paNoError) && (InputParameters.channelCount > 2)) {
			// if we tried for 4 audio channels and it failed, then try to open an ASIO channel with just 2
			log_message("Portaudio OpenStream error: %s \n\t InputChannels = %d, OuputChannels = %d, Samplerate = %lf, Blocksize= %d",
				Pa_GetErrorText(err),
				InputParameters.channelCount,
				OutputParameters.channelCount,
				gRadarConfig.SampleRate,
				gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock);

			log_message("Retrying with different parameters.\n");

			// Try changing the number of channels- most likely cause of error
			if (!gRadarConfig.ReceiveRealOnly)
				InputParameters.channelCount = 2; // Force to just one radar,  Only ASIO under windows supports more than 2 channels
			else
			{
				InputParameters.channelCount = MIN(gRadarConfig.NumRadars, 2);
			}
			gRadarState.NumADCChans = InputParameters.channelCount;
			// And, change to use just the default input and output devices
			OutputParameters.device = Pa_GetDefaultOutputDevice();
			InputParameters.device = Pa_GetDefaultInputDevice();
			double SampleRate = gRadarConfig.SampleRate; // Pa_GetDeviceInfo(InputParameters.device)->defaultSampleRate;

			// Basically, this is opening the vanilla input and output
			err = Pa_OpenStream(
				&stream,
				&InputParameters,
				&OutputParameters,
				SampleRate,
				gRadarConfig.NSamplesPerWRI * gRadarConfig.NWRIPerBlock,        /* data samples (frames since multichannel) per buffer */
				paClipOff,      /* we won't output out of range samples so don't bother clipping them */
				paWaveCallback,
				&PaADCFrameCount);   /* No user data */
			if (err == paNoError) {
				if (!gRadarConfig.ReceiveRealOnly)
					gRadarState.NumSensorsSet = 1; // Force to just one radar,  Only ASIO under windows supports more than 2 channels
				else {
					gRadarState.NumSensorsSet = InputParameters.channelCount;
				}
				gRadarConfig.SampleRate = SampleRate;
			}

		}
		if (err != paNoError)
		{
			snprintf(msg, sizeof(msg), "Failed to open Portaudio stream even for device with one radar/stereo input: %s", Pa_GetErrorText(err));
			log_message((const char*)msg);
			Pa_Terminate();
			return(-1);
		}

		}


	log_message("Successfully opened ADC data channel");
	return(0);
	}

	void stopStreamADC()
	{
		PaError err = paNoError;

		log_message("Stop data stream");
		err = Pa_StopStream(stream);

		if (err != paNoError) {
			log_message("Warning: Unable to stop radar data input stream : %s ", Pa_GetErrorText(err));
		}
		log_message("Closing stream");
		err = Pa_CloseStream(stream);
		if (err != paNoError) {
			log_message("Warning: Unable to close radar data input stream : %s ", Pa_GetErrorText(err));
		}

		Pa_Terminate();
	}

	double	getADCTimeRef()
	{
		return(Pa_GetStreamTime(stream));  // This is what we have for timestamps on the data. It is a double in seconds.
	}

	int startStreamADC() {
		int err;
		// The following statement and the close later are why the stream variable is made a global
		if ((err = Pa_StartStream(stream))) {
			Pa_Terminate();
			log_message("An error occured while starting the portaudio stream : %s ", Pa_GetErrorText(err));
			return err;
		}
		else
		{
			return(0);
		}
	}