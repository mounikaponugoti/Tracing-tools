
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

// Original Author: Albert Mayers

// mlvCFiat.cpp
// Multithreaded load instruction tracing 
// using cache first access filtering

#include <unistd.h>
#include <stdio.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <iterator>
#include <deque>
#include <vector>
#include <map>
#include <list>
#include <sys/time.h>
#include "pin.H"
#include "instlib.H"
#include "cache.h"
#include "mlvCFiatAssist.h"
#include "mlvCFiat.h"

//Can be used to filter which parts of the binary to instrument
INSTLIB::FILTER filter;

// Instrument in ASCII 
VOID ASCIILoadTrace(TRACE trace, VOID *v) {
	//return if not a selected routine 
	if (!filter.SelectTrace(trace))
		return;

	//iterate over instructions
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);

			UINT32 memOperands = INS_MemoryOperandCount(ins);
			//iterate over operands
			for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
				const UINT32 size = INS_MemoryOperandSize(ins, memOp);
				const BOOL single = (size <= 4);

				//if operand is load
				if (INS_MemoryOperandIsRead(ins, memOp)) {
					if (single) {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_ASCII_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_ASCII_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_ASCII_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_ASCII_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
					}
				}
				//if operand is store
				if (INS_MemoryOperandIsWritten(ins, memOp)) {
					if (single) {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       

					}
				}
			}
		}
	}
}

// Instrument in ASCII with annotated disassembly
VOID DisLoadTrace(TRACE trace, VOID *v) {
	//return if not a selected routine 
	if (!filter.SelectTrace(trace))
		return;
	//iterate over instructions
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);

			UINT32 memOperands = INS_MemoryOperandCount(ins);
			//iterate over operands
			for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
				const UINT32 size = INS_MemoryOperandSize(ins, memOp);
				const BOOL single = (size <= 4);

				//if operand is load
				if (INS_MemoryOperandIsRead(ins, memOp)) {
					if (single) {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_Dis_Shared,
							IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_Dis_Private,
							IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_Dis_Shared,
							IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_Dis_Private,
							IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
					}
				}
				//if operand is store
				if (INS_MemoryOperandIsWritten(ins, memOp)) {
					if (single) {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       

					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       

					}
				}
			}
		}
	}
}
// Instrument in Binary mode 
VOID BinLoadTrace(TRACE trace, VOID *v) {
	//return if not a selected routine 
	if (!filter.SelectTrace(trace))
		return;

	//iterate over instructions
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SetFastForwardAndLength, IARG_THREAD_ID, IARG_END);

			UINT32 memOperands = INS_MemoryOperandCount(ins);
			//iterate over operands
			for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
				const UINT32 size = INS_MemoryOperandSize(ins, memOp);
				const BOOL single = (size <= 4);
				//if operand is load
				if (INS_MemoryOperandIsRead(ins, memOp)) {
					if (single) { 
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_Bin_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, ins addr, operand effec address , operand size in bytes                       
						else {
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_SingleCacheLine_Bin_Private,
								IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
							//Arguments: TID, ins addr, operand effec address , operand size in bytes                       
						}
					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_Bin_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, ins addr, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Load_MultiCacheLines_Bin_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYREAD_SIZE, IARG_END);
						//Arguments: TID, ins addr, operand effec address , operand size in bytes                       
					}
				}
				//if operand is store
				if (INS_MemoryOperandIsWritten(ins, memOp)) {
					if (single) {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_SingleCacheLine_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       

					}
					else {
						if (KnobShareCache.Value())
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Shared,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       
						else
							INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Store_MultiCacheLines_Private,
							IARG_THREAD_ID, IARG_MEMORYOP_EA, memOp, IARG_MEMORYWRITE_SIZE, IARG_END);
						//Arguments: TID, operand effec address , operand size in bytes                       

					}
				}
			}
		}
	}
}
VOID Fini(INT32 code, VOID *v) {
	gettimeofday(&t2, NULL);
	//write remaining descriptors to file
	if (ASCII.Value()) {
		if (usingCompression)
			WriteASCIIDescriptorTableToCompressor(PIN_GetTid());
		else
			WriteASCIIDescriptorTableToFile(PIN_GetTid());
	}
	else {
		if (usingCompression)
			WriteBinaryDescriptorTableToCompressor(PIN_GetTid());
		else
			WriteBinaryDescriptorTableToFile(PIN_GetTid());
	}
	//close 
	closeOutput();
	PrintStatistics();

	//delete shared cache
	if (KnobShareCache.Value())
		delete sharedCache;

	//delete local caches
	else {
		for (int i = 0; i < numThreads; i++){
			tls *localStorage = allCaches[threadIDs[i]];
			delete localStorage->localCache;
		}
		allCaches.clear();
		threadIDs.clear();
	}
}

