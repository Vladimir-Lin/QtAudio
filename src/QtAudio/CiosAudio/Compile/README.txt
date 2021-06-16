Compilation for CIOS Audio Core

Compile tool is cmake.  You need to obtain a copy of CMake from:

http://www.cmake.org

* Separate building directory and source directory

It is highly recommented that your separate building directory and source
directory.  For example, the CiosAudio directory is SOMEWHERE/CiosAudio.
If you are going to build the CIOS Audio library, go to TEMP directory,
and mkdir CiosAudio.

cd TEMP
mkdir CiosAudio
cd CiosAudio
cmake SOMEWHERE/CiosAudio/Compile/MacOSX
(optional instructions)
cd ..
cmake-gui CiosAudio
cd CiosAudio
(optional instructions)
make

* Mac OS X

=> x64

cmake SOMEWHERE/CiosAudio/Compile/MacOSX


* Windows

=> x64

cmake -G "Visual Studio 10 Win64" SOMEWHERE/CiosAudio/Compile/Windows

=> x86

cmake SOMEWHERE/CiosAudio/Compile/Windows


* Linux

x64 / x86

cmake SOMEWHERE/CiosAudio/Compile/Linux

