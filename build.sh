#!/bin/sh

#call ..\bin\buildsuper_x64-win.bat .\4coder_fleury.cpp
../bin/buildsuper_x64-linux.sh 4coder_fleury.cpp

cp -f custom_4coder.so ../../.
#copy .\custom_4coder.dll ..\..\custom_4coder.dll
#copy .\custom_4coder.pdb ..\..\custom_4coder.pdb
