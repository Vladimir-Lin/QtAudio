/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/18                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CAJACK_HPP
#define CAJACK_HPP

#include "CaLinux.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>  /* EBUSY */
#include <signal.h> /* sig_atomic_t */
#include <math.h>
#include <semaphore.h>

#include <jack/types.h>
#include <jack/jack.h>

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

///////////////////////////////////////////////////////////////////////////////

class JackHostApi ;

class JackStream : public Stream
{
  public:

    BufferProcessor       bufferProcessor          ;
    CpuLoad               cpuLoadMeasurer          ;
    JackHostApi        *  hostApi                  ;
    jack_port_t        ** local_input_ports        ;
    jack_port_t        ** local_output_ports       ;
    jack_port_t        ** remote_input_ports       ;
    jack_port_t        ** remote_output_ports      ;
    int                   num_incoming_connections ;
    int                   num_outgoing_connections ;
    jack_client_t      *  jack_client              ;
    volatile sig_atomic_t is_running               ;
    volatile sig_atomic_t is_active                ;
    volatile sig_atomic_t doStart                  ;
    volatile sig_atomic_t doStop                   ;
    volatile sig_atomic_t doAbort                  ;
    jack_nframes_t        t0                       ;
    AllocationGroup    *  stream_memory            ;
    int                   callbackResult           ;
    int                   isSilenced               ;
    int                   xrun                     ;
    int                   isBlockingStream         ;
    RingBuffer            inFIFO                   ;
    RingBuffer            outFIFO                  ;
    volatile sig_atomic_t data_available           ;
    sem_t                 data_semaphore           ;
    int                   bytesPerFrame            ;
    int                   samplesPerFrame          ;
    JackStream         *  next                     ;

    explicit         JackStream     (void) ;
    virtual         ~JackStream     (void) ;

    virtual bool     isRealTime     (void) ;

    virtual CaError  Start          (void) ;
    virtual CaError  Stop           (void) ;
    virtual CaError  Close          (void) ;
    virtual CaError  Abort          (void) ;
    virtual CaError  IsStopped      (void) ;
    virtual CaError  IsActive       (void) ;
    virtual CaTime   GetTime        (void) ;
    virtual double   GetCpuLoad     (void) ;
    virtual CaInt32  ReadAvailable  (void) ;
    virtual CaInt32  WriteAvailable (void) ;
    virtual CaError  Read           (void       * buffer,unsigned long frames) ;
    virtual CaError  Write          (const void * buffer,unsigned long frames) ;
    virtual bool     hasVolume       (void) ;
    virtual CaVolume MinVolume      (void) ;
    virtual CaVolume MaxVolume      (void) ;
    virtual CaVolume Volume         (int atChannel = -1) ;
    virtual CaVolume setVolume      (CaVolume volume,int atChannel = -1) ;

    virtual CaError  RealStop       (int abort) ;

    virtual CaError  BlockingBegin  (int minimum_buffer_size) ;
    virtual void     BlockingEnd    (void) ;
    virtual CaError  RealProcess    (jack_nframes_t frames) ;
    virtual CaError  RemoveStream   (void) ;
    virtual CaError  AddStream      (void) ;

    virtual CaError     BlockingReadStream  (void * data,unsigned long numFrames) ;
    virtual CaError     BlockingWriteStream (const void * data,unsigned long numFrames) ;
    virtual signed long BlockingGetStreamReadAvailable  (void) ;
    virtual signed long BlockingGetStreamWriteAvailable (void) ;
    virtual CaError     BlockingWaitEmpty   (void) ;
    virtual void        UpdateSampleRate    (double sampleRate) ;
    virtual void        CleanUpStream       (int terminateStreamRepresentation ,
                                             int terminateBufferProcessor    ) ;

  protected:

  private:

};

class JackDeviceInfo : public DeviceInfo
{
  public:

    explicit JackDeviceInfo (void) ;
    virtual ~JackDeviceInfo (void) ;

  protected:

  private:

};

class JackHostApi : public HostApi
{
  public:

    AllocationGroup     * deviceInfoMemory ;
    jack_client_t       * jack_client      ;
    int                   jack_buffer_size ;
    CaHostApiIndex        hostApiIndex     ;
    pthread_mutex_t       mtx              ;
    pthread_cond_t        cond             ;
    unsigned long         inputBase        ;
    unsigned long         outputBase       ;
    volatile int          xrun             ;
    volatile JackStream * toAdd            ;
    volatile JackStream * toRemove         ;
    struct   JackStream * processQueue     ;
    volatile sig_atomic_t jackIsDown       ;

    explicit  JackHostApi        (void) ;
    virtual  ~JackHostApi        (void) ;

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
    virtual Encoding encoding    (void) const ;
    virtual bool     hasDuplex   (void) const ;

    virtual CaError  Initialize  (JackStream * stream              ,
                                  int          numInputChannels    ,
                                  int          numOutputChannels ) ;
    virtual CaError  BuildDeviceList (void) ;
    virtual CaError  UpdateQueue     (void) ;
    virtual CaError  WaitCondition   (void) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError JackInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAJACK_HPP
