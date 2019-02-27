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

//mProfileAssist.h
//Contains globals and utility procedures

//KNOB types used to parse tool parameters
KNOB<BOOL> TraceIsOn(KNOB_MODE_WRITEONCE, "pintool",
	"trace", "0", "Record and write traces to a file is turned on or off (default is off, only collects the statistics)");
KNOB<BOOL> mcfTrace(KNOB_MODE_WRITEONCE, "pintool",
	"mcf", "1", "Capture Control-flow traces (default turned on). See trace flag");
KNOB<BOOL> mlsTrace(KNOB_MODE_WRITEONCE, "pintool",
	"mls", "0", "Capture Data-flow traces (default turned off). See trace flag");
KNOB<BOOL> TraceLoad(KNOB_MODE_WRITEONCE, "pintool",
	"load", "1", "Capture traces for load instructions (default is on, only works with mls)");
KNOB<BOOL> TraceStore(KNOB_MODE_WRITEONCE, "pintool",
	"store", "0", "Capture traces for store instructions (default is off, only works with mls)");
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
	"o", "mProfile.out", "Specify trace output file name");

//When collecting the statistics without traces, there is no difference between the options a, d and binary
//Flags a, c, d, f are useful only when tracing is on
KNOB<BOOL> ASCII(KNOB_MODE_WRITEONCE, "pintool",
	"a", "0", "Use ASCII output file instead of binary");
KNOB<UINT64> Skip(KNOB_MODE_WRITEONCE, "pintool",
	"s", "0", "Begin emitting branch descriptors after executing a specified number of instructions ");
KNOB<UINT64> Length(KNOB_MODE_WRITEONCE, "pintool",
	"l", "0", "Number of instructions to profile (default is no limit)");
KNOB<std::string> Compression(KNOB_MODE_WRITEONCE, "pintool",
	"c", "0", "Compress trace. Supports bzip2, pbzip2, gzip, and pigz");
KNOB<BOOL> AnnotateDisassembly(KNOB_MODE_WRITEONCE, "pintool",
	"d", "0", "Annotate descriptors with disassembly (only works when output is ASCII");
KNOB<UINT64> FileLimit(KNOB_MODE_WRITEONCE, "pintool",
	"f", "50000", "Output file size limit in MB. Tracing will end after reaching this limit. Default is 50000 MB");
KNOB<BOOL> Dynamic(KNOB_MODE_WRITEONCE, "pintool",
	"dynamic", "0", "Collect the characteristics periodically. Default is off");
KNOB<UINT64> PeriodOfIns(KNOB_MODE_WRITEONCE, "pintool",
	"n", "100000", "Period (number of instructions) for dynamic characteristics. Default is 100000 instructions (works only with dynamic analysis)");
KNOB<UINT32> appThreads(KNOB_MODE_WRITEONCE, "pintool",
	"t", "0", "Total number of application threads. Default 0 (required with -dynamic)");

//holds the number of threads
UINT8 numThreads;
// Holds the maximum number of possible application threads
UINT8 MaxThreads = 32;

TLS_KEY tls_key;

volatile BOOL IsProcessExiting = FALSE;
volatile BOOL IsEndOfApplication = FALSE;
//holds actual threadids and assigned thread ids
std::map<THREADID, int>  threadIDs;

//get local thread id
UINT8 get_localtid(THREADID threadid)
{
	UINT8 localtid = threadIDs[threadid];
	return localtid;
}

//protects descriptor table
PIN_LOCK table_lock;
//protects xed decoder
PIN_LOCK decoder_lock;
//protects streams
PIN_LOCK cout_lock;
//protects statistics globals
PIN_LOCK count_lock;
//protects memory operations by pin
//also used to protect store instructions
//store values are received by inspecting the 
//memory location after the instruction executes,
//but the process needs to be atomic, since another thread
//could change the value before it is inspected
PIN_LOCK mem_lock;


//Globals for file
struct tm stamp;
std::string fileName;
std::string mcfStatFileName;
std::string mlsStatFileName;
//Used to find pin tool overhead time.
timeval t1, t2;

//ofstream for control flow raw files
std::ofstream mcfOutFile;
//ofstream for load/store raw files
std::ofstream mlsOutFile;
//ofstream for control flow statistics
std::ofstream mcfStatFile;
//ofstream for load/store statistics
std::ofstream mlsStatFile;
//ofstream for writing control flow periodic statistics
std::ofstream *mcfDynamicStatFile;
//ofstream for writing periodic load store statistics
std::ofstream *mlsDynamicStatFile;
//FILE for piping to compression utility
FILE *mcfOutPipe;
FILE *mlsOutPipe;