VOID PrepareForFini(VOID *v)
{
	IsProcessExiting = TRUE;
}

//Called if Pin detaches from application
VOID DetachCallback(VOID *args) {
	gettimeofday(&t2, NULL);

	std::cout << "mlvCFiat: Detaching..." << std::endl;

	//write remaining descriptors to file
	if (ASCII.Value()) {
		if (usingCompression)
			WriteASCIIDescriptorTableToCompressor(PIN_GetTid());
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
	PrintStatistics();

	//delete shared cache
	if (KnobShareCache.Value())
		delete sharedCache;

	//delete local caches
	else {
		for (int i = 0; i < numThreads; i++){
			tls *localStorage = allCaches[threadIDs[i]];
			delete localStorage->localCache;
		}
		allCaches.clear();
		threadIDs.clear();
	}
}

//called on thread creation
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	PIN_GetLock(&thread_lock, threadid + 1);
	PIN_GetLock(&cout_lock, threadid + 1);
#ifdef DEBUG
    std::cout << "threadid: " << static_cast<int> (threadid) << std::endl;
#endif

	threadIDs.push_back(threadid);
	tls *localStorage = new tls;
	localStorage->tid = threadid;
	localStorage->fahCnt = 0;

	allCaches.insert(std::pair<THREADID, tls*>(threadid, localStorage));

	if (KnobShareCache.Value()) {
		//allocate shared cache
		if (numThreads == 0) {
			sharedCache = new DL1::CACHE("L1 Data Cache", KnobCacheSize.Value() * KILO,
				KnobLineSize.Value(), KnobAssociativity.Value(), KnobGranularity.Value());
			std::cout << "Cache Size: " << sharedCache->CacheSize() << " KB" << std::endl;
			std::cout << "Line Size: " << sharedCache->LineSize() << " Bytes" << std::endl;
			std::cout << "Associativity: " << sharedCache->Associativity() << std::endl;
			std::cout << "Granularity: " << sharedCache->Granularity() << std::endl << std::endl;
		}
	}
	else {
		allCaches[threadid]->localCache = new DL1::CACHE("L1 Data Cache", KnobCacheSize.Value() * KILO,
			KnobLineSize.Value(), KnobAssociativity.Value(), KnobGranularity.Value());
		// print out configuration of one
		if (numThreads == 0) {
			std::cout << "Cache Size: " << allCaches[threadid]->localCache->CacheSize() << " KB" << std::endl;
			std::cout << "Line Size: " << allCaches[threadid]->localCache->LineSize() << " Bytes" << std::endl;
			std::cout << "Associativity: " << allCaches[threadid]->localCache->Associativity() << std::endl;
			std::cout << "Granularity: " << allCaches[threadid]->localCache->Granularity() << std::endl << std::endl;
		}
	}
	std::cout << "mlvCFiat: thread begin " << static_cast<UINT32>(numThreads) << " " << PIN_GetTid() << std::endl;
	numThreads++;
	PIN_ReleaseLock(&cout_lock);
	PIN_ReleaseLock(&thread_lock);
}

//delete thread local storage
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v) {
	tls *localStorage = new tls;
	localStorage = allCaches[threadid];

	UINT8 tid = localStorage->tid;
	UINT32 fahCnt = localStorage->fahCnt;
	//dump tid and fahCnt for thread
	if (ASCII.Value()) 	{
		std::ostringstream convert;
		convert << static_cast<UINT32>(tid);
		convert << ", " << fahCnt << std::endl;
		std::string ASCIIDescriptor = convert.str();
		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);
	}
	else {
		BinaryDescriptorTableEntry BinDescriptor;
		BinDescriptor.tid = tid;
		BinDescriptor.fahCnt = fahCnt;
		BinDescriptor.operandSize = 0;

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		binDescriptorTable.push_back(BinDescriptor);
		IncrementFileCount(sizeof(UINT8) + sizeof(UINT32));
		PIN_ReleaseLock(&table_lock);
	}

}

