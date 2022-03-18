// SensorData Cube/Matrix Template Class
// This is a mixture of my own class circa 1988, Ed Kelly's linear class, circa 1989 and recent additions.
// This module leverages some of the ideas from Robert Mashlan's article in Mar 1997 Dr. Dobbs "A C++ matrix template class," which helped to streamline this.
// Unfortunately, at this time, Dr. Dobbs does not appear to be on-line.
// Latest additions, F. Robey, Aug, 2019
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

#ifndef __SensorData_h
#define __SensorData_h
#pragma once

#include <stdexcept>
#include <vector>
//#include <function>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include "CPIParameters.h"
#include "logMessages.h"
// undefine this to disable array bounds checking
#define BoundsCheck

template<typename T>
struct sensordata {
   private:
      std::vector<T> data; // array of data
	  std::condition_variable DataHere; // Data ready for processing
	  std::mutex OwnBuffers;			// Mutex for the data buffers
	  CPI_Params SetupParams;			// How the current data buffers are configured

   public:
	  CPI_Params Params;	// Parameters to use in setting up data buffers
      const unsigned rows=1;  // number of rows
      const unsigned cols=1;  // number of columns
	  const unsigned sensors=1; // number of sensors
	  
	   // constructors
	  sensordata<T>(unsigned rows, unsigned cols, unsigned NumSensors)
		  : rows(rows), cols(cols), sensors(NumSensors), data(rows*cols*NumSensors)
	  {
		  if (rows == 0 || cols == 0 || NumSensors == 0)
			  throw std::out_of_range("Attempt to create a degenerate sensordata matrix");
		  log_message("SensorData Constructor");
	 };
	 
	  sensordata<T>(CPI_Params InParams)
		  : Params(InParams), 
		  rows(Params.Num_WRI),
		  cols(Params.Samp_Per_WRI), 
		  sensors(Params.NumSensorsSet),
	  data(InParams.Num_WRI*InParams.NumSensorsSet*InParams.Samp_Per_WRI)
	  {
		  Params = InParams;
		  log_message("SensorData Params Constructor");
		  if (Params.NumSensorsSet == 0 || Params.Num_WRI == 0 || Params.Samp_Per_WRI == 0)
			  throw std::out_of_range("Attempt to create a degenerate matrix in sensordata Params Constructor");
		  else {
			  data.resize(InParams.Num_WRI*InParams.NumSensorsSet*InParams.Samp_Per_WRI);

		  }
	  };
	  sensordata<T>(unsigned rows, unsigned cols, unsigned NumSensors, const T data)
		  : rows(rows), cols(cols), sensors(NumSensors),data(rows*cols*NumSensors, T(data))
	  {
		  if (rows == 0 || cols == 0 || NumSensors==0)
			  throw std::out_of_range("Attempt to create a degenerate initialized sensordata matrix");
		  // initialze from scalar input
		  for (unsigned i = 0; i < rows*cols*NumSensors; i++)
			  this->data[i] = data;
		  log_message("SensorData Initialized Constructor");
	  };
	  sensordata<T>(const sensordata<T>& cp) : rows(cp.rows), cols(cp.cols), sensors(cp.sensors), data(cp.data)
	  {
		  log_message("SensorData Copy Constructor");
	  }

	  int loadRaw() {
		  std::unique_lock <std::mutex> Bufferlock(OwnBuffers, std::defer_lock);  // Lock the buffer before manipulating
		  log_message("Warning: called loadRaw. This function has not been completed");
		return(-1);
	  }
      
   protected:
      // verify that the indices are not out of bounds
      
	  void bounds_check(unsigned i, unsigned j, unsigned ThisSensor = 0) const
	  {
		  if (rows <= i)
			  throw std::out_of_range("Data access row out of bounds");
		  if (cols <= j)
			  throw std::out_of_range("Data access col out of range");
		  if (sensors <= ThisSensor)
			  throw std::out_of_range("Data access number of sensors out of range");
	  }

	
   public:

	  T& operator()(unsigned i, unsigned j, unsigned NumSensor=0) {
#ifdef BoundsCheck
		  bounds_check(i, j, NumSensor);
#endif
		  return data[((int) i*cols + j)*sensors+NumSensor];
	  }
      const T& operator()( unsigned i, unsigned j, unsigned NumSensor=0 ) const {
         #ifdef BoundsCheck
         bounds_check(i,j, NumSensor);
         #endif
         return data[(i*cols+j)*sensors+NumSensor];
      }
 
   public:

      // destructor
	   ~sensordata()  {  	   }// Thus far, default destructor fits fine.

      // assignment
      sensordata<T>& operator=( const sensordata<T>& );

      // scalar multiplication/division
      sensordata<T> operator*( const T scalar ) const { return sensordata<T>(*this).operator*=(scalar); }
      sensordata<T> operator/( const T& a ) { return sensordata<T>(*this).operator/=(a); }
	  sensordata<T> operator+(const sensordata<T>& M) const { return sensordata<T>(*this).operator+=(M); }
	  sensordata<T> operator-(const sensordata<T>& M) const { return sensordata<T>(*this).operator-=(M); }

// TODO: The following are standard C++ functions that should be moved to be member functions. 
// SetRawData
// DopplerProcess()
// PulseCompress()
// RangeDoppProcess()
// ConverttoImage()
// FindPeaks()
// DetectPeaks()
// TargetSlice()
// 

};


template<class T>
sensordata<T>& sensordata<T>::operator=( const sensordata<T>& cp )
{
   if(cp.rows != rows && cp.cols != cols )
      throw std::domain_error("sensordata op= not of same order");
   for(unsigned i=0;i<rows*cols;i++)
      data[i] = cp.data[i];
   return *this;
}

#endif


