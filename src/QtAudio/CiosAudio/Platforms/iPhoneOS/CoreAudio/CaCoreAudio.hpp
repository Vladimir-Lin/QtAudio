#ifndef CACOREAUDIO_HPP
#define CACOREAUDIO_HPP

#include "CiosAudioPrivate.hpp"

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

class CoreAudioStream : public Stream
{
  public:

    explicit        CoreAudioStream (void) ;
    virtual        ~CoreAudioStream (void) ;

    virtual CaError Start           (void) ;
    virtual CaError Stop            (void) ;
    virtual CaError Close           (void) ;
    virtual CaError Abort           (void) ;
    virtual CaError IsStopped       (void) ;
    virtual CaError IsActive        (void) ;
    virtual CaTime  GetTime         (void) ;
    virtual double  GetCpuLoad      (void) ;
    virtual CaInt32 ReadAvailable   (void) ;
    virtual CaInt32 WriteAvailable  (void) ;
    virtual CaError Read            (void       * buffer,unsigned long frames) ;
    virtual CaError Write           (const void * buffer,unsigned long frames) ;

  protected:

  private:

};

class CoreAudioHostApi : public HostApi
{
  public:

    explicit  CoreAudioHostApi   (void) ;
    virtual  ~CoreAudioHostApi   (void) ;

    virtual CaError  Open        (Stream                ** stream            ,
                                  const StreamParameters * inputParameters   ,
                                  const StreamParameters * outputParameters  ,
                                  double                   sampleRate        ,
                                  CaUint32                 framesPerCallback ,
                                  CaStreamFlags            streamFlags       ,
                                  Conduit                * conduit         ) ;
    virtual void     Terminate   (void) ;
    virtual CaError  isSupported (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double sampleRate                        ) ;
    virtual bool     hasMixer    (void) ;
    virtual CaUint32 MinVolume   (void) ;
    virtual CaUint32 MaxVolume   (void) ;
    virtual CaUint32 Volume      (void) ;
    virtual CaUint32 setVolume   (CaUint32 volume) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError CoreAudioInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CACOREAUDIO_HPP
