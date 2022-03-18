// radarConfig.cpp 
// Reads configuration file and sets up the radar configuration and state structures
// This routine is based on Ben Hoyt's inih library https://github.com/benhoyt/inih.
// The "inih" library itself is distributed under the New BSD license, and is (c) Ben Hoyt
// This module written by F. Robey
// First version Sept 2014
//  Sept 2017, added hooks to use for sonar and real data processing (not yet implemented in processing module)
//	Dec 2017,  Added configuration for number of threads to use
//  Feb 2018, Added log file recording. Moved ADC buffer allocation from this module to the buffers module.
//  Mar 2018, Added simulation variables, corrected initialization of cal variables
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
#include "INIReader.h"
#ifdef _WIN32
#include <io.h>
#define F_OK 04
#define access _access
#else
#include <unistd.h>
#endif

int ReadConfiguration(void)
{
	int retval=0;
	char fname[64];
	snprintf(fname,sizeof(fname),"%s", "radarconfig.ini");
	if (access(fname, F_OK) != 0) {
		snprintf(fname, sizeof(fname), "%s", "/data/radarconfig.ini");
		std::cout<<"Configuration file radarconfig.ini is not in current directory. Trying in default c:\\data directory. This will not be saved in the log file.\n";
	}
	
	INIReader reader(fname);

	if (reader.ParseError() < 0) {
		log_message( "Warning: Can't load 'radarConfig.ini'.  Will use default configuration values.");
		retval=1;
	}

	gRadarConfig.ProgramName=reader.Get("system", "programname", "RadarRTP");
	gRadarConfig.Version=reader.Get("system", "version", "2.1");
	gRadarConfig.ProcWindowFName = reader.Get("system", "procwindowfname", "RadarRTP V2.1.15.4");
	gRadarConfig.DataFileRoot=reader.Get("system", "DataFileRoot", "/data/");
	gRadarConfig.SPWinDir= reader.Get("system", "SPWinDir", "/data/SPWindows/");
	gRadarConfig.LogToFile = reader.GetBoolean("system", "LogToFile", true);
	gRadarConfig.RecordRawDataFromStart=reader.GetBoolean("system", "RecordRawDataFromStart", false);
	gRadarConfig.RecordProcDataFromStart=reader.GetBoolean("system", "RecordProcDataFromStart", false);
	gRadarConfig.MaxRawFileTime = reader.GetReal("system","MaxRawFileTimeSec", 600.0);
	gRadarConfig.MaxProcFileTime = reader.GetReal("system", "MaxProcTimeSec", 24.0*60.0*60.0);
	// Should I be checking to set the following here, or do it later?
	gRadarState.DataRecording=false; // Processed recording is off
	gRadarState.RawRecording=false; // Raw recording is off

	gRadarState.PeakOverlay = reader.GetBoolean("system", "ShowPeakOverlay", true);

	gRadarConfig.NumRadars=(int) reader.GetInteger("system", "NumRadars", 1);
	if(gRadarConfig.NumRadars > MaxRadars) {
		gRadarConfig.NumRadars=MaxRadars;
		log_message("Warning: Number of radars in configuration file is greater than allowed maximum of %d", MaxRadars );
	}
	gRadarState.NumSensorsSet=gRadarConfig.NumRadars;

	gRadarConfig.NumThreads = (int) reader.GetInteger("system", "NumThreads", 16);
	if (gRadarConfig.NumThreads > MaxThreads) {
		gRadarConfig.NumThreads = MaxThreads;
		log_message("Warning: Number of Signal Processing worker threads in configuration file is greater than allowed maximum of %d", MaxThreads);
	} 
	if (gRadarConfig.NumThreads < 2* gRadarConfig.NumRadars) {
		gRadarConfig.NumThreads = 2 * gRadarConfig.NumRadars;
		log_message("Warning: Number of Signal Processing worker threads in configuration file is less than minimum of 2*NumRadars, (to allow ping-pong processing) %d. Recommend increasing thread count", 2 * gRadarConfig.NumRadars);
	}

	// Interface setup
	gRadarConfig.ASIOPriority = reader.GetBoolean("system", "ASIOPriority", false);
	gRadarConfig.SampleRate= reader.GetReal("system", "SampleRate", 44100.0);
	gRadarConfig.RxADC_Chan = reader.GetInteger("system", "RxAdcChan", -1);
	gRadarConfig.TxADC_Chan = reader.GetInteger("system", "TxAdcChan", -1);

	
	// Processing
	gRadarConfig.NSamplesPerWRI= (int) reader.GetInteger("system", "NSamplesPerWRI", 64); 
	if (gRadarConfig.NSamplesPerWRI<0) gRadarConfig.NSamplesPerWRI = -gRadarConfig.NSamplesPerWRI;
	if (gRadarConfig.NSamplesPerWRI == 0) gRadarConfig.NSamplesPerWRI = 64;
	gRadarConfig.NWRIPerCPI= (int) reader.GetInteger("system", "NWRIPerCPI", 128);
	if (gRadarConfig.NWRIPerCPI == 0) gRadarConfig.NWRIPerCPI = 128;
	if (gRadarConfig.NWRIPerCPI < 0)gRadarConfig.NWRIPerCPI = -gRadarConfig.NWRIPerCPI;
	gRadarConfig.NWRIPerBlock= (int) reader.GetInteger("system", "NWRIPerBlock", 32);
	if (gRadarConfig.NWRIPerBlock > gRadarConfig.NWRIPerCPI) {
		gRadarConfig.NWRIPerBlock = gRadarConfig.NWRIPerCPI;
		log_message("Warning: over-riding the error in number of WRI per block to be = NWRIPerCPI");
	}

	gRadarConfig.FadeMemVal=reader.GetReal("system", "FadeMemVal", 0.95);
	gRadarConfig.CenterFreq = reader.GetReal("system", "Frequency", 24125000000.0); // Default is 24GHz
	gRadarConfig.Bandwidth = reader.GetReal("system", "Bandwidth", 100000000.0);
	gRadarState.TransmitModAmp = reader.GetReal("system", "TransmitModAmp", 1.0);
	gRadarState.TransmitModOn = reader.GetBoolean("system", "TransmitModOn", true);
	gRadarState.TransmitLongSweep = reader.GetBoolean("system", "TransmitLongSweep", false);
	gRadarConfig.TxRxSampleOffset = reader.GetReal("system", "TxRxSampleOffset", 0.0);

	// The following is not yet implemented
	gRadarConfig.ReceiveRealOnly = reader.GetBoolean("system", "ReceiveRealOnly", false);

	// Some placeholders to operate as a sonar too
	gRadarConfig.Sonar = reader.GetBoolean("system", "Sonar", false);
	gRadarConfig.SonarFreq = reader.GetReal("system", "SonarFrequency", 12000.0); // Default is 12kHz
	gRadarConfig.SonarBandwidth = reader.GetReal("system", "SonarBandwidth",5000.0); // Default bandwidth

	// Display control
	gRadarConfig.DTI_Height= (int) reader.GetInteger("Display", "DTI_Height", 300);
	gRadarConfig.ScaleData=reader.GetReal("Display", "ScaleData", 20.0);
	gRadarState.RefLeveldB=reader.GetReal("Display", "MinDispdB", -90.0); 
	gRadarState.DispRange=reader.GetReal("Display", "DispRange", 80.0);
	gRadarConfig.MinRefLevel = reader.GetReal("Display", "DispRefLevlMin", -100.0);

	// Simulated signal source

	gRadarConfig.SimADC = reader.GetBoolean("target", "SimADC", false);
	gRadarConfig.SimPend = reader.GetBoolean("target", "SimPend", false);
	gRadarConfig.PendLength= reader.GetReal("target", "PendLength", 20.0);
	gRadarConfig.ADCVariance = reader.GetReal("target", "ADCVariance", -40.0); // per sample noise variance
	gRadarConfig.PendMajorAxis = reader.GetReal("target", "PendMajorAxis", 1.1);
	gRadarConfig.PendMinorAxis = reader.GetReal("target", "PendMinorAxis", 0.0);
	gRadarConfig.RadarDistance = reader.GetReal("target", "RadarDistance", 1.8);
	gRadarConfig.PendLatitude = reader.GetReal("target", "RadarLatitude", 35.2);
	gRadarState.SimOn=reader.GetBoolean("target", "SimOn", false);
	gRadarState.SimAmp=reader.GetReal("target", "SimAmp", -20.0);
	//if (gRadarState.SimAmp > 0.0) gRadarState.SimAmp = 0.0; // 0 is the max value for data
	// Calibration
	gRadarConfig.DC_CalOnly=reader.GetBoolean("system", "DC_CalOnly", true);
	gRadarState.AutoCalOn = reader.GetBoolean("system", "AutoCalOn", true);
	std::string CalDCTemp = reader.Get("system", "Cal_DC_Offset", "0.0 0.0 0.0 0.0");
	std::string CalTransfTemp = reader.Get("system", "Cal_rr_ri_ir_ii", "1.0 0.0 0.0 1.0");
	std::string parseval;

	std::stringstream ss(CalTransfTemp), ss2(CalDCTemp);

	int index = 0;
//	log_message(CalDCTemp);

	while ( (ss>>parseval) && (index< 4*gRadarConfig.NumRadars) ) {
		if(isdigit(parseval[0])){
			gRadarConfig.CalTransForm[index/4].value[index%4]= (float) std::stod(parseval);
			index++;
		}
	}
	double caltemp[4] = { 1.0, 0.0, 0.0, 1.0 };
	if(index<4*gRadarConfig.NumRadars){
		log_message("Warning: Insufficient initial values for the Cal Transformation.  Setting remainder to default values.");
		for( ; (index < 4*gRadarConfig.NumRadars) ; index++ ){
			gRadarConfig.CalTransForm[index / 4].value[index % 4] = (float) caltemp[ index % 4];
		}
	}

	index = 0;
	while ( (ss2>>parseval) && (index< 2*gRadarConfig.NumRadars) ) {
		if(isdigit(parseval[0])){
			gRadarConfig.CalDCVal[index]= std::stod(parseval);
			index++;
		}
	}
	std::cout << "\n";
	if(index<2*gRadarConfig.NumRadars){
		log_message("Warning: Insufficient initial values for the Cal DC offset.  Setting offset values to zero.");
		for( ;  index<2*gRadarConfig.NumRadars; index++) {
			gRadarConfig.CalDCVal[index]= 0.0;
		}
	}

	// This is a calculated result based on the inputs.
	gRadarConfig.UAmbDoppler = VLIGHT*gRadarConfig.SampleRate / (4.0*gRadarConfig.NSamplesPerWRI*gRadarConfig.CenterFreq);
#ifndef __WithoutDataBase__
	gRadarConfig.DataBaseHost = reader.Get("SQL", "databasehost", " ");
	gRadarConfig.DataBaseUser = reader.Get("SQL", "databaseuser", " ");
	gRadarConfig.DataBasePasswd = reader.Get("SQL", "databasepasswd", "");
	gRadarConfig.DataBaseName = reader.Get("SQL", "databasename", "radardata");
	gRadarConfig.DataBaseRows = (int)reader.GetInteger("SQL", "databaserows", 2000);
#endif
	return( retval);
}

