# Tracing-tools
This repository includes collection of **Pin** based tracing and profiling tools for control flow and data flow (memory reads and memory writes)

## What is Pin?
Pin is a dynamic binary instrumentation framework developed by [Intel](https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool) for the IA-32, x86-64, and MIC instruction set architectures. This framework helps to create dynamic program analysis tools. Pin can instrument the binaries of the application program dynamically. Thus, it does not require any source code modification and recompilation of source code.

## mProfile
mProfile is a pin tool developed to record the control-flow (branches, calls, and return) and/or data-flow 
(memory reads and writes) traces and/or statistics of multithreaded programs. Additionally, it is also capable of collect periodic statistics when the program is running to help the user to analyze the hot regions of the program. 
To learn more about different features of mProfile tool [click here](mProfile/docs/examples_mProfile.md).
