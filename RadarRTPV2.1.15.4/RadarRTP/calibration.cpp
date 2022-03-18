// Calibration routines to determine DC offset and covariance/whitening transformation
//	This file was split out of the process.cpp in order to try to make the calculation of
//	the calibration coefficients more modular.
//  The calibration is a separate low priority task/thread that is kicked off by the processMaster
//  Data is provided to this routine using shared memory buffers.
//	The indexing is complicated because most routines are dealing with 3D arrays of complex data.
//  by Frank Robey
//	Nov 2014, Original implementation as a set of routines in process.cpp split off into this file
//	Feb-Mar 2018, Moved thread model to use C++11 threads and mutex.
//                Re-wrote the calibration functions for means, covariance and specialized eigen decomposition
//  April 2019, added the N-point moving average approach to calculating the DC offset and fixed a bug in the fading memory for the covariance.
//              The moving average works far better, at least in simulation, than fading memory for DC offset calculation.
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
#include <complex>
#include <iostream>
#include <sstream>

// Calculate the complex mean of the data array.  Data is interleaved real/imaginary
// 
void CalData::complexMean()
{
	int index;
	unsigned int rindex;
	RTPComplex sum(0.0, 0.0);
	int nsamp = Params.Num_WRI*Params.Samp_Per_WRI;
	for (rindex = 0; rindex < Params.NumSensorsSet; rindex++)
	{
		for (sum = 0.0, index = 0; index < nsamp; index++)
		{
			sum += pCalData[rindex][index];

		}
		DCOffset[rindex] = sum / ((float)(nsamp));
	}
	return;
}


// Calculate the 2 by 2 correlation matrix between the real and imag terms for each radar
// Subtracting the mean before forming the products
//void CalData::covarianceSingleMean(RTPComplex *data[], RTPComplex mean[], float corr[][4], int nsamp, int nradar)
// covarianceSingleMean(pData, &DCOffsetHist[0], R, NPerWRI*NWRI, NumSensorsSet);
void CalData::covarianceSingleMean()
{
	unsigned int rindex, i;
	float Rest[4], dr, di;
	unsigned int nsamp = Params.Num_WRI*Params.Samp_Per_WRI;

	for (rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
		for (i = 0; i < 4; i++)
			Rest[i] = 0.0;
		for (i = 0; i < nsamp; i++) {
			dr = pCalData[rindex][i].real() - DCOffsetHist[rindex].real();
			di = pCalData[rindex][i].imag() - DCOffsetHist[rindex].imag();
			Rest[0] += (dr * dr); // Calculate the covariance
			Rest[3] += (di * di);
			Rest[1] += (dr * di);
		}
		Rest[2] = Rest[1];
		for (i = 0; i < 4; i++)
			R[rindex].value[i] = Rest[i] / (float)nsamp;
	}

}

