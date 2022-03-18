/* 
Realtime Radar header file

Initial version by Frank Robey
December 2013
Aug/Sept 2014	Realtime processing for two radars (4 soundcard channels) added
Nov		 2014   Add calibration data structure.
Mar		 2017   Moved configuration to ini (inih) file and added modules to setup
Sept	 2017	Move interface between HMI and radar to shared memory, delete some windows specific calls
Feb		 2018   Moved to c++11 threads and mutex
Mar		 2018	Moved timing to c++11 std:chrono.  Cleaned up Linux timestamps. Added memory to cal structure. 
Apr		 2018   Added database parameters to configuration

RadarRTP - Radar Real time Program (RTP)

© 2022 Massachusetts Institute of Technology.

Distributed under GNU GPLv2.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License, Version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details, supplied with this source as License.txt

*/
#pragma once
//#define __WithoutDataBase__


#include "stdafx.h"
#include "colormap.h"

#include <string>
#include <errno.h>
#include <math.h>

#include <ctime>
#include <vector>
#include <sstream>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <ratio>
#include <atomic>
#include <complex>
#include "CPIParameters.h" // CPIParams definition
#include "sensordata.h"  // Class definition for radar data

#ifdef _WIN32
//#include <complex.h>  // This needs to come before fftw3.h to ensure compatible complex types.
#endif

extern "C" {
#include <portaudio.h>
#include <sndfile.h>
#include <fftw3.h> // does the extern "C" in the header file
#ifdef _WIN32
#if PA_USE_ASIO
#include "pa_asio.h"
#endif
#endif
}
//#include <sndfile.h>

#include <iostream>

#ifdef _WIN32 
#include <windows.h>
#include <process.h>

#else   /* assume POSIX */
#define __int64  int64_t
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#include <errno.h>
#define GetLastError() errno
#endif

using DataTOV = std::chrono::system_clock::time_point ;

using RTPComplex = std::complex<float>;

#ifndef		TWOPI
#define		TWOPI		2.0 * 3.14159265358979323846264338
#endif

#ifndef VLIGHT
#define VLIGHT 299792458.0
#endif // !VLIGHT

#ifndef GEARTH
#define GEARTH 9.86960
#endif

// The maximum number of radar channels that the program can process has not been tested beyond 3 simultaneous radars

#define MaxRadars 4
/* some nice to have definitions */
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define CMULr(ar,ai,br,bi) (ar*br-ai*bi)
#define CMULi(ar,ai,br,bi) (ar*bi+ai*br)
#define CDIV(ar,ai,br,bi,cr,ci,dtemp) (dtemp=br*br+bi*bi; cr=((ar*br+ai*bi)/dtemp);ci=((ai*br-ar*bi)/dtemp);

/* Input parameters */

#define NUMOUTCHAN (2) /* Number of output channels driving the radar - only 2 channels are supported, no less, no more and no error checking */
#define NBYTES_PER_PIXEL (4) /* Number of bytes in a pixel for the image array */
#define MaxThreads 64

/* Global declarations */

#define NBUFF 8  /* number of buffers in circular buffer between soundcard ADC and processing thread */

/* Recording parameters */
extern FILE * filedat ;	// handle for processed data file
extern time_t tfiledat;	// Time data file opened.
extern SNDFILE * fileraw ; /* I'm using soundfiles to record raw data since I had that library */
extern time_t tfileraw;

/* Global variables */
extern float * WaveData;  // Waveform data.  Memory allocated elsewhere, this is the pointer to the data
extern RTPComplex *  WaveformReplica ;
extern RTPComplex *  DownConvertWF;

// The following are for the ring buffer between the ADC data input thread and the radar processing dispatch thread
extern float *Buff_Data[NBUFF];						// storage for ringbuffer data
extern PaStreamCallbackTimeInfo Buff_Time[NBUFF];   // structure with callback time, ADC time, 
extern DataTics Buff_Time_ticks[NBUFF];			// time of validity of data in 1 usec time ticks
extern int Buff_count[NBUFF];						//ADC frame count


extern PaTime gStreamPATimeRef; // Reference time for stream
extern std::chrono::system_clock::time_point gStreamSysTimeRef; 
extern std::chrono::steady_clock::time_point gStreamTPTimeRef; // Reference time epoch for gStreamPATimeRef in c++ standard time_point


