// This module provides a circular block buffer for incoming radar data and timing
// It assumes there is only a single producer and a single consumer
// There are probably more efficient ways to do this, but it works.
// Note that timing storage isn't mentioned in this file. Timing metadata buffers
// are handled outside this module, but the lock/ and buffer management is done by this module
//  
// Written by Frank Robey
// Jan 2014  Initial version
// Nov 2014: Cleaned up some of the error handling.
// Sept 2017  Cleaned up block overrun that occurred after about 45 seconds and would never occur again.
//            (after having tested for many weeks.  Over-run due to error in how blocks were counted)
// Feb 2018: moved buffer initialization to this module (from radarConfig module).
// Mar 2018: converted to use c++11 mutex and condition variable
//			 Originally written in C. Converted to c++ only with this change
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
#include <condition_variable>

std::condition_variable BuffDataHere; // Data ready for processing
std::mutex BuffOwnBuffers;			// Mutex for the data buffers

int Buff_Next_Open = 0;		// only the producer should change this value through buff_mark_used()
int Buff_Next_Read = 0;		// Only the consumer should change this value through buff_free()
int BuffCount;				// THe number of buffers currently in use 
bool BuffInitialized = FALSE;
bool BuffDataFlowing = FALSE;
void buff_init(void)
{
	// Allocate global memory for the circular buffers [NBUFF][NSAMP_PER_WRI*NWRI_Per_Block*NUMRXCHAN]
	Buff_Data[0] = (float*)malloc(2 * NBUFF*gRadarConfig.NSamplesPerWRI *gRadarConfig.NWRIPerBlock * gRadarConfig.NumRadars * sizeof(float));
	if (Buff_Data != NULL) {
		for (int index = 1; index<NBUFF; index++) {
			Buff_Data[index] = Buff_Data[index - 1] + 2 * gRadarConfig.NSamplesPerWRI *gRadarConfig.NWRIPerBlock * gRadarConfig.NumRadars;
		}
	}
	else {
		log_error_message("Circular Buffer: Unable to allocate memory for cicular buffers.", GetLastError());
		exit(1);
	}
	std::unique_lock<std::mutex> bufferlock(BuffOwnBuffers);
	Buff_Next_Open=0;
	Buff_Next_Read=0;
	BuffCount = 0;
	bufferlock.unlock();
	log_message("Circular buffers have been initialized");
	BuffInitialized = TRUE;

}

void buff_free(void)
{	
	std::unique_lock<std::mutex> bufferlock(BuffOwnBuffers);
	if (Buff_Next_Read == Buff_Next_Open) log_message("Warning: Circular Buffer %d synchronization error, resynching", 
		Buff_Next_Read*10+Buff_Next_Open);
	Buff_Next_Read= (Buff_Next_Read+1) % NBUFF;  /* increment read pointer to next block (might not be data there yet */
	
	BuffCount--;
	if (BuffCount < 0) {
		log_message("Warning: Circular Buffer count error %d", BuffCount);
		BuffCount = 0;
	}
	bufferlock.unlock();
}

void buff_mark_used(int index)  		/* Mark buffer as being used (has data)  */
{
	std::unique_lock<std::mutex> bufferlock(BuffOwnBuffers);

	// Check that buffer being marked is correct
	if (index != (Buff_Next_Open)) log_message("Circular Buffer: mismatch %d check warning",index*10+Buff_Next_Open);

	// Calculate how many buffers are open
	int BuffCountTemp = Buff_Next_Open - Buff_Next_Read;
	if (BuffCountTemp<0) BuffCountTemp += NBUFF;
	if (BuffCountTemp != BuffCount) log_message("Warning: Buffer mark used lost count at %d", 
		BuffCount*100+Buff_Next_Open*10+Buff_Next_Read);
	if (BuffCount==(NBUFF-1)) {
		log_message("Warning: Circular Buffer: over run. Skipping input data block.  Currently blocks = %d", BuffCount);
		// Don't advance the block buffer pointer even though the data might have been consumed before the next callback */
		// This buffer will be over-written and warning logged - data will be currupted

	} else {
		Buff_Next_Open= (Buff_Next_Open+1) % NBUFF;  /* increment write pointer to next block */
		BuffCount++;
	}
	bufferlock.unlock();
	BuffDataHere.notify_one();
}

// Give data writer index of next open memory block
// Don't mark it as used until data is posted
int buff_get_next_free(void)
{
	/* could check for a buffer overrun here instead to give it a bit more time to finish */
	return(Buff_Next_Open);
}

/* The following will block until a full data buffer block is available or timeout occurs */
/* It will return the index to the data block that is next to be read/processed */
/* Will return -1 if no data is provided in last 1000ms */
// Spurious wakeups are also now handled.
int buff_Wait_For_Data(void)
{
	std::unique_lock<std::mutex> bufferlock(BuffOwnBuffers);
	while (BuffCount==0) {
		std::cv_status cvstat = BuffDataHere.wait_for(bufferlock, std::chrono::milliseconds(400));
		if (cvstat == std::cv_status::timeout) {
			return( -1);
		}
	}
	BuffDataFlowing = TRUE;
	if (Buff_Data[Buff_Next_Read] == NULL) {
		log_message("Warning: Circular Buffer: invalid buffer: %d ", Buff_Next_Read);
		return(-1);
	}
	return(Buff_Next_Read);  /* return the index to the read buffer */
}

void buff_destroy(void)
{
	free(Buff_Data[0]);
	Buff_Data[0] = NULL;
	return;
}

