// This module interprets radar configuration commands
//  
// Written by Frank Robey
// Apr 2019: Initial implementation
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
#include "commandIF.h"
#ifndef _RTP_Headless
#ifdef _WINGUI
#include "winGUI.h"
#endif
#ifdef _FLTKGUI
#include "fltkgui.h"
#endif
#endif

// Forward declare local functions in this module
int SetSim(float SimampdB);
int SetDisplayScale(double ScaleMin, double Range);

std::string Commands[10] = {
"DisplayScale",
//"DisplayRef",
"DisplayColormap",
"DisplayMarkPeak",
"RecordRaw",
"RecordProc",
"SimulationOn",
"SimulationAmp",
"CharacterCMD",
"Exit" };


// The following only needs to be started as a thread if it is intended to monitor a port
/*
void CommandThread()
{
	//	int x;
	log_message("Command Interpreter Thread starting up");
	return;
}
*/

// The following is not designed as reentrant code, so multiple command interfaces can potentially confuse the command interpreter
// Should probably put a lock on it.
int CommandParse(cmdbuffer Command)
{
	switch (Command.cmd) {
	//case SetDispRef:
	case SetDispScale: {
		SetDisplayScale(Command.fparams[0], Command.fparams[1]);
		break;
	}
	
	case SetDispColormap: {
		SetColorMap(Command.cmap);
#ifndef _RTP_Headless
#ifdef _WINGUI
		SetColorBarImage();
#endif
#endif
		break;
	}
	case SetDispMarkPeak: {
		if (Command.onoff)
			gRadarState.PeakOverlay = TRUE;
		else
			gRadarState.PeakOverlay = FALSE;
		break;
	}
	case SetRecordRaw: {
		if (Command.onoff)
			start_raw_recording();
		else
			stop_raw_recording();
		break;
	}
	case SetRecordProc: {
		if (Command.onoff)
			start_proc_recording();
		else
			stop_proc_recording();
		break;
	}
	case SetSimOn: {
		if (Command.onoff)
			gRadarState.SimOn = TRUE;
		else
			gRadarState.SimOn = FALSE;
		break;
	}
	case SetSimAmp: {
		SetSim(Command.fparams[0]);
		break;
	}
	case SetCharCMD: {
		break;
	}
	case SetExit: {
		stop_radar();
#ifndef _RTP_Headless
#ifdef _WINGUI
		StopRadarDisp();
		PostQuitMessage(0); // A windows thing
#endif

#ifdef _FLTKGUI
		log_message("Stopping FLTK in command");
		
		//StopFLTKGui();
#endif
#endif
		break;
	}
	default:
		log_message("Warning: Unknown command to the command interface");
	}
	return(0);
}



int SetSim(float SimampdB)
{
	// Do some error checking
	//volatile float temp = SimampdB;
	if (isnan(SimampdB) || (! isfinite(SimampdB))) {
	//if ((SimampdB != temp)| (isfinite(SimampdB)) ) {
		log_message("Warning: Command interface provide with a Sim amplitude of NaN or inf");
		return(-1); 
	}
	if (SimampdB < -100.0f) SimampdB = -100.0f;
	else if (SimampdB > 0.0f) SimampdB = 0.0f;
	else if (SimampdB)

	gRadarState.SimAmp = SimampdB;
	return(0);
}

int SetDisplayScale(double ScaleMin, double Range)
{
	if (isnan(ScaleMin) || (!isfinite(ScaleMin)) ||
		isnan(Range) || (!isfinite(Range)) ) {
		log_message("Warning: Command interface provide with a SetDisplayScale of NaN or inf");
		return(-1);
	}

	gRadarState.RefLeveldB = ScaleMin;
	if ((Range > 5) && (Range < 100))
		gRadarState.DispRange = Range;
	else
	{
		log_message("Poorly formed display scaling command in SetDisplayScale.");
		return(-1);
	}
	return(0);
}

void SetColorMap(cmaptype CMapIn)
{
	colormap(CMAP_SIZE, gColormapRed, gColormapGreen, gColormapBlue, CMapIn);
	gRadarState.ColorMapSelected = CMapIn;
	return;
}


returnStat GetState()
{
	returnStat currStat;
	currStat.statRecRaw = gRadarState.RawRecording;
	currStat.statRecProc = gRadarState.DataRecording ;
	currStat.statSimAmp = (float) gRadarState.SimAmp;
	currStat.statMarkPeak = gRadarState.PeakOverlay;

	currStat.statBW = (float) gRadarConfig.Bandwidth;
	currStat.statCMAP = gRadarState.ColorMapSelected;
	currStat.statSimOn = gRadarState.SimOn;
	
	currStat.statDispRange= (float) gRadarState.DispRange;
	currStat.statDispRef= (float) gRadarState.RefLeveldB;
	currStat.statCentFreq= (float) gRadarConfig.CenterFreq;
	currStat.statSampRate= (float) gRadarConfig.SampleRate;
	currStat.statNWRI = gRadarConfig.NWRIPerCPI;
	currStat.statNSampPerWRI = gRadarConfig.NSamplesPerWRI;
	return(currStat);

}