// This is a specialized function for the RTP that takes the mean across columns returning
// a matrix of dimension NSPerWRI by nradar
// The variables NSPerWRI corresponds to the number of time/range samples per waveform repetition
// The NWri corresponds to the number of repetitions in a coherent processing interval
// 
// 
//void CalData::complexMeanColumns3D(RTPComplex *data[], RTPComplex mean[], int NSPerWRI, int Nwri, int nradar)
void CalData::complexMeanColumns3D()
{
	unsigned int index, WRIindex, samp_index, rindex;
	float norm = 1.0f / ((float)Params.Num_WRI);
	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++)
		for (index = 0; index < Params.Samp_Per_WRI; index++)
		{
			DCOffsetWRI[rindex][index] = RTPComplex(0.0, 0.0);
		}

	for (samp_index = 0; samp_index < Params.Samp_Per_WRI; samp_index++) {
		for (WRIindex = 0; WRIindex < Params.Num_WRI; WRIindex++)
			for (rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
				{
					DCOffsetWRI[rindex][samp_index] += pCalData[rindex][samp_index + WRIindex * Params.Samp_Per_WRI];
				}
			}
	}
	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++)
		for (index = 0; index < Params.Samp_Per_WRI; index++)
		{
			DCOffsetWRI[rindex][index] *= norm;
		}
	return;

}
// Calculate the 2 by 2 correlation matrix between the real and imag terms for each radar
// Subtracting a "range" dependent mean before forming the products.  Range is in quotes since 
// it is range prior to pulse compression.
//void CalData::covarianceRangeDepMean(RTPComplex *data[], RTPComplex mean[], float corr[][4], int NSPerWRI, int Nwri, int nradar)
void CalData::covarianceRangeDepMean()
{
	unsigned int rindex, i;
	float Rest[4], dr, di, norm;
	norm = 1.0F / (float)(Params.Samp_Per_WRI*Params.Num_WRI);

	for (rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
		for (i = 0; i < 4; i++)
			Rest[i] = 0.0;
		for (unsigned int samp_index = 0; samp_index < Params.Samp_Per_WRI; samp_index++) {
			for (unsigned int index1 = 0; index1 < Params.Num_WRI; index1++) {
				dr = pCalData[rindex][index1*Params.Samp_Per_WRI + samp_index].real() - DCOffsetWRI[rindex][samp_index].real();
				di = pCalData[rindex][index1*Params.Samp_Per_WRI + samp_index].imag() - DCOffsetWRI[rindex][samp_index].imag();
				Rest[0] += (dr * dr); // Accumulate the covariance
				Rest[3] += (di * di);
				Rest[1] += (dr * di);
			}
		}
		Rest[2] = Rest[1];
		for (i = 0; i < 4; i++)
			R[rindex].value[i] = Rest[i] * norm;
	}
	return;
}



// The following calculates a whitening transformation R^(-1/2) of the data
// It is a brute force calculation based on Eigen decomposition of the 2 by 2 covariance
//int CalData::whitenTransformEigen(float R[][4], float transform[][4], int numradars)
int CalData::whitenTransformEigen()
{
	float det, Tr, temp, ev1, ev2;
	float v11, v12, v21, v22;
	float A, B;
	float a, b, c;
	float ev11, ev22;

	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++)
	{
		a = R[rindex].value[0];
		b = R[rindex].value[1];
		c = R[rindex].value[3];

		det = a * c - b * b;	// Determinant
		Tr = a + c;					// Trace
		temp = 0.25f*Tr*Tr - det;
		if ((temp >= 0) && (det > 0.0)) { // Pos definite, non-singular, so go ahead with calculation
			// Check for b=0
			if (b == 0.0f) {
				ev1 = a;
				ev2 = c;
				v11 = 1.0f; v12 = 0.0f;
				v21 = 0.0f; v22 = 1.0f;
				CalXform[rindex].value[1] = 0.0f;
				CalXform[rindex].value[2] = 0.0f;
				CalXform[rindex].value[3] = sqrtf(a / c);  // Since positive definite check already done, c is not 0
				CalXform[rindex].value[0] = 1.0f;
			}
			else {
				// The following would simplify to a if b=0 
				ev1 = Tr / 2 + sqrtf(temp);	// Eigenvalues
				ev2 = Tr / 2 - sqrtf(temp);

				v11 = b / sqrtf(b * b + (ev1 - a)*(ev1 - a));			// Unit 2-norm eigenvectors
				v12 = (ev1 - a) / sqrtf(b * b + (ev1 - a)*(ev1 - a));
				v21 = b / sqrtf(b * b + (ev2 - a)*(ev2 - a));
				v22 = (ev2 - a) / sqrtf(b * b + (ev2 - a)*(ev2 - a));

				if (fabs(v11*a + v12 * b) > fabs(v11*b + v12 * c)) {
					ev11 = (v11*a + v12 * b) / v11;
				}
				else {
					ev11 = (v11*b + v12 * c) / v12;
				}
				if (fabs(v21*a + v22 * b) > fabs(v21*b + v22 * c)) {
					ev22 = (v21*a + v22 * b) / v21;
				}
				else {
					ev22 = (v21*b + v22 * c) / v22;
				}
				if ((ev22 <= 0) || (ev11 <= 0)) {
					// Should never get here, but check for validity anyway
					log_message("Huh? Eigenvalues are <=0. Error in whitenTransformEigen"); // Should do something to recover.
				}
				A = 1.0f; // / sqrtf(ev1);
				B = sqrtf(ev11 / ev22);

				CalXform[rindex].value[1] = (A*v11*v12 + B * v21*v22);
				CalXform[rindex].value[2] = CalXform[rindex].value[1];
				CalXform[rindex].value[3] = (A*v12*v12 + B * v22*v22);
				CalXform[rindex].value[0] = A * v11*v11 + B * v21*v21;
			}
			/*
			fprintf(fpDebugFile, "\nEV,  = \n[%13.9f %13.9f ; %f %13.9f]\n[%f %f ; %f %f]\n[%f %f ; %f %f]",
				R[rindex][0], R[rindex][1], R[rindex][2], R[rindex][3], v11, v12,  v21, v22,  ev1, ev2, ev11, ev22);
				*/
		}
		else {
			log_message("Warning: Problem!!! non positive definite covariance in whitenTransformEigen cal routine");
			return(1);
		}
	}
	return(0);

}

