#include "CaAudio.hpp"

MtAudio:: MtAudio           (void)
        : CiosAudio::Thread (    )
        , Entries           (0   )
        , DeviceId          (-1  )
{
  wMessage = new wchar_t [8192] ;
   Message = new char    [8192] ;
  wMessage [ 0 ] = 0            ;
   Message [ 0 ] = 0            ;
}

MtAudio::~MtAudio (void)
{
  delete [] wMessage ;
  delete []  Message ;
  wMessage = NULL    ;
   Message = NULL    ;
}

bool MtAudio::Loaded(void)
{
  Entries++ ;
  return ( Entries > 0 ) ;
}

bool MtAudio::Unload(void)
{
  Entries-- ;
  return ( Entries <= 0 ) ;
}

int MtAudio::Devices(void)
{
  int devices = 0 ;
  CiosAudio::Core core             ;
  core . Initialize ( )            ;
  //////////////////////////////////
  devices = core . DeviceCount ( ) ;
  //////////////////////////////////
  core . Terminate  ( )            ;
  return devices                   ;
}

int MtAudio::setDevice(int deviceId)
{
  DeviceId = deviceId ;
  return DeviceId     ;
}

int MtAudio::getDeviceId(void) const
{
  return DeviceId ;
}

wchar_t * MtAudio::DeviceName(int deviceId)
{
  CiosAudio::Core         core                                 ;
  CiosAudio::DeviceInfo * dev                                  ;
  core . Initialize ( )                                        ;
  wMessage [ 0 ] = 0                                           ;
  dev = core.GetDeviceInfo((CiosAudio::CaDeviceIndex)deviceId) ;
  if ( NULL != dev && dev->maxOutputChannels>0)                {
    if ( NULL != dev->name )                                   {
      ::strcpy  ( Message , dev->name )                        ;
      if ( ::strlen(Message) > 0 )                             {
        toWString ( Message , wMessage , 8190 )                ;
      }                                                        ;
    }                                                          ;
  }                                                            ;
  core . Terminate ( )                                         ;
  return wMessage                                              ;
}

bool MtAudio::toString(wchar_t * s,char * u,int len)
{
  DWORD  dwNum                             ;
  dwNum = ::WideCharToMultiByte ( CP_OEMCP ,
                                  NULL     ,
                                  s        ,
                                  -1       ,
                                  NULL     ,
                                  0        ,
                                  NULL     ,
                                  FALSE  ) ;
  ::WideCharToMultiByte ( CP_OEMCP         ,
                          NULL             ,
                          s                ,
                          -1               ,
                          u                ,
                          dwNum            ,
                          NULL             ,
                          FALSE          ) ;
  return true                              ;
}

bool MtAudio::toWString(char * u,wchar_t * w,int wlen)
{
  int len                                         ;
  len = ::MultiByteToWideChar ( CP_ACP            ,
                                0                 ,
                                (LPCSTR)u         ,
                                -1                ,
                                NULL              ,
                                0               ) ;
  if (len<=0)                                     {
    w [ 0 ] = 0                                   ;
    return true                                   ;
  }                                               ;
  ::memset ( w , 0 , wlen * sizeof(wchar_t) )     ;
  ::MultiByteToWideChar ( CP_ACP                  ,
                          0                       ,
                          (LPCSTR)u               ,
                          -1                      ,
                          (LPWSTR)w               ,
                          len                   ) ;
  return true                                     ;
}

bool MtAudio::Play(wchar_t * filename)
{
  if ( NULL == filename ) return false     ;
  char * file = NULL                       ;
  Message [ 0 ] = 0                        ;
  toString(filename,Message,8190)          ;
  if ( strlen(Message) <= 0 ) return false ;
  file = ::strdup(Message)                 ;
  start ( 10001 , file )                   ;
  return true                              ;
}

bool MtAudio::PlayAudio(char * filename)
{
  return CiosAudio::MediaPlay(filename,DeviceId,-1,NULL) ;
}

void MtAudio::run(int Type,CiosAudio::ThreadData * data)
{
  switch ( Type )                    {
    case 10001                       :
      if ( NULL != data->Arguments ) {
        PlayAudio(data->Arguments)   ;
        delete [] data->Arguments    ;
        data->Arguments = NULL       ;
      }                              ;
    break                            ;
  }                                  ;
}
