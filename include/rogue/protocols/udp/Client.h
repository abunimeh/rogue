/**
 *-----------------------------------------------------------------------------
 * Title      : UDP Client Class
 * ----------------------------------------------------------------------------
 * File       : Client.h
 * Created    : 2017-01-07
 * Last update: 2017-01-07
 * ----------------------------------------------------------------------------
 * Description:
 * UDP Client
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
#ifndef __ROGUE_PROTOCOLS_UDP_CLIENT_H__
#define __ROGUE_PROTOCOLS_UDP_CLIENT_H__
#include <rogue/interfaces/stream/Master.h>
#include <rogue/interfaces/stream/Slave.h>
#include <boost/python.hpp>
#include <boost/thread.hpp>
#include <stdint.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

namespace rogue {
   namespace protocols {
      namespace udp {

         //! PGP Card class
         class Client : public rogue::interfaces::stream::Master, 
                        public rogue::interfaces::stream::Slave {

               //! Socket
               int32_t  fd_;

               //! Max packet size
               uint32_t maxSize_;

               //! Address, hostname or ip address
               std::string address_;

               //! Remote port number
               uint16_t port_;

               //! Remote socket address
               struct sockaddr_in addr_;

               //! Timeout value
               uint32_t timeout_;

               boost::thread* thread_;

               //! mutex
               boost::mutex mtx_;

               //! Thread background
               void runThread();

            public:

               //! Class creation
               static boost::shared_ptr<rogue::protocols::udp::Client> 
                  create (std::string host, uint16_t port, uint16_t maxSize);

               //! Setup class in python
               static void setup_python();

               //! Creator
               Client(std::string host, uint16_t port, uint16_t maxSize);

               //! Destructor
               ~Client();

               //! Set UDP RX Size
               bool setRxSize(uint32_t size);

               //! Set timeout for frame transmits in microseconds
               void setTimeout(uint32_t timeout);

               //! Accept a frame from master
               void acceptFrame ( boost::shared_ptr<rogue::interfaces::stream::Frame> frame );
         };

         // Convienence
         typedef boost::shared_ptr<rogue::protocols::udp::Client> ClientPtr;

      }
   }
};

#endif

