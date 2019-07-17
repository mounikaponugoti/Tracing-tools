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

// Reference:Albert Mayers

// mlvCfiat.h
// contains instruction analysis functions injected with mlvCFiat.h

//procedures to reverse endianess
static inline VOID reverseOther(UINT8* valBuf, const UINT16 size) {
	UINT8* end = valBuf + size;
	std::reverse(valBuf, end);
}
static inline VOID reverseHalfWord(UINT8* valBuf) {
	UINT8 cTemp;
	cTemp = valBuf[1]; valBuf[1] = valBuf[0]; valBuf[0] = cTemp;
}
static inline VOID reverseWord(UINT8* valBuf) {
	UINT8 cTemp;
	cTemp = valBuf[3]; valBuf[3] = valBuf[0]; valBuf[0] = cTemp;
	cTemp = valBuf[2]; valBuf[2] = valBuf[1]; valBuf[1] = cTemp;
}
static inline VOID reverseDoubleWord(UINT8* valBuf) {
	UINT8 cTemp;
	cTemp = valBuf[7]; valBuf[7] = valBuf[0]; valBuf[0] = cTemp;
	cTemp = valBuf[6]; valBuf[6] = valBuf[1]; valBuf[1] = cTemp;
	cTemp = valBuf[5]; valBuf[5] = valBuf[2]; valBuf[2] = cTemp;
	cTemp = valBuf[4]; valBuf[4] = valBuf[3]; valBuf[3] = cTemp;
}
static inline VOID reverseExtendedPrecision(UINT8* valBuf) {
	UINT8 cTemp;
	cTemp = valBuf[9]; valBuf[9] = valBuf[0]; valBuf[0] = cTemp;
	cTemp = valBuf[8]; valBuf[8] = valBuf[1]; valBuf[1] = cTemp;
	cTemp = valBuf[7]; valBuf[7] = valBuf[2]; valBuf[2] = cTemp;
	cTemp = valBuf[6]; valBuf[6] = valBuf[3]; valBuf[3] = cTemp;
	cTemp = valBuf[5]; valBuf[5] = valBuf[4]; valBuf[4] = cTemp;
}
static inline VOID ConvertToBigEndian(UINT8* valBuf, UINT16 size) {
	switch (size) {
		//byte
	case 1: 
		break;
		//half-word
	case 2:
		reverseHalfWord(valBuf);
		break;
		//word
	case 4:
		reverseWord(valBuf);
		break;
		//double word
	case 8:
		reverseDoubleWord(valBuf);
		break;
		//extended precision
	case 10:
		reverseExtendedPrecision(valBuf);
		break;
	default:
		reverseOther(valBuf, size);
		break;
	}
}

/* If the cacheblock written by the thread is hit in any other caches, then the current
thread (which is writting) evicts the cache block from other caches */

tls* get_tls(const THREADID threadid) {
	tls *localStorage = new tls;
	localStorage = allCaches[threadid];
	return localStorage;
}

/*

   Instrumentation for store operands

*/

VOID Store_MultiCacheLines_Shared(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	PIN_GetLock(&cache_lock, threadid + 1);
	sharedCache->StoreMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);
}

VOID Store_SingleCacheLine_Shared(const THREADID threadid, const ADDRINT * addr, UINT32 size) {
	if (!CanEmit(threadid)) return;
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	PIN_GetLock(&cache_lock, threadid + 1);
	sharedCache->StoreSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);
}

VOID Store_MultiCacheLines_Private(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	//tls *localStorage = new tls;
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];

	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	localStorage->localCache->StoreMultiLine(address, size);

	PIN_GetLock(&thread_lock, threadid + 1);
	for (UINT8 i = 0; i < threadIDs.size(); i++) {
		tls *localOtherStorage = get_tls(threadIDs[i]);
		if (localOtherStorage->tid != localStorage->tid) {
			localOtherStorage->localCache->EvictMultiLine(address, size);
		}
	}
	PIN_ReleaseLock(&thread_lock);
	PIN_ReleaseLock(&cache_lock);
}

