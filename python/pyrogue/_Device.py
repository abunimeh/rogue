#!/usr/bin/env python
#-----------------------------------------------------------------------------
# Title      : PyRogue base module - Device Class
#-----------------------------------------------------------------------------
# File       : pyrogue/_Device.py
# Created    : 2017-05-16
#-----------------------------------------------------------------------------
# This file is part of the rogue software platform. It is subject to 
# the license terms in the LICENSE.txt file found in the top-level directory 
# of this distribution and at: 
#    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
# No part of the rogue software platform, including this file, may be 
# copied, modified, propagated, or distributed except according to the terms 
# contained in the LICENSE.txt file.
#-----------------------------------------------------------------------------
import rogue.interfaces.memory as rim
import collections
import datetime
import functools as ft
import pyrogue as pr
import inspect
import threading
import Pyro4
import math
import time

class EnableVariable(pr.BaseVariable):
    def __init__(self, *, enabled):
        pr.BaseVariable.__init__(
            self,
            description='Determines if device is enabled for hardware access',            
            name='enable',
            mode='RW',
            value=enabled, 
            disp={False: 'False', True: 'True', 'parent': 'ParentFalse'})

        self._value = enabled
        self._lock = threading.Lock()

    def nativeType(self):
        return bool

    @Pyro4.expose
    def get(self, read=False):
        ret = self._value
        with self._lock:
            if self._value is False:
                ret = False
            elif self._parent == self._root:
                #print("Root enable = {}".format(self._value))
                ret = self._value
            else:
                if self._parent._parent.enable.value() is not True:
                    ret = 'parent'
                else:
                    ret = True

        if read:
            self.updated()
        return ret
        
    @Pyro4.expose
    def set(self, value, write=True):
        with self._lock:
            if value != 'parent':
                self._value = value
        self.updated()

    def _rootAttached(self,parent,root):
        pr.Node._rootAttached(self,parent,root)

        if parent != root:
            parent._parent.enable.addListener(self)

class DeviceError(Exception):
    """ Exception for device manipulation errors."""
    pass

