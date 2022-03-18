// This set of functions creates simulated data for the radar. It includes simulation of the 
// ADC input (noise) data as well as sim of a simple sinewave and a pendulum.
//
// The radar simulation module was not written to be thread-safe. 
// It assumes that it will only be called from a single thread and will have unrestricted read/write access to the PendSimParms structure.
//
// Simulation of the ADC input noise is accomplished by an independent thread that calls the ADC data callback
// in place of the normal portAudio ADC callback.
//
// Initial version by Frank Robey
// Mar 2018	Moved target simulator from process.cpp, added ADC simulation
// May 2019 Changed arrays of pointers over to use std::vector construct

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
#include <complex>

// Forward declare routine local to this module
void fillADCBuffer(float *adcBuff, int nsamples, float ADCStdDev);
int simADCdata(void);
void startSimADC();
void stopSimADC();

// Another local variable used to count input ADC buffer callbacks
int ADCFrameCount = 0;
volatile bool ADC_SimStopFlag = 0;
std::thread ADCSimThread;
std::default_random_engine generator;
//FILE * fpFileSimTemp;
int _tmpSimCount = 0;


int PendSimParms::RadarSimInit()
{
	NumSampPerWRI = gRadarConfig.NSamplesPerWRI;
	NumWRIperBlock = gRadarConfig.NWRIPerBlock;
	NumRadars = gRadarState.NumSensorsSet;
	MajorAxis = gRadarConfig.PendMajorAxis;
	MinorAxis = gRadarConfig.PendMinorAxis;
	PendInitPhase = ((double)TWOPI) / 8.0;
	PendRadFreq = sqrt(GEARTH / gRadarConfig.PendLength);
	MajorAxisRadFreq = TWOPI * sin(gRadarConfig.PendLatitude * (double)TWOPI / 360.0) / (24.0*3600.0);
	samplePeriod = 1.0 / gRadarConfig.SampleRate;
	RadarFreq = gRadarConfig.CenterFreq;
	RadarBandwidth = gRadarConfig.Bandwidth;
	double RadarAngle = (double)TWOPI /2.0 / (double)NumRadars;
	//fopen_s(&fpFileSimTemp, "C:/data/tempsimdat.txt", "w");
	if (NumRadars > MaxRadars) {
		log_message("Error: Edit the program to increase the number of radars.  Invalid number in configuration");
		return -1;
	}
	
	std::ostringstream msgstr;
	msgstr << "Sim radar base angle & locations:  "<<RadarAngle<<" rad\n ";
	
	// The following fills in the member variables needed for the sim
	for (int rindex = 0; rindex < NumRadars; rindex++) {
		RadarXv.push_back(gRadarConfig.RadarDistance* cos(RadarAngle*(double)rindex));
		RadarYv.push_back( gRadarConfig.RadarDistance* sin(RadarAngle*(double)rindex));
		msgstr << rindex << " (" << RadarXv[rindex] << " " << RadarYv[rindex] << ") ";
	}
	
	std::string msgbuf = msgstr.str();
	log_message((const char*)msgbuf.c_str());

	phaseinc = 140.0*TWOPI / gRadarConfig.SampleRate;
	pendfreqinc = 0.125*TWOPI / gRadarConfig.SampleRate;
	
	for (int rindex = 0; rindex < NumRadars; rindex++) {
		Phase.push_back(TWOPI / 3.0); 
		PendFreq.push_back(TWOPI / 4.0);
	}

	siminit = true;
	log_message("Initialized radar data simulation");
	return 0;
}
void fillADCBuffer(float *adcBuff, int nsamples, float ADCStdDev)
{
	std::normal_distribution<float> ADCDistribution(0.0, ADCStdDev);
	for (int i = 0; i < nsamples; i++)
	{
		adcBuff[i] = ADCDistribution(generator);
	}
	return;
}