void DumpConfig(void)
{
	std::ostringstream msgstr;
	
	char msgtxt[2048];
	
	msgstr << "Default/programmed configuration values are:"
		<< "\n\tPRogname = " << gRadarConfig.ProgramName
		<< "\n\tVersion = " << gRadarConfig.Version
		<< "\n\tNumradars = " << gRadarConfig.NumRadars
		<< "\n\tNumThreads = " << gRadarConfig.NumThreads
		<< "\n\tDataFileRoot = " << gRadarConfig.DataFileRoot
		<< "\n\tSPWinDir = " << gRadarConfig.SPWinDir
		<< "\n\tRecordRawDataFromStart = " << gRadarConfig.RecordRawDataFromStart
		<< "\n\tRecordProcDataFromStart = " << gRadarConfig.RecordProcDataFromStart
		<< "\n\tLogToFile = " << gRadarConfig.LogToFile
		<< "\n\tMaxRawFileTime = " <<gRadarConfig.MaxRawFileTime
		<< "\n\tMaxProcFileTime = " << gRadarConfig.MaxProcFileTime 
		<< "\n\tRadar Frequency = " << gRadarConfig.CenterFreq
		<< "\n\tRadar Bandwidth = " << gRadarConfig.Bandwidth
		<< "\n\tReceiveRealOnly = " << gRadarConfig.ReceiveRealOnly
		<< "\n\tSampleRate = " << gRadarConfig.SampleRate
		<< "\n\tNSamplesPerWRI = " << gRadarConfig.NSamplesPerWRI
		<< "\n\tNWRIPerCPI = " << gRadarConfig.NWRIPerCPI
		<< "\n\tNWRIPerBlock = " << gRadarConfig.NWRIPerBlock
		<< "\n\tFadeMemVal = " << gRadarConfig.FadeMemVal
		<< "\n\tDC_CalOnly = " << gRadarConfig.DC_CalOnly
		<< "\n\tAutoCalOn = " << gRadarState.AutoCalOn
		<< "\n\tASIO Priority = " << gRadarConfig.ASIOPriority 
		<< "\n\tRx ADC Channel = " << gRadarConfig.RxADC_Chan
		<< "\n\tTx ADC Channel = " << gRadarConfig.TxADC_Chan
		<< "\n\tDTI_Height = " << gRadarConfig.DTI_Height
		<< "\n\tScaleData = " << gRadarConfig.ScaleData
		<< "\n\tRefLeveldB = " << gRadarState.RefLeveldB
		<< "\n\tMinRefLevel = " << gRadarConfig.MinRefLevel
		<< "\n\tTransmitModAmp = " << gRadarState.TransmitModAmp
		<< "\n\tTransmitModOn = " << gRadarState.TransmitModOn
		<< "\n\tTransmitLongSweep = " << gRadarState.TransmitLongSweep << " (record only)"
		<< "\n\tTxRxSampleOffset = " << gRadarConfig.TxRxSampleOffset
		<< "\n\tDispRange = " << gRadarState.DispRange
		<< "\n\tSimOn = " << gRadarState.SimOn
		<< "\n\tSimAmp = " << gRadarState.SimAmp
		<< "\n\tSimADC = " << gRadarConfig.SimADC 
		<< "\n\tADCVariance = " << gRadarConfig.ADCVariance
		<< "\n\tSimPend = " << gRadarConfig.SimPend
		<< "\n\tPendLength = " << gRadarConfig.PendLength
		<< "\n\tPendMajorAxis = " << gRadarConfig.PendMajorAxis
		<< "\n\tPendMinorAxis = " << gRadarConfig.PendMinorAxis 
		<< "\n\tRadarDistance = " << gRadarConfig.RadarDistance
		<< "\n\tPendLatitude = " << gRadarConfig.PendLatitude
		<< "\n\tSonar = " << gRadarConfig.Sonar << "(not fully implemented)"
		<< "\n\tSonarFrequency = " << gRadarConfig.SonarFreq
		<< "\n\tSonarBandwidth = " << gRadarConfig.SonarBandwidth;


		msgstr << "\n\tCalTransform = ";
	for(int ind=0; ind< gRadarConfig.NumRadars ; ind++){
		for(int counter=0; counter<4; counter++){
			msgstr << gRadarConfig.CalTransForm[ind].value[counter] << "  ";
		}
		msgstr<<"\n            ";
	}

	msgstr << "\n\tCalDCVals = ";
	for(int ind=0;  ind< 2*gRadarConfig.NumRadars ; ind++){
		msgstr << gRadarConfig.CalDCVal[ind] << "  ";
	}

#ifndef __WithoutDataBase__
	msgstr << "\n\tWith Database\n\tDatBaseHost = " << gRadarConfig.DataBaseHost
		<< "\n\tDataBaseUser = " << gRadarConfig.DataBaseUser
		<< "\n\tDataBasePasswd = *******" 
		<< "\n\tDataBaseName = " << gRadarConfig.DataBaseName
		<< "\n\tDataBaseRows = " << gRadarConfig.DataBaseRows;
#endif

	std::string msgbuf = msgstr.str();
	snprintf(msgtxt, sizeof(msgtxt),"%s",msgbuf.c_str());  // Roundabout way to log stringstream text.
	log_message((const char *)msgtxt);

	return;

}