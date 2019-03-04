mProfile or any other pin tool can be build outside the pin root directory or with in the pin root directory. 
Both the options are discussed in the following sections. However, you can choose to follow any one of them.

##	Setup to Build mProfile Outside the Pin Root Directory:
*	Download the mProfile source files, mProfile.cpp, mProfile.h, and mProfileAssist.h.
*	Create a working directory, *mProfileTool*, and copy the downloaded source files.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
-bash-4.2$ mkdir mProfileTool
-bash-4.2$ cd mProfileTool
-bash-4.2$ cp ~/Downloads/mProfile* .
-bash-4.2$ ls -lat
total 96
drwxr-xr-x  2 milenka milenka  4096 Mar  1 09:09 .
-rwxrwxr-x  1 milenka milenka 23558 Mar  1 09:09 mProfile.cpp
-rwxrwxr-x  1 milenka milenka 28108 Mar  1 09:09 mProfile.h
-rwxrwxr-x  1 milenka milenka 34576 Mar  1 09:09 mProfileAssist.h
drwxr-xr-x 16 root   root    4096 Mar  1 09:08 ..
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*	Copy the makefile and makefile.rules from /path/to/pin/source/tools/ManualExamples to current working directory.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
-bash-4.2$ cp /opt/pin-3.7-97619-g0d0c92f4f-gcc-linux/source/tools/ManualExamples/makefile* .
-bash-4.2$ ls -lat
total 108
drwxr-xr-x  2 milenka milenka  4096 Mar  1 09:15 .
-rwxrwxr-x  1 milenka milenka   676 Mar  1 09:15 makefile
-rwxrwxr-x  1 milenka milenka  7293 Mar  1 09:15 makefile.rules
-rwxrwxr-x  1 milenka milenka 23558 Mar  1 09:09 mProfile.cpp
-rwxrwxr-x  1 milenka milenka 28108 Mar  1 09:09 mProfile.h
-rwxrwxr-x  1 milenka milenka 34576 Mar  1 09:09 mProfileAssist.h
drwxr-xr-x 16 root   root    4096 Mar  1 09:08 ..
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