// The following is a thread that creates a simulation of the streaming portaudio interface
// Only noise and timing are simulated
// It calls the normal portaudio callback
int simADCdata(void)
{
	// Allocate and create buffers
	int nADCsamps = gRadarConfig.NSamplesPerWRI*gRadarConfig.NWRIPerBlock*gRadarConfig.NumRadars;
	if (!gRadarConfig.ReceiveRealOnly) nADCsamps *= 2;
	int nDACsamps = gRadarConfig.NSamplesPerWRI*gRadarConfig.NWRIPerBlock * 2;

	std::vector<float> adcBuffer(nADCsamps * 4);
	std::vector<float> dacBuffer(nDACsamps);
	unsigned long framesPerBuffer = gRadarConfig.NSamplesPerWRI*gRadarConfig.NWRIPerBlock;
	PaStreamCallbackTimeInfo ADCtimeInfo;
	PaStreamCallbackFlags statusFlags = 0;
	int framecount = 0;
	auto x = std::chrono::steady_clock::now();
	float stdDev;
	if (gRadarConfig.ADCVariance != 0.0)
	{
		stdDev = (float)pow(10.0, gRadarConfig.ADCVariance / 20.0);
	}
	else {
		stdDev = 1e-4f;
	}
	double timeincr = (double(gRadarConfig.NSamplesPerWRI*gRadarConfig.NWRIPerBlock)) / gRadarConfig.SampleRate;
	//std::cout << "Time increment is: " << timeincr << " " << gRadarConfig.SampleRate << std::endl;
	ADCtimeInfo.currentTime = 1e-3;
	ADCtimeInfo.inputBufferAdcTime = 0.0;
	ADCtimeInfo.outputBufferDacTime = 2 * timeincr;  // PaTime is in seconds
	long int timeIncruSec;
	timeIncruSec = (long int)(timeincr * 1e6);
	// Wait a bit before starting data flowing.  Should probably wait on a flag instead.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	while (!ADC_SimStopFlag) {
		fillADCBuffer(&adcBuffer[0], nADCsamps, stdDev);
		ADCtimeInfo.currentTime += timeincr;
		ADCtimeInfo.inputBufferAdcTime += timeincr;
		ADCtimeInfo.outputBufferDacTime += timeincr;
		x += std::chrono::microseconds(timeIncruSec);
		std::this_thread::sleep_until(x);
		paWaveCallback(&adcBuffer[0], &dacBuffer[0], framesPerBuffer, &ADCtimeInfo, statusFlags, &framecount);
	}
	return 1;
}

void stopSimADC()
{
	ADC_SimStopFlag = true;
	ADCSimThread.join();
	return;
}

void startSimADC()
{
	ADC_SimStopFlag = false;
	gRadarState.NumSensorsSet = gRadarConfig.NumRadars;  // Variable used in case actual ADC doesn't have the channels needed.
	ADCSimThread = std::thread(simADCdata);

}

void PendSimParms::pendPosition(double postime, double pos[3])
{

	double cosF, sinF;
	double xi, yi;
	if (!siminit) RadarSimInit();
	xi = MajorAxis * cos(postime*PendRadFreq + PendInitPhase+TWOPI/4.0);
	yi = MinorAxis * sin(postime*PendRadFreq + PendInitPhase + TWOPI / 4.0);

	cosF = cos(postime *MajorAxisRadFreq + initMajorAxisPhase );
	sinF = sin(postime *MajorAxisRadFreq + initMajorAxisPhase );

	pos[0] = cosF * xi - sinF * yi;
	pos[1] = sinF * xi + cosF * yi;
	pos[2] = 0.0;
	return;

}

void PendSimParms::pendDistances(double postime, std::vector<double> *distances)
{
	double pendpos[3];
	if (!siminit) RadarSimInit();
	pendPosition(postime, pendpos);

	for (int rindex = 0; rindex < NumRadars; rindex++)
		distances->at(rindex) = sqrt(
		(RadarXv[rindex] - pendpos[0]) * (RadarXv[rindex] - pendpos[0]) +
			(RadarYv[rindex] - pendpos[1])*  (RadarYv[rindex] - pendpos[1]))
		- BobRadius;

	return;

}

