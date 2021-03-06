/**
 *-----------------------------------------------------------------------------
 * Title      : Stream memory pool
 * ----------------------------------------------------------------------------
 * File       : Pool.cpp
 * Created    : 2016-09-16
 * ----------------------------------------------------------------------------
 * Description:
 * Stream memory pool
 *    The function calls in this are a mess! create buffer, allocate buffer, etc
 *    need to be reworked.
 * ----------------------------------------------------------------------------
 * This file is part of the rogue software platform. It is subject to 
 * the license terms in the LICENSE.txt file found in the top-level directory 
 * of this distribution and at: 
 *    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
 * No part of the rogue software platform, including this file, may be 
 * copied, modified, propagated, or distributed except according to the terms 
 * contained in the LICENSE.txt file.
 * ----------------------------------------------------------------------------
**/
#include <unistd.h>
#include <string>
#include <rogue/interfaces/stream/Pool.h>
#include <rogue/interfaces/stream/Buffer.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/GeneralError.h>
#include <boost/make_shared.hpp>
#include <rogue/GilRelease.h>

namespace ris = rogue::interfaces::stream;
namespace bp  = boost::python;

//! Creator
ris::Pool::Pool() { 
   allocMeta_  = 0;
   allocBytes_ = 0;
   allocCount_ = 0;
   fixedSize_  = 0;
   maxCount_   = 0;
}

//! Destructor
ris::Pool::~Pool() { }

//! Get allocated memory
uint32_t ris::Pool::getAllocBytes() {
   return(allocBytes_);
}

//! Get allocated count
uint32_t ris::Pool::getAllocCount() {
   return(allocCount_);
}

//! Accept a frame request. Called from master
/*
 * Pass total size required.
 * Pass flag indicating if zero copy buffers are acceptable
 */
ris::FramePtr ris::Pool::acceptReq (uint32_t size, bool zeroCopyEn, uint32_t maxBuffSize ) {
   ris::FramePtr ret;
   uint32_t bSize;
   uint32_t frSize;

   ret  = ris::Frame::create();
   frSize = 0;

   // Determine buffer size
   if ( maxBuffSize == 0 ) bSize = size;
   else bSize = maxBuffSize;

   while ( frSize < size ) ret->appendBuffer(allocBuffer(bSize,&frSize));

   return(ret);
}

//! Return a buffer
/*
 * Called when this instance is marked as owner of a Buffer entity
 */
void ris::Pool::retBuffer(uint8_t * data, uint32_t meta, uint32_t rawSize) {
   rogue::GilRelease noGil;
   boost::lock_guard<boost::mutex> lock(mtx_);

   if ( data != NULL ) {
      if ( rawSize == fixedSize_ && maxCount_ > dataQ_.size() ) dataQ_.push(data);
      else free(data);
   }
   allocBytes_ -= rawSize;
   allocCount_--;
}

void ris::Pool::setup_python() {
   bp::class_<ris::Pool, ris::PoolPtr, boost::noncopyable>("Pool",bp::init<>())
      .def("getAllocCount",  &ris::Pool::getAllocCount)
      .def("getAllocBytes",  &ris::Pool::getAllocBytes)
   ;
}

//! Set fixed size mode
void ris::Pool::enBufferPool(uint32_t size, uint32_t count) {
   if ( fixedSize_ != 0 ) 
      throw(rogue::GeneralError("Pool::enBufferPool","Method can only be called once!"));
   fixedSize_  = size;
   maxCount_   = count;
}

//! Allocate a buffer passed size
// Buffer container and raw data should be allocated from shared memory pool
ris::BufferPtr ris::Pool::allocBuffer ( uint32_t size, uint32_t *total ) {
   uint8_t * data;
   uint32_t  bAlloc;
   uint32_t  bSize;
   uint32_t  meta = 0;

   bAlloc = size;
   bSize  = size;

   rogue::GilRelease noGil;
   boost::lock_guard<boost::mutex> lock(mtx_);
   if ( fixedSize_ > 0 ) {
      bAlloc = fixedSize_;
      if ( bSize > bAlloc ) bSize = bAlloc;
   }

   if ( dataQ_.size() > 0 ) {
      data = dataQ_.front();
      dataQ_.pop();
   }
   else if ( (data = (uint8_t *)malloc(bAlloc)) == NULL ) 
      throw(rogue::GeneralError::allocation("Pool::allocBuffer",bAlloc));

   // Only use lower 24 bits of meta. 
   // Upper 8 bits may have special meaning to sub-class
   meta = allocMeta_;
   allocMeta_++;
   allocMeta_ &= 0xFFFFFF;
   allocBytes_ += bAlloc;
   allocCount_++;
   if ( total != NULL ) *total += bSize;
   return(ris::Buffer::create(shared_from_this(),data,meta,bSize,bAlloc));
}

//! Create a Buffer with passed data
ris::BufferPtr ris::Pool::createBuffer( void * data, uint32_t meta, uint32_t size, uint32_t alloc) {
   ris::BufferPtr buff;

   rogue::GilRelease noGil;
   boost::lock_guard<boost::mutex> lock(mtx_);

   buff = ris::Buffer::create(shared_from_this(),data,meta,size,alloc);

   allocBytes_ += alloc;
   allocCount_++;
   return(buff);
}

//! Track buffer deletion
void ris::Pool::decCounter( uint32_t alloc) {
   rogue::GilRelease noGil;
   boost::lock_guard<boost::mutex> lock(mtx_);
   allocBytes_ -= alloc;
   allocCount_--;
}

