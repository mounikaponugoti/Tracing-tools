/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2005 Intel Corporation
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */

//Reference: Albert Myers

// mProfile.cpp
// Multithreaded control flow (branch, call, ret) and data flow (load/store)
// Instruction tracing

//Basic control tracing for multithreaded programs
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <vector>
#include <limits>
#include <sys/time.h>
#include <sys/stat.h>
#include <numeric>

#include "pin.H"
#include "instlib.H"
#include "mProfileAssist.h"
#include "mProfile.h"

//Can be used to filter which parts of the binary to instrument
INSTLIB::FILTER filter;

//instrument instructions
//three seperate instrumentation routines - ASCII, ASCII with disassembly, and binary
VOID print(ADDRINT target) {
    std::cout << std::hex << target << ",";// << target2 << '\n';
}

// Is called for every trace
// Trace: Sequence of instructions that is always entered at the top and may have multiple exits.
VOID ASCIItrace(TRACE trace, VOID *v) {
    //return if not a selected routine
    if (!filter.SelectTrace(trace))
        return;

    //iterate through instructions
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);

            //From the pin manual
            //HasFallThrough is TRUE for instructions which don't change the control flow (most instructions), 
            //or for conditional branches (which might change the control flow, but might not),
            //
            //HasFallThrough is FALSE for unconditional branches and calls 
            //(where the next instruction to be executed is always explicitly specified).
            if (mcfTrace.Value() == TRUE) {	 // Profile branches
                // Is Unconditional and Direct and Taken
                if (INS_IsDirectBranchOrCall(ins) && !INS_HasFallThrough(ins)) {
                    /* Can also get the target address as shown below if using IARG_BRANCH_TARGET_ADDR giving any issues
                     const ADDRINT target = INS_DirectBranchOrCallTargetAddress(ins);
                     INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalDirect_ASCII,
                     //Args: Thread ID, Instruction Address, Target Address
                     IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, target, IARG_END); */
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalDirect_ASCII,
                        //Args: Thread ID, Instruction Address, Target Address 
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
                }
                // Is Conditional and Direct
                else if (INS_IsDirectBranchOrCall(ins) && INS_HasFallThrough(ins)) {
                    /* Can also get the target address as shown below if using IARG_BRANCH_TARGET_ADDR giving any issues
                    const ADDRINT target = INS_DirectBranchOrCallTargetAddress(ins);
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_ConditionalDirect_ASCII,
                    //Args: Thread ID, Instruction Address, Target Address, Taken?
                    IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, target, IARG_BRANCH_TAKEN, IARG_END);*/
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_ConditionalDirect_ASCII,
                        //Args: Thread ID, Instruction Address, Target Address, Taken?
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);
                }

                // Is Unconditional and Indirect - Returns are indirect, filter XEND temporary to solve the issue with this instruction
                else if ((INS_IsIndirectBranchOrCall(ins) || INS_IsRet(ins)) && (!INS_IsXend(ins))) {
                    // get statistics for only returns
                    if (INS_IsRet(ins))
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementReturns, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_END);

                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalIndirect_ASCII,
                        //Args: Thread ID, Instruction Address, Target Address 
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
                }
            }

            if (mlsTrace.Value() == TRUE) { // Profile loads/stores
                //If load instruction or store instruction ( can be both )
                if ((INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) && (!INS_IsVgather(ins)) && (!INS_IsVscatter(ins))) {
                    // counts the load and store instructions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementLoadStoreLength, IARG_FAST_ANALYSIS_CALL,
                        //Arguments: TID, is instruction is read 
                        IARG_THREAD_ID, IARG_BOOL, INS_IsMemoryRead(ins), IARG_END);

                    UINT32 memOperands = INS_MemoryOperandCount(ins);

                    //iterate over operands
                    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
                        //if operand is load
                        if (INS_MemoryOperandIsRead(ins, memOp) && TraceLoad.Value()) {
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_LoadValueDescriptor_ASCII,
                                //Arguments: TID, ins address, operand effec address , operand size in bytes
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
                        }
                        //if operand is store
                        if (INS_MemoryOperandIsWritten(ins, memOp) && TraceStore.Value()) {
                            //protect store instruction with lock
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)lock_WriteLocation, IARG_FAST_ANALYSIS_CALL,
                                //Arguments: TID, memory operand
                                IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_END);

                            IPOINT where = IPOINT_AFTER;
                            if (!INS_HasFallThrough(ins))
                            where = IPOINT_TAKEN_BRANCH;

                            INS_InsertPredicatedCall(ins, where, (AFUNPTR)Emit_StoreValueDescriptor_ASCII,
                                //Arguments: TID, ins address, operand size in bytes 
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYWRITE_SIZE, IARG_END);
                        }
                    }
                }
            }
        }
    }
}

