// This is some useful time manipulation routines
// Pulled out of other modules
// Original: Frank Robey Feb-Mar 2018
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
/* Some notes about timing:
 I elected to time stamp metadata as time ticks, or equivalently c++11 duration. 
 The reason for this is that the resolution of the c++11 system_time varies. 
 System time is also non-monotonic, but that could have been dealt with. 
 I was uncomfortable with how portable the c++11 system_time would be, so use timings relative
 to an initial c++11 steady_clock.  That is then converted back to GMT by adding the duration to the 
 GMT time referenced to the steady_clock epoch. 
 Logged messages are always in GMT using the system_time clock. That was because that is how I started.
 This means that, e.g. the time printed in a log_message can 
 differ from the data time of validity by a varying amount as the system_time clock is adjusted.

 To tag the data with microsecond resolution with time extents on the order of years requires ~45-50 bits.
 So, I'm using 64 bit ints for the durations ( int64_t for portability with c++11)
*/

// Time of validity for data block in time ticks is (PaStreamCallbackTimeInfo.InputBufferAdcTime - gStreamPATimeRef) converted to 
// a duration in microseconds. 
// This is converted to GMT by adding gStreamSysTimeRef
// The portaudio callback ADC time is used as the basis for the timestamp. This timestamp has 1us resolution and is the time of the first ADC
// sample in the buffer. 
// To convert this time to a radar CPI time of validity then half the CPI time needs to be added as well as adding a factor for the time of flight.
// Since time of flight is short here, the time of flight is ignored.

#include "stdafx.h"  // Needed to use pre-compiled headers
#include <chrono>
#include <ctime>

// The following is needed to convert the DataTics to GMT. It is defined in globals.cpp
// std::chrono::system_clock::time_point gStreamSysTimeRef;

int clock_to_char(char* timestring, const int length, DataTOV msgTOV)
{
	// Cast time to ms resolution
	auto TOV_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(msgTOV);

	// Cast to sec resolution (auto = std::chrono::system_clock::timepoint
	auto TOV_sec = std::chrono::time_point_cast<std::chrono::seconds>(TOV_ms);
	// Now determine fractional seconds
	std::chrono::system_clock::duration frac_ms = TOV_ms.time_since_epoch();
	frac_ms -= std::chrono::duration_cast<std::chrono::seconds>(TOV_sec.time_since_epoch());
	time_t st1 = std::chrono::system_clock::to_time_t(TOV_sec);
	std::tm timestruc;
#ifdef _WIN32
	gmtime_s(&timestruc, &st1);
#else
	gmtime_r(&st1, &timestruc);
#endif
	char msg[256];
	std::strftime(msg, sizeof(msg), "%m/%d:%H:%M:%S", &timestruc);
	return snprintf(timestring, length, "%s.%03d", msg, static_cast<int>(frac_ms / std::chrono::milliseconds(1)));
}

int clock_to_char_long(char* timestring, const int length, DataTOV msgTOV)
{
	// Cast time to ms resolution
	auto TOV_ms = std::chrono::time_point_cast<std::chrono::microseconds>(msgTOV);

	// Cast to sec resolution (auto = std::chrono::system_clock::timepoint
	auto TOV_sec = std::chrono::time_point_cast<std::chrono::seconds>(TOV_ms);
	// Now determine fractional seconds
	std::chrono::system_clock::duration frac_ms = TOV_ms.time_since_epoch();
	frac_ms -= std::chrono::duration_cast<std::chrono::seconds>(TOV_sec.time_since_epoch());
	time_t st1 = std::chrono::system_clock::to_time_t(TOV_sec);
	std::tm timestruc;
#ifdef _WIN32
	gmtime_s(&timestruc, &st1);
#else
	gmtime_r(&st1, &timestruc);
#endif
	char msg[256];
	std::strftime(msg, sizeof(msg), "%Y,%m,%d,%H:%M:%S", &timestruc);
	return snprintf(timestring, length, "%s.%06d", msg, static_cast<int>(frac_ms / std::chrono::microseconds(1)));
}




DataTOV duration_to_systime(DataTics duration, DataTOV &time_pt)
{
	return (gStreamSysTimeRef + duration);
}