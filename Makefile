# 
# ----------------------------------------------------------------------------
# Title      : ROGUE makefile
# ----------------------------------------------------------------------------
# File       : Makefile
# Author     : Ryan Herbst, rherbst@slac.stanford.edu
# Created    : 2016-08-08
# Last update: 2016-08-08
# ----------------------------------------------------------------------------
# Description:
# ROGUE makefile
# ----------------------------------------------------------------------------
# This file is part of the rogue software package. It is subject to 
# the license terms in the LICENSE.txt file found in the top-level directory 
# of this distribution and at: 
#    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
# No part of the rogue software package, including this file, may be 
# copied, modified, propagated, or distributed except according to the terms 
# contained in the LICENSE.txt file.
# ----------------------------------------------------------------------------
# 

# Python 3
PYCONF = python3-config
PYBOOST = -lboost_python3

# Python 2
#PYCONF = python2.7-config
#PYBOOST = -lboost_python

# Variables
CC       := g++
DEF      :=
CFLAGS   := -Wall `$(PYCONF) --cflags | sed s/-Wstrict-prototypes//` -fno-strict-aliasing -Wno-deprecated-declarations
CFLAGS   += -I $(BOOST_PATH)/include -I$(PWD)/include -I$(PWD)/drivers/include -std=c++0x -fPIC
LFLAGS   := `$(PYCONF) --ldflags` -lboost_thread $(PYBOOST) -lboost_system
LFLAGS   += -L`$(PYCONF) --prefix`/lib/ -L$(BOOST_PATH)/lib 

# Rogue Library Sources
LIB_HDR  := $(PWD)/include/rogue
LIB_SRC  := $(PWD)/src/rogue
LIB_CPP  := $(foreach dir,$(shell find $(LIB_SRC) -type d),$(wildcard $(dir)/*.cpp))
LIB_OBJ  := $(patsubst %.cpp,%.o,$(LIB_CPP))

# Python library
PYLIB      := $(PWD)/python/rogue.so
PYLIB_NAME := rogue

# C++ Library
CPPLIB := $(PWD)/lib/librogue.so

# Targets
all: $(LIB_OBJ) $(PYLIB) $(CPPLIB)

# Doxygen
docs:
	cd doc; doxygen

# Clean
clean:
	@rm -f $(PYLIB)
	@rm -f $(CPPLIB)
	@rm -f $(LIB_OBJ)

# Compile sources with headers
%.o: %.cpp %.h
	@echo "Compiling $@"; $(CC) -c $(CFLAGS) $(DEF) -o $@ $<

# Compile sources without headers
%.o: %.cpp
	@echo "Compiling $@"; $(CC) -c $(CFLAGS) $(DEF) -o $@ $<

# Compile Shared Library
$(PYLIB): $(LIB_OBJ)
	@echo "Creating $@"; $(CC) -shared -Wl,-soname,$(PYLIB_NAME) $(LIB_OBJ) $(LFLAGS) -o $@

# Compile Shared Library
$(CPPLIB): $(LIB_OBJ)
	@mkdir -p $(PWD)/lib
	@echo "Creating $@"; $(CC) -shared $(LIB_OBJ) $(LFLAGS) -o $@

# Driver compile
driver:
	make -C $(PWD)/drivers driver
	make -C $(PWD)/drivers app

# Driver install
driver_install:
	make -C $(PWD)/drivers linux_install
	make -C $(PWD)/drivers rce_install