class Device(pr.Node,rim.Hub):
    """Device class holder. TODO: Update comments"""

    def __init__(self, *,
                 name=None,
                 description='',
                 memBase=None,
                 offset=0,
                 size=0,
                 hidden=False,
                 variables=None,
                 expand=True,
                 enabled=True):
        
        """Initialize device class"""
        if name is None:
            name = self.__class__.__name__

        # Hub.__init__ must be called first for _setSlave to work below
        rim.Hub.__init__(self,offset)

        # Blocks
        self._blocks    = []
        self._memBase   = memBase
        self._memLock   = threading.RLock()
        self._size      = size

        # Connect to memory slave
        if memBase: self._setSlave(memBase)

        # Node.__init__ can't be called until after self._memBase is created
        pr.Node.__init__(self, name=name, hidden=hidden, description=description, expand=expand)

        self._log.info("Making device {:s}".format(name))

        # Convenience methods
        self.addVariable = ft.partial(self.addNode, pr.Variable) # Legacy
        self.addVariables = ft.partial(self.addNodes, pr.Variable) # Legacy

        self.addCommand = ft.partial(self.addNode, pr.Command) # Legacy
        self.addCommands = ft.partial(self.addNodes, pr.Command) # Legacy
        self.addRemoteCommands = ft.partial(self.addNodes, pr.RemoteCommand)

        # Variable interface to enable flag
        self.add(EnableVariable(enabled=enabled))

        if variables is not None and isinstance(variables, collections.Iterable):
            if all(isinstance(v, pr.BaseVariable) for v in variables):
                # add the list of Variable objects
                self.add(variables)
            elif all(isinstance(v, dict) for v in variables):
                # create Variable objects from a dict list
                self.add(pr.Variable(**v) for v in variables)

        cmds = sorted((d for d in (getattr(self, c) for c in dir(self)) if hasattr(d, 'PyrogueCommandArgs')),
                      key=lambda x: x.PyrogueCommandOrder)
        for cmd in cmds:
            args = getattr(cmd, 'PyrogueCommandArgs')
            if 'name' not in args:
                args['name'] = cmd.__name__
            self.add(pr.LocalCommand(function=cmd, **args))

    @Pyro4.expose
    @property
    def address(self):
        return self._getAddress()

    @Pyro4.expose
    @property
    def offset(self):
        return self._getOffset()

    def add(self,node):
        # Call node add
        pr.Node.add(self,node)

        # Adding device
        if isinstance(node,Device):
           
            # Device does not have a membase
            if node._memBase is None:
                node._setSlave(self)

    def addRemoteVariables(self, number, stride, pack=False, **kwargs):
        hidden = pack or kwargs.pop('hidden', False)
        self.addNodes(pr.RemoteVariable, number, stride, hidden=hidden, **kwargs)

        # If pack specified, create a linked variable to combine everything
        if pack:
            varList = getattr(self, kwargs['name']).values()
            
            def linkedSet(dev, var, val, write):
                if val == '': return
                values = reversed(val.split('_'))
                for variable, value in zip(varList, values):
                    variable.setDisp(value, write=write)

            def linkedGet(dev, var, read):
                values = [v.getDisp(read=read) for v in varList]
                return '_'.join(reversed(values))

            name = kwargs.pop('name')
            kwargs.pop('value', None)
            
            lv = pr.LinkVariable(name=name, value='', dependencies=varList, linkedGet=linkedGet, linkedSet=linkedSet, **kwargs)
            self.add(lv)


    def hideVariables(self, hidden, variables=None):
        """Hide a list of Variables (or Variable names)"""
        if variables is None:
            variables=self.variables.values()
            
        for v in variables:
            if isinstance(v, pr.BaseVariable):
                v._hidden = hidden;
            elif isinstance(variables[0], str):
                self.variables[v]._hidden = hidden

    def softReset(self):
        pass

    def hardReset(self):
        pass

    def countReset(self):
        pass

    def writeBlocks(self, force=False, recurse=True, variable=None):
        """
        Write all of the blocks held by this Device to memory
        """
        if not self.enable.get(): return
        self._log.debug(f'Calling {self.path}._writeBlocks')

        # Process local blocks.
        if variable is not None:
            variable._block.backgroundTransaction(rim.Write)
        else:
            for block in self._blocks:
                if force or block.stale:
                    if block.bulkEn:
                        block.backgroundTransaction(rim.Write)

            if recurse:
                for key,value in self.devices.items():
                    value.writeBlocks(force=force, recurse=True)

    def verifyBlocks(self, recurse=True, variable=None):
        """
        Perform background verify
        """
        if not self.enable.get(): return

        # Process local blocks.
        if variable is not None:
            variable._block.backgroundTransaction(rim.Verify)
        else:
            for block in self._blocks:
                if block.bulkEn:
                    block.backgroundTransaction(rim.Verify)

            if recurse:
                for key,value in self.devices.items():
                    value.verifyBlocks(recurse=True)

    def readBlocks(self, recurse=True, variable=None):
        """
        Perform background reads
        """
        if not self.enable.get(): return
        self._log.debug(f'Calling {self.path}._readBlocks')

        # Process local blocks. 
        if variable is not None:
            variable._block.backgroundTransaction(rim.Read)
        else:
            for block in self._blocks:
                if block.bulkEn:
                    block.backgroundTransaction(rim.Read)

            if recurse:
                for key,value in self.devices.items():
                    value.readBlocks(recurse=True)

    def checkBlocks(self, recurse=True, variable=None):
        """Check errors in all blocks and generate variable update nofifications"""
        if not self.enable.get(): return
        self._log.debug(f'Calling {self.path}._checkBlocks')

        # Process local blocks
        if variable is not None:
            variable._block._checkTransaction()
        else:
            for block in self._blocks:
                block._checkTransaction()

            if recurse:
                for key,value in self.devices.items():
                        value.checkBlocks(recurse=True)

    def _rawTxnChunker(self, offset, data, base=pr.UInt, stride=4, wordBitSize=32, txnType=rim.Write, numWords=1):
        if wordBitSize > stride*8:
            raise pr.MemoryError(name=self.name, address=offset|self.address,
                                 msg='Called raw memory access with wordBitSize > stride')

        if txnType == rim.Write:
            if isinstance(data, bytearray):
                ldata = data
            elif isinstance(data, collections.Iterable):
                ldata = b''.join(base.mask(base.toBytes(word, wordBitSize), wordBitSize) for word in data)
            else:
                ldata = base.mask(base.toBytes(data, wordBitSize), wordBitSize)

        else:
            if data is not None:
                ldata = data
            else:
                ldata = bytearray(numWords*stride)
            
        with self._memLock:
            for i in range(offset, offset+len(ldata), self._maxTxnSize):
                sliceOffset = i | self.offset
                txnSize = min(self._maxTxnSize, len(ldata)-(i-offset))
                #print(f'sliceOffset: {sliceOffset:#x}, ldata: {ldata}, txnSize: {txnSize}, buffOffset: {i-offset}')
                self._reqTransaction(sliceOffset, ldata, txnSize, i-offset, txnType)

            return ldata

    def _rawWrite(self, offset, data, base=pr.UInt, stride=4, wordBitSize=32):
        with self._memLock:
            self._rawTxnChunker(offset, data, base, stride, wordBitSize, txnType=rim.Write)
            self._waitTransaction(0)

            if self._getError() > 0:
                raise pr.MemoryError (name=self.name, address=sliceOffset|self.address, error=self._getError())

        
    def _rawRead(self, offset, numWords=1, base=pr.UInt, stride=4, wordBitSize=32, data=None):
        with self._memLock:
            ldata = self._rawTxnChunker(offset, data, base, stride, wordBitSize, txnType=rim.Read, numWords=numWords)
            self._waitTransaction(0)

            if self._getError() > 0:
                raise pr.MemoryError (name=self.name, address=sliceOffset|self.address, error=self._getError())

            if numWords == 1:
                return base.fromBytes(base.mask(ldata, wordBitSize))
            else:
                return [base.fromBytes(base.mask(ldata[i:i+stride], wordBitSize)) for i in range(0, len(ldata), stride)]
            

    def _buildBlocks(self):
        remVars = []

        minSize = self._reqMinAccess()

        # Process all of the variables
        for k,n in self.nodes.items():

            # Local variables have a 1:1 block association
            if isinstance(n,pr.LocalVariable):
                self._blocks.append(n._block)

            # Align to min access, create list softed by offset 
            elif isinstance(n,pr.RemoteVariable) and n.offset is not None:
                n._shiftOffsetDown(n.offset % minSize, minSize)
                remVars += [n]

        # Loop until no overlaps found
        done = False
        while done == False:
            done = True

            # Sort byte offset and size
            remVars.sort(key=lambda x: (x.offset, x.varBytes))

            # Look for overlaps and adjust offset
            for i in range(1,len(remVars)):

                # Variable overlaps the range of the previous variable
                if (remVars[i].offset != remVars[i-1].offset) and (remVars[i].offset <= (remVars[i-1].varBytes-1)):
                    self._log.warning("Overlap detected cur offset={} prev offset={} prev bytes={}".format(remVars[i].offset,remVars[i-1].offset,remVars[i-1].varBytes))
                    remVars[i]._shiftOffsetDown(remVars[i].offset - remVars[i-1].offset, minSize)
                    done = False
                    break

        # Add variables
        for n in remVars:
            if not any(block._addVariable(n) for block in self._blocks):
                self._log.debug("Adding new block {} at offset {:#02x}".format(n.name,n.offset))
                self._blocks.append(pr.RemoteBlock(variable=n))

    def _rootAttached(self, parent, root):
        pr.Node._rootAttached(self, parent, root)

        self._maxTxnSize = self._reqMaxAccess()
        self._minTxnSize = self._reqMinAccess()        

        for key,value in self._nodes.items():
            value._rootAttached(self,root)

        self._buildBlocks()

        # Some variable initialization can run until the blocks are built
        for v in self.variables.values():
            v._finishInit()


    def _devReset(self,rstType):
        """Generate a count, soft or hard reset"""

        if rstType == 'hard':
            self.hardReset()
        elif rstType == 'soft':
            self.softReset()
        elif rstType == 'count':
            self.countReset()

        # process remaining blocks
        for key,value in self._nodes.items():
            if isinstance(value,Device):
                value._devReset(rstType)

    def _setTimeout(self,timeout):
        """
        Set timeout value on all devices & blocks
        """

        for block in self._blocks:
            block.timeout = timeout

        rim.Master._setTimeout(self, timeout*1000000)

        for key,value in self._nodes.items():
            if isinstance(value,Device):
                value._setTimeout(timeout)

    def command(self, **kwargs):
        """A Decorator to add inline constructor functions as commands"""
        def _decorator(func):
            if 'name' not in kwargs:
                kwargs['name'] = func.__name__

            fargs = inspect.getfullargspec(func).args

            # Handle functions with the wrong arg name and genere warning
            if len(fargs) > 0 and 'arg' not in fargs:
                self._log.warning("Decorated init functions must have the parameter name 'arg': {}".format(self.path))

                def newFunc(arg):
                    return func(arg)

                self.add(pr.LocalCommand(function=newFunc, **kwargs))
            else:
                self.add(pr.LocalCommand(function=func, **kwargs))

            return func
        return _decorator