//output file size limit;
UINT64 fileCount;
UINT64 fileLimit;
//number of instructions to skip;
UINT64  fastforwardLength;
//number of instructions profiled
UINT64 traceLength;
//total number of load instructions profiled
std::vector<UINT64> loadLength;
//total number of store instructions profiled
std::vector<UINT64> storeLength;
//number of instructions profile in current period for each thread
std::vector<UINT64> traceLengthInCurrentPeriod;
//if a length isn't specififed
BOOL noLength;
//if using compression
BOOL usingCompression;

//enumeration for branch types
typedef enum : UINT8 {
	// Unconditional Indirect Taken
	UnconditionalIndirectTaken = 0,
	// DNE Unconditional Indirect Not Taken (Unconditional -> Taken)
	// Unconditional Direct Taken
	UnconditionalDirectTaken = 1,
	// DNE Unconditional Direct Not Taken (Unconditional -> Taken)
	// DNE Conditional Indirect Taken (x86,x86_64 doesn't have conditional indirect branches)
	// DNE Conditional Indirect Not Taken (x86,x86_64 doesn't have conditional indirect branches)
	// Conditional Direct Taken 
	ConditionalDirectTaken = 2,
	// Conditional Direct Not Taken
	ConditionalDirectNotTaken = 3,
	//number of types
} BranchTypes;

//Total number of branches
//automatically initialized to zero
std::vector<UINT64> numTotalBranches;

//records number of branches for each type
//only four types supported currently
//global arrays are initialized to zero by default
struct countBranches{
	UINT64 Total;
	UINT64 UnconditionalIndirectTaken;
	UINT64 UnconditionalDirectTaken;
	UINT64 ConditionalDirectTaken;
	UINT64 ConditionalDirectNotTaken;
	UINT64 Returns; // part of UnconditionalIndirectTaken
} branches;

std::vector<countBranches> numBranches;
//counting loads and stores periodically
std::vector<countBranches> PeriodicNumBranches;

//struct used when writing binary file
typedef struct{
	//logical thread id
	UINT8 tid;
	//address of brnach instruction   
	ADDRINT branchAddress;
	//address of target
	ADDRINT targetAddress;
	//BranchType enum to record addressing mode, conditionality, and whether or not it is taken  
	BranchTypes branchType;
} mcfBinaryDescriptorTableEntry;
//size of BinaryDescriptorTableEntry;
UINT8 mcfBinDescriptorTableEntrySize = sizeof(UINT8) + sizeof(ADDRINT) * 2 + sizeof(BranchTypes);

//Deques used to hold branch descriptors
std::deque<mcfBinaryDescriptorTableEntry> mcfBinDescriptorTable;
std::deque<string> mcfASCIIDescriptorTable;

//automatically initialized to zero
//holds operand count
struct totalOperands{
	UINT64 Total;
	UINT64 Byte;
	UINT64 Word;
	UINT64 DoubleWord;
	UINT64 QuadWord;
	UINT64 OctaWord;
	UINT64 HexaWord;
	UINT64 ExtendedPrecision;
	UINT64 Other;
} totalOps;

std::vector<totalOperands> NumLoads;
std::vector<totalOperands> NumStores;
// To count loads and stores periodically
std::vector<totalOperands> PeriodicNumLoads;
std::vector<totalOperands> PeriodicNumStores;

//enumeration for memory operations
typedef enum : UINT8 {
	load = 0,
	store = 1,
} traceType;

//Bin struct
typedef struct {
	UINT8 tid;
	traceType type;
	ADDRINT insAddr;
	intptr_t operandEffAddr;
	UINT8 operandSize;
	UINT8 *data;
} mlsBinaryDescriptorTableEntry;

UINT16 mlsBinaryDescriptorSize = 3 * sizeof(UINT8) + sizeof(traceType) + 2 * sizeof(ADDRINT);

//holds load instruction descriptors
std::deque<mlsBinaryDescriptorTableEntry> mlsBinDescriptorTable;
std::deque<std::string> mlsASCIIDescriptorTable;

//write ASCII descriptor table to file
inline VOID WriteASCIIDescriptorTableToFile(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	//write contents to disk
	if (mcfTrace.Value() == TRUE){
		while (!mcfASCIIDescriptorTable.empty()) {
			mcfOutFile << mcfASCIIDescriptorTable.front();
			mcfASCIIDescriptorTable.pop_front();
		}
	}
	if (mlsTrace.Value() == TRUE){
		while (!mlsASCIIDescriptorTable.empty()) {
			mlsOutFile << mlsASCIIDescriptorTable.front();
			mlsASCIIDescriptorTable.pop_front();
		}
	}
	PIN_ReleaseLock(&table_lock);
}

