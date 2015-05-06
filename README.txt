//===----------------------------------------------------------------------===//
// Fortran Language Family Front-end
//===----------------------------------------------------------------------===//

flang:
  n. 1. A miner's two-pointed pick.

Flang is a Fortran front-end.

//===----------------------------------------------------------------------===//
// Compiling Flang (master branch)
//===----------------------------------------------------------------------===//

Download LLVM source and move to a branch with a reference known to work with
Flang.

> git clone https://github.com/llvm-mirror/llvm.git
> cd llvm
> git clone http://llvm.org/git/llvm.git

Download the Flang source code within the LLVM tree, the two will be compiled
together.

> cd llvm/tools & git clone git://github.com/llvm-flang/flang.git

Compile LLVM and Flang together.

> mkdir build
> cd build & cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
> cd build & make
> cd build & make install

You will also need to install the Flang standard library, which the Flang 
compiler links to by default. This is built outside the LLVM tree.

> git clone git://github.com/llvm-flang/libflangrt.git
> cd libflangrt
> mkdir build
> cd build & cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
> cd build & make
> cd build & make install

//===----------------------------------------------------------------------===//
// Using flang
//===----------------------------------------------------------------------===//

Flang's driver will instruct the linker to link with the libflang runtime. 
You can get libflang at https://github.com/llvm-flang/libflangrt . Once you have libflang, 
you'll need to tell flang where it is - you can use the -L option (e.g. -L~/libflang).

//===----------------------------------------------------------------------===//
// To Do List
//===----------------------------------------------------------------------===//

Short term:

* Fix lexing bugs
  - Fixed form for numerical literals (i.e. ignore whitespace)
  - Continuations in BOZ literals
  - Others
* 'INCLUDE' which search for files in the directory of the current file first.
* Full parsing of statements

Long term:

* Flang driver (?)
* Parsing GNU modules
* Add (or hoist) Clang style TargetInfo class template

Longer term:

* Fortran90/95 support
* IO support.
