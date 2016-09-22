/**
 *-----------------------------------------------------------------------------
 * Title      : Python Module
 * ----------------------------------------------------------------------------
 * File       : module.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-08-08
 * Last update: 2016-08-08
 * ----------------------------------------------------------------------------
 * Description:
 * Python module setup
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
#include <rogue/protocols/srp/module.h>
#include <rogue/protocols/srp/Transaction.h>
#include <rogue/protocols/srp/TransactionV0.h>
#include <rogue/protocols/srp/TransactionV1.h>
#include <rogue/protocols/srp/TransactionV2.h>
#include <rogue/protocols/srp/TransactionV3.h>

namespace bp  = boost::python;
namespace rps = rogue::protocols::srp;

void rps::setup_module() {

   // map the IO namespace to a sub-module
   bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule("rogue.protocols.srp"))));

   // make "from mypackage import class1" work
   bp::scope().attr("srp") = module;

   // set the current scope to the new sub-module
   bp::scope io_scope = module;

   rps::Transaction::setup_python();
   rps::TransactionV0::setup_python();
   rps::TransactionV1::setup_python();
   rps::TransactionV2::setup_python();
   rps::TransactionV3::setup_python();

}

