#property copyright   "2011-2015, Neutrino International Inc."

#include <CaAudio.mqh>

extern string music         ;
extern int    DeviceId = -1 ;

void init()
{
  int    devices                      ;
  int    i                            ;
  string name                         ;
  string m                            ;
  /////////////////////////////////////
  devices = CaDevices ( )             ;
  Print( "Audio devices " + devices ) ;
  /////////////////////////////////////
  if ( devices > 0 )                  {
    for (i=0;i<devices;i++)           {
      name = CaDeviceName(i)          ;
      m    = i                        ;
      m   += " : "                    ;
      m   += name                     ;
      Print ( m )                     ;
    }                                 ;
  }                                   ;
  /////////////////////////////////////
  if ( DeviceId >= 0 )                {
    CaSetDevice ( DeviceId )          ;
  }                                   ;
  /////////////////////////////////////
  if (StringLen(music)>0)             {
    m  = "Play : "                    ;
    m += music                        ;
    Print  ( m     )                  ;
    CaPlay ( music )                  ;
  }                                   ;
}

void deinit()
{
}

void start()
{
}