class DataWriter(Device):
    """Special base class to control data files. TODO: Update comments"""

    def __init__(self, *, name, **kwargs):
        """Initialize device class"""

        Device.__init__(self, name=name, **kwargs)

        self.add(pr.LocalVariable(
            name='dataFile',
            mode='RW',
            value='',
            description='Data file for storing frames for connected streams.'))

        self.add(pr.LocalVariable(
            name='open',
            mode='RW',
            value=False,
            localSet=self._setOpen,
            description='Data file open state'))

        self.add(pr.LocalVariable(
            name='bufferSize',
            mode='RW',
            value=0,
            localSet=self._setBufferSize,
            description='File buffering size. Enables caching of data before call to file system.'))

        self.add(pr.LocalVariable(
            name='maxFileSize',
            mode='RW',
            value=0,
            localSet=self._setMaxFileSize,
            description='Maximum size for an individual file. Setting to a non zero splits the run data into multiple files.'))

        self.add(pr.LocalVariable(
            name='fileSize',
            mode='RO',
            value=0,
            pollInterval=1,
            localGet=self._getFileSize,
            description='Size of data files(s) for current open session in bytes.'))

        self.add(pr.LocalVariable(
            name='frameCount',
            mode='RO',
            value=0,
            pollInterval=1,
            localGet=self._getFrameCount,
            description='Frame in data file(s) for current open session in bytes.'))

        self.add(pr.LocalCommand(
            name='autoName',
            function=self._genFileName,
            description='Auto create data file name using data and time.'))

    def _setOpen(self,value,changed):
        """Set open state. Override in sub-class"""
        pass

    def _setBufferSize(self,value):
        """Set buffer size. Override in sub-class"""
        pass

    def _setMaxFileSize(self,value):
        """Set max file size. Override in sub-class"""
        pass

    def _getFileSize(self):
        """get current file size. Override in sub-class"""
        return(0)

    def _getFrameCount(self):
        """get current file frame count. Override in sub-class"""
        return(0)

    def _genFileName(self):
        """
        Auto create data file name based upon date and time.
        Preserve file's location in path.
        """
        idx = self.dataFile.value().rfind('/')

        if idx < 0:
            base = ''
        else:
            base = self.dataFile.value()[:idx+1]

        self.dataFile.set(base + datetime.datetime.now().strftime("%Y%m%d_%H%M%S.dat")) 

