/**
 *-----------------------------------------------------------------------------
 * Title      : General Error
 * ----------------------------------------------------------------------------
 * File       : GeneralError.h
 * Created    : 2017-12-05
 * ----------------------------------------------------------------------------
 * Description:
 * General exception for Rogue
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
#ifndef __ROGUE_GENERAL_ERROR_H__
#define __ROGUE_GENERAL_ERROR_H__
#include <exception>
#include <boost/python.hpp>
#include <stdint.h>

namespace rogue {

   extern PyObject * generalErrorObj;

   //! General exception
   /*
    * Called for all general errors that should not occur
    * in the system.
    */
   class GeneralError : public std::exception {
         char text_[200];
      public:
         GeneralError (std::string src,std::string text);

         static GeneralError timeout(std::string src, uint32_t time);
         static GeneralError open(std::string src, std::string file);
         static GeneralError dest(std::string src, std::string file, uint32_t dest);
         static GeneralError boundary(std::string src, uint32_t position, uint32_t limit);
         static GeneralError allocation(std::string src, uint32_t size);
         static GeneralError network(std::string src, std::string host, uint16_t port);
         static GeneralError ret(std::string src, std::string text, int32_t ret);

         char const * what() const throw();
         static void setup_python();
         static void translate(GeneralError const &e);
   };
}

#endif

