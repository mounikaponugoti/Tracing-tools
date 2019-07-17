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

//mlvCFiat.h
//Contains globals and utility procedures

//KNOB types used to parse tool parameters
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
	"o", "mlvCFiat.out", "specify trace output file name");
KNOB<BOOL> ASCII(KNOB_MODE_WRITEONCE, "pintool",
	"a", "0", "use ASCII output file instead of binary");
KNOB<UINT64> Skip(KNOB_MODE_WRITEONCE, "pintool",
	"s", "0", "Begin emitting descriptors after executing a specified number of instructions ");
KNOB<UINT64> Length(KNOB_MODE_WRITEONCE, "pintool",
	"l", "0", "Number of instructions to profile (default is no limit)");
KNOB<std::string> Compression(KNOB_MODE_WRITEONCE, "pintool",
	"c", "0", "Compress trace. Supports bzip2, pbzip2, gzip, and pigz");
KNOB<BOOL> AnnotateDisassembly(KNOB_MODE_WRITEONCE, "pintool",
	"d", "0", "Annotate descriptors with disassembly (only works when output is ASCII");
KNOB<UINT64> FileLimit(KNOB_MODE_WRITEONCE, "pintool",
	"f", "50000", "Output file size limit in MB. Tracing will end after reaching this limit. Default is 50000 MB");
//Cache options
KNOB<UINT32> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool",
	"cs", "32", "Cache size in kilobytes");
KNOB<UINT32> KnobLineSize(KNOB_MODE_WRITEONCE, "pintool",
	"cls", "32", "Cache line size in bytes");
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
	"ca", "4", "Cache associativity ");
KNOB<UINT32> KnobGranularity(KNOB_MODE_WRITEONCE, "pintool",
	"cfg", "4", "Cache first access flag granularity size");
KNOB<BOOL> KnobShareCache(KNOB_MODE_WRITEONCE, "pintool",
	"cshare", "0", "Global cache and first access flags. Threads have local cache by default.");
// wrap configuation constants into their own name space to avoid name clashes
namespace DL1 {
	const UINT32 max_sets = KILO; // cacheSize / (lineSize * associativity);
	const UINT32 max_associativity = 256; // associativity;
	const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
	typedef CACHE_MODEL(max_sets, max_associativity, allocation) CACHE;
}

//used for shared cache
DL1::CACHE* sharedCache = NULL;
//holds the number of threads
UINT8 numThreads;

static TLS_KEY tls_key = INVALID_TLS_KEY;

typedef struct {
	UINT32 tid;
	UINT32 fahCnt;
	//used when cache is private
	DL1::CACHE *localCache;
} tls;

std::map<int, tls*> allCaches;
volatile BOOL IsProcessExiting = FALSE;

//protects descriptor table
PIN_LOCK table_lock;
//protects xed decoder
PIN_LOCK decoder_lock;
//protects streams
PIN_LOCK cout_lock;
//protects statistics globals
PIN_LOCK count_lock;
//protects cache simulator for mlvCFiat
PIN_LOCK cache_lock;
//protects thread creation and accessing of thread data before they even created
PIN_LOCK thread_lock;

//Globals for file
struct tm stamp;
std::string fileName;
//Used to find pin tool overhead time.
timeval t1, t2;

//ofstream for raw files
std::ofstream OutFile;
//FILE for piping to compression utility
FILE *outPipe;

//target binary and paramters
std::string commandLine;
std::string targetName;
//output file size limit;
UINT64 fileCount;
UINT64 fileLimit;
//number of instructions to skip;
UINT64  fastforwardLength;
//number of instructions to profile
UINT64 traceLength;
//if a length isn't specififed
BOOL noLength;
//if using compression
BOOL usingCompression;

//holds threadids
std::vector<THREADID> threadIDs;

//struct for binary file
typedef struct {
	UINT8 tid;
	//Using 32 bits based on CDF from paper
	UINT32 fahCnt;
	UINT8 operandSize;
	UINT8* data;
} BinaryDescriptorTableEntry;
UINT8 BinaryDescriptorSize = 2 * sizeof(UINT8) + sizeof(UINT32);

