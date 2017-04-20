/**
 *-----------------------------------------------------------------------------
 * Title         : PRBS Receive And Transmit Class
 * ----------------------------------------------------------------------------
 * File          : Prbs.cpp
 * Author        : Ryan Herbst <rherbst@slac.stanford.edu>
 * Created       : 09/17/2016
 * Last update   : 09/17/2016
 *-----------------------------------------------------------------------------
 * Description :
 *    Class used to generate and receive PRBS test data.
 *-----------------------------------------------------------------------------
 * This file is part of the rogue software platform. It is subject to 
 * the license terms in the LICENSE.txt file found in the top-level directory 
 * of this distribution and at: 
    * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
 * No part of the rogue software platform, including this file, may be 
 * copied, modified, propagated, or distributed except according to the terms 
 * contained in the LICENSE.txt file.
 *-----------------------------------------------------------------------------
**/
#include <unistd.h>
#include <stdarg.h>
#include <rogue/interfaces/stream/Slave.h>
#include <rogue/interfaces/stream/Master.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/interfaces/stream/Buffer.h>
#include <rogue/utilities/Prbs.h>
#include <boost/make_shared.hpp>
#include <rogue/GilRelease.h>
#include <rogue/Logging.h>

namespace ris = rogue::interfaces::stream;
namespace ru  = rogue::utilities;
namespace bp  = boost::python;

//! Class creation
ru::PrbsPtr ru::Prbs::create () {
   ru::PrbsPtr p = boost::make_shared<ru::Prbs>();
   return(p);
}

//! Creator with width and variable taps
ru::Prbs::Prbs(uint32_t width, uint32_t tapCnt, ... ) {
   va_list a_list;
   uint32_t   x;

   init(width,tapCnt);

   va_start(a_list,tapCnt);
   for(x=0; x < tapCnt; x++) taps_[x] = va_arg(a_list,uint32_t);
   va_end(a_list);
}

//! Creator with default taps and size
ru::Prbs::Prbs() {
   init(32,4);

   taps_[0] = 1;
   taps_[1] = 2;
   taps_[2] = 6;
   taps_[3] = 31;
}

//! Deconstructor
ru::Prbs::~Prbs() {
   free(taps_);
}

void ru::Prbs::init(uint32_t width, uint32_t tapCnt) {
   txThread_   = NULL;
   tapCnt_     = tapCnt;
   rxSeq_      = 0;
   rxErrCount_ = 0;
   rxCount_    = 0;
   rxBytes_    = 0;
   txSeq_      = 0;
   txSize_     = 0;
   txErrCount_ = 0;
   txCount_    = 0;
   txBytes_    = 0;
   rxLog_      = new Logging("prbs.rx");
   txLog_      = new Logging("prbs.tx");

   if ( width == 16 ) {
      width_     = 16;
      byteWidth_ = 2;
      minSize_   = 6;
   }
   else {
      width_     = 32;
      byteWidth_ = 4;
      minSize_   = 12;
   }

   taps_ = (uint32_t *)malloc(sizeof(uint32_t)*tapCnt_);
}

uint32_t ru::Prbs::flfsr(uint32_t input) {
   uint32_t bit = 0;
   uint32_t x;
   uint32_t ret;

   for (x=0; x < tapCnt_; x++) bit = bit ^ (input >> taps_[x]);

   bit = bit & 1;
   ret = (input << 1) | bit;

   if ( width_ == 16 ) ret &= 0xFFFF;
   return (ret);
}

//! Thread background
void ru::Prbs::runThread() {
   try {
      while(1) {
         genFrame(txSize_);
         boost::this_thread::interruption_point();
      }
   } catch (boost::thread_interrupted&) {}
}

//! Read data with the appropriate width and set passed 32-bit integer pointer
uint32_t ru::Prbs::readSingle ( ris::FramePtr frame, uint32_t offset, uint32_t * value ) {
   uint32_t data32;
   uint16_t data16;
   uint32_t ret;

   if ( width_ == 16 ) {
      ret = frame->read(&data16,offset,2);
      *value = data16;
   }
   else {
      ret = frame->read(&data32,offset,4);
      *value = data32;
   }
   return(ret);
}

//! Write data with the appropriate width
uint32_t ru::Prbs::writeSingle ( ris::FramePtr frame, uint32_t offset, uint32_t value ) {
   uint32_t data32;
   uint16_t data16;
   uint32_t ret;

   if ( width_ == 16 ) {
      data16 = value & 0xFFFF;
      ret = frame->write(&data16,offset,2);
   }
   else {
      data32 = value;
      ret = frame->write(&data32,offset,4);
   }
   return(ret);
}

//! Auto run data generation
void ru::Prbs::enable(uint32_t size) {
   if ( txThread_ == NULL ) {
      txSize_ = size;
      txThread_ = new boost::thread(boost::bind(&Prbs::runThread, this));
   }
}

//! Disable auto generation
void ru::Prbs::disable() {
   if ( txThread_ != NULL ) {
      txThread_->interrupt();
      txThread_->join();
      delete txThread_;
      txThread_ = NULL;
   }
}

//! Get RX errors
uint32_t ru::Prbs::getRxErrors() {
   return(rxErrCount_);
}

//! Get rx count
uint32_t ru::Prbs::getRxCount() {
   return(rxCount_);
}

//! Get rx bytes
uint32_t ru::Prbs::getRxBytes() {
   return(rxBytes_);
}

//! Get TX errors
uint32_t ru::Prbs::getTxErrors() {
   return(txErrCount_);
}