// This routine generates the upper triangular portion of the cholesky factor and 
// the scaled inverse for use as a whitening transform.  THe scaling maintains a gain
// of 1 as the original real component is transformed into the whitened output.
//whitenTransformCholesky(&Rhist[0], &CalXform[0], NumSensorsSet);
int CalData::whitenTransformCholesky()
{
	//float det, ev1, ev2;
	//float v11, v12, v21, v22;
	float det;

	float a, b, c;

	float L11, L12, L21, L22;
	float A11, A12, A21, A22;

	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++)
	{
		a = Rhist[rindex].value[0];
		b = Rhist[rindex].value[1];
		c = Rhist[rindex].value[3];

		det = a * c - b * b;	// Determinant

		if ((det > 0.0) && (a > 0.0f) && (c > 0.0f)) { // Verify that it is positive definite matrix
			// Check for b=0
			if (b == 0.0f) {  // This is the same for Cholesky or for Eigen
				CalXform[rindex].value[1] = 0.0f;
				CalXform[rindex].value[2] = 0.0f;
				CalXform[rindex].value[3] = sqrtf(a / c);  // Since positive definite check already done, c is not 0
				CalXform[rindex].value[0] = 1.0f;
			}
			else {
				// Form the cholesky factor 
				L11 = sqrtf(a);
				L12 = b / L11;
				L21 = 0.0f;
				L22 = sqrtf(c - L12 * L12);


				// Then the inverse of the Cholesky factor.  
				A11 = 1.0f / L11;
				A12 = -L12 / (L11*L22);
				A21 = 0.0f;
				A22 = 1.0f / L22;

				// Normalize gain on real channel.
				CalXform[rindex].value[1] = 0.0f;
				CalXform[rindex].value[2] = A12 / A11;
				CalXform[rindex].value[3] = A22 / A11;  // Since positive definite check already done, c is not 0
				CalXform[rindex].value[0] = 1.0f;
			}

		}
		else {
			log_message("Warning: Problem!!! Non positive definite covariance in whitenTransformCholesky cal routine");
			return(1);
		}
	}
	return(0);

}


void CalData::FadeMemAveDCSingle()
{
	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++) {
		DCOffsetHist[rindex] = FadeMemVal * DCOffsetHist[rindex] + (1.0f - FadeMemVal)* DCOffset[rindex];
		//HistData[rindex] = RTPComplex(fade * HistData[rindex].real() + (1.0f - fade)* NewData[rindex].real(),
		//	fade * HistData[rindex].imag() + (1.0f - fade)* NewData[rindex].imag());
	}

}

