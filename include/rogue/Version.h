/**
 *-----------------------------------------------------------------------------
 * Title      : Rogue Version
 * ----------------------------------------------------------------------------
 * File       : Version.h
 * Created    : 2017-05-17
 * ----------------------------------------------------------------------------
 * Description:
 * Version helpers for Rogue
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
#ifndef __ROGUE_VERSION_H__
#define __ROGUE_VERSION_H__
#include <boost/python.hpp>
#include <stdint.h>

namespace rogue {

   //! Version
   class Version {

         static void extract(std::string compare, uint32_t *major, uint32_t *minor, uint32_t *maint);

      public:

         static const uint32_t Major = VER_MAJOR;
         static const uint32_t Minor = VER_MINOR;
         static const uint32_t Maint = VER_MAINT;

         Version() {}

         static std::string current();

         static bool greaterThanEqual(std::string compare);

         static bool lessThan(std::string compare);

         static void minVersion(std::string compare);

         static void setup_python();
   };
}

#endif

