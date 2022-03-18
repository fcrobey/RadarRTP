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
#pragma once
//#include "stdafx.h"
#include <iostream>
#include <chrono>

void log_message(const char* , ...);
void log_message(std::string log_text);
//void log_warning_message(const char*, int);
void log_error_message(const char*, int);

void remapstdConsole(void);
void close_log_file();
int open_log_file(const char *);
using logTOV = std::chrono::system_clock::time_point;
int logMsg_clock_to_char(char* timestring, const int length, logTOV msgTOV);