//holds load instruction descriptors
std::deque<BinaryDescriptorTableEntry> binDescriptorTable;
std::deque<std::string> ASCIIDescriptorTable;

//write ASCII descriptor table to file
inline VOID WriteASCIIDescriptorTableToFile(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	//write contents to disk
	while (!ASCIIDescriptorTable.empty()) {
		OutFile << ASCIIDescriptorTable.front();
		ASCIIDescriptorTable.pop_front();
	}
	PIN_ReleaseLock(&table_lock);
}

//write ASCII descriptor table to compression program
inline VOID WriteASCIIDescriptorTableToCompressor(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	//write contents to disk
	while (!ASCIIDescriptorTable.empty()) {
		fputs(ASCIIDescriptorTable.front().c_str(), outPipe);
		ASCIIDescriptorTable.pop_front();
	}
	PIN_ReleaseLock(&table_lock);
}

//write binary descriptor table to file
inline VOID WriteBinaryDescriptorTableToFile(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	while (!binDescriptorTable.empty()) {
		BinaryDescriptorTableEntry temp = binDescriptorTable.front();
		OutFile.write((char *)&temp.tid, sizeof(temp.tid));
		OutFile.write((char *)&temp.fahCnt, sizeof(temp.fahCnt));
		//if not end of thread descriptor
		if (temp.operandSize != 0) {
			OutFile.write((char *)&temp.operandSize, sizeof(temp.operandSize));
			OutFile.write((char *)temp.data, temp.operandSize);
		}
		//delete allocated memory
		delete[] temp.data;
		binDescriptorTable.pop_front();
	}
	PIN_ReleaseLock(&table_lock);
}
//write binary descriptor table to compression program
inline VOID WriteBinaryDescriptorTableToCompressor(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	while (!binDescriptorTable.empty()) {
		BinaryDescriptorTableEntry temp = binDescriptorTable.front();
		fwrite(&temp.tid, sizeof(temp.tid), 1, outPipe);
		fwrite(&temp.fahCnt, sizeof(temp.fahCnt), 1, outPipe);
		if (temp.operandSize != 0) {
			fwrite(&temp.operandSize, sizeof(temp.operandSize), 1, outPipe);
			fwrite(temp.data, temp.operandSize, 1, outPipe);
		}
		//delete allocated memory
		delete[] temp.data;
		binDescriptorTable.pop_front();
	}
	PIN_ReleaseLock(&table_lock);
}

//Worker ASCII write thread when not using compression
VOID ThreadWriteASCII(VOID *arg) {
	THREADID threadid = PIN_ThreadId();
	if (usingCompression) {
		while (1) {
			//if process is closing (entered fini()) kill thread
			if (IsProcessExiting)
				PIN_ExitThread(1);
			WriteASCIIDescriptorTableToCompressor(threadid);
		}
	}
	else {
		while (1) {
			//if process is closing (entered fini()) kill thread
			if (IsProcessExiting)
				PIN_ExitThread(1);
			WriteASCIIDescriptorTableToFile(threadid);
		}
	}
}

//Worker bin write thread when not using compression
VOID ThreadWriteBin(VOID *arg) {
	THREADID threadid = PIN_ThreadId();
	if (usingCompression) {
		while (1) {
			//if process is closing (entered fini()) kill thread
			if (IsProcessExiting)
				PIN_ExitThread(1);
			WriteBinaryDescriptorTableToCompressor(threadid);
		}
	}
	else {
		while (1) {
			//if process is closing (entered fini()) kill thread
			if (IsProcessExiting)
				PIN_ExitThread(1);
			WriteBinaryDescriptorTableToFile(threadid);
		}
	}
}

//Used to check fast forward and trace length in analysis functions
inline BOOL CanEmit(const THREADID threadid) {
	PIN_GetLock(&count_lock, threadid + 1);

	if (fastforwardLength < Skip.Value()) {
		PIN_ReleaseLock(&count_lock);
		return false;
	}
	else {
		PIN_ReleaseLock(&count_lock);
		return true;
	}
}
//check file size and exit early if too large
inline VOID IncrementFileCount(UINT32 size) {
	//increment counter
	fileCount += size;
	//if limit reached, exit
	if (fileCount >= fileLimit)
		PIN_Detach();
}