//! Get TX count
uint32_t ru::Prbs::getTxCount() {
   return(txCount_);
}

//! Get TX bytes
uint32_t ru::Prbs::getTxBytes() {
   return(txBytes_);
}

//! Reset counters
// Counters should really be locked!
void ru::Prbs::resetCount() {

   txMtx_.lock();
   txErrCount_ = 0;
   txCount_    = 0;
   txBytes_    = 0;
   txMtx_.unlock();

   rxMtx_.lock();
   rxErrCount_ = 0;
   rxCount_    = 0;
   rxBytes_    = 0;
   rxMtx_.unlock();

}

//! Generate a data frame
void ru::Prbs::genFrame (uint32_t size) {
   uint32_t      cnt;
   uint32_t      frSize;
   uint32_t      value;
   ris::FramePtr fr;

   boost::unique_lock<boost::mutex> lock(txMtx_,boost::defer_lock);

   rogue::GilRelease noGil;
   lock.lock();
   noGil.acquire();

   // Verify size first
   if ((( size % byteWidth_ ) != 0) || size < minSize_ ) {
      txLog_->warning("Size violation size=%i, count=%i",size,txCount_);
      txErrCount_++;
      return;
   }

   // Lock and update sequence
   value = txSeq_;
   txSeq_++;
   if ( width_ == 16 ) txSeq_ &= 0xFFFF;

   // Get frame
   fr = reqFrame(size,true,0);

   // Frame allocation failed
   if ( fr->getAvailable() < size ) {
      txErrCount_++;
      return;
   }

   // First word is sequence
   cnt = writeSingle(fr,0,value);

   // Second word is size
   frSize = (size - byteWidth_) / byteWidth_;
   cnt += writeSingle(fr,cnt,frSize);

   // Generate payload
   while ( cnt < size ) {
      for (uint32_t i=0; i<width_; i++) {
         value = flfsr(value);
      }
      cnt += writeSingle(fr,cnt,value);
   }

   // Update counters
   txCount_++;
   txBytes_ += size;
   sendFrame(fr);
}

//! Accept a frame from master
void ru::Prbs::acceptFrame ( ris::FramePtr frame ) {
   uint32_t   frSize;
   uint32_t   frSeq;
   uint32_t   curSeq;
   uint32_t   size;
   uint32_t   cnt;
   uint32_t   expValue;
   uint32_t   gotValue;

   boost::unique_lock<boost::mutex> lock(rxMtx_,boost::defer_lock);

   rogue::GilRelease noGil;
   lock.lock();
   noGil.acquire();

   size = frame->getPayload();

   // Verify size
   if ((( size % byteWidth_ ) != 0) || size < minSize_ ) {
      rxLog_->warning("Size violation size=%i, count=%i",size,rxCount_);
      rxErrCount_++;
      return;
   }

   // Get sequence value
   cnt = readSingle(frame,0,&frSeq);

   // Update sequence
   curSeq = rxSeq_;
   rxSeq_ = frSeq + 1;
   if ( width_ == 16 ) rxSeq_ &= 0xFFFF;

   // Get size
   cnt += readSingle(frame,cnt,&frSize);
   frSize = (frSize * byteWidth_) + byteWidth_;

   // Check size
   if ( frSize != size ) {
      rxLog_->warning("Bad size. exp=%i, got=%i, count=%i",frSize,size,rxCount_);
      rxErrCount_++;
      return;
   }

   // Check sequence, 
   // Accept any sequence if our local count is zero
   // incoming frames with seq = 0 never cause errors and treated as a restart
   if ( frSeq != 0 && curSeq != 0 && frSeq != curSeq ) {
      rxLog_->warning("Bad Sequence. cur=%i, got=%i, count=%i",curSeq,frSeq,rxCount_);
      rxErrCount_++;
      return;
   }
   expValue = frSeq;

   // Check payload
   while ( cnt < size ) {
      for (uint32_t i=0; i<width_; i++) {
         expValue = flfsr(expValue);
      }

      cnt += readSingle(frame,cnt,&gotValue);

      if (expValue != gotValue) {
         rxLog_->warning("Bad value at index %i. exp=0x%x, got=0x, count=%i%x",
                 (cnt-byteWidth_),expValue,gotValue,rxCount_);
         rxErrCount_++;
         return;
      }
   }

   rxCount_++;
   rxBytes_ += size;
}

void ru::Prbs::setup_python() {

   bp::class_<ru::Prbs, ru::PrbsPtr, bp::bases<ris::Master,ris::Slave>, boost::noncopyable >("Prbs",bp::init<>())
      .def("create",         &ru::Prbs::create)
      .staticmethod("create")
      .def("genFrame",       &ru::Prbs::genFrame)
      .def("enable",         &ru::Prbs::enable)
      .def("disable",        &ru::Prbs::disable)
      .def("getRxErrors",    &ru::Prbs::getRxErrors)
      .def("getRxCount",     &ru::Prbs::getRxCount)
      .def("getRxBytes",     &ru::Prbs::getRxBytes)
      .def("getTxErrors",    &ru::Prbs::getTxErrors)
      .def("getTxCount",     &ru::Prbs::getTxCount)
      .def("getTxBytes",     &ru::Prbs::getTxBytes)
      .def("resetCount",     &ru::Prbs::resetCount)
   ;

   bp::implicitly_convertible<ru::PrbsPtr, ris::SlavePtr>();
   bp::implicitly_convertible<ru::PrbsPtr, ris::MasterPtr>();

}