void PendSimParms::simTargetData(PaTime simTimeStart, sensordata< RTPComplex> *SimData, double SimAmpdB)
{
	int numpts = NumSampPerWRI * NumWRIperBlock;
	double simTimeEnd = simTimeStart + ((double)numpts) * samplePeriod;
	std::vector<double> distanceStart(SimData->sensors);
	std::vector<double> distanceEnd(SimData->sensors);
	double SimAmpLin;
	double omega0 = ((double)TWOPI)*RadarFreq;
	//std::cout << " " << simTimeStart;
	if (!siminit) RadarSimInit();

	if (fabs(gRadarState.SimAmp <= 0.0f))
		SimAmpLin = pow(10.0, (double)SimAmpdB / 20.0);
	else
		SimAmpLin = 1.0;
	
	if (gRadarConfig.SimPend) {
		pendDistances(simTimeStart, &distanceStart);
		for (int wri = 0; wri < NumWRIperBlock; wri++) {
			pendDistances(simTimeStart + (1L + wri)*NumSampPerWRI*samplePeriod, &distanceEnd);
		//	debugPrint("Sim time: %lf, %d, d: %lf", simTimeStart, wri, distanceEnd[0]);
			if ((!gRadarState.TransmitModOn) || (gRadarState.TransmitModAmp == 0.0)) { // CW operation
				for (int rindex = 0; rindex < NumRadars; rindex++) {
					double Delta = 2.0*(distanceEnd[rindex] - distanceStart[rindex]) / (VLIGHT * (double)NumSampPerWRI);
					double tau0 = 2.0*distanceStart[rindex] / VLIGHT;
					std::complex<double> PhaseRot;
					std::complex<double> tempData;
					PhaseRot = exp(std::complex<double>(0.0, Delta*omega0));
					int offset = wri * NumSampPerWRI;

					tempData = SimAmpLin * exp(std::complex<double>(0.0, tau0*omega0));
				
						for (int i = 0; i < NumSampPerWRI; i++) {
							(*SimData)(wri,i,rindex) = (RTPComplex)tempData;
							tempData = tempData * PhaseRot;
						}
	
					distanceStart[rindex] = distanceEnd[rindex];
				}
			}
			else {
				double Delta, tau0, T, delta, argument;
				double omegaB = RadarBandwidth * ((double)TWOPI);
				std::complex<double> PhaseRot, PhaseRot2, qRot;
				std::complex<double> tempData;
				T = ((double)NumSampPerWRI) * samplePeriod;
				delta = samplePeriod;
				for (int rindex = 0; rindex < NumRadars; rindex++) {
					Delta = 2.0*(distanceEnd[rindex] - distanceStart[rindex]) / (VLIGHT * (double)NumSampPerWRI);
					tau0 = 2.0*distanceStart[rindex] / VLIGHT;

					argument = ((omega0 - omegaB / 2.0) + omegaB * tau0 / (2.0*T))*tau0;
					tempData = SimAmpLin * exp(std::complex<double>(0.0, argument));
					argument = Delta * (omega0 - omegaB / 2.0)
						- 2.0*omegaB * tau0*(delta + Delta) / T +
						omegaB * Delta*(2 * delta + Delta) / (2.0* T);
					PhaseRot = exp(std::complex<double>(0.0, argument));
					argument = omegaB * Delta*(2 * delta - Delta) / (T);
					qRot = exp(std::complex<double>(0.0, argument));
					PhaseRot2 = qRot;
					int offset = wri * NumSampPerWRI;
					for (int i = 0; i < NumSampPerWRI; i++) {
						// Temporary test that cal works
						// (*SimData)(wri,i,rindex) = (RTPComplex)tempData+RTPComplex((float) (0.001*tempData.imag()), (float) (0.1*tempData.imag()));
						(*SimData)(wri, i, rindex) = (RTPComplex)tempData;
						tempData = tempData * PhaseRot*PhaseRot2;
						PhaseRot2 *= qRot;
					}
					distanceStart[rindex] = distanceEnd[rindex];
				}
			}
		}
	}
	else {   // Simple simulation injecting a sine wave varying as a sine wave
		float SimAmpLinf = (float)SimAmpLin;
		for (int rindex = 0; rindex < NumRadars; rindex++) {
			for( int wri=0;wri< NumWRIperBlock;++wri)
				for( int timesamp=0;timesamp< NumSampPerWRI ; timesamp++){
			
				(*SimData)(wri,timesamp,rindex) = 
					SimAmpLinf * exp(RTPComplex(0.0f, (float)Phase[rindex]));
				Phase[rindex] += (phaseinc*sin(PendFreq[rindex]));
				PendFreq[rindex] += pendfreqinc;
			}
		}
	}

	return;
}