//Address width
//extended
#if defined(TARGET_IA32E)
#define ADDRWID 16
//32 bit
#else
#define ADDRWID 8
#endif

//ins contains the instruction string
inline VOID getAssemblyString(string & ins, const THREADID threadid, const ADDRINT address) {
	PIN_GetLock(&decoder_lock, threadid + 1);

	//Use Intel XED to decode the instruction based on its address
#if defined(TARGET_IA32E)
	static const xed_state_t dstate = { XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b };
#else
	static const xed_state_t dstate = { XED_MACHINE_MODE_LEGACY_32, XED_ADDRESS_WIDTH_32b };
#endif
	xed_decoded_inst_t xedd;
	xed_decoded_inst_zero_set_mode(&xedd, &dstate);

	unsigned int max_inst_len = 15;

	xed_error_enum_t xed_code = xed_decode(&xedd, reinterpret_cast<UINT8*>(address), max_inst_len);
	BOOL xed_ok = (xed_code == XED_ERROR_NONE);
	if (xed_ok) {
		char buf[2048];
		xed_uint64_t runtime_address = static_cast<xed_uint64_t>(address);
		xed_decoded_inst_dump_xed_format(&xedd, buf, 2048, runtime_address);

		ins.assign(buf);
	}
	else
		ins = "XED failed to decode this instruction";

	PIN_ReleaseLock(&decoder_lock);
}

//return percentage
inline double Percentage(const UINT64 num, const UINT64 den) {
	if ((num == 0) || (den == 0))
		//return std::numeric_limits<double>::quiet_NaN();
		return 0;
	double ratio = (double)num / (double)den;
	return 100 * ratio;
}

