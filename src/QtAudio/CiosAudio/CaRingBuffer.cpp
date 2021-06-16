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

#include "CiosAudioPrivate.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

RingBuffer:: RingBuffer(void)
{
  bufferSize       = 0    ;
  writeIndex       = 0    ;
  readIndex        = 0    ;
  bigMask          = 0    ;
  smallMask        = 0    ;
  elementSizeBytes = 0    ;
  buffer           = NULL ;
}

RingBuffer:: RingBuffer                 (
               CaInt32 elementSizeBytes ,
               CaInt32 elementCount     ,
               void  * dataPtr          )
{
  Initialize ( elementSizeBytes ,
               elementCount     ,
               dataPtr        ) ;
}

RingBuffer::~RingBuffer(void)
{
}

CaInt32 RingBuffer::Initialize     (
          CaInt32 elementBytes     ,
          CaInt32 elementCount     ,
          void  * dataPtr          )
{
  if ( ( ( elementCount - 1 ) & elementCount ) != 0 ) return -1 ; /* Not Power of two. */
  bufferSize       = elementCount                               ;
  buffer           = (char *)dataPtr                            ;
  writeIndex       = 0                                          ;
  readIndex        = 0                                          ;
  bigMask          = ( elementCount * 2 ) - 1                   ;
  smallMask        = ( elementCount       - 1 )                 ;
  elementSizeBytes = elementBytes                               ;
  return 0                                                      ;
}

CaInt32 RingBuffer::ReadAvailable(void)
{
  return ( ( writeIndex - readIndex ) & bigMask ) ;
}

CaInt32 RingBuffer::WriteAvailable(void)
{
  return ( bufferSize - ReadAvailable ( ) ) ;
}

void RingBuffer::Flush(void)
{
  writeIndex = 0 ;
  readIndex  = 0 ;
}

CaInt32 RingBuffer::WriteRegions  (
          CaInt32    elementCount ,
          void    ** dataPtr1     ,
          CaInt32  * sizePtr1     ,
          void    ** dataPtr2     ,
          CaInt32  * sizePtr2     )
{
  CaInt32 index                                            ;
  CaInt32 available = WriteAvailable ( )                   ;
  //////////////////////////////////////////////////////////
  if ( elementCount > available ) elementCount = available ;
  /* Check to see if write is not contiguous.             */
  index = writeIndex & smallMask                           ;
  if ( ( index + elementCount ) > bufferSize )             {
    /* Write data in two blocks that wrap the buffer.     */
    CaInt32 firstHalf = bufferSize - index                 ;
    *dataPtr1 = &buffer [ index * elementSizeBytes ]       ;
    *sizePtr1 = firstHalf                                  ;
    *dataPtr2 = &buffer [ 0                        ]       ;
    *sizePtr2 = elementCount - firstHalf                   ;
  } else                                                   {
    *dataPtr1 = &buffer [ index * elementSizeBytes ]       ;
    *sizePtr1 = elementCount                               ;
    *dataPtr2 = NULL                                       ;
    *sizePtr2 = 0                                          ;
  }                                                        ;
  /* (write-after-read) => full barrier                   */
  if ( 0 != available ) CaFullMemoryBarrier ( )            ;
  //////////////////////////////////////////////////////////
  return elementCount                                      ;
}

CaInt32 RingBuffer::AdvanceWriteIndex(CaInt32 elementCount)
{
  /* ensure that previous writes are seen before we update
   *  the write index (write after write)                        */
  CaWriteMemoryBarrier  (                                       ) ;
  return ( writeIndex = ( writeIndex + elementCount ) & bigMask ) ;
}

CaInt32 RingBuffer::ReadRegions   (
          CaInt32    elementCount ,
          void    ** dataPtr1     ,
          CaInt32  * sizePtr1     ,
          void    ** dataPtr2     ,
          CaInt32  * sizePtr2     )
{
  CaInt32 index                                            ;
  CaInt32 available = ReadAvailable ( )                    ; /* doesn't use memory barrier */
  //////////////////////////////////////////////////////////
  if ( elementCount > available ) elementCount = available ;
  /* Check to see if read is not contiguous.              */
  index = readIndex & smallMask                            ;
  if ( ( index + elementCount ) > bufferSize )             {
    /* Write data in two blocks that wrap the buffer.     */
    CaInt32 firstHalf = bufferSize - index                 ;
    *dataPtr1 = &buffer [ index * elementSizeBytes ]       ;
    *sizePtr1 = firstHalf                                  ;
    *dataPtr2 = &buffer [ 0                        ]       ;
    *sizePtr2 = elementCount - firstHalf                   ;
  } else                                                   {
    *dataPtr1 = &buffer [ index * elementSizeBytes ]       ;
    *sizePtr1 = elementCount                               ;
    *dataPtr2 = NULL                                       ;
    *sizePtr2 = 0                                          ;
  }                                                        ;
  /* (read-after-read) => read barrier                    */
  if ( 0 != available ) CaReadMemoryBarrier ( )            ;
  //////////////////////////////////////////////////////////
  return elementCount                                      ;
}

CaInt32 RingBuffer::AdvanceReadIndex(CaInt32 elementCount)
{
  /* ensure that previous reads (copies out of the ring buffer)
   * are always completed before updating (writing) the read index.
   * (write-after-read) => full barrier  */
  CaFullMemoryBarrier  (                                      ) ;
  return ( readIndex = ( readIndex + elementCount ) & bigMask ) ;
}

CaInt32 RingBuffer::Write(const void * data,CaInt32 elementCount)
{
  CaInt32 size1      = 0                                              ;
  CaInt32 size2      = 0                                              ;
  CaInt32 numWritten = 0                                              ;
  void  * data1      = NULL                                           ;
  void  * data2      = NULL                                           ;
  /////////////////////////////////////////////////////////////////////
  numWritten = WriteRegions(elementCount,&data1,&size1,&data2,&size2) ;
  if ( size2 > 0 )                                                    {
    ::memcpy ( data1 , data , ( size1 * elementSizeBytes ) )          ;
    data =   ((char *)data) + ( size1 * elementSizeBytes )            ;
    ::memcpy ( data2 , data , ( size2 * elementSizeBytes ) )          ;
  } else                                                              {
    ::memcpy ( data1 , data , ( size1 * elementSizeBytes ) )          ;
  }                                                                   ;
  /////////////////////////////////////////////////////////////////////
  AdvanceWriteIndex ( numWritten )                                    ;
  /////////////////////////////////////////////////////////////////////
  return numWritten                                                   ;
}

CaInt32 RingBuffer::Read(void * data,CaInt32 elementCount)
{
  CaInt32 size1   = 0                                             ;
  CaInt32 size2   = 0                                             ;
  CaInt32 numRead = 0                                             ;
  void  * data1   = NULL                                          ;
  void  * data2   = NULL                                          ;
  /////////////////////////////////////////////////////////////////
  numRead = ReadRegions(elementCount,&data1,&size1,&data2,&size2) ;
  if ( size2 > 0 )                                                {
    ::memcpy ( data , data1 , ( size1 * elementSizeBytes ) )      ;
    data   = ((char *)data) + ( size1 * elementSizeBytes )        ;
    ::memcpy ( data , data2 , ( size2 * elementSizeBytes ) )      ;
  } else                                                          {
    ::memcpy ( data , data1 , ( size1 * elementSizeBytes ) )      ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  AdvanceReadIndex ( numRead )                                    ;
  /////////////////////////////////////////////////////////////////
  return numRead                                                  ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