// Time is gStreamSysTimeRef + <seconds> ( <measurement time>- gStreamTimeRef)
// Convert to clock time by calling FileTimeToSystemTime()
// Time of validity for data block is (PaStreamCallbackTimeInfo.InputBufferAdcTime - gStreamPATimeRef) converted to 
// same time reference duration  + gStreamSysTimeRef

// The following are in radarConfig.cpp
int ReadConfiguration(void); // Routine to read in the configuration file
void DumpConfig(void);		// Routine to print the configuration to std::stdout

// The following are in buffer.cpp
void buff_init(void);   /* Initialize the ring buffer semaphores and indices.  (TODO: move memory allocation to this routine) */
void buff_free(void);   /* Free the oldest buffer */
void buff_mark_used(int index);  /* Mark that the newest buffer has data.  Check for next available and buffer overrun*/
int buff_get_next_free(void);  /* get index of next free buffer to put data into */
void buff_destroy(void);  /* close buffers.  */
/* The following will block until a full data buffer block is available */
/* It will return the index to the data block that is next to be read/processed */
int buff_Wait_For_Data(void);  /* Return index to oldest data buffer.  Block if no data is available. */

int create_waveform(void);

// The following are in radar_io.cpp module
int open_proc_data_file(void);
int close_proc_file(void);
int open_raw_data_file(void ); 
int close_raw_file(void);
void close_all_open_files(void);
int save_raw_data(const float * pData, int num_samps);
int save_processed_data(void);
bool toggle_raw_recording();
bool toggle_proc_recording();
void start_raw_recording();
void start_proc_recording();
void stop_raw_recording();
void stop_proc_recording();
int load_window(float *vector, int nsamp, int sll);
int openDebugDataFile();
void debugPrint(const char*, ...);
void closeDebugDataFile();

#ifndef __WithoutDataBase__
int dBInitialize(void);
int dBOutput(uint32_t *imageRow[], int numpts);
void dBClose();
#endif

//void closeDebugDataFile();
//int openDebugDataFile();

// In timing.cpp
int clock_to_char(char* timestring, const int length, DataTOV TOV);
int clock_to_char_long(char* timestring, const int length, DataTOV TOV);
DataTOV duration_to_systime(DataTics duration, DataTOV &time_pt);

// In processMaster.cpp
int startProcessingThread( void );
/* The following is called by the main routine to stop the processing threads */
int stopProcessingThread( void );

/* These are the routines where the work takes place */
extern std::mutex fftwPlanLock;
int Process_data(void);

// In processWorkers.cpp
void hamming(float *, int);

// in consoleMonitor.cpp
int ConsoleKeyMonitor(std::atomic<bool> &exitflag);
void StopConsoleMonitor();
int interpretKey(int key, std::atomic<bool> &exitflag);

// In radarSim.cpp
// Ignore the default values. THeya re there to keep the compiler happy and will be over-written.
typedef struct PendSimParms {
	int NumSampPerWRI=128;
	int NumWRIperBlock=128;
	int NumRadars=1;
	double PendRadFreq=1.0/5.0;  //  omega = sqrt(GEARTH/pendLength )
	double PendInitPhase=0.0;	// Phase in swing
	double MajorAxis=2.0;
	double MinorAxis=0.01;
	double MajorAxisRadFreq=0.0001; // Rate major axis rotates TWOPI * sin(latitude)/24hrs
	double initMajorAxisPhase=0.0;
	double BobRadius = 0.22; //Radius of the pendulum bob

	double RadarFreq=24e9;
	double RadarBandwidth=0.0;
	double samplePeriod=0.00001;
	bool siminit = false;
	
	std::vector<double> RadarXv;
	std::vector<double> RadarYv;
	std::vector<double> Phase;
	std::vector<double> PendFreq;
	double 	phaseinc=0.01;
	double	pendfreqinc=0.01;
// Member functions
	int RadarSimInit();
	void simTargetData(PaTime simTimeStart, sensordata< RTPComplex> *SimData, double SimAmpdB);
	void pendDistances(double postime, std::vector<double> *distances);
	void pendPosition(double postime, double pos[3]);

} PendSimParms;
void startSimADC();
void stopSimADC(); 