VOID PrintStatistics() {
	std::ofstream StatFile;
	std::string temp = fileName + ".Statistics";
	StatFile.open(temp.c_str());
	//set fixed point
	StatFile.setf(std::ios::fixed);

	double elapsedTime;
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
	StatFile << "Instrumentation Time (ms): " << elapsedTime << std::endl;

	StatFile << "Instructions Traced: " << traceLength << std::endl;
	StatFile << "Skipped Instructions: " << fastforwardLength << std::endl;
	StatFile << std::endl;
	if (KnobShareCache.Value())
		StatFile << "Global Cache" << std::endl;
	else
		StatFile << "Local Caches" << std::endl;

	//cache info
	StatFile << "Cache Size (KB): " << KnobCacheSize.Value() << std::endl;
	StatFile << "Cache Associativity: " << KnobAssociativity.Value() << std::endl;
	StatFile << "Cache Line Size (B): " << KnobLineSize.Value() << std::endl;
	StatFile << "First Access Flag Granularity (B): " << KnobGranularity.Value() << std::endl;
	StatFile << std::endl;

	UINT64 totalHits, byteHits, wordHits, dwordHits, qwordHits, extendedHits, octaHits, hexaHits, otherHits;
	UINT64 totalMisses, byteMisses, wordMisses, dwordMisses, qwordMisses, extendedMisses, octaMisses, hexaMisses, otherMisses;
	UINT64 totalHR, byteHR, wordHR, dwordHR, qwordHR, extendedHR, octaHR, hexaHR, otherHR;

	//get statistics from shared cache
	if (KnobShareCache.Value()) {
		//cache read reference statistics  
		totalHits = sharedCache->CacheReadHits_Total();
		totalMisses = sharedCache->CacheReadMisses_Total();

		byteHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
		byteMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);

		wordHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
		wordMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);

		dwordHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
		dwordMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);

		qwordHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
		qwordMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);

		extendedHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
		extendedMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);

		octaHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
		octaMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);

		hexaHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
		hexaMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);

		otherHits = sharedCache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		otherMisses = sharedCache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);


		totalHR = Percentage(totalHits, totalHits + totalMisses);
		byteHR = Percentage(byteHits, byteHits + byteMisses);
		wordHR = Percentage(wordHits, wordHits + wordMisses);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);
		octaHR = Percentage(octaHits, octaHits + octaMisses);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);
		otherHR = Percentage(hexaHits, hexaHits + hexaMisses);

		StatFile << "-- Cache Read References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;

		totalHits = byteHits = wordHits = dwordHits = qwordHits = extendedHits = octaHits = hexaHits = otherHits = 0;
		totalMisses = byteMisses = wordMisses = dwordMisses = qwordMisses = extendedMisses = octaMisses = hexaMisses = otherMisses = 0;
		totalHR = byteHR = wordHR = dwordHR = qwordHR = extendedHR = octaHR = hexaHR = otherHR = 0.0;

		//cache reference statistics   
		totalHits = sharedCache->CacheHits_Total();
		totalMisses = sharedCache->CacheMisses_Total();
		totalHR = Percentage(totalHits, totalMisses + totalHits);

		byteHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
		byteMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
		byteHR = Percentage(byteHits, byteMisses + byteHits);

		wordHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
		wordMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
		wordHR = Percentage(wordHits, wordHits + wordMisses);

		dwordHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
		dwordMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);

		qwordHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
		qwordMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);

		extendedHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
		extendedMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);

		octaHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
		octaMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
		octaHR = Percentage(octaHits, octaHits + octaMisses);

		hexaHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
		hexaMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);

		otherHits = sharedCache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		otherMisses = sharedCache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		otherHR = Percentage(otherHits, otherHits + otherMisses);

		StatFile << "-- Cache References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;

		totalHits = byteHits = wordHits = dwordHits = qwordHits = extendedHits = octaHits = hexaHits = otherHits = 0;
		totalMisses = byteMisses = wordMisses = dwordMisses = qwordMisses = extendedMisses = octaMisses = hexaMisses = otherMisses = 0;
		totalHR = byteHR = wordHR = dwordHR = qwordHR = extendedHR = octaHR = hexaHR = otherHR = 0.0;

		//first access flag reference statistics   
		totalHits = sharedCache->FAFHits_Total();
		totalMisses = sharedCache->FAFMisses_Total();
		totalHR = Percentage(totalHits, totalMisses + totalHits);

		byteHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
		byteMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
		byteHR = Percentage(byteHits, byteMisses + byteHits);

		wordHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
		wordMisses = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
		wordHR = Percentage(wordHits, wordHits + wordMisses);

		dwordHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
		dwordMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);

		qwordHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
		qwordMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);

		extendedHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
		extendedMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);

		octaHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
		octaMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
		octaHR = Percentage(octaHits, octaHits + octaMisses);

		hexaHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
		hexaMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);

		otherHits = sharedCache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		otherMisses = sharedCache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		otherHR = Percentage(otherHits, otherHits + otherMisses);

		StatFile << "-- First Access Flag References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;

	}
	//iterate over local caches
	else {
		totalHits = byteHits = wordHits = dwordHits = qwordHits = extendedHits = octaHits = hexaHits = otherHits = 0;
		totalMisses = byteMisses = wordMisses = dwordMisses = qwordMisses = extendedMisses = octaMisses = hexaMisses = otherMisses = 0;
		totalHR = byteHR = wordHR = dwordHR = qwordHR = extendedHR = octaHR = hexaHR = otherHR = 0.0;

		for (UINT8 i = 0; i < threadIDs.size(); i++) {
			tls *localStorage = allCaches[threadIDs[i]];
			DL1::CACHE* cache = localStorage->localCache;

			//cache read reference statistics  
			totalHits += cache->CacheReadHits_Total();
			totalMisses += cache->CacheReadMisses_Total();

			byteHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
			byteMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);

			wordHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
			wordMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);

			dwordHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
			dwordMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);

			qwordHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
			qwordMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);

			extendedHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
			extendedMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);

			octaHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
			octaMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);

			hexaHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
			hexaMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);

			otherHits += cache->CacheReadHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
			otherMisses += cache->CacheReadMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);

		}

		totalHR = Percentage(totalHits, totalHits + totalMisses);
		byteHR = Percentage(byteHits, byteHits + byteMisses);
		wordHR = Percentage(wordHits, wordHits + wordMisses);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);
		octaHR = Percentage(octaHits, octaHits + octaMisses);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);
		otherHR = Percentage(hexaHits, hexaHits + hexaMisses);

		StatFile << "-- Cache Read References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;

		totalHits = byteHits = wordHits = dwordHits = qwordHits = extendedHits = octaHits = hexaHits = otherHits = 0;
		totalMisses = byteMisses = wordMisses = dwordMisses = qwordMisses = extendedMisses = octaMisses = hexaMisses = otherMisses = 0;
		totalHR = byteHR = wordHR = dwordHR = qwordHR = extendedHR = octaHR = hexaHR = otherHR = 0.0f;

		for (UINT8 i = 0; i < threadIDs.size(); i++) {
			tls *localStorage = allCaches[threadIDs[i]]; 
			DL1::CACHE* cache = localStorage->localCache;

			//cache reference statistics   
			totalHits += cache->CacheHits_Total();
			totalMisses += cache->CacheMisses_Total();

			byteHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
			byteMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);

			wordHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
			wordMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);

			dwordHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
			dwordMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);

			qwordHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
			qwordMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);

			extendedHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
			extendedMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);

			octaHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
			octaMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);

			hexaHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
			hexaMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);

			otherHits += cache->CacheHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
			otherMisses += cache->CacheMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		}

		totalHR = Percentage(totalHits, totalHits + totalMisses);
		byteHR = Percentage(byteHits, byteHits + byteMisses);
		wordHR = Percentage(wordHits, wordHits + wordMisses);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);
		octaHR = Percentage(octaHits, octaHits + octaMisses);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);
		otherHR = Percentage(hexaHits, hexaHits + hexaMisses);

		StatFile << "-- Cache References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;

		totalHits = byteHits = wordHits = dwordHits = qwordHits = extendedHits = octaHits = hexaHits = otherHits = 0;
		totalMisses = byteMisses = wordMisses = dwordMisses = qwordMisses = extendedMisses = octaMisses = hexaMisses = otherMisses = 0;
		totalHR = byteHR = wordHR = dwordHR = qwordHR = extendedHR = octaHR = hexaHR = otherHR = 0.0;

		for (UINT8 i = 0; i < threadIDs.size(); i++) {
			tls *localStorage = allCaches[threadIDs[i]]; 
			DL1::CACHE* cache = localStorage->localCache;

			//cache reference statistics   
			totalHits += cache->FAFHits_Total();
			totalMisses += cache->FAFMisses_Total();

			byteHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);
			byteMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_BYTE);

			wordHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);
			wordMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_WORD);

			dwordHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);
			dwordMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_DOUBLEWORD);

			qwordHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);
			qwordMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_QUADWORD);

			extendedHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);
			extendedMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_EXTENDEDPRECISION);

			octaHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);
			octaMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OCTAWORD);

			hexaHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);
			hexaMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_HEXAWORD);

			otherHits += cache->FAFHits_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
			otherMisses += cache->FAFMisses_Operand(FA_CACHE_BASE::OPERAND_TYPE_OTHER);
		}

		totalHR = Percentage(totalHits, totalHits + totalMisses);
		byteHR = Percentage(byteHits, byteHits + byteMisses);
		wordHR = Percentage(wordHits, wordHits + wordMisses);
		dwordHR = Percentage(dwordHits, dwordHits + dwordMisses);
		qwordHR = Percentage(qwordHits, qwordHits + qwordMisses);
		extendedHR = Percentage(extendedHits, extendedHits + extendedMisses);
		octaHR = Percentage(octaHits, octaHits + octaMisses);
		hexaHR = Percentage(hexaHits, hexaHits + hexaMisses);
		otherHR = Percentage(hexaHits, hexaHits + hexaMisses);

		StatFile << "-- First Access Flag References Hits:Misses (Hit Rate)" << std::endl;
		StatFile << "\tTotal " << totalHits << ":" << totalMisses << "(" << totalHR << "%)" << std::endl;
		StatFile << "\tByte Operands " << byteHits << ":" << byteMisses << "(" << byteHR << "%)" << std::endl;
		StatFile << "\tWord Operands " << wordHits << ":" << wordMisses << "(" << wordHR << "%)" << std::endl;
		StatFile << "\tDoubleword Operands " << dwordHits << ":" << dwordMisses << "(" << dwordHR << "%)" << std::endl;
		StatFile << "\tQuadword Operands " << qwordHits << ":" << qwordMisses << "(" << qwordHR << "%)" << std::endl;
		StatFile << "\tExtended Precision Operands " << extendedHits << ":" << extendedMisses << "(" << extendedHR << "%)" << std::endl;
		StatFile << "\tOctaword Operands " << octaHits << ":" << octaMisses << "(" << octaHR << "%)" << std::endl;
		StatFile << "\tHexaword Operands " << hexaHits << ":" << hexaMisses << "(" << hexaHR << "%)" << std::endl;
		StatFile << "\tOther Sized Operands " << otherHits << ":" << otherMisses << "(" << otherHR << "%)" << std::endl;
	}
}