//write ASCII descriptor table to compression program
inline VOID WriteASCIIDescriptorTableToCompressor(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	//write contents to disk
	if (mcfTrace.Value() == TRUE){
		while (!mcfASCIIDescriptorTable.empty()) {
			fputs(mcfASCIIDescriptorTable.front().c_str(), mcfOutPipe);
			mcfASCIIDescriptorTable.pop_front();
		}
	}
	if (mlsTrace.Value() == TRUE) {
		while (!mlsASCIIDescriptorTable.empty()) {
			fputs(mlsASCIIDescriptorTable.front().c_str(), mlsOutPipe);
			mlsASCIIDescriptorTable.pop_front();
		}
	}
	PIN_ReleaseLock(&table_lock);
}
//write binary descriptor table to file
inline VOID WriteBinaryDescriptorTableToFile(const THREADID threadid)
{
	PIN_GetLock(&table_lock, threadid + 1);
	if (mcfTrace.Value() == TRUE){
		while (!mcfBinDescriptorTable.empty()) {
			mcfBinaryDescriptorTableEntry temp = mcfBinDescriptorTable.front();
			mcfOutFile.write((char *)&temp.tid, sizeof(temp.tid));
			mcfOutFile.write((char *)&temp.branchAddress, sizeof(temp.branchAddress));
			mcfOutFile.write((char *)&temp.targetAddress, sizeof(temp.targetAddress));
			mcfOutFile.write((char *)&temp.branchType, sizeof(temp.branchType));
			mcfBinDescriptorTable.pop_front();
		}
	}
	if (mlsTrace.Value() == TRUE){
		while (!mlsBinDescriptorTable.empty()) {
			mlsBinaryDescriptorTableEntry temp = mlsBinDescriptorTable.front();
			mlsOutFile.write((char *)&temp.tid, sizeof(temp.tid));
			if (TraceLoad.Value() && TraceStore.Value())
				mlsOutFile.write((char *)&temp.type, sizeof(temp.type));
			mlsOutFile.write((char *)&temp.insAddr, sizeof(temp.insAddr));
			mlsOutFile.write((char *)&temp.operandEffAddr, sizeof(temp.operandEffAddr));
			mlsOutFile.write((char *)&temp.operandSize, sizeof(temp.operandSize));
			mlsOutFile.write((char *)temp.data, temp.operandSize);
			//delete allocated memory
			delete[] temp.data;
			mlsBinDescriptorTable.pop_front();
		}
	}
	PIN_ReleaseLock(&table_lock);
}

