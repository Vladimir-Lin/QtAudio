#include "CaAudio.hpp"

MtAudio * MA = NULL ;
wchar_t   EMPTY[2] = {0,0} ;

BOOL APIENTRY DllMain( HMODULE hModule    ,
                       DWORD   reason     ,
                       LPVOID  lpReserved )
{
  switch ( reason )                          {
    case DLL_PROCESS_ATTACH                  :
      if ( NULL == MA ) MA = new MtAudio ( ) ;
      MA->Loaded ( )                         ;
    break                                    ;
    case DLL_PROCESS_DETACH                  :
      if (MA->Unload())                      {
        delete MA                            ;
        MA = NULL                            ;
      }                                      ;
    break                                    ;
    case DLL_THREAD_ATTACH                   :
    break                                    ;
    case DLL_THREAD_DETACH                   :
    break                                    ;
  }                                          ;
  return TRUE                                ;
}

CaDll int __stdcall CaDevices (void)
{
  if ( NULL == MA ) return 0 ;
  return MA -> Devices ( )   ;
}

CaDll int __stdcall CaSetDevice (int deviceId)
{
  if ( NULL == MA ) return -1         ;
  return MA -> setDevice ( deviceId ) ;
}

CaDll int __stdcall CaGetDeviceId (void)
{
  if ( NULL == MA ) return -1  ;
  return MA -> getDeviceId ( ) ;
}

CaDll wchar_t * __stdcall CaDeviceName (int deviceId)
{
  if ( NULL == MA ) return EMPTY       ;
  return MA -> DeviceName ( deviceId ) ;
}

CaDll bool __stdcall CaPlay (wchar_t * filename)
{
  if ( NULL == MA ) return false ;
  return MA -> Play ( filename ) ;
}