VOID openOutput() {
	//Check compression knob
	string compressor = Compression.Value();
	if (compressor != "0" && compressor != "bzip2" && compressor != "pbzip2" && compressor != "gzip" && compressor != "pigz") {
		std::cout << "mlvCFiat: That compression program is not supported" << std::endl;
		std::exit(-1);
	}
	if (compressor != "0") {
		//const string temp = "/bin/" + compressor;
		if (access(string("/bin/" + compressor).c_str(), X_OK) == -1 && access(string("/usr/bin/" + compressor).c_str(), X_OK) == -1) {
			std::cout << "mlvCFiat: This system does not support " << compressor << ". Exiting..." << std::endl;
			std::exit(-1);
		}
		usingCompression = TRUE;
	}
	else
		usingCompression = FALSE;

	//attach extension
	string tempFileName;
	if (ASCII.Value() == FALSE)
		tempFileName = fileName + ".bin";
	else
		tempFileName = fileName + ".txt";


	if (usingCompression) {
		char temp[1000] = { 0 };
		if (compressor == "bzip2") {
			tempFileName += ".bz2";
			sprintf(temp, "bzip2 > %s", tempFileName.c_str());
			outPipe = popen(temp, "w");
		}
		else if (compressor == "pbzip2") {
			tempFileName += ".bz2";
			sprintf(temp, "pbzip2 > %s", tempFileName.c_str());
			outPipe = popen(temp, "w");
		}
		else if (compressor == "gzip") {
			tempFileName += ".gz";
			sprintf(temp, "gzip > %s", tempFileName.c_str());
			outPipe = popen(temp, "w");
		}
		else {
			tempFileName += ".gz";
			sprintf(temp, "pigz > %s", tempFileName.c_str());
			outPipe = popen(temp, "w");
		}

		if (outPipe == NULL) {
			std::cout << "mlvCFiat: Error opening outPipe..." << std::endl;
			std::exit(-1);
		}
	}

	//if not using compression, write to raw file
	else {
		if (ASCII.Value() == FALSE)
			OutFile.open(tempFileName.c_str(), std::ios::out | std::ios::binary);
		else
			OutFile.open(tempFileName.c_str());
	}

	THREADID err;
	//Spawn write thread
	if (ASCII.Value() == FALSE)
		err = PIN_SpawnInternalThread(ThreadWriteBin, NULL, 0, NULL);
	else
		err = PIN_SpawnInternalThread(ThreadWriteASCII, NULL, 0, NULL);

	if (err == INVALID_THREADID)
		std::cout << "Error creating internal Pin thread. Exiting.." << std::endl;
}

VOID closeOutput() {
	if (usingCompression)
		pclose(outPipe);
	else
		OutFile.close();

	ASCIIDescriptorTable.clear();
	binDescriptorTable.clear();
}