VOID DisTrace(TRACE trace, VOID *v) {
    //return if not a selected routine
    if (!filter.SelectTrace(trace))
        return;

    //iterate through instructions
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);
            if (mcfTrace.Value() == TRUE) {
                // Is Unconditional and Direct and Taken
                if (INS_IsDirectBranchOrCall(ins) && !INS_HasFallThrough(ins))
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalDirect_ASCII_Dis,
                        //Args: Thread ID, Instruction Address, Target Address, instruction
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
                // Is Conditional and Direct
                else if (INS_IsDirectBranchOrCall(ins) && INS_HasFallThrough(ins))
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_ConditionalDirect_ASCII_Dis,
                        //Args: Thread ID, Instruction Address, Target Address, Taken?
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);

                // Is Unconditional and Indirect - Returns are indirect, filter XEND temporary to solve the issue with this instruction
                else if ((INS_IsIndirectBranchOrCall(ins) || INS_IsRet(ins)) && (!INS_IsXend(ins))) {
                    //  Get statistics for only returns
                    if (INS_IsRet(ins))
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementReturns, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_END);

                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalIndirect_ASCII_Dis,
                        //Args: Thread ID, Instruction Address, Target Address, instruction
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
                }
            }
            if (mlsTrace.Value() == TRUE) {
                //If load instruction or store instruction ( can be both )
                if ((INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) && (!INS_IsVgather(ins)) && (!INS_IsVscatter(ins))) {
                    // counts the load and store instructions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementLoadStoreLength, IARG_FAST_ANALYSIS_CALL,
                        //Arguments: TID, is instruction is read 
                        IARG_THREAD_ID, IARG_BOOL, INS_IsMemoryRead(ins), IARG_END);

                    UINT32 memOperands = INS_MemoryOperandCount(ins);
                    //iterate over operands
                    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
                        //if operand is load
                        if (INS_MemoryOperandIsRead(ins, memOp) && TraceLoad.Value()) {
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_LoadValueDescriptor_Dis,
                                //Arguments: TID, ins address, operand effec address , operand size in bytes 
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
                        }
                        //if operand is store
                        if (INS_MemoryOperandIsWritten(ins, memOp) && TraceStore.Value()) {
                            //protect store instruction with lock
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)lock_WriteLocation, IARG_FAST_ANALYSIS_CALL,
                                //Arguments: TID, memory operand                                   
                                IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_END);

                            IPOINT where = IPOINT_AFTER;
                            if (!INS_HasFallThrough(ins))
                            where = IPOINT_TAKEN_BRANCH;

                            INS_InsertPredicatedCall(ins, where, (AFUNPTR)Emit_StoreValueDescriptor_Dis,
                                //Arguments: TID, ins address, operand size in bytes
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYWRITE_SIZE, IARG_END);
                        }
                    }
                }
            }
        }
    }
}

