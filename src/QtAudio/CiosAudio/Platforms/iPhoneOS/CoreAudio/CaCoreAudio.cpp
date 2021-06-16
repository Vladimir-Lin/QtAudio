#include "CaCoreAudio.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CiosAudio
{
#endif

///////////////////////////////////////////////////////////////////////////////

CaError CoreAudioInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  return 0 ;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioStream:: CoreAudioStream (void)
                : Stream          (    )
{
}

CoreAudioStream::~CoreAudioStream(void)
{
}

CaError CoreAudioStream::Start(void)
{
  return 0 ;
}

CaError CoreAudioStream::Stop(void)
{
  return 0 ;
}

CaError CoreAudioStream::Close(void)
{
  return 0 ;
}

CaError CoreAudioStream::Abort(void)
{
  return 0 ;
}

CaError CoreAudioStream::IsStopped(void)
{
  return 0 ;
}

CaError CoreAudioStream::IsActive(void)
{
  return 0 ;
}

CaTime CoreAudioStream::GetTime(void)
{
  return 0 ;
}

double CoreAudioStream::GetCpuLoad(void)
{
  return 0 ;
}

CaInt32 CoreAudioStream::ReadAvailable(void)
{
  return 0 ;
}

CaInt32 CoreAudioStream::WriteAvailable(void)
{
  return 0 ;
}

CaError CoreAudioStream::Read(void * buffer,unsigned long frames)
{
  return 0 ;
}

CaError CoreAudioStream::Write(const void * buffer,unsigned long frames)
{
  return 0 ;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioHostApi:: CoreAudioHostApi (void)
                 : HostApi          (    )
{
}

CoreAudioHostApi::~CoreAudioHostApi(void)
{
}

CaError CoreAudioHostApi::Open                       (
          Stream                ** stream            ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  return 0 ;
}

void CoreAudioHostApi::Terminate(void)
{
}

CaError CoreAudioHostApi::isSupported                (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  return 0 ;
}

bool CoreAudioHostApi::hasMixer(void)
{
  return true ;
}

CaUint32 CoreAudioHostApi::MinVolume(void)
{
  return 0 ;
}

CaUint32 CoreAudioHostApi::MaxVolume(void)
{
  return 0 ;
}

CaUint32 CoreAudioHostApi::Volume(void)
{
  return 0 ;
}

CaUint32 CoreAudioHostApi::setVolume(CaUint32 volume)
{
  return 0 ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
