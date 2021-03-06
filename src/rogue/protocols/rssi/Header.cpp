/**
 *-----------------------------------------------------------------------------
 * Title      : RSII Header Class
 * ----------------------------------------------------------------------------
 * File       : Header.h
 * Created    : 2017-01-07
 * Last update: 2017-01-07
 * ----------------------------------------------------------------------------
 * Description:
 * RSSI Header
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
#include <rogue/protocols/rssi/Header.h>
#include <rogue/GeneralError.h>
#include <boost/make_shared.hpp>
#include <rogue/GilRelease.h>
#include <stdint.h>
#include <iomanip>
#include <rogue/interfaces/stream/Buffer.h>
#include <arpa/inet.h>

namespace rpr = rogue::protocols::rssi;
namespace ris = rogue::interfaces::stream;
namespace bp  = boost::python;

//! Set 16-bit uint value
void rpr::Header::setUInt16 ( uint8_t byte, uint16_t value) {
   *((uint16_t *)(&(data_[byte]))) = htons(value);
}

//! Get 16-bit uint value
uint16_t rpr::Header::getUInt16 ( uint8_t byte ) {
   return(ntohs(*((uint16_t *)(&(data_[byte])))));
}

//! Set 32-bit uint value
void rpr::Header::setUInt32 ( uint8_t byte, uint32_t value) {
   *((uint32_t *)(&(data_[byte]))) = htonl(value);
}

//! Get 32-bit uint value
uint32_t rpr::Header::getUInt32 ( uint8_t byte ) {
   return(ntohl(*((uint32_t *)(&(data_[byte])))));
}

//! compute checksum
uint16_t rpr::Header::compSum (uint8_t size) {
   uint8_t  x;
   uint32_t sum;

   sum = 0;
   for (x=0; x < size-2; x = x+2) sum += getUInt16(x);

   sum = (sum % 0x10000) + (sum / 0x10000);
   sum = sum ^ 0xFFFF;
   return(sum);
}

//! Create
rpr::HeaderPtr rpr::Header::create(ris::FramePtr frame) {
   rpr::HeaderPtr r = boost::make_shared<rpr::Header>(frame);
   return(r);
}

void rpr::Header::setup_python() {
   // Nothing to do
}

//! Creator
rpr::Header::Header(ris::FramePtr frame) {
   if ( frame->getCount() == 0 ) 
      throw(rogue::GeneralError("Header::Header","Frame must not be empty!"));
   frame_ = frame;
   data_  = frame->getBuffer(0)->getPayloadData();
   count_ = 0;

   syn = false;
   ack = false;
   rst = false;
   nul = false;
   busy = false;
   //sequence = 0;
   //acknowledge = 0;
   //version = 0;
   //chk = false;
   //maxOutstandingSegments = 0;
   //maxSegmentSize = 0;
   //retransmissionTimeout = 0;
   //cumulativeAckTimeout = 0;
   //nullTimeout = 0;
   //maxRetransmissions = 0;
   //maxCumulativeAck = 0;
   //timeoutUnit = 0;
   //connectionId = 0;
}

//! Destructor
rpr::Header::~Header() { }

//! Get Frame
ris::FramePtr rpr::Header::getFrame() {
   return(frame_);
}

//! Verify header contents
bool rpr::Header::verify() {
   uint8_t size;

   if ( frame_->getBuffer(0)->getPayload() < HeaderSize ) return(false);
  
   syn  = data_[0] & 0x80;
   ack  = data_[0] & 0x40;
   rst  = data_[0] & 0x10;
   nul  = data_[0] & 0x08;
   busy = data_[0] & 0x01;

   size = (syn)?SynSize:HeaderSize;

   if ( (data_[1] != size) ||
        (frame_->getBuffer(0)->getPayload() < size) ||
        (getUInt16(size-2) != compSum(size))) return false;

   sequence    = data_[2];
   acknowledge = data_[3];

   if ( ! syn ) return true;

   version = data_[4] >> 4;
   chk  = data_[4] & 0x04;

   maxOutstandingSegments = data_[5];
   maxSegmentSize = getUInt16(6);
   retransmissionTimeout = getUInt16(8);
   cumulativeAckTimeout = getUInt16(10);
   nullTimeout = getUInt16(12);
   maxRetransmissions = data_[14];
   maxCumulativeAck = data_[15];
   timeoutUnit = data_[17];
   connectionId = data_[18];

   return(true);
}

//! Update checksum, set tx time and increment tx count
void rpr::Header::update() {
   uint8_t size;

   size = (syn)?SynSize:HeaderSize;

   if ( frame_->getBuffer(0)->getRawPayload() < size )
      throw(rogue::GeneralError::boundary("Header::update",size,frame_->getBuffer(0)->getRawPayload()));

   if ( frame_->getBuffer(0)->getPayload() == 0 )
      frame_->getBuffer(0)->setPayload(size);

   memset(data_,0,size);
   data_[1] = size;

   if ( ack  ) data_[0] |= 0x40;
   if ( rst  ) data_[0] |= 0x10;
   if ( nul  ) data_[0] |= 0x08;
   if ( busy ) data_[0] |= 0x01;

   data_[2] = sequence;
   data_[3] = acknowledge;

   if ( syn ) {
      data_[0] |= 0x80;
      data_[4] |= 0x08;
      data_[4] |= (version << 4);
      if ( chk ) data_[4] |= 0x04;

      data_[5] = maxOutstandingSegments;

      setUInt16(6,maxSegmentSize);
      setUInt16(8,retransmissionTimeout);
      setUInt16(10,cumulativeAckTimeout);
      setUInt16(12,nullTimeout);

      data_[14] = maxRetransmissions;
      data_[15] = maxCumulativeAck;
      data_[17] = timeoutUnit;
      data_[18] = connectionId;
   }

   setUInt16(size-2,compSum(size));
   gettimeofday(&time_,NULL);
   count_++;
}

//! Get time
struct timeval * rpr::Header::getTime() {
   return(&time_);
}

//! Get Count
uint32_t rpr::Header::count() {
   return(count_);
}

//! Reset timer
void rpr::Header::rstTime() {
   gettimeofday(&time_,NULL);
}

//! Dump message
std::string rpr::Header::dump() {
   uint32_t   x;

   std::stringstream ret("");

   ret << "   Total Size : " << std::dec << frame_->getBuffer(0)->getPayload() << std::endl;
   ret << "     Raw Size : " << std::dec << frame_->getBuffer(0)->getRawPayload() << std::endl;
   ret << "   Raw Header : ";

   for (x=0; x < data_[1]; x++) {
      ret << "0x" << std::hex << std::setw(2) << std::setfill('0') << (uint32_t)data_[x] << " ";
      if ( (x % 8) == 7 && (x+1) != data_[1]) ret << std::endl << "                ";
   }
   ret << std::endl;

   ret << "          Syn : " << std::dec << syn << std::endl;
   ret << "          Ack : " << std::dec << ack << std::endl;
   ret << "          Rst : " << std::dec << rst << std::endl;
   ret << "          Nul : " << std::dec << nul << std::endl;
   ret << "         Busy : " << std::dec << busy << std::endl;
   ret << "     Sequence : " << std::dec << (uint32_t)sequence << std::endl;
   ret << "  Acknowledge : " << std::dec << (uint32_t)acknowledge << std::endl;

   if ( ! syn ) return(ret.str());

   ret << "      Version : " << std::dec << (uint32_t)version << std::endl;
   ret << "          Chk : " << std::dec << chk << std::endl;
   ret << "  Max Out Seg : " << std::dec << (uint32_t)maxOutstandingSegments << std::endl;
   ret << " Max Seg Size : " << std::dec << (uint32_t)maxSegmentSize << std::endl;
   ret << "  Retran Tout : " << std::dec << (uint32_t)retransmissionTimeout << std::endl;
   ret << " Cum Ack Tout : " << std::dec << (uint32_t)cumulativeAckTimeout << std::endl;
   ret << "    Null Tout : " << std::dec << (uint32_t)nullTimeout << std::endl;
   ret << "  Max Retrans : " << std::dec << (uint32_t)maxRetransmissions << std::endl;
   ret << "  Max Cum Ack : " << std::dec << (uint32_t)maxCumulativeAck << std::endl;
   ret << " Timeout Unit : " << std::dec << (uint32_t)timeoutUnit << std::endl;
   ret << "      Conn Id : " << std::dec << (uint32_t)connectionId << std::endl;

   return(ret.str());
}

