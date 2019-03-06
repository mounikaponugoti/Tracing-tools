#	Setup mProfile Tool
* Prerequisites: Linux computer with intel Pin tool.
*	Latest version of pin can be downloaded [here](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads).
*	To learn more about Pin visit [here](https://software.intel.com/sites/landingpage/pintool/docs/97619/Pin/html/) <br/>

mProfile or any other pin tool can be build outside the pin root directory or with in the pin root directory. 
Both the options are discussed in the following sections. However, you can choose to follow any one of them.

##	Setup to Build mProfile Outside the Pin Root Directory:
*	Download the mProfile source files, *mProfile.cpp*, *mProfile.h*, and *mProfileAssist.h*.
*	Create a working directory, `mProfileTool`, and copy the downloaded source files.
```
-bash-4.2$ mkdir mProfileTool
-bash-4.2$ cd mProfileTool
-bash-4.2$ cp ~/Downloads/mProfile* .
-bash-4.2$ ls 
mProfileAssist.h   mProfile.cpp    mProfile.h 
```

*	Copy the `makefile` and `makefile.rules` from `/path/to/pin/source/tools/ManualExamples` to current working directory.
```
-bash-4.2$ cp /opt/pin-3.7-97619-g0d0c92f4f-gcc-linux/source/tools/ManualExamples/makefile* .
-bash-4.2$ ls -lat
makefile  makefile.rules  mProfileAssist.h   mProfile.cpp  mProfile.h 
```

*	Either add the pin root path, `PIN_ROOT=/opt/pin-3.7-97619-g0d0c92f4f-gcc-linux`, to `makefile` as shown below or run the make command with `PIN_ROOT`(see Build mProfile section). 
```
PIN_ROOT=/opt/pin-3.7-97619-g0d0c92f4f-gcc-linux
ifdef PIN_ROOT
CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
else
CONFIG_ROOT := ../Config
endif
include $(CONFIG_ROOT)/makefile.config
include makefile.rules
include $(TOOLS_ROOT)/Config/makefile.default.rules
```

##	Setup to Build mProfile Inside the Pin Root Directory:
*	Copy the source files, *mProfile.cpp*, *mProfile.h*, and *mProfileAssist.h* to `/path/to/pin/source/tools/ManualExamples`.<br/>
  **Note:** Please note that any directory in `/path/to/pin/source/tools` which has `makefile` and `makefile.rules` can be used. 
*	(optional) Add mProfile to the `TEST_TOOL_ROOTS` line in `/path/to/pin/source/tools/ManualExamples/makefile.rules` to enable building mPrile with `make all`.<br/>
  **Note:** If you are using other than ManualExamples, please edit the `makefile.rules` in the corresponding directory. 

##	Build mProfile:
*	Add `$(CPP11FLAGS)` to the `TOOL_CXXFLAGS` line in `/path/to/pin/source/tools/Config/makefile.unix.config`
*	To build 32-bit and 64-bit targets, create obj-ia32 and obj-intel64 directories respectively in the current working directory
```
-bash-4.2$ pwd
/mnt/drive01/mProfileTool
-bash-4.2$ mkdir obj-intel64 obj-ia32
```
* To build only mProfile tool for 64-bit target, include `TARGET=intel64` while invoking `make`. By default, `TARGET` is set to `intel64`.
```
-bash-4.2$ make obj-intel64/mProfile.so TARGET=intel64
```
* To build only mProfile tool for 32-bit target, include `TARGET=ia32` while invoking `make`.
```
-bash-4.2$ make obj-ia32/mProfile.so TARGET=ia32
```
* If makefile is not modified to include `PIN_ROOT`, use the following command to build mProfile from outside of pin root directory
```
-bash-4.2$ make obj-intel64/mProfile.so TARGET=intel64 PIN_ROOT=/opt/pin-3.7-97619-g0d0c92f4f-gcc-linux
-bash-4.2$ make obj-ia32/mProfile.so TARGET=ia32 PIN_ROOT=/opt/pin-3.7-97619-g0d0c92f4f-gcc-linux
```
* To build all the available tools in ManualExamples directory, run `make all`
```
-bash-4.2$ pwd
/opt/pin-3.7-97619-g0d0c92f4f-gcc-linux/source/tools/ManualExamples
-bash-4.2$ make all
```

