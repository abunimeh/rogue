/**
 *-----------------------------------------------------------------------------
 * Title      : Stream frame container
 * ----------------------------------------------------------------------------
 * File       : Frame.h
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-09-16
 * Last update: 2016-09-16
 * ----------------------------------------------------------------------------
 * Description:
 * Stream frame container
 * Some concepts borrowed from CPSW by Till Straumann
 * TODO:
 *    Add locking for thread safety. May not be needed since the source will
 *    set things up once before handing off to the various threads.
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
#ifndef __ROGUE_INTERFACES_STREAM_FRAME_H__
#define __ROGUE_INTERFACES_STREAM_FRAME_H__
#include <stdint.h>
#include <vector>

#include <boost/python.hpp>
namespace rogue {
   namespace interfaces {
      namespace stream {

         class Buffer;
         class FrameIterator;

         //! Frame container
         /*
          * This class is a container for a vector of buffers which make up a frame
          * container. Each buffer within the frame has a reserved header area and a 
          * payload. Calls to write and read take into account the header offset.
          * It is assumed only one thread will interact with a buffer. Buffers 
          * are not thread safe.
         */
         class Frame {

               //! Interface specific flags
               uint32_t flags_;

               //! Error state
               uint32_t error_;

               //! List of buffers which hold real data
               std::vector<boost::shared_ptr<rogue::interfaces::stream::Buffer> > buffers_;

            public:

               //! Setup class in python
               static void setup_python();

               //! Create an empty frame
               /*
                * Pass owner and zero copy status
                */
               static boost::shared_ptr<rogue::interfaces::stream::Frame> create();

               //! Create an empty frame
               Frame();

               //! Destroy a frame.
               ~Frame();

               //! Add a buffer to end of frame
               void appendBuffer(boost::shared_ptr<rogue::interfaces::stream::Buffer> buff);

               //! Append frame to end. Passed frame is emptied.
               void appendFrame(boost::shared_ptr<rogue::interfaces::stream::Frame> frame);

               //! Get buffer count
               uint32_t getCount();

               //! Remove buffers from frame
               void clear();

               //! Get buffer at index
               boost::shared_ptr<rogue::interfaces::stream::Buffer> getBuffer(uint32_t index);

               //! Get total available capacity (not including header space)
               uint32_t getAvailable();

               //! Get total real payload size (not including header space)
               uint32_t getPayload();

               //! Get flags
               uint32_t getFlags();

               //! Set flags
               void setFlags(uint32_t flags);

               //! Get error state
               uint32_t getError();

               //! Set error state
               void setError(uint32_t error);

               //! Read count bytes from frame payload, starting from offset.
               /* 
                * Frame reads can be from random offsets
                */
               uint32_t read  ( void *p, uint32_t offset, uint32_t count );

               //! Read count bytes from frame payload, starting from offset. Python version.
               /* 
                * Frame reads can be from random offsets
                */
               void readPy ( boost::python::object p, uint32_t offset );

               //! Write count bytes to frame payload, starting at offset
               /* 
                * Frame writes can be at random offsets. Payload size will
                * be set to highest write offset.
                */
               uint32_t write ( void *p, uint32_t offset, uint32_t count );

               //! Write count bytes to frame payload, starting at offset. Python Version
               /* 
                * Frame writes can be at random offsets. Payload size will
                * be set to highest write offset.
                */
               void writePy ( boost::python::object p, uint32_t offset );

               //! Start an iterative write
               /*
                * Pass offset and total size
                * Returns iterator object.
                * Use data and size fields in object to control transaction
                * Call writeNext to following data update.
                */
               boost::shared_ptr<rogue::interfaces::stream::FrameIterator> 
                   startWrite(uint32_t offset, uint32_t size);

               //! Continue an iterative write
               bool nextWrite(boost::shared_ptr<rogue::interfaces::stream::FrameIterator> iter);

               //! Start an iterative read
               /*
                * Pass offset and total size
                * Returns iterator object.
                * Use data and size fields in object to control transaction
                * Call readNext to following data update.
                */
                boost::shared_ptr<rogue::interfaces::stream::FrameIterator> 
                   startRead(uint32_t offset, uint32_t size);

               //! Continue an iterative read
               bool nextRead(boost::shared_ptr<rogue::interfaces::stream::FrameIterator> iter);

         };

         //! Frame iterator
         /*
          * Tracks accesses within a frame while iterating.
          * data is pointer to raw buffer to act on
          * size is the transaction size allowed for pointer
          */
         class FrameIterator {
            friend class rogue::interfaces::stream::Frame;
            protected:

               //! Buffer index
               uint32_t index_;

               //! Remaining bytes in transaction
               uint32_t remaining_;

               //! Buffer pointer
               uint8_t * data_;

               //! Buffer offset
               uint32_t offset_;

               //! Size of pointer
               uint32_t size_;

               //! Amount completed in transaction
               uint32_t completed_;

               //! Transaction total
               uint32_t total_;

            public:

               //! Get pointer
               uint8_t * data() { return(data_); }

               //! Get size
               uint32_t size() { return(size_); }

               //! Transaction total
               uint32_t total() {return(total_);}

               //! Update the amount accessed
               void completed(uint32_t value) {
                  if ( value < size_ ) completed_ = value;
               }
         };

         // Convienence
         typedef boost::shared_ptr<rogue::interfaces::stream::Frame> FramePtr;
         typedef boost::shared_ptr<rogue::interfaces::stream::FrameIterator> FrameIteratorPtr;

      }
   }
}

#endif

