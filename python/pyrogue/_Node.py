#!/usr/bin/env python
#-----------------------------------------------------------------------------
# Title      : PyRogue base module - Node Classes
#-----------------------------------------------------------------------------
# File       : pyrogue/_Node.py
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
import sys
from collections import OrderedDict as odict
import logging
import re
import inspect
import pyrogue as pr
import Pyro4
import functools as ft

def logInit(cls=None,name=None):
    """Init a logging pbject. Set global options."""
    logging.basicConfig(
        #level=logging.NOTSET,
        format="%(levelname)s:%(name)s:%(message)s",
        stream=sys.stdout)

    msg = 'pyrogue'
    if cls: msg += "." + cls.__class__.__name__
    if name: msg += "." + name
    return logging.getLogger(msg)


class NodeError(Exception):
    """ Exception for node manipulation errors."""
    pass

class Node(object):
    """
    Class which serves as a managed obect within the pyrogue package. 
    Each node has the following public fields:
        name: Global name of object
        description: Description of the object.
        hidden: Flag to indicate if object should be hidden from external interfaces.
        classtype: text string matching name of node sub-class
        path: Full path to the node (ie. node1.node2.node3)

    Each node is associated with a parent and has a link to the top node of a tree.
    A node has a list of sub-nodes as well as each sub-node being attached as an
    attribute. This allows tree browsing using: node1.node2.node3
    """

    def __init__(self, name, description="", hidden=False):
        """Init the node with passed attributes"""

        # Public attributes
        self._name        = name
        self._description = description
        self._hidden      = hidden
        self._path        = name

        # Tracking
        self._parent = None
        self._root   = self
        self._nodes  = odict()

        # Setup logging
        self._log = logInit(self,name)

    @Pyro4.expose
    @property
    def name(self):
        return self._name

    @Pyro4.expose
    @property
    def description(self):
        return self._description

    @Pyro4.expose
    @property
    def hidden(self):
        return self._hidden

    @Pyro4.expose
    @property
    def path(self):
        return self._path

    def __repr__(self):
        return self.path

    def __getattr__(self, name):
        """Allow child Nodes with the 'name[key]' naming convention to be accessed as if they belong to a 
        dictionary of Nodes with that 'name'.
        This override builds an OrderedDict of all child nodes named as 'name[key]' and returns it.
        Raises AttributeError if no such named Nodes are found. """

        ret = odict()
        rg = re.compile('{:s}\\[(.*?)\\]'.format(name))
        for k,v in self._nodes.items():
            m = rg.match(k)
            if m:
                key = m.group(1)
                if key.isdigit():
                    key = int(key)
                ret[key] = v

        if len(ret) == 0:
            raise AttributeError('{} has no attribute {}'.format(self, name))

        print("returning {}".format(ret))
        return ret

    def add(self,node):
        """Add node as sub-node"""

        # Error if added node already has a parent
        if node._parent is not None:
            raise NodeError('Error adding %s with name %s to %s. Node is already attached.' % 
                             (str(node.classType),node.name,self.name))

        # Names of all sub-nodes must be unique
        if node.name in self._nodes:
            raise NodeError('Error adding %s with name %s to %s. Name collision.' % 
                             (str(node.classType),node.name,self.name))

        # Attach directly as attribute and add to ordered node dictionary
        setattr(self,node.name,node)
        self._nodes[node.name] = node 

        # Update path related attributes
        node._updateTree(self)

    def addNode(self, nodeClass, **kwargs):
        self.add(nodeClass(**kwargs))

    def addNodes(self, nodeClass, number, stride, **kwargs):
        name = kwargs.pop('name')
        offset = kwargs.pop('offset')
        for i in range(number):
            self.add(nodeClass(name='{:s}[{:d}]'.format(name, i), offset=offset+(i*stride), **kwargs))

    def _getNodes(self,typ):
        """
        Get a ordered dictionary of nodes.
        pass a class type to receive a certain type of node
        """
        return odict([(k,n) for k,n in self._nodes.items() if isinstance(n, typ)])

    @Pyro4.expose
    @property
    def nodes(self):
        """
        Get a ordered dictionary of all nodes.
        """
        return self._nodes

    @Pyro4.expose
    @property
    def variables(self):
        """
        Return an OrderedDict of the variables but not commands (which are a subclass of Variable
        """
        return odict([(k,n) for k,n in self._nodes.items()
                      if isinstance(n, pr.BaseVariable) and not isinstance(n, pr.BaseCommand)])

    @Pyro4.expose
    @property
    def commands(self):
        """
        Return an OrderedDict of the Commands that are children of this Node
        """
        return self._getNodes(pr.BaseCommand)

    @Pyro4.expose
    @property
    def devices(self):
        """
        Return an OrderedDict of the Devices that are children of this Node
        """
        return self._getNodes(pr.Device)

    @Pyro4.expose
    @property
    def parent(self):
        """
        Return parent node or NULL if no parent exists.
        """
        return self._parent

    @Pyro4.expose
    @property
    def root(self):
        """
        Return root node of tree.
        """
        return self._root

    @Pyro4.expose
    def node(self, path):
        return self._nodes[path]

    def _rootAttached(self):
        """Called once the root node is attached. Can override to do anything depends on the full tree existing"""
        pass

    def _updateTree(self,parent):
        """
        Update tree. In some cases nodes such as variables, commands and devices will
        be added to a device before the device is inserted into a tree. This call
        ensures the nodes and sub-nodes attached to a device can be updated as the tree
        gets created.
        """
        self._parent = parent
        self._root   = self._parent._root
        self._path   = self._parent.path + '.' + self.name

        if isinstance(self._root, pr.Root):
            self._rootAttached()

        for key,value in self._nodes.items():
            value._updateTree(self)

    def _exportNodes(self,daemon):

        for k,n in self._nodes.items():
            daemon.register(n)

            if isinstance(n,pr.Device):
                n._exportNodes(daemon)

    def _getVariables(self,modes):
        """
        Get variable values in a dictionary starting from this level.
        Attributes that are Nodes are recursed.
        modes is a list of variable modes to include.
        Called from getYamlVariables in the root node.
        """
        data = odict()
        for key,value in self._nodes.items():
            if isinstance(value,pr.Device):
                data[key] = value._getVariables(modes)
            elif isinstance(value,pr.BaseVariable) and (value.mode in modes):
                data[key] = value.valueDisp()

        return data

    def _setOrExec(self,d,writeEach,modes):
        """
        Set variable values or execute commands from a dictionary starting 
        from this level.  Attributes that are Nodes are recursed.
        modes is a list of variable nodes to act on for variable accesses.
        Called from setOrExecYaml in the root node.
        """
        for key, value in d.items():

            # Entry is in node list
            if key in self._nodes:

                # If entry is a device, recurse
                if isinstance(self._nodes[key],pr.Device):
                    self._nodes[key]._setOrExec(value,writeEach,modes)

                # Execute if command
                elif isinstance(self._nodes[key],pr.BaseCommand):
                    self._nodes[key](value)

                # Set value if variable with enabled mode
                elif isinstance(self._nodes[key],pr.BaseVariable) and (self._nodes[key].mode in modes):
                    self._nodes[key].setDisp(value,writeEach)


