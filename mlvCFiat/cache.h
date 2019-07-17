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
//
// @ORIGINAL_AUTHOR: Artur Klauser
//

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHE_H
#define PIN_CACHE_H

#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters

// RMR (rodric@gmail.com) {
//   - temporary work around because decstr()
//     casts 64 bit ints to 32 bit ones
//   - use safe_fdiv to avoid NaNs in output
#include <sstream>

bool isReplacementLRU = true;
/*
static string mydecstr(UINT64 v, UINT32 w)
{
	ostringstream o;
	o.width(w);
	o << v;
	string str(o.str());
	return str;
}
*/
#define safe_fdiv(x) (x ? (double) x : 1.0)
// } RMR

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n)
{
	return ((n & (n - 1)) == 0);
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n) {
	INT32 p = 0;

	if (n == 0) return -1;

	if (n & 0xffff0000) { p += 16; n >>= 16; }
	if (n & 0x0000ff00)	{ p += 8; n >>= 8; }
	if (n & 0x000000f0) { p += 4; n >>= 4; }
	if (n & 0x0000000c) { p += 2; n >>= 2; }
	if (n & 0x00000002) { p += 1; }

	return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n) {
	return FloorLog2(n - 1) + 1;
}

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG {
private:
	ADDRINT _tag;

public:
	CACHE_TAG(ADDRINT tag = 0) { _tag = tag; }
	bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
	const ADDRINT getTag() { return _tag; }
	//Not used: operator ADDRINT() const { return _tag; }
};

class FA_FLAGS {
public:
	std::vector<UINT8> _flags;
	UINT32 numFlags;
	UINT32 granularity;

public:
	FA_FLAGS() {}
	FA_FLAGS(UINT32 lineSize, UINT32 granu) {
		granularity = granu;
		numFlags = lineSize / granularity;
		for (UINT32 i = 0; i < numFlags; i++)
			_flags.push_back(0);
	}

	VOID ClearFlags() {
		std::fill(_flags.begin(), _flags.end(), 0);
	}
	VOID ClearFlags(const UINT32 lineIndex, const UINT32 size) {
		UINT32 firstFlag = lineIndex / granularity;
		UINT32 lastFlag = (lineIndex + size - 1) / granularity;
		std::fill(_flags.begin() + firstFlag, _flags.begin() + lastFlag + 1, 0);
	}
	VOID SetFlags(const UINT32 lineIndex, const UINT32 size) {
		UINT32 firstFlag = lineIndex / granularity;
		UINT32 lastFlag = (lineIndex + size - 1) / granularity;
		//std::cout << "localSize: " << size << std::endl;
		//std::cout << "firstFlag: " << firstFlag << std::endl;
		//std::cout << "lastFlag: " << lastFlag << std::endl;

		/*for(unsigned i= 0; i < _flags.size(); i++)
		   std::cout << static_cast<UINT32>(_flags[i]) << ", ";
		   std::cout << std::endl;*/

		std::fill(_flags.begin() + firstFlag, _flags.begin() + lastFlag + 1, 1);

		/*for(unsigned i= 0; i < _flags.size(); i++)
		   std::cout << static_cast<UINT32>(_flags[i]) << ", ";
		   std::cout << std::endl << std::endl;*/

	}
	BOOL AreFlagsSet(const UINT32 lineIndex, const UINT32 size) {
		bool flagsSet = true;
		UINT32 firstFlag = lineIndex / granularity;
		UINT32 lastFlag = (lineIndex + size - 1) / granularity;
		for (UINT32 i = firstFlag; i <= lastFlag; i++) {
			if (_flags[i] == 0) {
				flagsSet = false;
				break;
			}
		}
		return flagsSet;
	}
};

/*!
 * Everything related to cache sets
 */
namespace CACHE_SET {
	
    /*!
	 *  @brief Cache set direct mapped
	 */
	class DIRECT_MAPPED {
	private:
		CACHE_TAG _tag;

