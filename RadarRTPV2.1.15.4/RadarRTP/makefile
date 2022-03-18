################# Makefile for microradarRTP ################

CC = g++
CFLAGS = -std=c++11 -O3 -P -Wall -Wcomment -fpermissive 
DEFINES = -D_RTP_Headless -D__WithoutDataBase__ -DFLTKGUI
INCLUDE = -I/usr/include -I/usr/local/include 
LIBS   = -lpthread -lini -lportaudio -lfftw3f -lsndfile -lm -lfltk

SEARCH  = 

RADAROBJS   =  fltkgui1.o command.o buffers.o colormap.o globals.o ImageDisplay.o logMessages.o processMaster.o processWorkers.o radar_io.o main.o radarConfig.o timing.o waveform.o GUI.o radarSim.o database.o calibration.o consoleMonitor.o radarControl.o

#.SUFFIXES: .o .c .f

all : radarRTP
.PHONY : all


.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFINES) -c $*.c

#.cc.o :
#	$(CC) $(CFLAGS) $(INCLUDE)  $(DEFINES) -c  $< 

.cpp.o :
	$(CC) $(CFLAGS) $(INCLUDE)  $(DEFINES) -c  $< 


radarRTP   :  $(RADAROBJS) 
	$(CC)  $(CFLAGS) $(INCLUDE) $(RADAROBJS) $(SEARCH) $(LIBS) -o radarRTP

.PHONY: clean
clean :
	-rm -f *.o a.out core 

################################################################