VOID Store_SingleCacheLine_Private(const THREADID threadid, const ADDRINT * addr, UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	localStorage->localCache->StoreSingleLine(address, size);

	/*   std::cout << "Threads: " << " ";
	   for( UINT8 i =0; i<threadIDs.size(); i++)
	   std::cout << static_cast<int> (threadIDs[i]) << ' ';
	   std::cout << "\n" ;
	   */
	PIN_GetLock(&thread_lock, threadid + 1);
	for (UINT8 i = 0; i < threadIDs.size(); i++) {
		tls *localOtherStorage = get_tls(threadIDs[i]);
		if (localOtherStorage->tid != localStorage->tid) {
			localOtherStorage->localCache->EvictSingleLine(address, size);
		}
	}
	PIN_ReleaseLock(&thread_lock);
	PIN_ReleaseLock(&cache_lock);
}
/*

	ASCII

*/
VOID Load_MultiCacheLines_ASCII_Shared(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	//check if we need to emit descriptor
	bool emit = sharedCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		std::string ins;
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		convert << std::endl;
		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_ASCII_Shared(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	//check if we need to emit descriptor
	bool emit = sharedCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		std::string ins;
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		convert << std::endl;
		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_MultiCacheLines_ASCII_Private(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	//check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		std::string ins;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		convert << std::endl;
		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_ASCII_Private(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	//tls *localStorage = new tls;
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	//check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		std::string ins;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		convert << std::endl;
		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);

		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

/*

	ASCII with dissasembly

*/
VOID Load_MultiCacheLines_Dis_Shared(const THREADID threadid, const ADDRINT ins_address, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	
    //check if we need to emit descriptor
	bool emit = sharedCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		std::string ins;
		getAssemblyString(ins, threadid, ins_address);
		convert << "\t" + ins << std::endl;

		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);

		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_Dis_Shared(const THREADID threadid, const ADDRINT ins_address, const ADDRINT * addr, const UINT32 size) {
	//tls *localStorage = new tls;
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	//check if we need to emit descriptor
	bool emit = sharedCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}

		std::string ins;
		getAssemblyString(ins, threadid, ins_address);
		convert << "\t" + ins << std::endl;

		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_MultiCacheLines_Dis_Private(const THREADID threadid, const ADDRINT ins_address, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);

	//check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);
	if (emit) {
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		std::string ins;
		getAssemblyString(ins, threadid, ins_address);
		convert << "\t" + ins << std::endl;

		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_Dis_Private(const THREADID threadid, const ADDRINT ins_address, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	//check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		std::ostringstream convert;
		UINT8 assignedTID = localStorage->tid;
		convert << std::dec << static_cast<UINT32>(assignedTID);
		convert << ", " << localStorage->fahCnt << ", 0x";

		for (UINT32 i = 0; i < size; i++) {
			convert << std::hex << std::setw(2) << std::setfill('0') << static_cast<UINT32>(valBuf[i]);
		}
		std::string ins;
		getAssemblyString(ins, threadid, ins_address);
		convert << "\t" + ins << std::endl;

		string ASCIIDescriptor = convert.str();

		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		ASCIIDescriptorTable.push_back(ASCIIDescriptor);
		IncrementFileCount(ASCIIDescriptor.size());
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

/*

	Binary

*/
VOID Load_MultiCacheLines_Bin_Shared(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	
    //check if we need to emit descriptor
	bool emit = sharedCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		BinaryDescriptorTableEntry BinDescriptor;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		BinDescriptor.tid = localStorage->tid;
		BinDescriptor.fahCnt = localStorage->fahCnt;
		BinDescriptor.operandSize = size;
		BinDescriptor.data = new UINT8[size];
		std::copy(valBuf, valBuf + size, BinDescriptor.data);

		PIN_GetLock(&table_lock, threadid + 1);
		
        //Push back, will write on close
		binDescriptorTable.push_back(BinDescriptor);
		IncrementFileCount(BinaryDescriptorSize + size);
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_Bin_Shared(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	
    //check if we need to emit descriptor
	bool emit = sharedCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		BinaryDescriptorTableEntry BinDescriptor;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		BinDescriptor.tid = localStorage->tid;
		BinDescriptor.fahCnt = localStorage->fahCnt;
		BinDescriptor.operandSize = size;
		BinDescriptor.data = new UINT8[size];
		std::copy(valBuf, valBuf + size, BinDescriptor.data);


		PIN_GetLock(&table_lock, threadid + 1);
		
        //Push back, will write on close
		binDescriptorTable.push_back(BinDescriptor);
		IncrementFileCount(BinaryDescriptorSize + size);
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_MultiCacheLines_Bin_Private(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	
    //check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadMultiLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		BinaryDescriptorTableEntry BinDescriptor;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		BinDescriptor.tid = localStorage->tid;
		BinDescriptor.fahCnt = localStorage->fahCnt;
		BinDescriptor.operandSize = size;
		BinDescriptor.data = new UINT8[size];
		std::copy(valBuf, valBuf + size, BinDescriptor.data);
        
		PIN_GetLock(&table_lock, threadid + 1);
		
        //Push back, will write on close
		binDescriptorTable.push_back(BinDescriptor);
		IncrementFileCount(BinaryDescriptorSize + size);
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

VOID Load_SingleCacheLine_Bin_Private(const THREADID threadid, const ADDRINT * addr, const UINT32 size) {
	if (!CanEmit(threadid)) return;

	PIN_GetLock(&cache_lock, threadid + 1);
	tls *localStorage = allCaches[threadid];
	uintptr_t address = reinterpret_cast<uintptr_t>(addr);
	
    //check if we need to emit descriptor
	bool emit = localStorage->localCache->LoadSingleLine(address, size);
	PIN_ReleaseLock(&cache_lock);

	if (emit) {
		BinaryDescriptorTableEntry BinDescriptor;

		//copy value
		UINT8 valBuf[size];
		PIN_SafeCopy(valBuf, addr, size);
		ConvertToBigEndian(valBuf, size);

		BinDescriptor.tid = localStorage->tid;
		BinDescriptor.fahCnt = localStorage->fahCnt;
		BinDescriptor.operandSize = size;
		BinDescriptor.data = new UINT8[size];
		std::copy(valBuf, valBuf + size, BinDescriptor.data);
        
		PIN_GetLock(&table_lock, threadid + 1);
		//Push back, will write on close
		binDescriptorTable.push_back(BinDescriptor);
		IncrementFileCount(BinaryDescriptorSize + size);
		PIN_ReleaseLock(&table_lock);

		localStorage->fahCnt = 0;
	}
	else
		localStorage->fahCnt++;
}

//used to decrement fastfoward_count and trace length for all instructions
VOID SetFastForwardAndLength(const THREADID threadid) {
	//critical section
	PIN_GetLock(&count_lock, threadid + 1);
	//if file limit is reacher or trace limit is reached
	//exit tool
	if ((traceLength + 1) == Length.Value() && !noLength) {
		PIN_Detach();
		PIN_ReleaseLock(&count_lock);
		return;
	}
	//decrement fast forward count
	if (fastforwardLength < Skip.Value())
		fastforwardLength++;

	//otherwise increment length
	else
		traceLength++;

	PIN_ReleaseLock(&count_lock);
	return;
}