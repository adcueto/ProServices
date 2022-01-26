# vscode-tdxlinux
This is forked from https://github.com/Wind-River/vscode-wrlinux.git. 
This proeject is for deugging c/c++ code on Toradex SoM with Linux when using Visual Studio Code IDE. SDK is built by Yocto Project. 

c_cpp_properties.json
Basic project setting, including macro define _aarch64__ and __LP64__ for aarch64 for iMX8 based SoMs. intelliSenseMode is set to linux-gcc-arm64. If you use
32bit Arm SoC, change it accordingly. 

launch.json
Using this file to set gdb. It uses gdbserver extended functions. Uplod program by gdbserver.

tasks.json
Because VS Code can't understand Yocto SDK now. We have to manually config build tasks. The vars in env come from Yocto SDK environment file 
e.g. environment-setup-aarch64-tdx-linux. Serveral tasks are provided, compile single file, build entire project by Makefile. One will compose Makefile
by herself/himself.


