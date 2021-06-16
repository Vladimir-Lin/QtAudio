#ifndef MTCAAUDIO_HPP
#define MTCAAUDIO_HPP

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CaDll __declspec(dllexport)

CaDll int       __stdcall CaDevices     (void) ;
CaDll int       __stdcall CaSetDevice   (int deviceId) ;
CaDll int       __stdcall CaGetDeviceId (void) ;
CaDll wchar_t * __stdcall CaDeviceName  (int deviceId) ;
CaDll bool      __stdcall CaPlay        (wchar_t * filename) ;

#include "CiosAudio.hpp"

class MtAudio : public CiosAudio::Thread
{
  public:

    explicit  MtAudio     (void) ;
    virtual  ~MtAudio     (void) ;

    bool      Loaded      (void) ;
    bool      Unload      (void) ;

    int       Devices     (void) ;
    int       setDevice   (int deviceId) ;
    int       getDeviceId (void) const ;
    wchar_t * DeviceName  (int deviceId) ;

    bool      Play        (wchar_t * filename) ;
    bool      PlayAudio   (char * filename) ;

  protected:

    int       Entries  ;
    int       DeviceId ;
    wchar_t * wMessage ;
    char    * Message  ;

    virtual void run      (int Type,CiosAudio::ThreadData * data) ;

    bool   toString       (wchar_t * s  ,char    * utf,int len ) ;
    bool   toWString      (char    * utf,wchar_t * w  ,int wlen) ;

  private:

};

#endif // MTCAAUDIO_HPP
