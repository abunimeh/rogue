/**
 *-----------------------------------------------------------------------------
 * Title      : Python Package
 * ----------------------------------------------------------------------------
 * File       : package.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-08-08
 * Last update: 2016-08-08
 * ----------------------------------------------------------------------------
 * Description:
 * Python package setup
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

#include <boost/python.hpp>
#include <rogue/interfaces/module.h>
#include <rogue/hardware/module.h>
#include <rogue/utilities/module.h>
#include <rogue/protocols/module.h>
#include <rogue/GeneralError.h>
#include <rogue/Logging.h>
#include <rogue/SMemControl.h>
#include <rogue/GilRelease.h>
#include <rogue/ScopedGil.h>
#include <rogue/Version.h>

BOOST_PYTHON_MODULE(rogue) {

   PyEval_InitThreads();

   rogue::interfaces::setup_module();
   rogue::protocols::setup_module();
   rogue::hardware::setup_module();
   rogue::utilities::setup_module();

   rogue::GeneralError::setup_python();
   rogue::Logging::setup_python();
   rogue::GilRelease::setup_python();
   rogue::ScopedGil::setup_python();
   rogue::Version::setup_python();
   rogue::SMemControl::setup_python();

   printf("Rogue/pyrogue version %s. https://github.com/slaclab/rogue\n",rogue::Version::current().c_str());

};