	public:
		DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

		VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
		UINT32 GetAssociativity(UINT32 associativity) { return 1; }

		UINT32 Find(CACHE_TAG tag) { return(_tag == tag); }
		VOID Replace(CACHE_TAG tag) { _tag = tag; }
	};

	/*!
	 *  @brief Cache set with round robin replacement
	 */
	template <UINT32 MAX_ASSOCIATIVITY = 4>
	class ROUND_ROBIN {
	private:
		CACHE_TAG _tags[MAX_ASSOCIATIVITY];
        //first access flags
		FA_FLAGS _flags[MAX_ASSOCIATIVITY];
		
		UINT32 _tagsLastIndex;
		UINT32 _nextReplaceIndex;

	public:
		ROUND_ROBIN(UINT32 associativity = MAX_ASSOCIATIVITY, UINT32 lineSize = 256, UINT32 granularity = 256) : _tagsLastIndex(associativity - 1) {
			ASSERTX(associativity <= MAX_ASSOCIATIVITY);
			_nextReplaceIndex = _tagsLastIndex;
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				_tags[index] = CACHE_TAG(0);
				_flags[index] = FA_FLAGS(lineSize, granularity);
			}
		}

		VOID SetAssociativity(UINT32 associativity, UINT32 lineSize, UINT32 granularity) {
			ASSERTX(associativity <= MAX_ASSOCIATIVITY);
			_tagsLastIndex = associativity - 1;
			_nextReplaceIndex = _tagsLastIndex;

			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				_flags[index] = FA_FLAGS(lineSize, granularity);
			}
		}

		UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

		VOID ReSizeTheContainer(UINT32 associativity) { }

		UINT32 Find(CACHE_TAG tag) {
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				// this is an ugly micro-optimization, but it does cause a
				// tighter assembly loop for ARM that way ...
				if (_tags[index] == tag) 
                    return true;
			}
		    return false;
		}

		UINT32 Find(CACHE_TAG tag, UINT32 & wayIndex) {
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				// this is an ugly micro-optimization, but it does cause a
				// tighter assembly loop for ARM that way ...
				if (_tags[index] == tag) {
					wayIndex = index;
                    return true;
				}
			}
		    return false;
		}

		UINT32 Replace(CACHE_TAG tag) {
			const UINT32 index = _nextReplaceIndex;
			_tags[index] = tag;
			// condition typically faster than modulo
			_nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
			return index;
		}

		VOID UpdateTheHistory(UINT32 index)	{	}

		VOID ResetTag(CACHE_TAG tag) {	}

		//Set flags
		VOID SetFlags(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.SetFlags(lineIndex, size);
		}

		//clear all flags for cache line
		VOID ClearFlags(const UINT32 wayIndex) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.ClearFlags();
		}

		//clear specific flags
		VOID ClearFlags(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.ClearFlags(lineIndex, size);
		}

		BOOL AreFlagsSet(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			return wayFlags.AreFlagsSet(lineIndex, size);
		}

	};

	/*!
	*  @brief Cache set with LRU replacement
	*/
	template <UINT32 MAX_ASSOCIATIVITY = 4>
	class LRU {
	private:
		CACHE_TAG _tags[MAX_ASSOCIATIVITY];
		FA_FLAGS _flags[MAX_ASSOCIATIVITY];
		std::list <UINT32> _lru;
		//first access flags
		UINT32 _tagsLastIndex;

	public:
		LRU(UINT32 associativity = MAX_ASSOCIATIVITY, UINT32 lineSize = 256, UINT32 granularity = 256) : _tagsLastIndex(associativity - 1) {
			ASSERTX(associativity <= MAX_ASSOCIATIVITY);
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				_tags[index] = CACHE_TAG(0xFFFFFFFF);
				_flags[index] = FA_FLAGS(lineSize, granularity);
				_lru.push_front(index);
			}
		}

		VOID SetAssociativity(UINT32 associativity, UINT32 lineSize, UINT32 granularity) {
			ASSERTX(associativity <= MAX_ASSOCIATIVITY);
			_tagsLastIndex = associativity - 1;

			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				_flags[index] = FA_FLAGS(lineSize, granularity);
			}
		}

		UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

		VOID ReSizeTheContainer(UINT32 associativity) { _lru.resize(associativity); }

		UINT32 Find(CACHE_TAG tag) {
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				// this is an ugly micro-optimization, but it does cause a
				// tighter assembly loop for ARM that way ...
				if (_tags[index] == tag)
					return true;
			}
			return false;
		}

		UINT32 Find(CACHE_TAG tag, UINT32 & wayIndex) {
			for (INT32 index = _tagsLastIndex; index >= 0; index--) {
				// this is an ugly micro-optimization, but it does cause a
				// tighter assembly loop for ARM that way ...
				if (_tags[index] == tag) {
					wayIndex = index;
                    return true;
				}
			}
		    return falsse;
		}

		UINT32 Replace(CACHE_TAG tag) {
			const UINT32 index = _lru.back();  // replace last element 
			_tags[index] = tag;
			return index;
		}

		VOID UpdateTheHistory(UINT32 index) {
			_lru.remove(index);
			_lru.push_front(index);
#ifdef DEBUG
			std::cout << "\n New list " ;
			for (std::list<UINT32>::iterator it=_lru.begin(); it!=_lru.end(); ++it)
			    std::cout << *it << " ,";
			std::cout << std::endl;
			std::cout << "New Tags ";
			for (INT32 index = 0;  index <= _tagsLastIndex; index++)
			    std::cout << _tags[index].getTag() << " ,";
            std::cout << std::endl;
#endif
		}

		VOID ResetTag(CACHE_TAG tag) {
			UINT32 index;
			UINT32 isExist = Find(tag, index);
			if (isExist) {
                // since there is no valid bit, just resetting tag
				_tags[index] = 0xFFFFFFFF;  
				_lru.remove(index);
                // make it as least recently used (since no valid bit doing this way)
				_lru.push_back(index);   
#ifdef DEBUG  //debug to check order of LRU list
				std::cout << "New list: " ;
				for (std::list<UINT32>::iterator it=_lru.begin(); it!=_lru.end(); ++it)
				    std::cout << *it << " ,";
                std::cout << std::endl;
#endif
			}
		}

		//Set flags
		VOID SetFlags(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.SetFlags(lineIndex, size);
		}

		//clear all flags for cache line
		VOID ClearFlags(const UINT32 wayIndex) {
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.ClearFlags();
		}

		//clear specific flags
		VOID ClearFlags(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size){
			FA_FLAGS & wayFlags = _flags[wayIndex];
			wayFlags.ClearFlags(lineIndex, size);
		}

		BOOL AreFlagsSet(const UINT32 wayIndex, const UINT32 lineIndex, const UINT32 size)
		{
			FA_FLAGS & wayFlags = _flags[wayIndex];
			return wayFlags.AreFlagsSet(lineIndex, size);
		}
	};

} // namespace CACHE_SET

