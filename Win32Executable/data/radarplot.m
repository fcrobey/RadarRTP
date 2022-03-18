% Plot a spectrogram of the Doppler shift observed with an RF motion sensor
% Written by F. Robey
%            4 Jan 2013
% This script was initially written in Octave and modified to run in matlab
% Octave specgram function was modified to return double sided spectrum
% for complex data.
% © 2022 Massachusetts Institute of Technology. 

% Distributed under GNU GPSv2. See License.txt for details.

% This program is free software; you can redistribute it and/or modify it 
% under the terms of the GNU General Public License as published by the 
% Free Software Foundation; either version 2 of the License, 
% or (at your option) any later version.

% This program is distributed in the hope that it will be useful, 
% but WITHOUT ANY WARRANTY; without even the implied warranty of 
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
% See License.txt and the GNU General Public License for more details.

Matlab=0;  % Run in matlab (=1), or Octave (=0)
numfreq=8192;
%[RawDat,sr]=wavread('c:\data\raw_radar1.wav');
[RawDat,sr]=wavread('c:\data\raw2017_10_25_02_20_32.wav');
nsamps=max(size(RawDat));
time=(1:nsamps)*1/sr;

% Plot time series
figure(1)
plot(time,RawDat(:,1),'b',time,RawDat(:,2),'r')
xlabel('Time (sec)')
ylabel('Amplitude')
legend('Real','Imag')

f=24100000000;
lambda=3e8/f


%%
% Do first order calibration of data - normalize energy in the I and Q
% First, calculate and subtract the DC offsets 
DC_off=sum(RawDat)/nsamps  % sums in columns so I&Q and different radars treated independently
RawDat1=RawDat - ones(nsamps,1)*DC_off;  % Subtract DC offset

% Calculate terms of the matrix transform that will make the resulting I and Q orthogonal for this data set
% Only a two by two- equation could be solved directly, but use lazy approach
% calculate correlation matrix
R=RawDat1' * RawDat1 /nsamps 
if(size(R,1)==4),
    R(1,3)=0;
    R(1,4)=0;
    R(2,3)=0;
    R(2,4)=0;
    R(3,1)=0;
    R(4,1)=0;
    R(3,2)=0;
    R(4,2)=0; 
end
[V,LAMBDA]=eig(R);
Rw=V*inv(sqrt(LAMBDA))* V';
Rw/Rw(1,1)
CalDat=RawDat1*Rw;   % Calibrate data
iqDat=CalDat(:,1) + 1j*CalDat(:,2);  % Now put into IQ format
if(size(R,1)==4),
    Rw/Rw(3,3)
    CalDat=RawDat1*Rw;   % Calibrate data
    iqDat2=CalDat(:,3) + 1j*CalDat(:,4);  % Now put into IQ format
else
    iqDat2=iqDat;
end
%%

if Matlab==1
    % calculate and plot spectrogram
    [S,F,T]=specgram(iqDat(:,1),numfreq,sr,numfreq,0.75*numfreq);
    [S1,F1,T1]=specgram(iqDat2(:,1),numfreq,sr,numfreq,0.75*numfreq);

    reord = fftshift(1:numfreq);
    S=S(reord,:);
    S1=S1(reord,:);
    F=sr*(1:numfreq)/numfreq-sr*floor(numfreq/2)/numfreq;
else
    [S,F,T]=cspecgram(iqDat(:,1),numfreq,sr);
    if(size(R,1)==4)
        S1=S;F1=F;T1=T;
    end
end
tm=max(T);
figure(3)
colormap('hot')  % My preferred colormap
imagesc(F*lambda/2,T,20*log10(abs(S)+1e-12)'-10)
axis([-5 5 0 tm])
caxis([30 80])
h=colorbar;
ylabel(h,'Power (dB)')
xlabel('Speed (m/s)')
ylabel('Time (s)')
return
figure(4)
colormap('hot')  % My preferred colormap
imagesc(F*lambda/2,T,20*log10(abs(S1)+1e-12)'-10)

axis([-5 5 0 tm])
caxis([30 80])
h=colorbar;
ylabel(h,'Power (dB)')
xlabel('Speed (m/s)')
ylabel('Time (s)')