// The following are in sensorIO.cpp
int startStreamADC();
double	getADCTimeRef();
void stopStreamADC();
int init_ADC_data();
int paWaveCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer, 
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);

// In radarControl.cpp
int start_radar();
int stop_radar();

typedef struct floatdim4 {
	float value[4] = { 0.0 };  // It is always of dim 4, so hard coding is OK
} floatdim4;


typedef struct Radar_Data_Flowing {
	int MyID;						// ID of a specific processing thread.  Assigned by initializing thread. 
	int RadarChan;					// Channel number of the radar data being passed here
	CPI_Params Params;

	fftwf_complex *pData=NULL;			// Working data, being changed by worker thread.  Initialized by master thread before initiating processing
	RTPComplex *pCData=NULL;
	//fftwf_complex  *pTargetLine;	// Pointer to processed data to pass on for further processing - currently only passing one line
	//RTPComplex *pCTargetLine; 
	
	fftwf_plan fftwfPlan;				// Creating fftwfPlan is not thread safe, so plan needs to be provided to thread.
	float *pPRI_WGT=NULL, *pWRI_WGT=NULL;		// Pointers to window used to control sidelobes
	bool DCOnly;					// Flag to say how to cal data
	RTPComplex DCOffset;			// DC offset cal coefficients
	RTPComplex *DCOffsetArr;		// Pointer to WRI level "DC offset" cal coefficient arrays [Samp_Per_WRI]
	floatdim4 CalTransform;			// Gain and correlation cal constants between I/Q - after subtracting for DC offset

	float *pRDIPower=NULL;				// Pointer to where the output RDI power is stored
	int index_max_d=0;				// Index of maximimum in processed line
	float index_frac_d=0.0f;			// Fractional part of index to account for peak splitting
	int index_max_r = 0;			// Index of peak in range
	float index_frac_r = 0.0f;		// Fraction in range
	float peakAmplitude = -2000.0f;		// Amplitude (power) value at the location of the peak
	// C++11 constructs
	std::condition_variable DataHere; // Data ready for processing
	std::mutex OwnBuffers;			// Mutex for the data buffers
	bool InBufferFull=false;		// Tell thread that input data is in buffer
	bool OutBufferFull = false;		// Tell following thread that output data is in output buffers
	bool StopRequested=false;		// Tell thread to stop execution and return
	int initialize(CPI_Params InitParams, float* win_cpi, float* win_wri);		// Initialization function
	int calibrate();				// Calibration function
	//Radar_Data_Flowing();			// Constructor
	//~Radar_Data_Flowing();
}  Radar_Data_Flowing, *pRadar_Data_Flowing;

int StartWorkerThreads(pRadar_Data_Flowing  *pRadarDataArray,
	std::thread  hWorkerThreads[],
	int numThreads,
	CPI_Params params);

void stopWorkerThreads(pRadar_Data_Flowing  *pRadarDataArray, 
	std::thread  hWorkerThreads[],
	int numThreads);

void RadarWorkerFunction(pRadar_Data_Flowing MyRadarData, float* windowx,
	float* windowy,
	CPI_Params Params);




/*
class SolenoidControl
{
public:
	SolenoidControl(){};
	int Iterate( float max_velocity, float othogonal_velocity){return(0);}; // call this function to iterate the loop once per pendulum period
	int Init(){return(0);};
	int Shutdown(){};
private:
};
*/