namespace CACHE_ALLOC {
	typedef enum {
		STORE_ALLOCATE,
		STORE_NO_ALLOCATE
	} STORE_ALLOCATION;
};

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class FA_CACHE_BASE {
public:
	// types, constants
	typedef enum {
		OPERAND_TYPE_BYTE,
		OPERAND_TYPE_WORD,
		OPERAND_TYPE_DOUBLEWORD,
		OPERAND_TYPE_QUADWORD,
		OPERAND_TYPE_EXTENDEDPRECISION,
		OPERAND_TYPE_OCTAWORD,
		OPERAND_TYPE_HEXAWORD,
		OPERAND_TYPE_OTHER,
		OPERAND_TYPE_NUM
	} OPERAND_TYPE;

	typedef enum {
		ACCESS_TYPE_LOAD,
		ACCESS_TYPE_STORE,
		ACCESS_TYPE_NUM
	} ACCESS_TYPE;

	typedef enum {
		CACHE_TYPE_ICACHE,
		CACHE_TYPE_DCACHE,
		CACHE_TYPE_NUM
	} CACHE_TYPE;

protected:
	static const UINT32 HIT_MISS_NUM = 2;
	CACHE_STATS _cache_accesses[OPERAND_TYPE_NUM][ACCESS_TYPE_NUM][HIT_MISS_NUM];
	CACHE_STATS _flag_accesses[OPERAND_TYPE_NUM][HIT_MISS_NUM];
    
private:    // input params
	const std::string _name;
	const UINT32 _cacheSize;
	const UINT32 _lineSize;
	const UINT32 _associativity;
	const UINT32 _granularity;

	// computed params
	const UINT32 _lineShift;
	const UINT32 _indexShift;
	const UINT32 _setIndexMask;

protected:
	UINT32 NumSets() const { return _setIndexMask + 1; }

public:
	// constructors/destructors
	FA_CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 granularity);

	// accessors
	UINT32 CacheSize(){ return _cacheSize; }
	UINT32 LineSize() { return _lineSize; }
	UINT32 Associativity() { return _associativity; }
	UINT32 Granularity() { return _granularity; }
	//
	OPERAND_TYPE getOperandType(const UINT32 size) const {
		switch (size)
		{
		case 1:
			return OPERAND_TYPE_BYTE;
			break;
		case 2:
			return OPERAND_TYPE_WORD;
			break;
		case 4:
			return OPERAND_TYPE_DOUBLEWORD;
			break;
		case 8:
			return OPERAND_TYPE_QUADWORD;
			break;
		case 10:
			return OPERAND_TYPE_EXTENDEDPRECISION;
			break;
		case 16:
			return OPERAND_TYPE_OCTAWORD;
			break;
		case 32:
			return OPERAND_TYPE_HEXAWORD;
			break;
		default:
			return OPERAND_TYPE_OTHER;
			break;
		}
	}
	/*
		cache hits
	*/
	CACHE_STATS CacheHits_Operand(OPERAND_TYPE optype) const
	{
		CACHE_STATS sum = 0;
		sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][true]
			+ _cache_accesses[optype][ACCESS_TYPE_STORE][true];
		return sum;
	}

	CACHE_STATS CacheHits_Total() const
	{
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++)
		{
			sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][true]
				+ _cache_accesses[optype][ACCESS_TYPE_STORE][true];
		}
		return sum;
	}

	/*
		cache misses
	*/
	CACHE_STATS CacheMisses_Operand(OPERAND_TYPE optype) const
	{
		CACHE_STATS sum = 0;
		sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][false]
			+ _cache_accesses[optype][ACCESS_TYPE_STORE][false];
		return sum;
	}

	CACHE_STATS CacheMisses_Total() const
	{
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++)
		{
			sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][false]
				+ _cache_accesses[optype][ACCESS_TYPE_STORE][false];
		}
		return sum;
	}

	/*
	    cache read hits
	*/
	CACHE_STATS CacheReadHits_Operand(OPERAND_TYPE optype) const
	{
		CACHE_STATS sum = 0;
		sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][true];
		return sum;
	}

	CACHE_STATS CacheReadHits_Total() const
	{
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++)
		{
			sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][true];
		}
		return sum;
	}

	/*
	    cache read misses
	*/
	CACHE_STATS CacheReadMisses_Operand(OPERAND_TYPE optype) const {
		CACHE_STATS sum = 0;
		sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][false];
		return sum;
	}

    CACHE_STATS CacheReadMisses_Total() const {
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++) {
			sum += _cache_accesses[optype][ACCESS_TYPE_LOAD][false];
		}
		return sum;
	}

	/*
		FA Flag hits
	*/
	CACHE_STATS FAFHits_Operand(OPERAND_TYPE optype) const {
		CACHE_STATS sum = 0;
		sum += _flag_accesses[optype][true];
		return sum;
	}

	CACHE_STATS FAFHits_Total() const {
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++) {
			sum += _flag_accesses[optype][true];
		}
		return sum;
	}

	/*
		FA Flag misses
	*/
	CACHE_STATS FAFMisses_Operand(OPERAND_TYPE optype) const {
		CACHE_STATS sum = 0;
		sum += _flag_accesses[optype][false];
		return sum;
	}

	CACHE_STATS FAFMisses_Total() const	{
		CACHE_STATS sum = 0;
		for (UINT32 optype = 0; optype < OPERAND_TYPE_NUM; optype++) {
			sum += _flag_accesses[optype][false];
		}
		return sum;
	}

	VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) const {
		tag = addr >> _lineShift;
		setIndex = tag.getTag() & _setIndexMask;
		tag = tag.getTag() >> _indexShift;
	}

	VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex) const	{
		const UINT32 lineMask = _lineSize - 1;
		lineIndex = addr & lineMask;
		SplitAddress(addr, tag, setIndex);
	}
};