VOID BinaryTrace(TRACE trace, VOID *v) {
    //return if not a selected routine
    if (!filter.SelectTrace(trace))
        return;

    //iterate through instructions
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);
            if (mcfTrace.Value() == TRUE) {
                // Is Unconditional and Direct and Taken
                if (INS_IsDirectBranchOrCall(ins) && !INS_HasFallThrough(ins))
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalDirect_Bin,
                        //Args: Thread ID, Instruction Address, Target Address 
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);

                // Is Conditional and Direct
                else if (INS_IsDirectBranchOrCall(ins) && INS_HasFallThrough(ins))
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_ConditionalDirect_Bin,
                        //Args: Thread ID, Instruction Address, Target Address, Taken
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);

                // Is Unconditional and Indirect - Returns are indirect, filter XEND temporary to solve the issue with this instruction
                else if ((INS_IsIndirectBranchOrCall(ins) || INS_IsRet(ins)) && (!INS_IsXend(ins))) {
                    // get statistics for only returns
                    if (INS_IsRet(ins))
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementReturns, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_END);

                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_UnconditionalIndirect_Bin,
                        //Args: Thread ID, Instruction Address, Target Address 
                        IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
                }
            }
            if (mlsTrace.Value() == TRUE) {
                //If load instruction or store instruction ( can be both )
                if ((INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) && (!INS_IsVgather(ins)) && (!INS_IsVscatter(ins))) {
                    // counts the load and store instructions
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementLoadStoreLength, IARG_FAST_ANALYSIS_CALL,
                        //Arguments: TID, is instruction is read 
                        IARG_THREAD_ID, IARG_BOOL, INS_IsMemoryRead(ins), IARG_END);

                    UINT32 memOperands = INS_MemoryOperandCount(ins);
                    //iterate over operands
                    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
                        //if operand is load
                        if (INS_MemoryOperandIsRead(ins, memOp) && TraceLoad.Value()) {
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Emit_LoadValueDescriptor_Bin,
                                //Arguments: TID, ins address, operand effec address , operand size in bytes
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);

                        }
                        //if operand is store
                        if (INS_MemoryOperandIsWritten(ins, memOp) && TraceStore.Value()) {
                            //protect store instruction with lock
                            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)lock_WriteLocation, IARG_FAST_ANALYSIS_CALL,
                                IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_END);

                            IPOINT where = IPOINT_AFTER;
                            if (!INS_HasFallThrough(ins))
                            where = IPOINT_TAKEN_BRANCH;

                            INS_InsertPredicatedCall(ins, where, (AFUNPTR)Emit_StoreValueDescriptor_Bin,
                                //Arguments: TID, ins address, operand size in bytes 
                                IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYWRITE_SIZE, IARG_END);
                        }
                    }
                }
            }
        }
    }
}
//Called when application closes
VOID Fini(INT32 code, VOID *v) {
    gettimeofday(&t2, NULL);
    //write remaining descriptors to file
    if (ASCII.Value()) {
        if (usingCompression)
            WriteASCIIDescriptorTableToCompressor(PIN_GetTid() + 1);
        else
            WriteASCIIDescriptorTableToFile(PIN_GetTid());
    }
    else {
        if (usingCompression)
            WriteBinaryDescriptorTableToCompressor(PIN_GetTid());
        else
            WriteBinaryDescriptorTableToFile(PIN_GetTid());
    }

    PrintStatistics();

    if (Dynamic.Value() == TRUE) {
        for (int i = 0; i < numThreads; i++)
            PrintDynamicStatistics(i, traceLengthInCurrentPeriod[i]);
    }
    IsEndOfApplication = TRUE;

    //close 
    closeOutput();
}

// Called when the application is about to exit
VOID PrepareForFini(VOID *v) {
    IsProcessExiting = TRUE;
}


//Called if Pin detaches from application
VOID DetachCallback(VOID *args) {
    gettimeofday(&t2, NULL);
    //write remaining descriptors to file
    if (ASCII.Value()) {
        if (usingCompression)
            WriteASCIIDescriptorTableToCompressor(PIN_GetTid() + 1);
        else
            WriteASCIIDescriptorTableToFile(PIN_GetTid());
    }
    else {
        if (usingCompression)
            WriteBinaryDescriptorTableToCompressor(PIN_GetTid());
        else
            WriteBinaryDescriptorTableToFile(PIN_GetTid());
    }

    //close file/pipe
    closeOutput();
    std::cout << "mProfile: Detaching..." << std::endl;
    PrintStatistics();

    if (Dynamic.Value() == TRUE) {
        for (int i = 0; i < numThreads; i++)
            PrintDynamicStatistics(i, traceLengthInCurrentPeriod[i]);
    }
    // delete the thread data 
    IsEndOfApplication = TRUE;
    closeOutput();
}