/*
typedef struct Target_Data {  // Assumes that target is a pendulum
	// C++11 semaphore and buffer lock constructs
	std::condition_variable DataHere; // Data ready for processing
	std::mutex OwnBuffers;			// Mutex for the data buffers
	bool InBufferFull = false;		// Tell thread that input data is in buffer
	DataTOV 	tTime_Ref;		// Time when the pendulum major axis will cross the center (note pi ambiguity)
	DataTOV Data_TOVtp;		// Measurement Time of validity
	int nTimeSamps;			// Number of samples in historical time record
	int current_indx;		// when filling time record, keep track of where the last value was written
	float MajorAxisAngle;
	float MajorAxis;
	float MinorAxis;
	float period;
	bool StopRequested;
}  Target_Data, *pTarget_Data;
*/
// I got tired of seeing the warnings about uninitialized variables so initialize most even though they are over-written
// shortly after this structure is realized.
typedef struct RadarConfig {
	std::string	ProgramName;	// Program title for window
	std::string Version;			// Program version number
	std::string DataFileRoot;			// Location to save data
	std::string SPWinDir;	//Location where signal processing windows are stored
	std::string ProcWindowFName;
#ifndef __WithoutDataBase__
	std::string DataBaseHost;
	std::string DataBaseUser;
	std::string DataBasePasswd;
	std::string DataBaseName;
	int DataBaseRows; // Number of rows before clearing out data
#endif

	bool RecordRawDataFromStart=0;	//Start recording raw data on startup or not
	bool RecordProcDataFromStart=0;	//Start recording processed data on startup or not
	bool LogToFile=0;		//Log warning and error messages to a file (opened based on start time, in DataFileRoot)
	double MaxProcFileTime=3600*24.0;	// Maximum time to write processed data to a file before the file is closed and a new file opened
	double MaxRawFileTime=300;	// Maximum time to write raw data to a file before the file is closed and a new file opened

	int NumRadars=2;		//Number of radars to expect and request in opening the I/O interface
	double SampleRate=48e3;		//Desired sample rate,  THis can be changed by the ADC interface, which needs to be done
						//before any threads using the SampleRate have initialized. So, initializing the ADC I/F is done first.
	int NSamplesPerWRI=128;	//Number of time samples per waveform repetition interval = number of Range bins
	int NWRIPerCPI=128;		//Number of waveform repetitions per coherent processing interval = number Doppler bins
	int NWRIPerBlock=32;	//Number of waveform repetitions per block of data collected from the ADCs
						// This block size determines the amount of overlap processing accomplished
	int NumThreads=16;		// Number of worker threads to use for signal processing, minimum is NumRadars*2 (so ping-pong)
						// Maximum number currently is MaxThreads = 64.

	double MinRefLevel=10.0;	// Minimum level for the reference level.  The scroll bar will go from this level to this level plus 100dB
	bool ASIOPriority=0; //ASIO interface, if true, then ASIO takes priority over default input
	bool SimADC = 0;	// If true then don't use the ADC/audio interface, but instead simulate the ADC data - noise only
	bool SimPend = 0;	// Simulate a pendulum (or just inject a sine wave)
	double PendLength = 20; // length of the pendulum in m for equations of motion
	double PendMajorAxis=0.8;		// Pendulum motion is an ellipse.  THis is the major axis in meters  
	double PendMinorAxis=0.0;		// The ellipse minor axis in meters
	double PendLatitude=34.5;
	double ADCVariance=0.001;	// variance of ADC noise power
	double RadarDistance=3.0;	// Radars distance from center of pendulum swing

	double CalDCVal[2 * MaxRadars] = {0.0}; // Pointer to array of initial DC calibration values 
	floatdim4 CalTransForm[MaxRadars] = {}; // Pointer to array of IQ transformation matrices
	double TxRxSampleOffset = 0.0;	// Sample offset to account for the transmit/receive delays
						// Only the integer part is currently used, but fractional samples should also be considered
	
	int DTI_Height=512;		//Height of the scrolling DTI
	double CenterFreq=24e9;	// center frequency of radar in Hz
	double Bandwidth=0.0;	// Analog hardware chirp modulation bandwidth used to scale range
	double UAmbDoppler=1.2;	// The unambiguous Doppler range
	double FadeMemVal=0.95;	// The fading memory filter constant e.g. newcal = 0.9 oldval + 0.1 newest
	double ScaleData=0.0;	// Add to the pixel value before displaying
	bool DC_CalOnly=TRUE;		// True when only a single DC offset value is corrected per radar.  False if each range corrected seperately.
	// The following are only partially implemented- need to deinterleave the ADC samples
	bool ReceiveRealOnly=FALSE;		// Whether the input data is real only or IQ pairs
	// And these require changing the processing flow in the worker threads
	bool Sonar=FALSE ;			// Create transmit waveform and process as a sonar system
	double SonarFreq=10e3 ;		// Center frequency of the sonar sweep (Default is 12kHz)
	double SonarBandwidth=2e3 ; // Bandwidth of the audio chirp (default is 5kHz)
	int RxADC_Chan = -1;
	int TxADC_Chan = -1;
}  RadarConfig, *pRadarConfig;

