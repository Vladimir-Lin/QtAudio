#property copyright "Copyright 2011-2015 , Neutrino International Inc."

#import "CaAudio.dll"

int    CaDevices     (void) ;
int    CaSetDevice   (int deviceId) ;
int    CaGetDeviceId (void) ;
string CaDeviceName  (int deviceId) ;
bool   CaPlay        (string filename) ;

#import