//Called on thread creation
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    PIN_GetLock(&cout_lock, threadid + 1);
    //This PIN tool spawns its own thread to write to file, which interrupts the logical ordering of threads
    // To maintain the logical ordering of thread id even after creating worker thread
    threadIDs.insert(std::pair<THREADID, int>(threadid, numThreads));
    if (mcfTrace.Value() == TRUE) {
        numBranches.push_back(branches);
        PeriodicNumBranches.push_back(branches);
    }
    if (mlsTrace.Value() == TRUE) {
        NumLoads.push_back(totalOps);
        NumStores.push_back(totalOps);
        PeriodicNumLoads.push_back(totalOps);
        PeriodicNumStores.push_back(totalOps);
    }
    traceLengthInCurrentPeriod.push_back(0);
    std::cout << "mProfile: thread begin " << static_cast<UINT16>(numThreads) << " " << PIN_GetTid() << std::endl;
    numThreads++;
    PIN_ReleaseLock(&cout_lock);
}

//delete thread local storage
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v) {
    //delete static_cast<UINT8*> threadIDs[threadid];
}
INT32 Usage() {
    //write out usage 
    std::cerr << KnobOutputFile.StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char * argv[]) {
    // Initialize pin
    PIN_InitSymbols();
    //if correct parameters, show usage
    if (PIN_Init(argc, argv)) return Usage();

    if (KnobOutputFile.Value() == "mProfile.out") {
        //Create time stamp for output file so user won't overwrite previous files
        time_t t = time(NULL);
        stamp = *localtime(&t);
        std::ostringstream ss;
        ss << static_cast<long long>(stamp.tm_year + 1900) << "_" << static_cast<long long>(stamp.tm_mon + 1) << "_"
            << static_cast<long long>(stamp.tm_mday) << "_" << static_cast<long long>(stamp.tm_hour) << "."
            << static_cast<long long>(stamp.tm_min) << "." << static_cast<long long>(stamp.tm_sec);
        //Concatenate file name and time stamp

        fileName = KnobOutputFile.Value() + ss.str();
    }
    else
        fileName = KnobOutputFile.Value();
    //Initialize locks
    PIN_InitLock(&table_lock);
    PIN_InitLock(&decoder_lock);
    PIN_InitLock(&cout_lock);
    PIN_InitLock(&count_lock);
    PIN_InitLock(&mem_lock);
    // reserve the memory for counters
    if (appThreads.Value() > MaxThreads)
        MaxThreads = appThreads.Value();
    // counters used to collect control flow statistics
    if (mcfTrace.Value() == TRUE) {
        numBranches.reserve(MaxThreads);
        PeriodicNumBranches.reserve(MaxThreads);
        traceLengthInCurrentPeriod.reserve(MaxThreads);
    }
    // counters used to collect data flow (load, store) statistics
    if (mlsTrace.Value() == TRUE) {
        traceLengthInCurrentPeriod.reserve(MaxThreads);
        NumLoads.reserve(MaxThreads);
        NumStores.reserve(MaxThreads);
        PeriodicNumLoads.reserve(MaxThreads);
        PeriodicNumStores.reserve(MaxThreads);
        // to avoid locks using seperate counter
        loadLength.reserve(MaxThreads);
        storeLength.reserve(MaxThreads);
    }
    //The number of instructions to trace 
    if (Length.Value() == 0)
        noLength = TRUE;
    else
        noLength = FALSE;

    if (mlsTrace.Value() == FALSE) {
        if (TraceStore.Value() == TRUE) {
            std::cout << "mProfile: Can trace stores only if mls flag is set" << std::endl;
            std::exit(-1);
        }
    }

    if (Dynamic.Value() == TRUE) {
        if (appThreads.Value() <= 0) {
            std::cout << "mProfile: Should mension the number of apllication threads" << std::endl;
            std::exit(-1);
        }
    }

    // Check the flags
    if (TraceIsOn.Value() == TRUE) {

        //If writing descriptors to binary file
        if (ASCII.Value() == FALSE) {
            if (AnnotateDisassembly.Value() == TRUE) {
                std::cout << "mProfile: Can only annotate disassembly if writing to ASCII file" << std::endl;
                std::exit(-1);
            }
            // If collecting control flow traces/statistics
            if (mcfTrace.Value() == TRUE) {
                std::cout << "mProfile: Writing control flow traces to binary file: " << fileName + "_mcf" + ".bin" << std::endl
                    << "Profile Descriptor encoding: "
                    << "ThreadID ( " << sizeof(UINT8) << " byte ), "
                    << "Instruction address ( " << sizeof(ADDRINT) << " bytes ), "
                    << "Branch target ( " << sizeof(ADDRINT) << " bytes ), "
                    << "Branch type and outcome ( " << sizeof(UINT8) << " byte ) " << std::endl << std::endl;
            }
            // If collecting data flow traces/statistics
            if (mlsTrace.Value() == TRUE) {
                std::cout << "mProfile: Writing data flow traces to binary file: " << fileName + "_mls" + ".bin" << std::endl;

                std::cout << "Profile descriptor: ";
                std::cout << "ThreadID ( " << sizeof(UINT8) << " byte ), ";

                if (TraceLoad.Value() && TraceStore.Value())
                    std::cout << "Load/Store ( " << sizeof(UINT8) << " byte ), ";

                std::cout << "Instruction Address ( " << sizeof(ADDRINT) << " bytes ), "
                    << "Operand Address ( " << sizeof(ADDRINT) << " bytes ), "
                    << "Operand Size ( " << sizeof(UINT8) << " byte ), "
                    << "Value ( Operand Size ) " << std::endl;
            }

        }
        //If writing descriptors to ASCII file	       
        else {
            // If collecting control flow traces/statistics
            if (mcfTrace.Value() == TRUE) {
                std::cout << "mProfile: Writing control flow traces to text file: " << fileName + "_mcf" + ".txt" << std::endl;
                std::cout << "Profile descriptor: ThreadID, Instruction Address, Target Address, Conditional or Unconditional, Direct or Indirect, Outcome" << std::endl << std::endl;
            }
            // If collecting data flow traces/statistics
            if (mlsTrace.Value() == TRUE) {
                std::cout << "mProfile: Writing data flow traces to text file: " << fileName + "_mls" + ".txt" << std::endl;
                std::cout << "Profile descriptor: ThreadID, ";
                if (TraceLoad.Value() && TraceStore.Value())
                    std::cout << "Load/Store, ";
                std::cout << "Instruction Address, Operand Address, Operand Size, Value" << std::endl;
            }

            //initialize decoder used to get instruction string
            if (AnnotateDisassembly.Value() == TRUE)
                xed_tables_init();
        }
    }

    //Open Output file/pipe
    openOutput();

    //megabytes to bytes, go with power of ten for disk
    //will detach if larger than 
    fileLimit = FileLimit.Value() * 1000000;

    //Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, NULL);
    //Register a function to be called when the application is about to exit
    PIN_AddPrepareForFiniFunction(PrepareForFini, 0);

    //Registers a function that will be called if the pin exits early
    PIN_AddDetachFunction(DetachCallback, NULL);

    //ThreadStart is called when an application thread is created
    PIN_AddThreadStartFunction(ThreadStart, NULL);
    //used to delete local storage
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    //instrumentation modes
    if (ASCII.Value()) {
        if (AnnotateDisassembly.Value())
            TRACE_AddInstrumentFunction(DisTrace, NULL);
        else
            TRACE_AddInstrumentFunction(ASCIItrace, NULL);
    }
    else
        TRACE_AddInstrumentFunction(BinaryTrace, NULL);

    //exception callback 
    PIN_AddContextChangeFunction(Sig, NULL);

    //Enable routine filter
    filter.Activate();
    // Start the program, never returns
    gettimeofday(&t1, NULL);
    PIN_StartProgram();

    return 0;
}
