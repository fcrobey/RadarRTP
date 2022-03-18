/*
This set of functions proides the control for the realtime radar. It includes the radar
startup as well as reading the radar data using portaudio.

Initial version authored by Frank Robey
December 2013	Initial implementation
Aug/Sept 2014	Realtime processing for two radars (4 soundcard channels) added rather than just one
Nov		 2014   Add calibration data structure. Offline calibration enabled.
Oct		2017	Edited to improve error checking. PortAudio startup moved to separate function
Mar		2018	Added ADC simulation to replace portaudio. Added modules to radarSim.cpp to support.
Sep		2020	Moved ADC IO into separate file called sensorIO. Expect it to be replaced in the future
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
//#include <random>
//#include <algorithm>

// Forward declare routine local to this module


// The following routine starts up the radar.  
// This includes the following: waveform generation, circular buffers, ADC data path, 
// calls to start the processing threads, and starts the data flowing.
bool radar_running = FALSE;
int start_radar()
{
	
	std::chrono::system_clock::time_point systime_sec;
	std::chrono::steady_clock::time_point testtime, testtime_sec;
	//	std::chrono::steady_clock::duration frac_ms;
	DataTOV TOVtp;
	//char msg[1024];

	log_message("Radar control startup");

	log_message("Creating transmit waveform.");
	create_waveform();

	buff_init();	/* Initialize the circular block buffer */

	log_message("Initiating/starting Signal Processing threads.");
	/* The following call spawns all processing threads (using multi-threaded processing) and waits for them to startup    */
	if (startProcessingThread()) {
		log_error_message("Could not initialize processing thread, exiting...", GetLastError());
		return 1;
	}

	log_message("Initialize ADC raw data path");
	int err = init_ADC_data();
	if (err!=0) {
		log_message("Error: Unable to open ADC audio path on this computer. Recheck interfaces and try again");
		return(-1);
	}

	log_message("ADC data path has been initialized");

	// Determine relationship between portaudio stream time and clock time.  Needed to accurately timestamp the data.
	if (gRadarConfig.SimADC) {
		gStreamPATimeRef = 0.0;
	} else {

		gStreamPATimeRef = getADCTimeRef();
		
	}

	gStreamTPTimeRef = std::chrono::steady_clock::now(); // Program time reference in standard clock units
	gStreamSysTimeRef = std::chrono::system_clock::now(); 
	log_message( "Portaudio Stream time reference %lf", gStreamPATimeRef);
	
	if (gRadarConfig.RecordProcDataFromStart) open_proc_data_file();
	if (gRadarConfig.RecordRawDataFromStart) open_raw_data_file();


	// Delay a bit (only to make interpreting the log file easier) so that the signal processing
	// startup messages are in better order.
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	log_message("Start PortAudio stream data flowing");

	if (!gRadarConfig.SimADC) {
		err = startStreamADC();
		// The following statement and the close later are why the stream variable is made a global
		if (err ) {
			log_message( "Error: An error occured while starting the ADC stream");
			return err;
		}
	}
	radar_running = TRUE;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	log_message("Successfuly returning from start_radar()");

	return(0);

}

// This routine stops the ADC data and processing threads.
int stop_radar()
{
	if (radar_running) {
		log_message("Stop Radar called: Stopping audio data streams.");
		if (gRadarConfig.SimADC) {
			stopSimADC();
		}
		else {
			stopStreamADC();


		}
		/* Tell signal processing thread to stop (and do the join() ) */
		log_message("Stop_radar: Stopping processing threads.");
		stopProcessingThread();


		log_message("Stop_radar: Closing circular buffers");
		/* nicely close ring buffers */
		buff_destroy();


		log_message("Simple radar program finished.");
		radar_running = FALSE;
	}
	else {
		log_message("Warning: Request to stop radar, but radar is not running.");
	}

	return 0;

}

