// This routine provides a mechanism for logging status, warnings, errors, etc.
// By Frank Robey
// Nov 2014 Original implementation 
// Oct 2017 Added file locking to avoid overwriting messages from multiple threads. Functions are now thread safe
// Jan 2018 Changed to c++11 mutex, added variable arguments to logmessage as in printf

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
#include "logMessages.h"
#include <mutex>
#include <iostream>
#include <fstream>
//#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <stdarg.h>
//#include "radarc.h"
#define _MAX_LOG 4096	
std::mutex con_lock;  // Lock so that multiple threads don't overwrite each other when printing
std::ofstream filelog;  // Log file stream identification

bool _log_to_file = TRUE;

//void open_log_file();

// To properly log the messages to the screen we want ms resolution.  This is finer than historical portable c time structures.
// The standar chrono and ctime functions don't conveniently provide function to format current time to ms for display
// Simple under windows using SYSTEMTIME, but not portable. The following is portable.

int logMsg_clock_to_char(char* timestring, const int length, logTOV msgTOV)
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


bool LogRecordOn = FALSE;

// Log the message. log_text can either be a straightforward char string, or it can be a format string for a variable
// number of arguments.  This was done so I can stop printing to a string and then passing the string.
void log_message(const char* log_text,  ... )
{

	va_list myargs;

	if (_log_to_file && !LogRecordOn) open_log_file("./");

	// This is a c++11 portable approach. More complicated than windows-only approach
	// First get system time and cast to ms resolution
	std::chrono::system_clock::time_point systime =std::chrono::system_clock::now();
	char timestr[256];

	logMsg_clock_to_char(timestr, sizeof(timestr), systime);
	char fmt[_MAX_LOG];
	snprintf(fmt, sizeof(fmt), "%s: %s\n", timestr, log_text);
	
	// Now print the message to a character string
	char		msg[_MAX_LOG];
	va_start(myargs, log_text);
	vsnprintf(msg, sizeof(msg), fmt , myargs);
	va_end(myargs);

	std::unique_lock<std::mutex> console_lock(con_lock);
	std::cout << msg;
	if (!std::cout){// Do something to reset cout
	}
	if (LogRecordOn && filelog.is_open()) filelog << msg;
	console_lock.unlock();
}

void log_message(std::string log_text)
{

//	std::string temp = ssmsg.str();

	if (_log_to_file && !LogRecordOn) open_log_file("./");

	// This is a c++11 portable approach. More complicated than windows-only approach though
	// First get system time and cast to ms resolution
	std::chrono::system_clock::time_point systime = std::chrono::system_clock::now();
	char timestr[256];

	logMsg_clock_to_char(timestr, sizeof(timestr), systime);

	std::ostringstream ssmsg;
	ssmsg << timestr << ": " << log_text << std::endl;
	
	std::unique_lock<std::mutex> console_lock(con_lock);  // I got tired of multiple threads writing at the same time so put a lock on it.
	std::cout << ssmsg.str();
	if (!std::cout) {// TODO: Do something to reset cout
	}
	if (LogRecordOn && filelog.is_open()) filelog << ssmsg.str();
	console_lock.unlock();
}

void log_error_message(const char* log_text, int myerrno = -1)
{
	char		msg[2048];
	int msgerrno;
	if (myerrno == -1) {
#ifdef _WIN32
		msgerrno = (int) GetLastError();
#else
		msgerrno = errno;
#endif
	}
	else {
		msgerrno = myerrno;
	}
	
	snprintf(msg, sizeof(msg), "Error: %s, %d\n", log_text, msgerrno);  // Puts a blank line after errors

	log_message((const char *)msg);

	return;
}
 /*
void log_warning_message(const char* log_text, int mywarnno)
{
	char		msg[2048];

	snprintf(msg, sizeof(msg), "Warning: %s, %d", log_text, mywarnno);
	log_message((const char *)msg);
	return;
}
*/

int open_log_file(const char * logFileRoot)
// There is only one thread-safe routine that will write to this file, so no lock is needed
{
	char fname[256], msg[256];

	if (filelog.is_open()) {
		log_message("Closing existing log file to open a new file");
		filelog.close();
	}

	std::time_t currtime = std::time(nullptr);
	std::tm timestruc;
#ifdef _WIN32
	gmtime_s(&timestruc, &currtime);
#else
	gmtime_r(&currtime, &timestruc);
#endif
	std::strftime(msg, sizeof(msg), "log_%Y%m%d_%H%M%S", &timestruc);
	snprintf(fname, sizeof(fname), "%s%s.txt", logFileRoot, msg);
	
	filelog.open(fname);
	LogRecordOn = TRUE;  // Set this to true so we don't end up in an infinite loop trying to open log file
		// State is checked so we don't record to a file that isn't open

	if (!filelog.is_open())
	{
		log_message("Warning: Unable to create/open log file named '%s'.  Log will not be saved.  Clean up and try again.", fname);
		return 1;
	} 	else 	{
		log_message( "File for saving log data opened as: fname=%s", fname);
		
	}
	
	return 0;
}

void close_log_file()
{
	con_lock.lock();  // This will prevent closing the file during a write operation
	LogRecordOn = FALSE;
	filelog.close();
	con_lock.unlock();
}

// The following routine opens a console window when in windows to display error and debug messages.  
// These messages can also be captured to a file by redirecting the output e.g. winradar >>log.txt
// We never close the console once opened.  We let it be closed when the program exits.
// The idea and related examples are on the web but can't find a reference.
void remapstdConsole(void)
{
#ifdef WIN32
	FILE * pCON;
	AllocConsole();
	freopen_s(&pCON, "CONIN$", "r", stdin);
	freopen_s(&pCON, "CONOUT$", "w", stdout);
	freopen_s(&pCON, "CONOUT$", "w", stderr);
#else
	if (freopen("/dev/tty", "r", stdin) ==NULL||
		freopen("/dev/tty", "w", stdout) ==NULL ||
		freopen("/dev/tty", "w", stderr) ==NULL )
		log_message("Problem reopening linux console windows /dev/tty...\n         Not even sure this is needed as it is on Windows.");
#endif
	return;
}