// The following are all the things that are normally changed as the program runs
// Most are initialized on start-up
typedef struct RadarState {
	// The following lines work under Visual studio but not under g++, haven't figured out why not
	// To allow compiling under g++, use the non-atomic version
	//std::atomic<bool> DataRecording = FALSE;	// Processed recording is on
	//std::atomic<bool> RawRecording=FALSE;		// Raw recording is on
	//std::atomic<bool> LogRecording=FALSE;		// Log recording is on
	// The following should be atomic. Left this way so one code base can be used on both Win and Linux
	bool DataRecording = FALSE;		// Processed recording is on
	bool RawRecording = FALSE;		// Raw recording is on
	bool LogRecording = FALSE;		// Raw recording is on
	int NumSensorsSet=1;		// Ideally equal to the number of radars requested, NumRadars in config file.  
	//If the I/O interface can't be set as requested then the default RadarConfig NumRadars will be over-ridden
	int NumADCChans=2;	// Number of ADC channels. Avoids excessive logic in determining buffer sizes

	// Some current Display parameters
	double RefLeveldB=-40;	// Current reference level for the display
	double DispRange=80;	// Range from minimum to maximum display
	bool PeakOverlay=TRUE;		// Turn on the tracked cell overlay
	cmaptype ColorMapSelected = cmaptype::COLORMAP_HOT; // The colormap is in use

	double TransmitModAmp=0.0;	// Amplitude of the transmit driver ramp (should probably allow an offset too
	bool TransmitModOn=FALSE;		// Is transmit modulation on?
	bool TransmitLongSweep=FALSE; // Sweep is the entire data block (NSamplesPerWRI* NWRIPerBlock vs. just NSamplesPerWRI)

	double SimAmp=-30.0;			//Amplitude of the injected tone in dBV (whose frequency varies as a sinusoid)
	bool SimOn=TRUE;				//start with sim on (true) or off (false)
	
	bool AutoCalOn=TRUE;			// Whether auto calibration is enabled or not
	int Current_block_id=0;	// Block counter - index of input data block being processed - maintained by processing thread
	
}  RadarState, *pRadarState;

typedef struct ProcessedRadarData {
	CPI_Params Params;
										// Now using C++11 constructs
	std::condition_variable DataHere;	// Data ready for processing
	std::mutex OwnBuffers;				// Mutex for the data buffers
	bool InBufferFull = false;			// Tell thread that input data is in buffer
	float *pRDIPower[MaxRadars];		// Storage for the Range-Doppler Image
	float *pDTI[MaxRadars];				// Scrolling Doppler time intensity plot for each radar
	char *target_line[MaxRadars];		// Pointer to where the last Doppler row is stored.  Redundant with image, but has corner turn
	float peakDoppler[MaxRadars];		// Speed corresponding to the Doppler peak
	float peakAmplitude[MaxRadars];		// Amplitude corresponding to the peak return
	int index_max_d[MaxRadars];			// Index of maximimum in processed line
	int index_max_r[MaxRadars];			// Index of maximimum in processed line
	float index_frac_d[MaxRadars];		// Fractional part of index to account for peak splitting
	float index_frac_r[MaxRadars];		// Fractional part of index to account for peak splitting
//	DataTics Data_TOVtt;		// Time ticks for data validity
	//int initialize();
	//~ProcessedRadarData();
} ProcessedRadarData, *pProcessedRadarData;



extern ProcessedRadarData gProcessedData;
extern RadarConfig gRadarConfig;
extern RadarState gRadarState;

inline RadarState GetRadarState()
{
		return gRadarState;
}


inline RadarConfig GetRadarConfig()
{
	return gRadarConfig;
}

//_RTP_Thread_Type MagnetFunction(LPVOID lpParam);
void OutputWorkerFunction(pRadar_Data_Flowing  pRadarData[], CPI_Params InitParams);

// In command.cpp

int SetDisplayScale(double ScaleMin, double ScaleMax);
void SetColorMap(cmaptype CMapIn);

