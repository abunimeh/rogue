/**
 *-----------------------------------------------------------------------------
 * Title      : PGP Card Pci Status Class
 * ----------------------------------------------------------------------------
 * File       : PciStatus.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2017-09-17
 * Last update: 2017-09-17
 * ----------------------------------------------------------------------------
 * Description:
 * Wrapper for PciStatus structure
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
#include <rogue/hardware/pgp/PciStatus.h>
#include <boost/make_shared.hpp>

namespace rhp = rogue::hardware::pgp;
namespace bp  = boost::python;

//! Create the info class with pointer
rhp::PciStatusPtr rhp::PciStatus::create() {
   rhp::PciStatusPtr r = boost::make_shared<rhp::PciStatus>();
   return(r);
}

void rhp::PciStatus::setup_python() {

   bp::class_<rhp::PciStatus, rhp::PciStatusPtr>("PciStatus",bp::no_init)
      .def_readonly("pciCommand",   &rhp::PciStatus::pciCommand)
      .def_readonly("pciStatus",    &rhp::PciStatus::pciStatus)
      .def_readonly("pciDCommand",  &rhp::PciStatus::pciDCommand)
      .def_readonly("pciDStatus",   &rhp::PciStatus::pciDStatus)
      .def_readonly("pciLCommand",  &rhp::PciStatus::pciLCommand)
      .def_readonly("pciLStatus",   &rhp::PciStatus::pciLStatus)
      .def_readonly("pciLinkState", &rhp::PciStatus::pciLinkState)
      .def_readonly("pciFunction",  &rhp::PciStatus::pciFunction)
      .def_readonly("pciDevice",    &rhp::PciStatus::pciDevice)
      .def_readonly("pciBus",       &rhp::PciStatus::pciBus)
      .def_readonly("pciLanes",     &rhp::PciStatus::pciLanes)
   ; 

}