void CalData::DCAveSingle()
{
	// This is method 2 using an average of the last Nhist points. It seems to work better in simulation
	// I haven't tested this since moving it to this member function. 
	if (this->CalCount < Nhist)
		for ( int rindex = 0; rindex < NumSensorsSet; rindex++) {
			DCOffsetHistN[this->CalCount*NumSensorsSet + rindex] = DCOffset[rindex];
		}
	else
	{
		for (int index = 0; index < NumSensorsSet*(Nhist - 1); index++) { // Move up history
			DCOffsetHistN[index] = DCOffsetHistN[index + NumSensorsSet];
		}
		for ( int rindex = 0; rindex < NumSensorsSet; rindex++) {	// Insert last measurement into history
			DCOffsetHistN[(Nhist - 1)*NumSensorsSet + rindex] = DCOffset[rindex];
		}
	}

	int curHist = (this->CalCount >= Nhist) ? Nhist : this->CalCount + 1;
	std::vector<RTPComplex> DCOffsetAve(NumSensorsSet);
	for (int rindex = 0; rindex < NumSensorsSet; rindex++) {	// Initialize to zero
		DCOffsetAve[rindex] = RTPComplex(0.0f, 0.0f);
	}

	for (int index = 0; index < curHist; index++) {
		for ( int rindex = 0; rindex < NumSensorsSet; rindex++) {	// Average over the number of points
			DCOffsetAve[rindex] += DCOffsetHistN[index*NumSensorsSet + rindex];
		}
	}

	for ( int rindex = 0; rindex < NumSensorsSet; rindex++) {	// Scale it properly
		DCOffsetAve[rindex] *= RTPComplex(1.0f / (float)(curHist + 1), 0.0f);
	}

}

void CalData::FadeMemAveDCWRI()
{
	for (unsigned int rindex = 0; rindex < Params.NumSensorsSet; rindex++)
		for (unsigned int indx = 0; indx < Params.Samp_Per_WRI; indx++) {
			DCOffsetWRIHist[rindex][indx] = FadeMemVal * DCOffsetWRIHist[rindex][indx] + (1.0f - FadeMemVal)* DCOffsetWRI[rindex][indx];
			//DCOffsetHist[rindex] = RTPComplex(FadeMemVal * DCOffsetHist[rindex].real() + (1.0f - FadeMemVal)* NewData[rindex].real(),
			//	FadeMemVal * HistData[rindex].imag() + (1.0f - FadeMemVal)* NewData[rindex].imag());
		}

}