class PyroNode(object):
    def __init__(self, node,daemon):
        self._node   = node
        self._nodes  = None
        self._daemon = daemon

    def __repr__(self):
        return self._node.path

    def __getattr__(self, name):
        if self._nodes is None:
            self._nodes = self._convert(self._node.nodes)

        if name in self._nodes:
            return self._nodes[name]
        else:
            return self._node.__getattr__(name)

    def _convert(self,d):
        ret = odict()
        for k,n in d.items():

            if isinstance(n,dict):
                ret[k] = PyroNode(Pyro4.util.SerializerBase.dict_to_class(n),self._daemon)
            else:
                ret[k] = PyroNode(n,self._daemon)

        return ret

    def addInstance(self,node):
        self._daemon.register(node)

    def node(self, path):
        return PyroNode(self._node.node(path),self._daemon)

    @property
    def nodes(self):
        return self._convert(self._node.nodes)

    @property
    def variables(self):
        return self._convert(self._node.variables)

    @property
    def commands(self):
        return self._convert(self._node.commands)

    @property
    def devices(self):
        return self._convert(self._node.devices)

    @property
    def parent(self):
        return PyroNode(self._node.parent,self._daemon)

    @property
    def root(self):
        return pr.PyroRoot(self._node.root,self._daemon)

    def addListener(self, listener):
        self._daemon.register(listener)
        self._node.addListener(listener)

    def __call__(self,arg=None):
        self._node.call(arg)