class RunControl(Device):
    """Special base class to control runs. TODO: Update comments."""

    def __init__(self, *, name, description='Run Controller', hidden=True, rates=None, states=None, cmd=None):
        """Initialize device class"""

        if rates is None:
            rates={1:'1 Hz', 10:'10 Hz'}

        if states is None:
            states={0:'Stopped', 1:'Running'}

        Device.__init__(self, name=name, description=description, hidden=hidden)

        value = [k for k,v in states.items()][0]

        self._thread = None
        self._cmd = cmd

        self.add(pr.LocalVariable(
            name='runState',
            value=value,
            mode='RW',
            disp=states,
            localSet=self._setRunState,
            description='Run state of the system.'))

        value = [k for k,v in rates.items()][0]

        self.add(pr.LocalVariable(
            name='runRate',
            value=value,
            mode='RW',
            disp=rates,
            localSet=self._setRunRate,
            description='Run rate of the system.'))

        self.add(pr.LocalVariable(
            name='runCount',
            value=0,
            mode='RW',
            pollInterval=1,
            description='Run Counter updated by run thread.'))

    def _setRunState(self,value,changed):
        """
        Set run state. Reimplement in sub-class.
        Enum of run states can also be overriden.
        Underlying run control must update runCount variable.
        """
        if changed:
            if self.runState.valueDisp() == 'Running':
                #print("Starting run")
                self._thread = threading.Thread(target=self._run)
                self._thread.start()
            elif self._thread is not None:
                #print("Stopping run")
                self._thread.join()
                self._thread = None

    def _setRunRate(self,value):
        """
        Set run rate. Reimplement in sub-class if neccessary.
        """
        pass

    def _run(self):
        #print("Thread start")
        self.runCount.set(0)

        while (self.runState.valueDisp() == 'Running'):
            time.sleep(1.0 / float(self.runRate.value()))
            if self._cmd is not None:
                self._cmd()

            self.runCount.set(self.runCount.value() + 1)
        #print("Thread stop")

