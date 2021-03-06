/**
 *-----------------------------------------------------------------------------
 * Title      : EXO TEM Base Class
 * ----------------------------------------------------------------------------
 * File       : Tem.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2017-09-17
 * Last update: 2017-09-17
 * ----------------------------------------------------------------------------
 * Description:
 * Class for interfacing to Tem Driver.
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
#include <rogue/hardware/exo/Tem.h>
#include <rogue/hardware/exo/Info.h>
#include <rogue/hardware/exo/PciStatus.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/interfaces/stream/Buffer.h>
#include <rogue/GeneralError.h>
#include <boost/make_shared.hpp>
#include <rogue/GilRelease.h>

namespace rhe = rogue::hardware::exo;
namespace ris = rogue::interfaces::stream;
namespace bp  = boost::python;

//! Class creation
rhe::TemPtr rhe::Tem::create (std::string path, bool data) {
   rhe::TemPtr r = boost::make_shared<rhe::Tem>(path,data);
   return(r);
}

//! Open the device. Pass lane & vc.
rhe::Tem::Tem(std::string path, bool data) {
   int32_t res;

   isData_  = data;
   timeout_ = 1000000;

   rogue::GilRelease noGil;

   if ( (fd_ = ::open(path.c_str(), O_RDWR)) < 0 )
      throw(rogue::GeneralError::open("Tem::Tem",path.c_str()));

   if ( dmaCheckVersion(fd_) < 0 ) 
      throw(rogue::GeneralError("Tem::Tem","Bad kernel driver version detected. Please re-compile kernel driver"));

   if ( isData_ ) {
      if ( (res = temEnableDataRead(fd_)) < 0 ) ::close(fd_);
   } else {
      if ((res =  temEnableCmdRead(fd_)) < 0 ) ::close(fd_);
   }

   if ( res < 0 ) throw(rogue::GeneralError::dest("Tem::Tem",path.c_str(),1));

   // Start read thread
   thread_ = new boost::thread(boost::bind(&rhe::Tem::runThread, this));
}

//! Close the device
rhe::Tem::~Tem() {
   rogue::GilRelease noGil;

   // Stop read thread
   thread_->interrupt();
   thread_->join();

   ::close(fd_);
}

//! Set timeout for frame transmits in microseconds
void rhe::Tem::setTimeout(uint32_t timeout) {
   timeout_ = timeout;
}

//! Get card info.
rhe::InfoPtr rhe::Tem::getInfo() {
   rhe::InfoPtr r = rhe::Info::create();

   if ( fd_ >= 0 ) temGetInfo(fd_,r.get());
   return(r);
}

//! Get pci status.
rhe::PciStatusPtr rhe::Tem::getPciStatus() {
   rhe::PciStatusPtr r = rhe::PciStatus::create();

   if ( fd_ >= 0 ) temGetPci(fd_,r.get());
   return(r);
}

//! Accept a frame from master
void rhe::Tem::acceptFrame ( ris::FramePtr frame ) {
   ris::BufferPtr   buff;
   int32_t          res;
   fd_set           fds;
   struct timeval   tout;

   buff = frame->getBuffer(0);

   rogue::GilRelease noGil;

   // Keep trying since select call can fire 
   // but write fails because we did not win the buffer lock
   do {

      // Setup fds for select call
      FD_ZERO(&fds);
      FD_SET(fd_,&fds);

      // Setup select timeout
      tout.tv_sec=(timeout_ > 0)?(timeout_ / 1000000):0;
      tout.tv_usec=(timeout_ > 0)?(timeout_ % 1000000):10000;

      if ( select(fd_+1,NULL,&fds,NULL,&tout) <= 0 ) {
         if ( timeout_ > 0) throw(rogue::GeneralError::timeout("Tem::acceptFrame",timeout_));
         res = 0;
      }
      else {
         // Write
         if ( (res = temWriteCmd(fd_, buff->getRawData(), buff->getCount())) < 0 ) 
            throw(rogue::GeneralError("Tem::acceptFrame","Tem Write Call Failed"));
      }
   }

   // Exit out if return flag was set false
   while ( res == 0 );
}

//! Run thread
void rhe::Tem::runThread() {
   ris::BufferPtr buff;
   ris::FramePtr  frame;
   fd_set         fds;
   int32_t        res;
   struct timeval tout;

   try {
      while(1) {

         // Setup fds for select call
         FD_ZERO(&fds);
         FD_SET(fd_,&fds);

         // Setup select timeout
         tout.tv_sec  = 0;
         tout.tv_usec = 100;

         // Select returns with available buffer
         if ( select(fd_+1,&fds,NULL,NULL,&tout) > 0 ) {

            // Allocate frame
            frame = acceptReq(1024*1024*2,false,0);
            buff = frame->getBuffer(0);

            // Attempt read, lane and vc not needed since only one lane/vc is open
            res = temRead(fd_, buff->getRawData(), buff->getRawSize());

            // Read was successfull
            if ( res > 0 ) {
               buff->setSize(res);
               sendFrame(frame);
               buff.reset();
               frame.reset();
            }
         }
         boost::this_thread::interruption_point();
      }
   } catch (boost::thread_interrupted&) { }
}

void rhe::Tem::setup_python () {

   bp::class_<rhe::Tem, rhe::TemPtr, bp::bases<ris::Master,ris::Slave>, boost::noncopyable >("Tem",bp::init<std::string,bool>())
      .def("create",         &rhe::Tem::create)
      .staticmethod("create")
      .def("getInfo",        &rhe::Tem::getInfo)
      .def("getPciStatus",   &rhe::Tem::getPciStatus)
      .def("setTimeout",     &rhe::Tem::setTimeout)
   ;

   bp::implicitly_convertible<rhe::TemPtr, ris::MasterPtr>();
   bp::implicitly_convertible<rhe::TemPtr, ris::SlavePtr>();
}

