// This module was written so that when operating on Linux without a GUI that the program can be controlled.
// The primary routine in this module is a thread that listens for key presses then interprets the keypresses.
// Only a limited number of commands have been implemented primarily dealing with 
// By Frank Robey
// Feb-Mar 2018 Original implementation
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
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#ifndef _RTP_Headless
#ifdef _WINGUI
#include "winGUI.h" // Needed to update button status
extern HWND hWndApp;
#endif
#ifdef _FLTKGUI
#include "fltkgui.h"
#endif
#endif
#define QUIT_NOW 222

#ifdef _WIN32
#include <conio.h>
#else
extern "C" {
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
}

/* reads keypress, doesn't echo. Does not shut off stdout while waiting for keypress. */
/* This is put together from several postings on stack exchange.*/
/* TODO: Check to see if it disables CNTRL-C under Linux.  If so, need to check for key=03 */
int _getch(void)
{
	struct termios oldattr, newattr;
	int ch;
	tcgetattr(STDIN_FILENO, &oldattr);
	newattr = oldattr;
	cfmakeraw(&newattr);
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	setvbuf(stdout, NULL, _IONBF, 0);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
	setvbuf(stdout, NULL, _IOLBF, 0);
	return ch;

}
#endif

void StopConsoleMonitor()
{
	//int key = (int) 'X';
	// Send an 'X' to the console monitor and that process will exit
	if (ungetc('X', stdin) != 'X') log_message("Cannot signal console monitor to exit");
}
int ConsoleKeyMonitor(std::atomic<bool> &exitflag)
{
	int  key;

	while (!exitflag) {
#ifdef _WIN32
		while (!_kbhit()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if (exitflag) break;
		}
		if (exitflag) break;
#endif
		key = _getch();  // Built-in function for windows, defined for Linux
		int retval = interpretKey(key, exitflag);

#ifndef _RTP_Headless
#ifdef _WINGUI
		if (!retval) {
			updateButtonStatus();
		}
#endif
#endif

	}
	log_message("Console monitor exiting");
	return 0;
}

int interpretKey(int key, std::atomic<bool> &exitflag)
{
	std::cout << "Key pressed is: " << char(key)
		<< " (" << std::hex << std::showbase << std::internal
		<< key << std::dec << ")" << std::endl;
	key = toupper(key);
	if ((key >= 'A') && (key <= 'Z'))
	{
		switch (key) {
		case 'Q':
		case 'X':
		case 0x03:    // Control-C
		{
			exitflag = true;  // This will tell monitor thread to exit
			log_message("Console exit program entered");

#ifdef _WIN32
			// Post message to main message loop to quit
			//PostMessage(hWndApp, WM_QUIT, 0, 0);
			PostQuitMessage(0);
			//break;
#endif
#ifdef _FLTKGUI
			// Nothing to do here
			
			break;
#endif
			// If not running a GUI, and not WIN32. Need to shut down anyway
			// It will do so, but having the main program monitor the console monitor thread return value
			break;
		}
		case 'R':
		{ // Record raw
			log_message("Console start raw data recording entered");
			start_raw_recording();
			break;
		}
		case 'P':
		{ // Record processed
			log_message("Console start processed data recording entered");
			start_proc_recording();
			break;
		}
		// Ignore Carraige return, line feed
		case 'S':
		{ // Stop recording
			log_message("Console request stop recording entered");
			close_all_open_files();
			break;
		}
 
		default:
		{
			log_message("Unknown key command entered");
			return(1);
		}
		}
	}
	return 0;
}