FA_CACHE_BASE::FA_CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 granularity)
	: _name(name),
	_cacheSize(cacheSize),
	_lineSize(lineSize),
	_associativity(associativity),
	_granularity(granularity),
	_lineShift(FloorLog2(lineSize)),
	_indexShift(FloorLog2(cacheSize / (associativity * lineSize))),
	_setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{

	ASSERTX(IsPower2(_lineSize));
	ASSERTX(IsPower2(_setIndexMask + 1));
	ASSERTX(IsPower2(_granularity));
	ASSERTX(_granularity <= _lineSize);
	for (UINT32 OpType = 0; OpType < OPERAND_TYPE_NUM; OpType++) {
		for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)	{
			_cache_accesses[OpType][accessType][false] = 0;
			_cache_accesses[OpType][accessType][true] = 0;
		}
		_flag_accesses[OpType][false] = 0;
		_flag_accesses[OpType][true] = 0;
	}
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE : public FA_CACHE_BASE {
private:
	SET _sets[MAX_SETS];
	UINT32 _granularity;
public:
	// constructors/destructors
	CACHE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 granularity) : FA_CACHE_BASE(name, cacheSize, lineSize, associativity, granularity) {
		ASSERTX(NumSets() <= MAX_SETS);
		_granularity = granularity;
		for (UINT32 i = 0; i < NumSets(); i++) {
			_sets[i].SetAssociativity(associativity, lineSize, granularity);
			_sets[i].ReSizeTheContainer(associativity);
		}
	}

	//Writes, multi and single lines
	void StoreMultiLine(ADDRINT addr, UINT32 size);
	void StoreSingleLine(ADDRINT addr, UINT32 size);

	//Reads, multi and single lines
	//returns hit/miss
	bool LoadMultiLine(ADDRINT addr, UINT32 size);
	bool LoadSingleLine(ADDRINT addr, UINT32 size);
	void EvictSingleLine(ADDRINT addr, const UINT32 size);
	void EvictMultiLine(ADDRINT addr, const UINT32 size);

};
//helper

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET, MAX_SETS, STORE_ALLOCATION>::LoadSingleLine(ADDRINT addr, const UINT32 size) {
	bool emit;
	bool flagsHit = false;
	OPERAND_TYPE opType = getOperandType(size);

	CACHE_TAG tag;
	UINT32 setIndex;
	UINT32 lineIndex;
	UINT32 wayIndex;

	SplitAddress(addr, tag, setIndex, lineIndex);
	SET & set = _sets[setIndex];

	//check if it hits in cachse and get wayIndex if hit
	bool cacheHit = set.Find(tag, wayIndex);
	// if hit and check flags
	if (cacheHit) {
		set.UpdateTheHistory(wayIndex);
		// if flags are set
		flagsHit = set.AreFlagsSet(wayIndex, lineIndex, size);

		if (flagsHit)
			emit = false;
		else {
			//setFlags
			set.SetFlags(wayIndex, lineIndex, size);
			//emit
			emit = true;
		}
	}
	// if miss
	else {
		//replace in cache and get replaced way index
		wayIndex = set.Replace(tag);
		set.UpdateTheHistory(wayIndex);
		//flags
		set.ClearFlags(wayIndex);
		set.SetFlags(wayIndex, lineIndex, size);
		//emit
		emit = true;
	}
	//increment flag stats
	_flag_accesses[opType][flagsHit]++;
	_cache_accesses[getOperandType(size)][ACCESS_TYPE_LOAD][cacheHit]++;
	return emit;
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET, MAX_SETS, STORE_ALLOCATION>::LoadMultiLine(ADDRINT addr, const UINT32 size) {
	bool emit = false;
	const ADDRINT highAddr = addr + size;
	bool cacheAllHit = true;
	bool flagsAllHit = true;

	const ADDRINT lineSize = LineSize();
	const ADDRINT notLineMask = ~(lineSize - 1);
	UINT32 globalSize = size;

	do 	{
		CACHE_TAG tag;
		UINT32 setIndex;
		UINT32 wayIndex;
		UINT32 lineIndex;

		SplitAddress(addr, tag, setIndex, lineIndex);
		SET & set = _sets[setIndex];

		bool localCacheHit = set.Find(tag, wayIndex);
		cacheAllHit &= localCacheHit;

		//if hit on cache line
		if (localCacheHit) {
			set.UpdateTheHistory(wayIndex);
			// Set FA Flags
			UINT32 localSize;
			if (globalSize + lineIndex > lineSize)
				localSize = lineSize - lineIndex;
			else
				localSize = globalSize;
			globalSize -= localSize;

			bool localFlagsHit = set.AreFlagsSet(wayIndex, lineIndex, localSize);
			//if at least one flag is not set
			if (!localFlagsHit) {
				flagsAllHit = false;
				emit = true;
				//set FA flags
				set.SetFlags(wayIndex, lineIndex, localSize);
			}
		}
		//if miss on cache line
		else {
			wayIndex = set.Replace(tag);  //get way index of line to be replaced
			set.UpdateTheHistory(wayIndex);
			set.ClearFlags(wayIndex); //clear flags for entire cache line
			emit = true;
			flagsAllHit = false;

			// Set FA Flags
			UINT32 localSize;
			if (globalSize + lineIndex > lineSize)
				localSize = lineSize - lineIndex;
			else
				localSize = globalSize;

			globalSize -= localSize;
			set.SetFlags(wayIndex, lineIndex, localSize); // Set FA Flags
		}
		addr = (addr & notLineMask) + lineSize; // start of next cache line
	} while (addr < highAddr);

	OPERAND_TYPE opType = getOperandType(size);
	//increment cache statistics 
	_cache_accesses[opType][ACCESS_TYPE_LOAD][cacheAllHit]++;
	//increment flag statistics if cache was hit
	_flag_accesses[opType][flagsAllHit]++;

	return emit;
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::StoreMultiLine(ADDRINT addr, const UINT32 size) {
	const ADDRINT highAddr = addr + size;
	bool cacheAllHit = true;
	UINT32 upperBound;
	UINT32 lowerBound;

	const ADDRINT lineSize = LineSize();
	const ADDRINT notLineMask = ~(lineSize - 1);
	UINT32 globalSize = size;
	do 	{
		CACHE_TAG tag;
		UINT32 setIndex;
		UINT32 wayIndex;
		UINT32 lineIndex;

		SplitAddress(addr, tag, setIndex, lineIndex);
		SET & set = _sets[setIndex];

		bool localHit = set.Find(tag, wayIndex);
		cacheAllHit &= localHit;

		// if miss and cache is write allocate
		if ((!localHit) && STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE) {
			wayIndex = set.Replace(tag);
			set.UpdateTheHistory(wayIndex);

			// Set FA Flags
			UINT32 localSize;
			if (globalSize + lineIndex > lineSize)
				localSize = lineSize - lineIndex;
			else
				localSize = globalSize;

			globalSize -= localSize;
			// miss, so clear all flags for way
			set.ClearFlags(wayIndex);
			// Set FA Flags
			//set.SetFlags( wayIndex, lineIndex, localSize);
			if (localSize >= _granularity) {
				upperBound = localSize + lineIndex;
				if (lineIndex % _granularity == 0) {
                    for (lowerBound = lineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                        // check each sub block whether whole block is written or not
                        set.SetFlags(wayIndex, lowerBound, _granularity);   
                    }
				}
				else {
                    // If adress is different than sub-block starting point this gives exactly where sub-block starts
					uint32_t adjustedLineIndex = lineIndex + _granularity - (lineIndex % _granularity);   
                    for (lowerBound = adjustedLineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                        // check each sub block whether whole block is written or not
                        set.SetFlags(wayIndex, lowerBound, _granularity);  
                    }
				}
			}
		}
		else if (localHit && STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE) {
			set.UpdateTheHistory(wayIndex);
            UINT32 localSize;

			// Set FA Flags
            if (globalSize + lineIndex > lineSize)
				localSize = lineSize - lineIndex;
			else
				localSize = globalSize;

			globalSize -= localSize;

			if (localSize >= _granularity)	{
				upperBound = localSize + lineIndex;
				if (lineIndex % _granularity == 0) 	{
                    for (lowerBound = lineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                        // check each sub block whether whole block is written or not
                        set.SetFlags(wayIndex, lowerBound, _granularity);   
                    }
				}
				else {
                    // If adress is different than sub-block starting address, this gives exactly where sub-block starts
					uint32_t adjustedLineIndex = lineIndex + _granularity - (lineIndex % _granularity);   
                    for (lowerBound = adjustedLineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                        // check each sub block whether whole block is written or not
                        set.SetFlags(wayIndex, lowerBound, _granularity);
                    }
				}
			}
		}

		addr = (addr & notLineMask) + lineSize; // start of next cache line
	} while (addr < highAddr);

	//increment cache statistics
	_cache_accesses[getOperandType(size)][ACCESS_TYPE_STORE][cacheAllHit]++;
}


template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::StoreSingleLine(ADDRINT addr, const UINT32 size) {
	CACHE_TAG tag;
	UINT32 setIndex;
	UINT32 lineIndex;
	UINT32 wayIndex;
	UINT32 upperBound;
	UINT32 lowerBound;

	SplitAddress(addr, tag, setIndex, lineIndex);
	SET & set = _sets[setIndex];

	//check if hit and get wayIndex if hit
	bool cacheHit = set.Find(tag, wayIndex);

	// if miss and cache is write allocate
	if ((!cacheHit) && STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE) {
		//get index of replaced way
		wayIndex = set.Replace(tag);
		set.UpdateTheHistory(wayIndex);
		
        // miss, so clear all flags for way
		set.ClearFlags(wayIndex);
		// Set FA Flags
		if (size >= _granularity) {
			upperBound = size + lineIndex;
			if (lineIndex % _granularity == 0) {
                for (lowerBound = lineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                    // check each sub block wheather whole block is written or not
                    set.SetFlags(wayIndex, lowerBound, _granularity);
                }
			}
			else {
                // If adress is different than sub-block starting address, this gives exactly where sub-block starts
				uint32_t adjustedLineIndex = lineIndex + _granularity - (lineIndex % _granularity);   
                for (lowerBound = adjustedLineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                    // check each sub block whether whole block is written or not
                    set.SetFlags(wayIndex, lowerBound, _granularity);
                }
			}
		}
	}
	else if (cacheHit && STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE) {
		set.UpdateTheHistory(wayIndex);

		//set FA flags
		if (size >= _granularity) {
			upperBound = size + lineIndex;
			if (lineIndex % _granularity == 0) {
                for (lowerBound = lineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                    // check each sub block whether whole block is written or not
                    set.SetFlags(wayIndex, lowerBound, _granularity);   
                }
			}
			else {
                // If adress is different than sub-block starting point this gives exactly where sub-block starts
				uint32_t adjustedLineIndex = lineIndex + _granularity - (lineIndex % _granularity);  
                for (lowerBound = adjustedLineIndex; upperBound - _granularity >= lowerBound; lowerBound += _granularity) {
                    // check each sub block whether whole block is written or not
                    set.SetFlags(wayIndex, lowerBound, _granularity);   
                }
			}
		}
#ifdef DEBUG
        std::cout << " Found in wayIndex = " << wayIndex << std::endl;
#endif
	}
	// increment cache statistics
	_cache_accesses[getOperandType(size)][ACCESS_TYPE_STORE][cacheHit]++;
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::EvictMultiLine(ADDRINT addr, const UINT32 size) {
	const unsigned int highAddr = addr + size;

#ifdef DEBUG
    std::cout << "storeMulti: "
        << "highAddr = " << highAddr << std::endl;
#endif

	const UINT32 lineSize = LineSize();
	const UINT32 notLineMask = ~(lineSize - 1);
	UINT32 globalSize = size;

	do 	{
		CACHE_TAG tag;
		UINT32 setIndex;
		UINT32 wayIndex;
		UINT32 lineIndex;
		SplitAddress(addr, tag, setIndex, lineIndex);
		
		SET & set = _sets[setIndex];
		bool cacheHit = set.Find(tag, wayIndex);

#ifdef DEBUG
        std::cout << "storeSingle : "
            << " tag = " << std::hex << tag.getTag()
            << " setIndex = " << std::hex << setIndex
            << " lineIndex = " << std::hex << lineIndex << std::endl;
        std::cout << "CacheHit = " << cacheHit;
#endif
		// if tag matches
		if (cacheHit) {
			set.ResetTag(tag); // clear tag since no valid bit to invalidate
#ifdef DEBUG
			std::cout << " newWayIndex = " << wayIndex; */
#endif
			// Set FA Flags
			UINT32 localSize;
			if (globalSize + lineIndex > lineSize)
				localSize = lineSize - lineIndex;
			else
				localSize = globalSize;

			globalSize -= localSize;
			// clear all flags for way since it is invalidation
			set.ClearFlags(wayIndex);
		}
		addr = (addr & notLineMask) + lineSize; // start of next cache line

#ifdef DEBUG
        std::cout << " nextCacheLineAddr = " << addr << std::endl;
#endif

	} while (addr < highAddr);
}



template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::EvictSingleLine(ADDRINT addr, const UINT32 size) {
	CACHE_TAG tag;
	UINT32 setIndex;
	UINT32 lineIndex;
	UINT32 wayIndex;

	SplitAddress(addr, tag, setIndex, lineIndex);
	SET & set = _sets[setIndex];

	//check if hit and get wayIndex if hit
	bool cacheHit = set.Find(tag, wayIndex);

#ifdef DEBUG
    std::cout << "storeSingle : "
        << " tag = " << std::hex << tag.getTag()
        << " setIndex = " << std::hex << setIndex
        << " lineIndex = " << std::hex << lineIndex << std::endl;
    std::cout << "CacheHit = " << cacheHit;
#endif

	// if tag maches 
	if (cacheHit) {
		//get index of replaced way
		set.ResetTag(tag); // clear tag since no valid bit to invalidate

		// clear all flags for way to invalidate
		set.ClearFlags(wayIndex);
	}
}

// define shortcuts for both replacing policies. If roundrobin is used isReplacementLRU should set to false.
#if isReplacementLRU
#define CACHE_MODEL(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::LRU<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#else
#define CACHE_MODEL(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#endif
#endif // PIN_CACHE_H

