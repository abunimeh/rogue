# ZeroMq

Choose an install location:

> mkdir /path/to/zeromq/4.2.1
> mkdir /path/to/zeromq/4.2.1/src/

Zeromq libraries

> cd /path/to/zeromq/4.2.1/src/
> wget https://github.com/zeromq/libzmq/releases/download/v4.2.0/zeromq-4.2.1.tar.gz
> tar -xvvzpf zeromq-4.2.1.tar.gz
> cd zeromq-4.2.1
> ./autogen.sh
> ./configure --prefix=/path/to/zeromq/4.2.1/
> make 
> make install

Setup environment

Add /path/to/zeromq/4.2.1/bin to your $PATH
Add /path/to/zeromq/4.2.1/lib to your $LD_LIBRARY_PATH

It is recommended to create a settings.csh and settings.sh file in
/path/to/zeromq/4.2.1 to allow users to add this specific python
install to their environment when needed. ZEROMQ_PATH should be set
to allow makefiles to include the proper include files and libraries.