void CalData::CalibrateFunction(int bob)
// This thread calculates the calibration of the radars in the background
// It is kicked off with a new data set every so often.  The statistics from the
// new data set are fading memory averaged with the statistics from previous data
// sets prior to calculating the cal coefficients
{
	int index, rindex;
	index = bob; // To do something
	// Copy some values, just to save typing
	int NumSensorsSet = this->Params.NumSensorsSet;
	int NWRI = Params.Num_WRI;
	int NPerWRI = this->Params.Samp_Per_WRI;
	bool DCOnly = this->CalDCOnly;
	//int count = 1;
	this->CalCount = 0;
	const int Nhist = 50; // Number of points to save history
	float fadeVal = this->FadeMemVal;

	bool initHist = false;	// Whether the history has been initialized

	log_message("Calibration thread is started.");
	
	// Initialize the historical values using what was passed into the function for the DC Offset
	if ((!initHist)) {
		log_message("Initialize Calibration History.");
		for (rindex = 0; rindex < NumSensorsSet; rindex++) {
			DCOffsetHist[rindex] = this->DCOffset[rindex];  // Initialize to the saved means
		// The following is useful for testing - this will ignore the DCoffset value from the ini file
		//	DCOffsetHist[rindex] = RTPComplex(0.0f, 0.0f);  // Zero the history.
		// The following initialize the covariance history to a very small value so it is ignored
			Rhist[rindex].value[0] = 1.0e-12f;
			Rhist[rindex].value[1] = 0.0f;
			Rhist[rindex].value[2] = 0.0f;
			Rhist[rindex].value[3] = 1.0e-12f;
			R[rindex] = Rhist[rindex];
		}

		// This currently resets the calculation of the covariance matrix for the WRI based approach. No historical value is passed in.

		// This assumes only a single covariance history value per sensor. Otherwise, use rindex < NumSensorsSet*Nhist
		for (int rindex = 0; rindex < NumSensorsSet; rindex++)
			for (int index = 0; index < NPerWRI; index++) {
				DCOffsetWRI[rindex][index] = RTPComplex(0.0f, 0.0f);
				DCOffsetWRIHist[rindex][index] = RTPComplex(0.0f, 0.0f);
			}

		initHist = true;
	}

	while (TRUE)
	{
		// Grab the calibration data buffer
		std::unique_lock<std::mutex> bufferlock(this->OwnBuffers);
		// Wait for results to be ready or for request that this function be stopped.		
		while (!this->InBufferFull && !this->StopRequested) {
			/*std::cv_status cvstat =*/ this->DataHere.wait_for(bufferlock, std::chrono::milliseconds(100));
		}
		if (this->StopRequested == TRUE) break; // stop flag
		this->InBufferFull = FALSE;

		//pData = (RTPComplex**)this->pCalData;  // Point to the current calibration data

		if (DCOnly) {

			this->complexMean(); // calculate mean and store in local DCOffset variable

			// The following is the single DC offset for all ranges approach to calibration
			if (!gRadarConfig.ReceiveRealOnly) {

				//	std::cout << "DC Off: " << DCOffset[0] << ", Hist: " << DCOffsetHist[0] << std::endl;
				// This is method 1 for determining DC offset with a fading memory
				FadeMemAveDCSingle();


				covarianceSingleMean();
				if (this->CalCount == 0) Rhist = R; // If the first time through, then initialize the fading memory history to the current estimate

				if (this->CalCount < 20) { // Make the filter very responsive at startup
					float fval = 0.5;
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {
						Rhist[rindex].value[0] = fval * Rhist[rindex].value[0] + (1.0f - fval)*R[rindex].value[0];
						Rhist[rindex].value[3] = fval * Rhist[rindex].value[3] + (1.0f - fval)*R[rindex].value[3];
						Rhist[rindex].value[1] = fval * Rhist[rindex].value[1] + (1.0f - fval)*R[rindex].value[1];
						Rhist[rindex].value[2] = Rhist[rindex].value[1];
					}
				}

				else

				{
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {
						Rhist[rindex].value[0] = fadeVal * Rhist[rindex].value[0] + (1.0f - fadeVal)*R[rindex].value[0];
						Rhist[rindex].value[3] = fadeVal * Rhist[rindex].value[3] + (1.0f - fadeVal)*R[rindex].value[3];
						Rhist[rindex].value[1] = fadeVal * Rhist[rindex].value[1] + (1.0f - fadeVal)*R[rindex].value[1];
						Rhist[rindex].value[2] = Rhist[rindex].value[1];
					}
				}
				//std::cout << "Cov: " << R[0].value[0] << "," << R[0].value[1] << "," << R[0].value[3]  <<
				//	", Hist: " << Rhist[0].value[0] << "," << Rhist[0].value[1] << "," << Rhist[0].value[3]  << std::endl;

				//whitenTransformEigen();
				int stat = whitenTransformCholesky();
				if (stat == 0) {
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {

						this->CalXform[rindex] = CalXform[rindex];
						this->DCOffset[rindex] = DCOffsetHist[rindex];
					}
				}
				else log_message("Calibration error: not updating coefficients this cycle");

			}
			else {
				for (rindex = 0; rindex < NumSensorsSet; rindex++) {
					this->CalXform[rindex].value[0] = 1.0; // Gain and correlation default cal constants;
					this->CalXform[rindex].value[1] = 0.0;
					this->CalXform[rindex].value[2] = 0.0;
					this->CalXform[rindex].value[3] = 1.0;
					this->DCOffset[rindex] = DCOffsetHist[rindex];
				}
			}
		}
		else {
			// This will do a DC offset per range bin. Needed when there is large mutual coupling between transmit and receive

			complexMeanColumns3D(); // calculate mean and store in local DCOffset variable
			/* if ( (calcount % 5 == 0)) {
					log_message("Using per sample DC offset");
					std::cout << "Cal Count: " << calcount << " DC_OffsetArr = ";
					for (int nnn = 0; nnn < NPerWRI; nnn++)
						std::cout << DCOffsetWRI[nnn];
					std::cout << std::endl;
				}
				*/
			if (CalCount == 0) { // Do a brute force initialization of DC Offset history
				for (int index = 0; index < NumSensorsSet*NPerWRI; index++) {
					DCOffsetWRIHist[index] = DCOffsetWRI[index];
				}
			}
			if (!gRadarConfig.ReceiveRealOnly) {


				FadeMemAveDCWRI();


				covarianceRangeDepMean();

				if (CalCount == 0) { // Do a brute force initialization first time through
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {
						/*Rhist[rindex][0] = R[rindex][0];
						Rhist[rindex][3] = R[rindex][3];
						Rhist[rindex][1] = R[rindex][1];
						Rhist[rindex][2] = Rhist[rindex][1];*/
						Rhist[rindex] = R[rindex];
					}
				}
				else {
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {
						Rhist[rindex].value[0] = fadeVal * Rhist[rindex].value[0] + (1.0f - fadeVal)*R[rindex].value[0];
						Rhist[rindex].value[3] = fadeVal * Rhist[rindex].value[3] + (1.0f - fadeVal)*R[rindex].value[3];
						Rhist[rindex].value[1] = fadeVal * Rhist[rindex].value[1] + (1.0f - fadeVal)*R[rindex].value[1];
						Rhist[rindex].value[2] = Rhist[rindex].value[1];
					}
				}

				int stat = whitenTransformCholesky();
				if (stat == 0) {
					std::vector<RTPComplex> temp_mean(NumSensorsSet);
					for (index = 0; index < NumSensorsSet; index++)
						temp_mean[index] = RTPComplex(0.0f, 0.0f);
					for (rindex = 0; rindex < NumSensorsSet; rindex++) {
						this->CalXform[rindex] = CalXform[rindex];
						for (int samp_index = 0; samp_index < NPerWRI; samp_index++) {
							this->pOffsetsdat[rindex][samp_index] = DCOffsetWRIHist[rindex][samp_index];
							temp_mean[rindex] += DCOffsetWRIHist[rindex][samp_index];
						}
						this->DCOffset[rindex] = temp_mean[rindex] * (1.0f / ((float)NPerWRI));  // Calculate mean
					}
				}
				std::cout << "Cov: " << R[0].value[0] << "," << R[0].value[1] << "," << R[0].value[3] <<
					", Hist: " << Rhist[0].value[0] << "," << Rhist[0].value[1] << "," << Rhist[0].value[3] <<
					"\n  DC vals:" << DCOffsetWRI[0] << DCOffsetWRIHist[0] << ", " << this->DCOffset[0] << std::endl;

			}
			else {
				for (rindex = 0; rindex < NumSensorsSet; rindex++) {
					this->CalXform[rindex].value[0] = 1.0; // Gain and correlation default cal constants;
					this->CalXform[rindex].value[1] = 0.0;
					this->CalXform[rindex].value[2] = 0.0;
					this->CalXform[rindex].value[3] = 1.0;
					this->DCOffset[rindex] = DCOffsetHist[rindex];
				}
			}
		}
		this->CalCount++;
		//= calcount;
		// Notify calling thread that results are ready
		this->Cal_ready = TRUE;
		// pRadarCalData->Cal_ready = FALSE;
		bufferlock.unlock();
		this->DataHere.notify_one();
	}

	// Drops through to here when signaled to stop

	std::ostringstream msg;
	msg << "Calibration thread is exiting.\n                    Number of blocks processed is " << this->CalCount << "." << std::endl;
	for (rindex = 0; rindex < NumSensorsSet; rindex++) {
		msg << "                    Radar " << rindex << ", DC: " << this->DCOffset[rindex] << ", XForm: "
			<< this->CalXform[rindex].value[0] << ", "
			<< this->CalXform[rindex].value[1] << ", "
			<< this->CalXform[rindex].value[2] << ", "
			<< this->CalXform[rindex].value[3] << std::endl;
	}
	log_message(msg.str());
	return;
}