//write binary descriptor table to compression program
inline VOID WriteBinaryDescriptorTableToCompressor(const THREADID threadid) {
	PIN_GetLock(&table_lock, threadid + 1);
	if (mlsTrace.Value() == TRUE){
		while (!mcfBinDescriptorTable.empty()){
			mcfBinaryDescriptorTableEntry temp = mcfBinDescriptorTable.front();
			fwrite(&temp.tid, sizeof(temp.tid), 1, mcfOutPipe);
			fwrite(&temp.branchAddress, sizeof(temp.branchAddress), 1, mcfOutPipe);
			fwrite(&temp.targetAddress, sizeof(temp.targetAddress), 1, mcfOutPipe);
			fwrite(&temp.branchType, sizeof(temp.branchType), 1, mcfOutPipe);
			mcfBinDescriptorTable.pop_front();
		}
	}
	if (mlsTrace.Value() == TRUE){
		while (!mlsBinDescriptorTable.empty()){
			mlsBinaryDescriptorTableEntry temp = mlsBinDescriptorTable.front();
			fwrite(&temp.tid, sizeof(temp.tid), 1, mlsOutPipe);
			if (TraceLoad.Value() && TraceStore.Value())
				fwrite(&temp.type, sizeof(temp.type), 1, mlsOutPipe);
			fwrite(&temp.insAddr, sizeof(temp.insAddr), 1, mlsOutPipe);
			fwrite(&temp.operandEffAddr, sizeof(temp.operandEffAddr), 1, mlsOutPipe);
			fwrite(&temp.operandSize, sizeof(temp.operandSize), 1, mlsOutPipe);
			fwrite(temp.data, temp.operandSize, 1, mlsOutPipe);
			//delete allocated memory
			delete[] temp.data;
			mlsBinDescriptorTable.pop_front();
		}
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
inline BOOL CanEmit(const THREADID threadid)
{
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
inline VOID getAssemblyString(std::string & ins, const THREADID threadid, const ADDRINT address) {
	PIN_GetLock(&decoder_lock, threadid + 1);

	//Use Intel XED to decode the instruction based on its address
#if defined(TARGET_IA32E)
	static const xed_state_t dstate = { XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b };
#else
	static const xed_state_t dstate = { XED_MACHINE_MODE_LEGACY_32, XED_ADDRESS_WIDTH_32b };
#endif
	xed_decoded_inst_t xedd;
	xed_decoded_inst_zero_set_mode(&xedd, &dstate);

	UINT32 max_inst_len = 15;

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
//Increments branch statistics for each thread.
inline void IncrementEachThreadBranchStatistics(const BranchTypes branchType, UINT8 id) {
	// Update the counters
	numBranches[id].Total++;
	PeriodicNumBranches[id].Total++;
	switch (branchType) 	{
	case UnconditionalIndirectTaken:
		numBranches[id].UnconditionalIndirectTaken++;
		PeriodicNumBranches[id].UnconditionalIndirectTaken++;
		break;
	case UnconditionalDirectTaken:
		numBranches[id].UnconditionalDirectTaken++;
		PeriodicNumBranches[id].UnconditionalDirectTaken++;
		break;
	case ConditionalDirectTaken:
		numBranches[id].ConditionalDirectTaken++;
		PeriodicNumBranches[id].ConditionalDirectTaken++;
		break;
	case ConditionalDirectNotTaken:
		numBranches[id].ConditionalDirectNotTaken++;
		PeriodicNumBranches[id].ConditionalDirectNotTaken++;
		break;
	default: break;
	}
}

//Increments load statistics for each thread.
inline void IncrementEachThreadLoadStatistics(const UINT8 opSize, UINT8 id)
{
	// Update the counters
	NumLoads[id].Total += 1;
	PeriodicNumLoads[id].Total += 1;
	switch (opSize) {
	case 1:
		NumLoads[id].Byte += 1;
		PeriodicNumLoads[id].Byte += 1;
		break;
	case 2:
		NumLoads[id].Word += 1;
		PeriodicNumLoads[id].Word += 1;
		break;
	case 4:
		NumLoads[id].DoubleWord += 1;
		PeriodicNumLoads[id].DoubleWord += 1;
		break;
	case 8:
		NumLoads[id].QuadWord += 1;
		PeriodicNumLoads[id].QuadWord += 1;
		break;
	case 10:
		NumLoads[id].ExtendedPrecision += 1;
		PeriodicNumLoads[id].ExtendedPrecision += 1;
		break;
	case 16:
		NumLoads[id].OctaWord += 1;
		PeriodicNumLoads[id].OctaWord += 1;
		break;
	case 32:
		NumLoads[id].HexaWord += 1;
		PeriodicNumLoads[id].HexaWord += 1;
		break;
	default:
		NumLoads[id].Other += 1;
		PeriodicNumLoads[id].Other += 1;
		break;
	}
}
//Increments store statistics for each thread.
inline void IncrementEachThreadStoreStatistics(const UINT8 opSize, UINT8 id)
{
	NumStores[id].Total += 1;
	PeriodicNumStores[id].Total += 1;
	switch (opSize) {
	case 1:
		NumStores[id].Byte += 1;
		PeriodicNumStores[id].Byte += 1;
		break;
	case 2:
		NumStores[id].Word += 1;
		PeriodicNumStores[id].Word += 1;
		break;
	case 4:
		NumStores[id].DoubleWord += 1;
		PeriodicNumStores[id].DoubleWord += 1;
		break;
	case 8:
		NumStores[id].QuadWord += 1;
		PeriodicNumStores[id].QuadWord += 1;
		break;
	case 10:
		NumStores[id].ExtendedPrecision += 1;
		PeriodicNumStores[id].ExtendedPrecision += 1;
		break;
	case 16:
		NumStores[id].OctaWord += 1;
		PeriodicNumStores[id].OctaWord += 1;
		break;
	case 32:
		NumStores[id].HexaWord += 1;
		PeriodicNumStores[id].HexaWord += 1;
		break;
	default:
		NumStores[id].Other += 1;
		PeriodicNumStores[id].Other += 1;
		break;
	}
}

//return percentage
inline double Percentage(const UINT64 num, const UINT64 den) {
	if (den == 0)
		return std::numeric_limits<double>::quiet_NaN();

	double ratio = (double)num / (double)den;
	return 100 * ratio;
}

void mcfStatisticsWriteToFile(countBranches temp) {
	mcfStatFile << "\t" << temp.ConditionalDirectTaken << " ( %" << std::setprecision(2)
		<< Percentage(temp.ConditionalDirectTaken, temp.Total) << " ) Conditional Direct Taken" << std::endl;

	mcfStatFile << "\t" << temp.ConditionalDirectNotTaken << " ( %" << std::setprecision(2)
		<< Percentage(temp.ConditionalDirectNotTaken, temp.Total) << " ) Conditional Direct Not Taken" << std::endl;

	mcfStatFile << "\t" << temp.UnconditionalDirectTaken << " ( %" << std::setprecision(2)
		<< Percentage(temp.UnconditionalDirectTaken, temp.Total) << " ) Unconditional Direct" << std::endl;

	mcfStatFile << "\t" << temp.UnconditionalIndirectTaken << " ( %" << std::setprecision(2)
		<< Percentage(temp.UnconditionalIndirectTaken, temp.Total) << " ) Unconditional Indirect" << std::endl;
	// spilt the unconditional indirect branches
	mcfStatFile << "\t\t" << temp.Returns << " ( %" << std::setprecision(2)
		<< Percentage(temp.Returns, temp.UnconditionalIndirectTaken) << " ) Returns" << std::endl;

	mcfStatFile << "\t\t" << temp.UnconditionalIndirectTaken - temp.Returns << " ( %" << std::setprecision(2)
		<< Percentage(temp.UnconditionalIndirectTaken - temp.Returns, temp.UnconditionalIndirectTaken) << " ) Other" << std::endl;
}

void mlsStatisticsWriteToFile(totalOperands temp) {
	//1 byte
	mlsStatFile << "\t" << temp.Byte << " ( %" << std::setprecision(2)
		<< Percentage(temp.Byte, temp.Total) << " ) Byte Operands" << std::endl;
	//2 bytes
	mlsStatFile << "\t" << temp.Word << " ( %" << std::setprecision(2)
		<< Percentage(temp.Word, temp.Total) << " ) Word Operands" << std::endl;
	//4 bytes
	mlsStatFile << "\t" << temp.DoubleWord << " ( %" << std::setprecision(2)
		<< Percentage(temp.DoubleWord, temp.Total) << " ) DoubleWord Operands" << std::endl;
	//8 bytes
	mlsStatFile << "\t" << temp.QuadWord << " ( %" << std::setprecision(2)
		<< Percentage(temp.QuadWord, temp.Total) << " ) QuadWord Operands" << std::endl;
	//10 bytes
	mlsStatFile << "\t" << temp.ExtendedPrecision << " ( %" << std::setprecision(2)
		<< Percentage(temp.ExtendedPrecision, temp.Total) << " ) Extended Precision Operands" << std::endl;
	//16 bytes
	mlsStatFile << "\t" << temp.OctaWord << " ( %" << std::setprecision(2)
		<< Percentage(temp.OctaWord, temp.Total) << " ) OctaWord Operands" << std::endl;
	//32 bytes
	mlsStatFile << "\t" << temp.HexaWord << " ( %" << std::setprecision(2)
		<< Percentage(temp.HexaWord, temp.Total) << " ) HexaWord Operands" << std::endl;
	//other sizes  
	mlsStatFile << "\t" << temp.Other << " ( %" << std::setprecision(2)
		<< Percentage(temp.Other, temp.Total) << " ) Operands of other size" << std::endl;
}

VOID PrintStatistics() {
	double elapsedTime;
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
	// print branch statistics
	if (mcfTrace.Value() == TRUE){
		mcfStatFile << "Instrumentation Time: " << elapsedTime << " ms" << std::endl;
		mcfStatFile << "Number of Threads: " << static_cast<UINT32>(numThreads) << std::endl;
		mcfStatFile << "Traced " << traceLength + 1 << " instructions" << std::endl;
		mcfStatFile << "Skipped " << fastforwardLength << " instructions" << std::endl;

		branches.Total = branches.UnconditionalIndirectTaken = branches.UnconditionalDirectTaken = branches.ConditionalDirectTaken = branches.ConditionalDirectNotTaken = 0;
		for (int i = 0; i < numThreads; i++){
			branches.Total += numBranches[i].Total;
			branches.Returns += numBranches[i].Returns;
			branches.UnconditionalIndirectTaken += numBranches[i].UnconditionalIndirectTaken;
			branches.UnconditionalDirectTaken += numBranches[i].UnconditionalDirectTaken;
			branches.ConditionalDirectTaken += numBranches[i].ConditionalDirectTaken;
			branches.ConditionalDirectNotTaken += numBranches[i].ConditionalDirectNotTaken;
		}
		mcfStatFile << "All The Threads Total Control Transfer Instructions: " << branches.Total << std::endl;
		mcfStatFile.setf(std::ios::fixed);
		mcfStatisticsWriteToFile(branches);

		for (int i = 0; i < numThreads; i++){
			mcfStatFile << "-- Thread " << i << " Total Control Transfer Instructions: " << numBranches[i].Total << std::endl;
			mcfStatisticsWriteToFile(numBranches[i]);
		}
	}
	// print load statistics
	if (mlsTrace.Value() == TRUE){
		UINT64 TotalLoads = 0;
		UINT64 TotalStores = 0;
		for (int i = 0; i < numThreads; i++){
			TotalLoads += loadLength[i];
			TotalStores += storeLength[i];
		}

		mlsStatFile << "Instrumentation Time: " << elapsedTime << " ms" << std::endl;
		mlsStatFile << "Number of Threads: " << static_cast<UINT32>(numThreads) << std::endl;
		mlsStatFile << "Traced " << traceLength + 1 << " instructions" << std::endl;
		mlsStatFile << "Traced " << TotalLoads << " load instructions" << std::endl;
		mlsStatFile << "Traced " << TotalStores << " store instructions" << std::endl;
		mlsStatFile << "Skipped " << fastforwardLength << " instructions" << std::endl;
		if (TraceLoad.Value()) {
			totalOps.Total = totalOps.Byte = totalOps.Word = totalOps.DoubleWord = totalOps.QuadWord = totalOps.ExtendedPrecision = totalOps.OctaWord = totalOps.HexaWord = totalOps.Other = 0;
			for (int i = 0; i < numThreads; i++){
				totalOps.Total += NumLoads[i].Total;
				totalOps.Byte += NumLoads[i].Byte;
				totalOps.Word += NumLoads[i].Word;
				totalOps.DoubleWord += NumLoads[i].DoubleWord;
				totalOps.QuadWord += NumLoads[i].QuadWord;
				totalOps.ExtendedPrecision += NumLoads[i].ExtendedPrecision;
				totalOps.OctaWord += NumLoads[i].OctaWord;
				totalOps.HexaWord += NumLoads[i].HexaWord;
				totalOps.Other += NumLoads[i].Other;
			}

			mlsStatFile << "All The Threads Total Load Operands: " << totalOps.Total << std::endl;
			mlsStatFile.setf(std::ios::fixed);
			mlsStatisticsWriteToFile(totalOps);

			for (int i = 0; i < numThreads; i++){
				mlsStatFile << "-- Thread " << i << " Total Load Operands: " << NumLoads[i].Total << std::endl;
				mlsStatFile.setf(std::ios::fixed);
				mlsStatisticsWriteToFile(NumLoads[i]);
			}
		}
		// print store statistics
		if (TraceStore.Value()){
			totalOps.Total = totalOps.Byte = totalOps.Word = totalOps.DoubleWord = totalOps.QuadWord = totalOps.ExtendedPrecision = totalOps.OctaWord = totalOps.HexaWord = totalOps.Other = 0;
			for (int i = 0; i < numThreads; i++){
				totalOps.Total += NumStores[i].Total;
				totalOps.Byte += NumStores[i].Byte;
				totalOps.Word += NumStores[i].Word;
				totalOps.DoubleWord += NumStores[i].DoubleWord;
				totalOps.QuadWord += NumStores[i].QuadWord;
				totalOps.ExtendedPrecision += NumStores[i].ExtendedPrecision;
				totalOps.OctaWord += NumStores[i].OctaWord;
				totalOps.HexaWord += NumStores[i].HexaWord;
				totalOps.Other += NumStores[i].Other;
			}

			mlsStatFile << "All The Threads Total Store Operands: " << totalOps.Total << std::endl;
			mlsStatFile.setf(std::ios::fixed);
			mlsStatisticsWriteToFile(totalOps);

			for (int i = 0; i < numThreads; i++){
				mlsStatFile << "-- Thread " << i << " Total Store Operands: " << NumStores[i].Total << std::endl;
				mlsStatFile.setf(std::ios::fixed);
				mlsStatisticsWriteToFile(NumStores[i]);
			}
		}
	}
}

// print periodic statistics
VOID PrintDynamicStatistics(UINT8 i, UINT64 CurrentPeriod) {
	if (mcfTrace.Value() == TRUE){
		mcfDynamicStatFile[i] << (UINT16)i << ", " << CurrentPeriod << ", " << PeriodicNumBranches[i].Total << ", " << PeriodicNumBranches[i].ConditionalDirectTaken
			<< ", " << PeriodicNumBranches[i].ConditionalDirectNotTaken << ", " << PeriodicNumBranches[i].UnconditionalDirectTaken
			<< ", " << PeriodicNumBranches[i].UnconditionalIndirectTaken << ", " << PeriodicNumBranches[i].Returns
			<< ", " << (PeriodicNumBranches[i].UnconditionalIndirectTaken - PeriodicNumBranches[i].Returns) << std::endl;
		//Reset the variables
		PeriodicNumBranches[i].Total = PeriodicNumBranches[i].ConditionalDirectTaken = PeriodicNumBranches[i].ConditionalDirectNotTaken = 0;
		PeriodicNumBranches[i].UnconditionalDirectTaken = PeriodicNumBranches[i].UnconditionalIndirectTaken = PeriodicNumBranches[i].Returns = 0;
	}
	if (mlsTrace.Value() == TRUE){
		if (TraceLoad.Value()){
			mlsDynamicStatFile[i] << (UINT16)i << ", " << CurrentPeriod << ", " << PeriodicNumLoads[i].Total << ", " << PeriodicNumLoads[i].Byte << ", " << PeriodicNumLoads[i].Word << ", "
				<< PeriodicNumLoads[i].DoubleWord << ", " << PeriodicNumLoads[i].QuadWord << ", " << PeriodicNumLoads[i].ExtendedPrecision << ", "
				<< PeriodicNumLoads[i].OctaWord << ", " << PeriodicNumLoads[i].HexaWord << ", " << PeriodicNumLoads[i].Other;
		}
		if (TraceStore.Value()){
			mlsDynamicStatFile[i] << ", " << PeriodicNumStores[i].Total << ", " << PeriodicNumStores[i].Byte << ", " << PeriodicNumStores[i].Word << ", "
				<< PeriodicNumStores[i].DoubleWord << ", " << PeriodicNumStores[i].QuadWord << ", " << PeriodicNumStores[i].ExtendedPrecision << ", "
				<< PeriodicNumStores[i].OctaWord << ", " << PeriodicNumStores[i].HexaWord << ", " << PeriodicNumStores[i].Other;
		}

		mlsDynamicStatFile[i] << std::endl;
		//Reset the variables
		PeriodicNumLoads[i].Total = PeriodicNumLoads[i].Byte = PeriodicNumLoads[i].Word = PeriodicNumLoads[i].DoubleWord = PeriodicNumLoads[i].QuadWord = 0;
		PeriodicNumLoads[i].ExtendedPrecision = PeriodicNumLoads[i].OctaWord = PeriodicNumLoads[i].HexaWord = PeriodicNumLoads[i].Other = 0;
		PeriodicNumStores[i].Total = PeriodicNumStores[i].Byte = PeriodicNumStores[i].Word = PeriodicNumStores[i].DoubleWord = PeriodicNumStores[i].QuadWord = 0;
		PeriodicNumStores[i].ExtendedPrecision = PeriodicNumStores[i].OctaWord = PeriodicNumStores[i].HexaWord = PeriodicNumStores[i].Other = 0;
	}
}

//open file/pipe
VOID openOutput()
{
	// Writing traces to a respective file depending on input options
	if (TraceIsOn.Value() == TRUE){
		//Check compression knob
		std::string compressor = Compression.Value();
		if (compressor != "0" && compressor != "bzip2" && compressor != "pbzip2" && compressor != "gzip" && compressor != "pigz") {
			std::cout << "mProfile: That compression program is not supported" << std::endl;
			std::exit(-1);
		}
		if (compressor != "0") {
			//const string temp = "/bin/" + compressor;
			if (access(string("/bin/" + compressor).c_str(), X_OK) == -1 && access(string("/usr/bin/" + compressor).c_str(), X_OK) == -1)
			{
				std::cout << "mProfile: This system does not support " << compressor << ". Exiting..." << std::endl;
				std::exit(-1);
			}
			usingCompression = TRUE;
		}
		else
			usingCompression = FALSE;

		//attach extension
		std::string mcfTempFileName;
		std::string mlsTempFileName;
		if (ASCII.Value() == FALSE){
			if (mcfTrace.Value() == TRUE)
				mcfTempFileName = fileName + "_mcf" + ".bin";
			if (mlsTrace.Value() == TRUE)
				mlsTempFileName = fileName + "_mls" + ".bin";
		}
		else {
			if (mcfTrace.Value() == TRUE)
				mcfTempFileName = fileName + "_mcf" + ".txt";
			if (mlsTrace.Value() == TRUE)
				mlsTempFileName = fileName + "_mls" + ".txt";
		}

		if (usingCompression) {
			char temp[1000] = { 0 };
			if (compressor == "bzip2") {
				if (mcfTrace.Value() == TRUE){
					mcfTempFileName += ".bz2";
					sprintf(temp, "bzip2 > %s", mcfTempFileName.c_str());
					mcfOutPipe = popen(temp, "w");
				}
				if (mlsTrace.Value() == TRUE){
					mlsTempFileName += ".bz2";
					sprintf(temp, "bzip2 > %s", mlsTempFileName.c_str());
					mlsOutPipe = popen(temp, "w");
				}
			}
			else if (compressor == "pbzip2") {
				if (mcfTrace.Value() == TRUE) {
					mcfTempFileName += ".bz2";
					sprintf(temp, "pbzip2 > %s", mcfTempFileName.c_str());
					mcfOutPipe = popen(temp, "w");
				}
				if (mlsTrace.Value() == TRUE) {
					mlsTempFileName += ".bz2";
					sprintf(temp, "pbzip2 > %s", mlsTempFileName.c_str());
					mlsOutPipe = popen(temp, "w");
				}
			}
			else if (compressor == "gzip") {
				if (mcfTrace.Value() == TRUE){
					mcfTempFileName += ".gz";
					sprintf(temp, "gzip > %s", mcfTempFileName.c_str());
					mcfOutPipe = popen(temp, "w");
				}
				if (mlsTrace.Value() == TRUE){
					mlsTempFileName += ".gz";
					sprintf(temp, "gzip > %s", mlsTempFileName.c_str());
					mlsOutPipe = popen(temp, "w");
				}
			}
			else {
				if (mcfTrace.Value() == TRUE){
					mcfTempFileName += ".gz";
					sprintf(temp, "pigz > %s", mcfTempFileName.c_str());
					mcfOutPipe = popen(temp, "w");
				}
				if (mlsTrace.Value() == TRUE){
					mlsTempFileName += ".gz";
					sprintf(temp, "pigz > %s", mlsTempFileName.c_str());
					mlsOutPipe = popen(temp, "w");
				}
			}
			if ((mcfTrace.Value() == TRUE) && (mcfOutPipe == NULL)) {
				std::cout << "mProfile: Error opening mcfOutPipe..." << std::endl;
				std::exit(-1);
			}
			if ((mlsTrace.Value() == TRUE) && (mlsOutPipe == NULL)) {
				std::cout << "mProfile: Error opening mlsOutPipe..." << std::endl;
				std::exit(-1);
			}
		}
		//if not using compression, write to raw file
		else {
			if (ASCII.Value() == FALSE){
				if (mcfTrace.Value() == TRUE)
					mcfOutFile.open(mcfTempFileName.c_str(), ios::out | ios::binary);
				if (mlsTrace.Value() == TRUE)
					mlsOutFile.open(mlsTempFileName.c_str(), ios::out | ios::binary);
			}
			else {
				if (mcfTrace.Value() == TRUE)
					mcfOutFile.open(mcfTempFileName.c_str());
				if (mlsTrace.Value() == TRUE)
					mlsOutFile.open(mlsTempFileName.c_str());
			}
		}

		mlsStatFileName = mlsTempFileName;
		mcfStatFileName = mcfTempFileName;

		THREADID err;
		//Spawn write thread
		if (ASCII.Value())
			err = PIN_SpawnInternalThread(ThreadWriteASCII, NULL, 0, NULL);
		else
			err = PIN_SpawnInternalThread(ThreadWriteBin, NULL, 0, NULL);

		if (err == INVALID_THREADID)
			std::cout << "Error creating internal Pin thread. Exiting.." << std::endl;
	}
	// file to print final statistics
	if (mcfTrace.Value() == TRUE){
		string nameOfFile = fileName + "_mcf" + ".Statistics";
		mcfStatFile.open(nameOfFile.c_str());
	}
	if (mlsTrace.Value() == TRUE){
		string nameOfFile = fileName + "_mls" + ".Statistics";
		mlsStatFile.open(nameOfFile.c_str());
	}

	// create a file to write dynamic characteristics for each thread
	if ((Dynamic.Value()) && (mcfTrace.Value() == TRUE)){
		mcfDynamicStatFile = new std::ofstream[appThreads.Value()];
		for (UINT8 i = 0; i < appThreads.Value(); i++){
			std::ostringstream nameOfFile;
			nameOfFile << fileName << "_mcf_" << (UINT16)i << "_Dynamic.Statistics";
			mcfDynamicStatFile[i].open(nameOfFile.str().c_str());
			mcfDynamicStatFile[i] << "ThreadId, Elapsed Instructions, Control Transfer: Total, Conditional Direct Taken, Conditional Direct Not Taken,  Unconditional Direct, "
				<< "Unconditional Indirect: Total, Unconditional Indirect: Returns, Unconditional Indirect: Other" << '\n';
		}
	}
	if ((Dynamic.Value()) && (mlsTrace.Value() == TRUE)){
		mlsDynamicStatFile = new std::ofstream[appThreads.Value()];
		for (UINT8 i = 0; i < appThreads.Value(); i++){
			std::ostringstream nameOfFile;
			nameOfFile << fileName << "_mls_" << (UINT16)i << "_Dynamic.Statistics";
			mlsDynamicStatFile[i].open(nameOfFile.str().c_str());
			mlsDynamicStatFile[i] << "ThreadId, Elapsed Instructions";
			if (TraceLoad.Value())
				mlsDynamicStatFile[i] << ", Loads: Total, Byte, Word, DoubleWord, QuadWord, ExtendedPrecision, OctaWord, HexaWord, Other";
			if (TraceStore.Value())
				mlsDynamicStatFile[i] << ", Stores: Total, Byte, Word, DoubleWord, QuadWord, ExtendedPrecision, OctaWord, HexaWord, Other";
			mlsDynamicStatFile[i] << '\n';
		}
	}
}

VOID closeOutput() {
	if (mcfTrace.Value() == TRUE){
		if (usingCompression)
			pclose(mcfOutPipe);
		else
			mcfOutFile.close();

		mcfASCIIDescriptorTable.clear();
		mcfBinDescriptorTable.clear();
		if (IsEndOfApplication){
			if (Dynamic.Value()){
				for (int i = 0; i < numThreads; i++)
					mcfDynamicStatFile[i].close();
			}
			mcfStatFile.close();
		}
	}
	if (mlsTrace.Value() == TRUE){
		if (usingCompression)
			pclose(mlsOutPipe);
		else
			mlsOutFile.close();

		mlsASCIIDescriptorTable.clear();
		mlsBinDescriptorTable.clear();
		if (IsEndOfApplication){
			if (Dynamic.Value()){
				for (int i = 0; i < numThreads; i++)
					mlsDynamicStatFile[i].close();
			}
			mlsStatFile.close();
		}
	}
}
