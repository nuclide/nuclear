Nuclear
=======

Nuclear is a shell plugin for Weston. It implements a custom protocol to synchronize with a
shell client such as [Orbital](https://github.com/giucam/orbital), but does not carry a client
itself, and serves no purpose without one.

## Dependencies

Nuclear depends only on Weston master aside the toolchain to build it, that is a C++11 compiler 
and CMake.

## Building

To build it run these commands from the repository root directory:

```sh
mkdir build
cd build
cmake ..
make
sudo make install
```

Nuclear will be installed in the "/usr/local" prefix, unless you specified otherwise using the 
CMAKE_INSTALL_PREFIX variable:
```sh
cmake -DCMAKE_INSTALL_PREFIX=/my/prefix ..
```

Running `make install` will install the *nuclear-shell.so* plugin in *$prefix/lib/nuclear-shell*,
the protocol file *nuclear-desktop-shell.xml* in *$prefix/share/nuclear-shell* and the pkg-config
file *nuclear.pc* in *$prefix/lib/pkgconfig*.