int CalData::initializeCal(CPI_Params InParams)

{
	unsigned int rindex;
	unsigned int NumSensorsSet = InParams.NumSensorsSet;
	this->Params = InParams;

	this->pCalData.resize(NumSensorsSet);		// Working data provided to cal worker thread.  Can be changed by worker
	this->pOffsetsdat.resize(NumSensorsSet);	// DC offset that varies with range sample

	/* Allocate memory arrays for radar raw data storage */
	for (rindex = 0; rindex < NumSensorsSet; rindex++) {
		

		this->pCalData[rindex] = (RTPComplex*)
			fftwf_malloc(sizeof(RTPComplex) * Params.Samp_Per_WRI*Params.Num_WRI);
		this->pOffsetsdat[rindex] = (RTPComplex*)
			malloc(sizeof(fftwf_complex) * Params.Samp_Per_WRI);
		if ((this->pCalData[rindex] == NULL) || (this->pOffsetsdat[rindex] == NULL)) {
			log_message("Error %d: Can't allocate memory for data in processing thread, exiting", GetLastError());
			exit(1L);
		}
	}
	// Intitialize default calibration variables
	for (rindex = 0; rindex < NumSensorsSet; rindex++) {
		this->DCOffset[rindex] = RTPComplex((float) gRadarConfig.CalDCVal[2 * rindex], (float) gRadarConfig.CalDCVal[2 * rindex + 1]);
		this->CalXform[rindex].value[0] = 1.0f; // Gain and correlation default cal constants;
		this->CalXform[rindex].value[1] = 0.0f;
		this->CalXform[rindex].value[2] = 0.0f;
		this->CalXform[rindex].value[3] = 1.0f;
	}

	std::ostringstream msg;
	msg << "Calibration thread is starting. Initial values are:" << std::endl;
	for (rindex = 0; rindex < NumSensorsSet; rindex++) {
		msg << "                    Radar " << rindex << ", DC: " << this->DCOffset[rindex] << ", XForm: "
			<< this->CalXform[rindex].value[0] << ", "
			<< this->CalXform[rindex].value[1] << ", "
			<< this->CalXform[rindex].value[2] << ", "
			<< this->CalXform[rindex].value[3] << std::endl;
	}
	log_message(msg.str());


	this->CalDCOnly = gRadarConfig.DC_CalOnly;
	this->FadeMemVal = (float)gRadarConfig.FadeMemVal;
	this->Cal_ready = FALSE;
	this->StopRequested = FALSE;

	return 0;
}

int cleanupCalThread(pCalData pRadarCalData, RTPComplex *DCOffsetVals[], RTPComplex DCOffset[])
{
	int rindex;
	int NumSensorsSet;
	NumSensorsSet = pRadarCalData->Params.NumSensorsSet;
	for (rindex = 0; rindex < NumSensorsSet; rindex++) {
		//free(DCOffsetVals[rindex]); DCOffsetVals[rindex] = NULL;
		fftwf_free(pRadarCalData->pCalData[rindex]);  pRadarCalData->pCalData[rindex] = NULL;
		free(pRadarCalData->pOffsetsdat[rindex]); pRadarCalData->pOffsetsdat[rindex] = NULL;
	}
	return 0;
}
