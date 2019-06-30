#	Run mProfile Tool
##	Command to Run mProfile with Program
*	If Pin root path is added to `.bashrc`, use the command
```
pin -t obj-intel64/mProfile.so -- ./executable arguments
```
*	If Pin path is not added to `.bashrc`, use absolute path to pin executable located in the root directory.
```
/path/to/pin/pin -t obj-intel64/mProfile.so -- ./executable arguments
```
### Sample Runs
```
-bash-4.2$ pin -t obj-intel64/mProfile.so -- pwd
mProfile: thread begin 0 35857
/mnt/drive01/mProfilePinTool

-bash-4.2$ /opt/pin-3.7-97619-g0d0c92f4f-gcc-linux/pin -t obj-intel64/mProfile.so -- pwd
mProfile: thread begin 0 35860
/mnt/drive01/mProfilePinTool
```
##	mProfile Tool Switches
*mProfile* tool offers rich set of switches to profile and record traces for control flow and/or data-flow instructions.
You can see all the available options by running the following command.
```
-bash-4.2$ pwd
/mnt/drive01/mProfilePinTool

-bash-4.2$ pin -t obj-intel64/mProfile.so -h -- pwd
Pin tools switches

-a  [default 0]
        Use ASCII output file instead of binary
-c  [default 0]
        Compress trace. Supports bzip2, pbzip2, gzip, and pigz
-d  [default 0]
        Annotate descriptors with disassembly (only works when output is ASCII
-dynamic  [default 0]
        Collect the characteristics periodically. Default is off
-f  [default 50000]
        Output file size limit in MB. Tracing will end after reaching this limit. Default is 50000 MB
-filter_no_shared_libs  [default ]
        Do not instrument shared libraries
-filter_rtn
        Routines to instrument
-h  [default 0]
        Print help message (Return failure of PIN_Init() in order to allow the tool to print help message)
-help  [default 0]
        Print help message (Return failure of PIN_Init() in order to allow the tool to print help message)
-l  [default 0]
        Number of instructions to profile (default is no limit)
-load  [default 1]
        Capture traces for load instructions (default is on, only works with mls)
-logfile  [default pintool.log]
        The log file path and file name
-mcf  [default 1]
        Capture Control-flow traces (default turned on). See trace flag
-mls  [default 0]
        Capture Data-flow traces (default turned off). See trace flag
-n  [default 100000]
        Period (number of instructions) for dynamic characteristics. 
        Default is 100000 instructions (works only with dynamic analysis)
-o  [default mProfile.out]
        Specify trace output file name
-s  [default 0]
        Begin emitting branch descriptors after executing a specified number of instructions
-store  [default 0]
        Capture traces for store instructions (default is off, only works with mls)
-t  [default 0]
        Total number of application threads. Default 0 (required with -dynamic)
-trace  [default 0]
        Record and write traces to a file is turned on or off (default is off, only collects the statistics)
-unique_logfile  [default 0]
        The log file names will contain the pid

Line information controls

-discard_line_info
        Discard line information for specific module. Module name should be a short name without path, 
        not a symbolic link
-discard_line_info_all  [default 0]
        Discard line information for all modules.
-dwarf_file
        Point pin to a different file for debug information. Syntax:
        app_executable:<path_to_different_fileExaple (OS X): -dwarf_file
        get_source_app:get_s
        ource_app.dSYM/Contents/Resources/DWARF/get_source_app

Symbols controls

-ignore_debug_info  [default 0]
        Ignore debug info for the image. Symbols are taken from the symbol tables.
-reduce_rtn_size_mode  [default auto]
        Mode for RTN size reduction: delete trailing instructions after RET if there is no 
        jump to the rtn part after the RET. Possible modes are: auto/never/always
-short_name  [default 0]
        Use the shortest name for the RTN. Names with version substrings are
        preferred over the same name without the substring.
-support_jit_api  [default 0]
        Enables the Jitted Functions Support
-unrestricted_rtn_size  [default 0]
        Use the unrestricted RTN size. When set the RTN size defined by the
        distance between RTN start to the beginning of next RTN.

Statistic switches

-profile  [default 0]
        print amount of memory dynamically allocated but not yet freed by the tool
-statistic  [default 0]
        print general statistics

General switches (available in pin and tool)

-slow_asserts  [default 0]
        Perform expensive sanity checks
```
**Note:** When collecting the statistics without recording the traces (i.e. `-trace` is not set), there is no difference 
in results with default (binary traces), `-a` (ASCII format), and `-d` (disassembly) flags. Please notice that some of the flags 
`-c` and `-f` are useful only when tracing is on.

*mProfile* tool flags allow users to specify the following:
*	Whether to record and write the traces to a file or not (default, traces are not collected);
*	Format of the output trace file (ASCII or binary, default is binary);
*	Capture either control-flow, data-flow traces or both (default is control-flow);
*	Captured traces can be piped to a selected general-purpose compressor (e.g., bzip2, pbzip2, gzip, ...);
*	Control-flow and/or data-flow trace descriptors can be annotated with disassembled instructions;
*	Control-flow and/or data-flow traces descriptors can be recorded during entire benchmark execution;
*	Or during a selected program segment (`-l` to specify the number of instructions to collect statistics (and traces ), 
`-s` to specify the number of instructions to skip from the beginning of the program execution before collecting statistics 
(and traces), and `-filter_rtn` to specify the functions to instrument).
*	Output file to write the traces and/or statistics can be specified (`-o`, default mProfile.out+timestamp+mcf/mls.txt/Statistics). 
*	Collect the periodic statistics with the user defined period in terms of number of instructions (default is 100,000 instructions).