INT32 Usage() {
	std::cerr << KnobOutputFile.StringKnobSummary() << std::endl;
	return -1;
}

int main(int argc, char * argv[]) {
	// Initialize pin
	PIN_InitSymbols();
	if (PIN_Init(argc, argv)) return Usage();

	commandLine = "";
	for (int i = 0; i<argc - 9; i++)
		commandLine += std::string(argv[9 + i]) + " ";
	commandLine = "pin " + commandLine;
	targetName = std::string(argv[12]);

	threadIDs.clear();
	//check cache parameters
	if (!IsPower2(KnobLineSize.Value())) {
		std::cout << "Cache line size must be a power of two. " << std::endl;
		std::exit(-1);
	}
	if (!IsPower2(KnobGranularity.Value())) {
		std::cout << "Cache first access flag granularity size must be a power of two. " << std::endl;
		std::exit(-1);
	}
	if (KnobGranularity.Value() > KnobLineSize.Value()) {
		std::cout << "Cache first access flag granularity must be less than cache line size. " << std::endl;
		std::exit(-1);
	}

	if (KnobOutputFile.Value() == "mlvCFiat.out") {
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


	//Initialize locked used in analysis routines
	PIN_InitLock(&table_lock);
	PIN_InitLock(&decoder_lock);
	PIN_InitLock(&cout_lock);
	PIN_InitLock(&count_lock);
	PIN_InitLock(&cache_lock);

	// Obtain  a key for TLS storage.
	tls_key = PIN_CreateThreadDataKey(NULL);
	if (tls_key == INVALID_TLS_KEY) {
		std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" << std::endl;
		PIN_ExitProcess(1);
	}

	//The number of instructions to trace 
	if (Length.Value() == 0)
		noLength = TRUE;
	else
		noLength = FALSE;

	//If writing descripts to binary file
	if (ASCII.Value() == FALSE)	{
		if (AnnotateDisassembly.Value() == TRUE) {
			std::cout << "mlvCFiat: Can only annotate disassembly if writing to ASCII file" << std::endl;
			std::exit(-1);
		}

		std::cout << "mlvCFiat: Writing to binary file: " << fileName + ".bin" << std::endl
			<< "mlvCFiat: ThreadID ( " << sizeof(UINT8) << " byte ), "
			<< "fahCnt (" << sizeof(UINT32) << " bytes ), "
			<< "Operand Size" << " ( Variable ), "
			<< "Load Value ( Operand Size  ) " << std::endl;
	}
	//ASCII
	else {
		std::cout << "mlvCFiat: Writing to text file: " << fileName + ".txt" << std::endl;
		std::cout << "mlvCFiat descriptor: ThreadID, fahCnt, Load Value" << std::endl;

		//initialize decoder used to get instruction string
		if (AnnotateDisassembly.Value() == TRUE)
			xed_tables_init();
	}

	//Open Output file/pipe
	openOutput();

	//megabytes to bytes, go with power of ten for disk
	//will detach if larger than
	fileLimit = FileLimit.Value() * 1000000;
	//Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, NULL);
	//Register a detach function to be called if the pin exits early
	PIN_AddDetachFunction(DetachCallback, NULL);

	//ThreadStart is called when an application thread is created
	PIN_AddThreadStartFunction(ThreadStart, NULL);
	//Used to emit final descriptor
	PIN_AddThreadFiniFunction(ThreadFini, NULL);

	//insert instrumentation procedures
	if (ASCII.Value()) {
		if (AnnotateDisassembly.Value())
			TRACE_AddInstrumentFunction(DisLoadTrace, NULL);

		else
			TRACE_AddInstrumentFunction(ASCIILoadTrace, NULL);
	}
	else
		TRACE_AddInstrumentFunction(BinLoadTrace, NULL);

	// Activate procedure filter
	filter.Activate();

	// Start the program, never returns
	gettimeofday(&t1, NULL);
	PIN_StartProgram();

	return 0;
}
