// This module is a collection of I/O routines that are used in the radar application.
//
// Written by Frank Robey
// 
/*
Dec 2013 Initial implementation
Oct 2017 Changed console lock mechanism to avoid contention for shared resource
Feb 2018 Added code so that it works on Linux
Mar 2018 Corrected recording of processed data (along with fixes in the processing chain- see process.cpp)
Mar 2018 Moved load_windows here
Mar 2018 Put in flag to avoid thrashing when opening the recording files fail.

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
#ifdef _WIN32
#include <tchar.h>
#include <strsafe.h>
#endif
#include "logMessages.h"
#include <math.h>
#include <thread>
#include <mutex>

std::mutex raw_file_lock;
std::mutex proc_file_lock;
bool __fopenRawFail = 0;	// Don't keep thrashing the file system if unable to open files for recording
bool __fopenProcFail = 0;

// These are to write debug info to a file
FILE * fpDebugFile;


int open_proc_data_file() 
// This routine opens a data file to save processed results.
{
	char fname[256],msg[256];
	if (filedat != NULL)	{	
		close_proc_file();
	} 

	std::time_t currtime = std::time(nullptr);
	std::tm timestruc;
#ifdef _WIN32
//#ifdef __STDC_LIB_EXT1__
	gmtime_s( &timestruc, &currtime);
#else
	gmtime_r(&currtime, &timestruc);
#endif
	std::strftime(msg, sizeof(msg), "proc%Y_%m_%d_%H_%M_%S", &timestruc);

	snprintf(fname, sizeof(fname), "%s%s.txt", gRadarConfig.DataFileRoot.c_str(), msg);
	snprintf(msg, sizeof(msg), "File for saving processed data being opened as: fname=%s", fname);
	log_message((const char *)msg);


#ifdef _WIN32
	fopen_s(&filedat, fname, "w");
#else
	filedat=fopen(fname,"wb");
#endif

	if (filedat == NULL)	{	
		log_message("Warning: Unable to create output data file named '%s', %d", fname, GetLastError());
		__fopenProcFail = TRUE;
		return (1) ;
	}
	else
	{
		__fopenProcFail = FALSE;
	}

	std::strftime(msg, sizeof(msg), "%Y,%m,%d,%H:%M:%S", &timestruc);
	fprintf(filedat, "%s,%d,%d,%d,%d,%d\n",msg,gRadarState.NumSensorsSet, gRadarConfig.NSamplesPerWRI, 
		gRadarConfig.NWRIPerCPI, gRadarConfig.NWRIPerBlock, (const int) gRadarConfig.SampleRate);

	time(&tfiledat);
	gRadarState.DataRecording=TRUE;
	
	return(0);
}


int open_raw_data_file()
// There should be a lock on the file so that either this module or the data writer owns the file.
{
	SF_INFO info;

	char fname[256], msg[256];

	if (fileraw != NULL) {
		log_message("Close recording file");
		close_raw_file();
	}
	std::time_t currtime = std::time(nullptr);
	std::tm timestruc;
#ifdef _WIN32
	//#ifdef __STDC_LIB_EXT1__
	gmtime_s(&timestruc, &currtime);
#else
	gmtime_r(&currtime, &timestruc);
#endif
	std::strftime(msg, sizeof(msg), "raw%Y_%m_%d_%H_%M_%S", &timestruc);

	snprintf(fname, sizeof(fname), "%s%s.wav", gRadarConfig.DataFileRoot.c_str(), msg);
	snprintf(msg, sizeof(msg), "File for saving raw data being opened as: fname=%s", fname);
	log_message((const char *)msg);

	memset(&info, 0, sizeof(info));

	info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	info.samplerate = (int)gRadarConfig.SampleRate;
	info.channels = 2 * gRadarState.NumSensorsSet;  // Two channels for each radar.
	raw_file_lock.lock();
	fileraw = sf_open(fname, SFM_WRITE, &info);

	if (fileraw == NULL)
	{
		log_message("Warning: Unable to create file named '%s' : %s.  Clean up and try again. %d", fname, sf_strerror(NULL), GetLastError());
		raw_file_lock.unlock();
		__fopenRawFail = TRUE;
		return (1);
	}
	else {
		__fopenRawFail = FALSE;
	}

	sf_set_string(fileraw, SF_STR_TITLE, "Radar Data");
	sf_set_string(fileraw, SF_STR_COMMENT, "Release 2.1");
	sf_set_string(fileraw, SF_STR_SOFTWARE, "RadarRTP");
	sf_set_string(fileraw, SF_STR_COPYRIGHT, "Data is not copyrighted.");

	time(&tfileraw);
	raw_file_lock.unlock();

	gRadarState.RawRecording = TRUE;

	return(0);
}


// The following is used to open a data file for use in debugging. Partial or full results can be stored in this file for later examination
int openDebugDataFile()
// This routine opens a data file to save processed results.
{
	char *fname =(char*) "/data/tmpDebug.txt";
	//char msg[256];
	if (fpDebugFile != NULL) {
		closeDebugDataFile();
	}

#ifdef _WIN32
	fopen_s(&fpDebugFile, fname, "w");
#else
	fpDebugFile = fopen(fname, "wb");
#endif

	if (fpDebugFile == NULL) {
		log_message("Warning: Unable to create debug data file named '%s', %d", fname, GetLastError());
		return (1);
	}
	
	return(0);
}


int close_proc_file(void)
{
	if(filedat!=NULL){
		proc_file_lock.lock();
		fclose (filedat) ;
		filedat = NULL;
		proc_file_lock.unlock();
	}

	return(0);
}


void closeDebugDataFile()
{
	if (fpDebugFile != NULL) {
		fclose(fpDebugFile);
		fpDebugFile = NULL;
	}

	return;
}


int close_raw_file(void)
{
	if(fileraw!=NULL){
		raw_file_lock.lock();
		sf_close (fileraw) ;
		fileraw = NULL;
		raw_file_lock.unlock();
	}

	return(0);
}

void close_all_open_files(void)
{
		if (gRadarState.RawRecording) {
			gRadarState.RawRecording=FALSE;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			//Sleep(5); // Should be long enough for any processes that are ready to write have finished
			close_raw_file() ;
		}
		if (gRadarState.DataRecording) {

			gRadarState.DataRecording=FALSE;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			//Sleep(5); // Should be long enough for any processes that are ready to write to have finished
			close_proc_file() ;
		}
}


int save_raw_data( const float * pData, int num_samps)
// This routine saves the raw data to the sound file previously opened.  
// If the file has been open for too long (currently 600 seconds) the current file is 
// closed and a new file opened for writing.
// 
{ 
	char strbuff[80],msg[256];
	time_t now;
	double seconds_open;
	if ((!gRadarState.RawRecording) || ( (fileraw == NULL) &&  __fopenRawFail )) {
		return(-1);
	}
	if ((gRadarState.RawRecording) && (fileraw == NULL) && !__fopenRawFail) {
		if(open_raw_data_file()) return 1;
	}
	raw_file_lock.lock();
		if(sf_write_float (fileraw, pData, num_samps) != num_samps )
	{
#ifdef _WIN32
			strerror_s(strbuff, sizeof(strbuff),errno);
			snprintf(msg, sizeof(msg), "Write of raw data failed : %s", strbuff);
#else
			if(strerror_r(errno, strbuff, sizeof(strbuff)))
				snprintf(msg, sizeof(msg), "Write of raw data failed : %s", strbuff);
			else
				snprintf(msg, sizeof(msg), "Write of raw data failed ");
#endif

		log_message((const char *)msg);
		raw_file_lock.unlock();
		return(1);
	}

	raw_file_lock.unlock();
	time(&now);
	seconds_open = difftime(now,tfileraw);
	if (seconds_open>(gRadarConfig.MaxRawFileTime)){
		close_raw_file();
		open_raw_data_file();
	}
	return(0);

}


int save_processed_data(void)
// This routine saves the processed data to the output file previously opened.  
// If the file has been open for too long (currently 24 hours seconds) the current file is 
// closed and a new file opened for writing.
{

	if ((!gRadarState.DataRecording) || ((filedat == NULL) && __fopenProcFail) ) {
		return(-1);
	}
	if ((gRadarState.DataRecording) && (filedat == NULL) && !__fopenProcFail) {
		if(open_proc_data_file()) return 1;  // Open file. If fails, then return.
	}
	DataTOV time_pt;
	// Reference the blocks time (using steady clock) to GMT time
	time_pt = gProcessedData.Params.Data_TOVtt + gStreamSysTimeRef;
	// Convert data block time to human-readable time
	char msg[1024];
	clock_to_char_long(msg, sizeof(msg), time_pt);
	
	// Lock the processed data file - only one thread currently writes, so this is more for future expansion
	proc_file_lock.lock();
	fprintf(filedat, "%d,%s", gProcessedData.Params.block_id,msg);

	for(int radar=0;radar<gRadarConfig.NumRadars;radar++){ 
		fprintf(filedat, ",%8.5lf,%8.4lf \n",gProcessedData.peakDoppler[radar],gProcessedData.peakAmplitude[radar]); 
	}
	fflush(filedat);
	proc_file_lock.unlock();

	// Check for length of time file has been open
	time_t now;
	double seconds_open;

	time(&now);
	seconds_open = difftime(now, tfiledat);
	if (seconds_open>(gRadarConfig.MaxProcFileTime)) {
		close_proc_file();
		open_proc_data_file();
	}
	return(0);

}


bool toggle_raw_recording()
{
	if (gRadarState.RawRecording) {
		stop_raw_recording();
		return FALSE;
	}
	else {
		start_raw_recording();
		return TRUE;
	}
}

bool toggle_proc_recording()
{
	if (gRadarState.DataRecording) {
		stop_proc_recording();
		return FALSE;
	}
	else {
		start_proc_recording();
		return TRUE;
	}
}

void start_proc_recording()
{
	if (gRadarState.DataRecording) {
		return;
	}
	else {
		open_proc_data_file();
		gRadarState.DataRecording = TRUE;
		log_message("Turning on process data recording.");
	}
}

void stop_proc_recording()
{
	if (!gRadarState.DataRecording) {
		return;
	}
	else {
		gRadarState.DataRecording = FALSE;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		log_message("Turning off process data recording.");
		close_proc_file();
	}
}

void start_raw_recording()
{
	if (gRadarState.RawRecording) {
		return;
	}
	else {
		log_message("Turning on raw recording.");
		open_raw_data_file();
		gRadarState.RawRecording = TRUE;
	}
}

void stop_raw_recording()
{
	if (!gRadarState.RawRecording) {
		return;
	}
	else {
		log_message("Turning off raw recording");
		gRadarState.RawRecording = FALSE;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		close_raw_file(); 
	}
}

int load_window(float *vector, int nsamp, int sll)
{
	FILE* fptr;
	char fname[64];
	int i;
	int iCount;
	snprintf(fname, sizeof(fname), "%swin%ds%ddB.txt", gRadarConfig.SPWinDir.c_str(), nsamp, sll);
#ifdef _WIN32
	fopen_s(&fptr, fname, "r");
	if (fptr == NULL) {
#else
	if ((fptr = fopen(fname, "r")) == NULL) {
#endif
		log_message("Warning: Problem opening window file during load_window: %s \nDefaulting to hamming window", fname);
		hamming(vector, nsamp);
		return(1);
	}

	for (i = 0, iCount = 0; i < nsamp; i++)

#ifdef _WIN32
		iCount += fscanf_s(fptr, "%f", vector + i);
#else
		iCount += fscanf(fptr, "%f", vector + i);
#endif

	if (iCount != nsamp) {
		
		log_message(
			"Warning: Problem reading values from window file during load_window: %s, %d of %d\nDefaulting to hamming window."
			, fname, i, nsamp);

		hamming(vector, nsamp);
		return(1);
	}
	return(0);
	}

// number of arguments.  This was done so I can stop printing to a string and then passing the string.
void debugPrint(const char* log_text, ...)
{

	va_list myargs;
	char fmt[256];
	//snprintf(fmt, sizeof(fmt), "%s\n", log_text);
	std::chrono::system_clock::time_point systime = std::chrono::system_clock::now();
	char timestr[256];

	logMsg_clock_to_char(timestr, sizeof(timestr), systime);

	snprintf(fmt, sizeof(fmt), "%s: %s\n", timestr, log_text);


	// Now print the message to a character string
	//char		msg[256];
	va_start(myargs, log_text);
	vfprintf(fpDebugFile, fmt, myargs);
	va_end(myargs);


}