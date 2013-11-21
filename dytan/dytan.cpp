/**

Copyright 2007
Georgia Tech Research Corporation
Atlanta, GA  30332-0415
All Rights Reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

   * Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
   * copyright notice, this list of conditions and the following
   * disclaimer in the documentation and/or other materials provided
   * with the distribution.

   * Neither the name of the Georgia Tech Research Coporation nor the
   * names of its contributors may be used to endorse or promote
   * products derived from this software without specific prior
   * written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**/

#include "dytan.h"
#include "config_parser.h"
#include "syscall_monitor.H"
#include "replace_functions.h"
#include "syscall_functions.h"

#include "RoutineGraph.H"
#include "BasicBlock.H"

#include "taint_func_args.h"
#include "taint_source.h"
#include "taint_source_path.h"
#include "taint_source_network.h"
#include "taint_source_func.h"

map<REG, bitset *> regTaintMap;
map<ADDRINT, bitset *> memTaintMap;
map<ADDRINT, bitset *> controlTaintMap;
map<string, int> profilingMap;

std::ofstream log;
std::ofstream taintAssignmentLog;
std::ofstream prof_log;
std::ostringstream prof_stream;

bitset *dest;
bitset *src;
bitset *eax;
bitset *edx;
bitset *base;
bitset *idx;
bitset *eflags;
bitset *cnt;

bool profiling_marks;
bool profiling_markop;
bool tracing;
bool controlFlowTainting; 
bool word;
int word_size;

int NUMBER_OF_TAINT_MARKS = 4096;
TaintGenerator *taintGen;

InstrumentFunction instrument_functions[XED_ICLASS_LAST];


VOID SysBefore(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *val)
{
    
    SyscallMonitor *monitor = static_cast<SyscallMonitor *>(val);    
    monitor->beginSyscall(threadIndex, PIN_GetSyscallNumber(ctxt, std), PIN_GetSyscallArgument(ctxt, std, 0), PIN_GetSyscallArgument(ctxt, std, 1),
                          PIN_GetSyscallArgument(ctxt, std, 2), PIN_GetSyscallArgument(ctxt, std, 3), PIN_GetSyscallArgument(ctxt, std, 4), PIN_GetSyscallArgument(ctxt, std, 5));
    
    
}

VOID SysAfter(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *val)
{
    SyscallMonitor *monitor = static_cast<SyscallMonitor *>(val);
    
    monitor->endSyscall(threadIndex, PIN_GetSyscallReturn(ctxt, std), PIN_GetSyscallErrno(ctxt, std));
}

/*
  Dumps the instruction to the log file
*/
void Print(ADDRINT address, string *disas)
{
  if(tracing) {
      log << std::hex << address << ": " << disas << " [" << RTN_FindNameByAddress(address) << "]\n";
      log.flush();
  }
}

void ClearTaintSet(bitset *set, unsigned int opcode)
{
  bitset_reset(set);
}

/* copies the taint marks for the register into the out bitset parameter */
void TaintForRegister(REG reg, bitset *set, unsigned int opcode)
{
  map<REG, bitset *>::iterator iter = regTaintMap.find(reg);
  if(regTaintMap.end() != iter) {
    bitset_set_bits(set, iter->second);
  }
  else {
    // this is important becuase we use global storage it's possible that
    // set will already have values
    bitset_reset(set);
  }

  if(profiling_markop){
	  prof_stream << bitset_str(set);
  }

#ifdef TRACE
  if(tracing) {
    const char *sep = "";
    if(REG_valid(reg)) {
       log << "\t-" << REG_StringShort(reg) << "[";
      for(int i = 0; i < (int)set->nbits; i++) {
	if(bitset_test_bit(set, i)) {
        log << sep << i; 
	  sep = ", ";
	}
      }
      log << "]\n";
      log.flush();
    }
  }
#endif

}

/* Return in the out parameter, set, the union of the taint marks
   from memory address start to start + size - 1, and if IMPLICIT is
   defined the taint marks for the base and index registers used to
   access memory
*/
void TaintForMemory(ADDRINT start, ADDRINT size, REG baseReg, REG indexReg,
		bitset *set, unsigned int opcode,  unsigned int to_profile) {
	//to_profile always one

	//profiling marks
	if (profiling_marks && to_profile) {
		//prof_log << "opcode:" << LEVEL_CORE::OPCODE_StringShort(opcode) << "#";
		prof_log << "opcode:" << std::dec << opcode << "#";
	}
	const char *marks_sep = "marks:";

	// need to clear out set incase there are preexisting values
	bitset_reset(set);

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << marks_sep << bitset_str(set);
		marks_sep = "*";
	}

	int count = 0;
	for (ADDRINT addr = start; addr < start + size; addr++) {
		map<ADDRINT, bitset *>::iterator iter = memTaintMap.find(addr);
		if (memTaintMap.end() != iter) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << marks_sep << bitset_str(iter->second);
				marks_sep = "*";
			}
			bitset_union(set, iter->second);

			if(profiling_markop && to_profile){
				if(word){
					if((count % word_size)==0){
						prof_stream << bitset_str(iter->second);
					}
				}
				else{
					prof_stream << bitset_str(iter->second);
				}
			}
		}
		count++;
	}

#ifdef TRACE
	const char *sep = "";
	if (tracing) {
		log << "\t-" << std::hex << start << "-" << std::hex << start + size - 1
				<< "[";
		for (int i = 0; i < (int) set->nbits; i++) {
			if (bitset_test_bit(set, i)) {
				log << sep << i;
				sep = ", ";
			}
		}
		log << "]\n";
		log.flush();
	}
#endif

#ifdef IMPLICIT
	if (REG_valid(baseReg)) {
		map<REG, bitset *>::iterator iter = regTaintMap.find(baseReg);
		if (regTaintMap.end() != iter) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << marks_sep << bitset_str(iter->second);
				marks_sep = "*";
			}
			bitset_union(set, iter->second);

			if(profiling_markop && to_profile){
				prof_stream << bitset_str(iter->second);
			}

#ifdef TRACE
			if (tracing) {
				sep = "";
				log << ", " << REG_StringShort(baseReg) << "[";
				for (int i = 0; i < (int) iter->second->nbits; i++) {
					if (bitset_test_bit(iter->second, i)) {
						log << sep << i;
						sep = ", ";
					}
				}
				log << "]\n";
				log.flush();
			}
#endif     

		}
	}

	if (REG_valid(indexReg)) {
		map<REG, bitset *>::iterator iter = regTaintMap.find(indexReg);
		if (regTaintMap.end() != iter) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << marks_sep << bitset_str(iter->second);
				marks_sep = "*";
			}
			bitset_union(set, iter->second);

			if(profiling_markop && to_profile){
				prof_stream << bitset_str(iter->second);
			}

#ifdef TRACE
			if (tracing) {
				sep = "";
				log << ", " << REG_StringShort(baseReg) << "[";
				for (int i = 0; i < (int) iter->second->nbits; i++) {
					if (bitset_test_bit(iter->second, i)) {
						log << sep << i;
						sep = ", ";
					}
				}
				log << "]\n";
				log.flush();
			}
#endif

		}
	}
#endif

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << "#";
		//profile taint mark operation, u stands for union
		prof_log << "u#";
	}

	const char *memory_sep = "memory:";

	for (ADDRINT addr = start; addr < start + size; addr++) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << memory_sep << std::hex << addr;
			memory_sep = "*";
		}
	}

	//profiling marks
	if (profiling_marks && to_profile) {
		//profile log flush
		prof_log << "\n";
		prof_log.flush();
	}

}

/* 
   sets the taint marks associated with the dest register to the union
   of the bitsets passed in the varargs parameter
 */
void SetTaintForRegister(REG dest, unsigned int opcode, unsigned int to_profile, int numOfArgs, ...) {
	va_list ap;
	bitset *src;
	int i;

	if (LEVEL_BASE::REG_ESP == dest || LEVEL_BASE::REG_EBP == dest) {
		//prof_log << "skip\n";
		return;
	}

	//profiling marks
	if (profiling_marks && to_profile) {
		//prof_log << "opcode:" << LEVEL_CORE::OPCODE_StringShort(opcode) << "#";
		prof_log << "opcode:" << std::dec << opcode << "#";
	}
	const char *marks_sep = "marks:";

	bitset *tmp = bitset_init(NUMBER_OF_TAINT_MARKS);

	va_start(ap, numOfArgs);
	for (i = 0; i < numOfArgs; i++) {
		src = va_arg(ap, bitset *);
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << marks_sep << bitset_str(src);
			marks_sep = "*";
		}
		bitset_union(tmp, src);
	}
	va_end(ap);

	// control flow
	bitset *controlTaint = bitset_init(NUMBER_OF_TAINT_MARKS);
	for (map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
			iter != controlTaintMap.end(); iter++) {
		//profiling marks
		if(profiling_marks && to_profile){
			prof_log << marks_sep << "cf=" << bitset_str(iter->second);
			marks_sep = "*";
		}
		if(profiling_markop && to_profile){
			prof_stream << bitset_str(iter->second);
		}
		bitset_union(controlTaint, iter->second);

	}
	bitset_union(tmp, controlTaint);

	//logging here such that control flow can be included
	if(profiling_markop && to_profile){
		prof_stream << std::dec << opcode;
		map<string, int>::iterator profiling_it = profilingMap.find(prof_stream.str());
		if (profilingMap.end() != profiling_it) {
			//prof_log << profiling_it->second << "\n";
			profiling_it->second++;
		}
		else{
			//prof_log << "1\n";
			profilingMap[prof_stream.str()]=1;
		}
		//prof_log << prof_stream.str() << "\n";
		prof_stream.str("");
	}

	/* This is where we account for subregisters */
	/*
	 This isn't totally complete yet.  For example edi and esi are not
	 included and setting [A|B|C|D]X won't set the super or subregisters
	 */

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << "#";
		//profile taint mark operation, u stands for union
		prof_log << "u#";
	}
	const char *registers_sep = "registers:";

	//eax
	if (LEVEL_BASE::REG_EAX == dest) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AX) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AL);
			registers_sep = "*";
		}

		//ax
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_AX)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_AX], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_AX] = bitset_copy(tmp);
		}

		//ah
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_AH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_AH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_AH] = bitset_copy(tmp);
		}

		//al
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_AL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_AL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_AL] = bitset_copy(tmp);
		}
	}

	//ebx
	else if (LEVEL_BASE::REG_EBX == dest) {

		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BX) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BL);
			registers_sep = "*";
		}

		//bx
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_BX)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_BX], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_BX] = bitset_copy(tmp);
		}

		//bh
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_BH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_BH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_BH] = bitset_copy(tmp);
		}

		//bl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_BL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_BL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_BL] = bitset_copy(tmp);
		}
	}

	//ecx
	else if (LEVEL_BASE::REG_ECX == dest) {

		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CX) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CL);
			registers_sep = "*";
		}

		//cx
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_CX)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_CX], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_CX] = bitset_copy(tmp);
		}

		//ch
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_CH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_CH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_CH] = bitset_copy(tmp);
		}

		//cl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_CL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_CL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_CL] = bitset_copy(tmp);
		}
	}

	//edx
	else if (LEVEL_BASE::REG_EDX == dest) {

		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DX) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DL);
			registers_sep = "*";
		}

		//dx
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_DX)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_DX], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_DX] = bitset_copy(tmp);
		}

		//dh
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_DH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_DH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_DH] = bitset_copy(tmp);
		}

		//dl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_DL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_DL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_DL] = bitset_copy(tmp);
		}
	} else if (LEVEL_BASE::REG_AX == dest) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AL);
			registers_sep = "*";
		}

		//ah
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_AH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_AH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_AH] = bitset_copy(tmp);
		}

		//al
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_AL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_AL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_AL] = bitset_copy(tmp);
		}
	} else if (LEVEL_BASE::REG_BX == dest) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BL);
			registers_sep = "*";
		}

		//bh
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_BH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_BH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_BH] = bitset_copy(tmp);
		}

		//bl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_BL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_BL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_BL] = bitset_copy(tmp);
		}
	} else if (LEVEL_BASE::REG_CX == dest) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CL);
			registers_sep = "*";
		}

		//ch
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_CH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_CH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_CH] = bitset_copy(tmp);
		}

		//cl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_CL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_CL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_CL] = bitset_copy(tmp);
		}
	} else if (LEVEL_BASE::REG_DX == dest) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DH) << "*"
					<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DL);
			registers_sep = "*";
		}

		//dh
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_DH)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_DH], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_DH] = bitset_copy(tmp);
		}

		//dl
		if (regTaintMap.end() != regTaintMap.find(LEVEL_BASE::REG_DL)) {
			bitset_set_bits(regTaintMap[LEVEL_BASE::REG_DL], tmp);
		} else {
			regTaintMap[LEVEL_BASE::REG_DL] = bitset_copy(tmp);
		}
	}

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << registers_sep << LEVEL_BASE::REG_StringShort(dest);
		prof_log << "\n";
		prof_log.flush();
	}

	//taking care here of the whole register
	if (regTaintMap.end() != regTaintMap.find(dest)) {
		bitset_set_bits(regTaintMap[dest], tmp);
	} else {
		regTaintMap[dest] = bitset_copy(tmp);
	}

#ifdef TRACE
	if (tracing) {
		const char *sep = "";
		log << ", " << REG_StringShort(dest) << "[";
		bitset *set = regTaintMap[dest];
		for (int i = 0; i < (int) set->nbits; i++) {
			if (bitset_test_bit(set, i)) {
				log << sep << i;
				sep = ", ";
			}
		}
		log << "] <- cf[";

		sep = "";
		for (int i = 0; i < (int) controlTaint->nbits; i++) {
			if (bitset_test_bit(controlTaint, i)) {
				log << sep << i;
				sep = ", ";
			}
		}
		log << "]\n";
		log.flush();
	}

#endif

	bitset_free(tmp);
	bitset_free(controlTaint);
}

/* Clears the taint marks associated with a register */
void ClearTaintForRegister(REG reg, unsigned int opcode, unsigned int to_profile) {

	//profile marks
	if (profiling_marks && to_profile) {
		//prof_log << "opcode:" << LEVEL_CORE::OPCODE_StringShort(opcode) << "#";
		prof_log << "opcode:" << std::dec << opcode << "#";
	}

	const char *marks_sep = "marks:";

	// control flow
	bitset *controlTaint = bitset_init(NUMBER_OF_TAINT_MARKS);

	//profile marks
	if(profiling_marks && to_profile){
		prof_log << marks_sep << bitset_str(controlTaint);
		marks_sep = "*";
	}

	for (map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
			iter != controlTaintMap.end(); iter++) {
		if(profiling_marks && to_profile){
			prof_log << marks_sep << "cf=" << bitset_str(iter->second);
			marks_sep = "*";
		}
		if(profiling_markop && to_profile){
			prof_stream << bitset_str(iter->second);
		}
		bitset_union(controlTaint, iter->second);
	}

	//logging here such that control flow can be included
	if(profiling_markop && to_profile){
		prof_stream << std::dec << opcode;
		map<string, int>::iterator profiling_it = profilingMap.find(prof_stream.str());
		if (profilingMap.end() != profiling_it) {
			//prof_log << profiling_it->second << "\n";
			profiling_it->second++;
		}
		else{
			//prof_log << "1\n";
			profilingMap[prof_stream.str()]=1;
		}
		//prof_log << prof_stream.str() << "\n";
		prof_stream.str("");
	}

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << "#";
		//profile taint mark operation, u stands for union
		prof_log << "u#";
	}

	const char *registers_sep = "registers:";

	//eax
	if (LEVEL_BASE::REG_EAX == reg) {
		//ax
		map<REG, bitset *>::iterator iter_ax = regTaintMap.find(
				LEVEL_BASE::REG_AX);
		if (regTaintMap.end() != iter_ax) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AX);
				registers_sep = "*";
			}
			bitset_set_bits(iter_ax->second, controlTaint);
		}

		//ah
		map<REG, bitset *>::iterator iter_ah = regTaintMap.find(
				LEVEL_BASE::REG_AH);
		if (regTaintMap.end() != iter_ah) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_ah->second, controlTaint);
		}

		//al
		map<REG, bitset *>::iterator iter_al = regTaintMap.find(
				LEVEL_BASE::REG_AL);
		if (regTaintMap.end() != iter_al) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_al->second, controlTaint);
		}
	}
	//ebx
	else if (LEVEL_BASE::REG_EBX == reg) {

		//bx
		map<REG, bitset *>::iterator iter_bx = regTaintMap.find(
				LEVEL_BASE::REG_BX);
		if (regTaintMap.end() != iter_bx) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BX);
				registers_sep = "*";
			}
			bitset_set_bits(iter_bx->second, controlTaint);
		}

		//bh
		map<REG, bitset *>::iterator iter_bh = regTaintMap.find(
				LEVEL_BASE::REG_BH);
		if (regTaintMap.end() != iter_bh) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_bh->second, controlTaint);
		}

		//bl
		map<REG, bitset *>::iterator iter_bl = regTaintMap.find(
				LEVEL_BASE::REG_BL);
		if (regTaintMap.end() != iter_bl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_bl->second, controlTaint);
		}
	}
	//ecx
	else if (LEVEL_BASE::REG_ECX == reg) {
		//cx
		map<REG, bitset *>::iterator iter_cx = regTaintMap.find(
				LEVEL_BASE::REG_CX);
		if (regTaintMap.end() != iter_cx) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CX);
				registers_sep = "*";
			}
			bitset_set_bits(iter_cx->second, controlTaint);
		}

		//ch
		map<REG, bitset *>::iterator iter_ch = regTaintMap.find(
				LEVEL_BASE::REG_CH);
		if (regTaintMap.end() != iter_ch) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_ch->second, controlTaint);
		}

		//cl
		map<REG, bitset *>::iterator iter_cl = regTaintMap.find(
				LEVEL_BASE::REG_CL);
		if (regTaintMap.end() != iter_cl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_cl->second, controlTaint);
		}
	}

	//edx
	else if (LEVEL_BASE::REG_EDX == reg) {

		//dx
		map<REG, bitset *>::iterator iter_dx = regTaintMap.find(
				LEVEL_BASE::REG_DX);
		if (regTaintMap.end() != iter_dx) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DX);
				registers_sep = "*";
			}
			bitset_set_bits(iter_dx->second, controlTaint);
		}

		//dh
		map<REG, bitset *>::iterator iter_dh = regTaintMap.find(
				LEVEL_BASE::REG_DH);
		if (regTaintMap.end() != iter_dh) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_dh->second, controlTaint);
		}

		//dl
		map<REG, bitset *>::iterator iter_dl = regTaintMap.find(
				LEVEL_BASE::REG_DL);
		if (regTaintMap.end() != iter_dl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_dl->second, controlTaint);
		}

	} else if (LEVEL_BASE::REG_AX == reg) {
		//ah
		map<REG, bitset *>::iterator iter_ah = regTaintMap.find(
				LEVEL_BASE::REG_AH);
		if (regTaintMap.end() != iter_ah) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_ah->second, controlTaint);
		}

		//al
		map<REG, bitset *>::iterator iter_al = regTaintMap.find(
				LEVEL_BASE::REG_AL);
		if (regTaintMap.end() != iter_al) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_AL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_al->second, controlTaint);
		}
	} else if (LEVEL_BASE::REG_BX == reg) {
		//bh
		map<REG, bitset *>::iterator iter_bh = regTaintMap.find(
				LEVEL_BASE::REG_BH);
		if (regTaintMap.end() != iter_bh) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_bh->second, controlTaint);
		}

		//bl
		map<REG, bitset *>::iterator iter_bl = regTaintMap.find(
				LEVEL_BASE::REG_BL);
		if (regTaintMap.end() != iter_bl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_BL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_bl->second, controlTaint);
		}
	} else if (LEVEL_BASE::REG_CX == reg) {
		//ch
		map<REG, bitset *>::iterator iter_ch = regTaintMap.find(
				LEVEL_BASE::REG_CH);
		if (regTaintMap.end() != iter_ch) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_ch->second, controlTaint);
		}

		//cl
		map<REG, bitset *>::iterator iter_cl = regTaintMap.find(
				LEVEL_BASE::REG_CL);
		if (regTaintMap.end() != iter_cl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_CL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_cl->second, controlTaint);
		}
	} else if (LEVEL_BASE::REG_DX == reg) {
		//dh
		map<REG, bitset *>::iterator iter_dh = regTaintMap.find(
				LEVEL_BASE::REG_DH);
		if (regTaintMap.end() != iter_dh) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DH);
				registers_sep = "*";
			}
			bitset_set_bits(iter_dh->second, controlTaint);
		}

		//dl
		map<REG, bitset *>::iterator iter_dl = regTaintMap.find(
				LEVEL_BASE::REG_DL);
		if (regTaintMap.end() != iter_dl) {
			//profiling marks
			if (profiling_marks && to_profile) {
				prof_log << registers_sep
						<< LEVEL_BASE::REG_StringShort(LEVEL_BASE::REG_DL);
				registers_sep = "*";
			}
			bitset_set_bits(iter_dl->second, controlTaint);
		}
	}

	map<REG, bitset *>::iterator iter = regTaintMap.find(reg);
	if (regTaintMap.end() != iter) {
		//profiling marks
		if (profiling_marks && to_profile) {
			prof_log << registers_sep << LEVEL_BASE::REG_StringShort(reg);
			registers_sep = "*";
		}
		bitset_set_bits(iter->second, controlTaint);
	}

	//profiling marks
	if (profiling_marks && to_profile) {
		prof_log << "\n";
		prof_log.flush();
	}

#ifdef TRACE
	if (tracing) {
		const char *sep = "";
		log << "\t" << REG_StringShort(reg) << "  <- cf[";
		sep = "";
		for (int i = 0; i < (int) controlTaint->nbits; i++) {
			if (bitset_test_bit(controlTaint, i)) {
				log << sep << i;
				sep = ", ";
			}
		}
		log << "]\n";
		log.flush();
	}
#endif

	bitset_free(controlTaint);
}

/* Set the taint marks associated with the memory range to the union
   of the bitsets passed in the varargs parameter
*/
void SetTaintForMemory(ADDRINT start, ADDRINT size, unsigned int opcode, unsigned int to_profile, int numOfArgs, ...)
{
  va_list ap;
  bitset *src;
  int i; 

  bitset *tmp = bitset_init(NUMBER_OF_TAINT_MARKS);
  
  //profiling marks
  if(profiling_marks && to_profile){
	  //prof_log << "opcode:" << LEVEL_CORE::OPCODE_StringShort(opcode) << "#";
	  prof_log << "opcode:" << std::dec << opcode << "#";
  }
  const char *marks_sep = "marks:";

  va_start(ap, numOfArgs);
  
    for(i = 0; i < numOfArgs; i++) {
        src = va_arg(ap, bitset *);
        //profiling marks
        if(profiling_marks && to_profile){
            prof_log << marks_sep << bitset_str(src);
            marks_sep = "*";
        }
        bitset_union(tmp, src);
    }
  
  va_end(ap);

  // control flow
  bitset *controlTaint = bitset_init(NUMBER_OF_TAINT_MARKS);
  for(map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
        iter != controlTaintMap.end(); iter++) {
			//profiling marks
			if(profiling_marks && to_profile){
				prof_log << marks_sep << "cf=" << bitset_str(iter->second);
				marks_sep = "*";
			}
			if(profiling_markop && to_profile){
				prof_stream << bitset_str(iter->second);
			}

            bitset_union(controlTaint, iter->second);
        }
  
  bitset_union(tmp, controlTaint);

  //logging here such that control flow can be included
  if(profiling_markop && to_profile){
	  prof_stream << std::dec << opcode;
		map<string, int>::iterator profiling_it = profilingMap.find(prof_stream.str());
		if (profilingMap.end() != profiling_it) {
			//prof_log << profiling_it->second << "\n";
			profiling_it->second++;
		}
		else{
			//prof_log << "1\n";
			profilingMap[prof_stream.str()]=1;
		}
  	  //prof_log << prof_stream.str() << "\n";
  	  prof_stream.str("");
  }

  //profiling marks
  if(profiling_marks && to_profile){
	  prof_log << "#";
	  //profile taint mark operation, u stands for union
	  prof_log << "u#";
  }
  const char *memory_sep = "memory:";

  if(profiling_marks){
	  prof_log << "start:" << std::hex << start << "sized:" << std::dec << size << "sizeh:" << std::hex << size;
  }

  for(ADDRINT addr = start; addr < start + size; addr++) {
	//profiling marks
	if(profiling_marks && to_profile){
		prof_log << memory_sep << std::hex << addr;
		memory_sep = "*";
	}
    if(memTaintMap.end() != memTaintMap.find(addr)) {
      //the address was already in the taint map
      bitset_set_bits(memTaintMap[addr], tmp);
    }
    else {
      //first time I have this address
      memTaintMap[addr] = bitset_copy(tmp);
    }
  }
  
  //profiling marks
  if(profiling_marks && to_profile){
	  //profile log flush
	  prof_log << "\n";
	  prof_log.flush();
  }

#ifdef TRACE
  if(tracing) {
     const char *sep = "";
      log <<"\t" << std::hex << start << "-" << std::hex << start + size - 1 << " <- cf[";
    for(int i = 0; i < (int)tmp->nbits; i++) {
      if(bitset_test_bit(tmp, i)) {
          log << sep << i;
      sep = ", ";
      }
    }
    log << "] <- cf[";

    sep = "";
    for(int i = 0; i < (int)controlTaint->nbits; i++) {
      if(bitset_test_bit(controlTaint, i)) {
          log << sep << i;
          sep = ", ";
      }
    }
    log << "]\n";
  }
#endif
  
  bitset_free(tmp);
  bitset_free(controlTaint);
}

/* Clears taint marks associted with the range of memory */
void ClearTaintForMemory(ADDRINT start, ADDRINT size, unsigned int opcode, unsigned int to_profile)
{

  //profiling marks
  if(profiling_marks && to_profile){
		//prof_log << "opcode:" << LEVEL_CORE::OPCODE_StringShort(opcode) << "#";
	  	prof_log << "opcode:" << std::dec << opcode << "#";
  }

  const char *marks_sep = "marks:";

  // control flow
    bitset *controlTaint = bitset_init(NUMBER_OF_TAINT_MARKS);

	//profile marks
	if(profiling_marks  && to_profile){
		prof_log << marks_sep << bitset_str(controlTaint);
		marks_sep = "*";
	}

    for(map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
          iter != controlTaintMap.end(); iter++) {
		if(profiling_marks  && to_profile){
			prof_log << marks_sep << "cf=" << bitset_str(iter->second);
			marks_sep = "*";
		}
		if(profiling_markop && to_profile){
			prof_stream << bitset_str(iter->second);
		}
          bitset_union(controlTaint, iter->second);
      }

    //logging here such that control flow can be included
    if(profiling_markop && to_profile){
  	  prof_stream << std::dec << opcode;
		map<string, int>::iterator profiling_it = profilingMap.find(prof_stream.str());
		if (profilingMap.end() != profiling_it) {
			///prof_log << profiling_it->second << "\n";
			profiling_it->second++;
		}
		else{
			//prof_log << "1\n";
			profilingMap[prof_stream.str()]=1;
		}
  	  //prof_log << prof_stream.str() << "\n";
  	  prof_stream.str("");
    }

	//profiling marks
	if (profiling_marks  && to_profile) {
		prof_log << "#";
		//profile taint mark operation, u stands for union
		prof_log << "u#";
	}

  const char *memory_sep = "memory:";

  for(ADDRINT addr = start; addr < start + size; addr++) {

    map<ADDRINT, bitset *>::iterator iter = memTaintMap.find(addr);
    if(memTaintMap.end() != iter) {
      //profiling marks
      if(profiling_marks && to_profile){
    	  prof_log << memory_sep << std::hex << addr;
    	  memory_sep = "*";
      }
      bitset_set_bits(iter->second, controlTaint);
    }
  }

  //profiling marks
  if(profiling_marks && to_profile){
		prof_log << "\n";
		prof_log.flush();
  }

#ifdef TRACE
  if(tracing) {
    const char *sep = "";
      log <<"\t" << std::hex << start << "-" << std::hex << start + size - 1 << " <- cf[";
    sep = "";
    for(int i = 0; i < (int)controlTaint->nbits; i++) {
      if(bitset_test_bit(controlTaint, i)) {
          log << sep << i;
	sep = ", ";
      }
    }
    log << "]\n";
    log.flush();
  }
#endif

  bitset_free(controlTaint);
}

#include "instrument_functions.c"

void PushControl(ADDRINT addr)
{
#ifdef TRACE
  if(tracing) {
      log << "\tpush control: " << std::hex << addr << "\n";
      log.flush();
  }
#endif

  if(regTaintMap.end() == regTaintMap.find(LEVEL_BASE::REG_EFLAGS) ||
     bitset_is_empty(regTaintMap[LEVEL_BASE::REG_EFLAGS])) return;

    if(controlTaintMap.end() == controlTaintMap.find(addr)) {
        controlTaintMap[addr] = bitset_init(NUMBER_OF_TAINT_MARKS);
    }
    bitset_union(controlTaintMap[addr], regTaintMap[LEVEL_BASE::REG_EFLAGS]);

    
  //dump control taint map
#ifdef TRACE
  if(tracing) {
    for(map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
	iter != controlTaintMap.end(); iter++) {
      
       const char *sep = "";
        log << "\t\t-" << std::hex << iter->first << " - [";
      for(int i = 0; i < (int)iter->second->nbits; i++) {
	if(bitset_test_bit(iter->second, i)) {
        log << sep << i;
	  sep = ", ";
	}
      }
        log << "]\n";
        log.flush();
    }
  }
#endif
}

void PopControl(int n, ...)
{
#ifdef TRACE
  if(tracing) {
    log <<  "\tpop control: ";
    log.flush();
  }
#endif

  va_list ap;
  ADDRINT addr;
  const char *sep = "";

  va_start(ap, n);
  
  for (; n; n--) {
    
    addr = va_arg(ap, ADDRINT);

#ifdef TRACE
    if(tracing) {
        log << sep << std::hex << addr << "\n";
        log.flush();
      sep = ", ";
    }
#endif
    if(controlTaintMap.end() == controlTaintMap.find(addr)) return;
    
    bitset *s = controlTaintMap[addr];
    bitset_free(s);
    
    controlTaintMap.erase(addr);

  }
  va_end(ap);

#ifdef TRACE
  if(tracing) {
    log << "\n";
    log.flush();
  }
#endif

  // dump control taint map
#ifdef TRACE
  if(tracing) {
    for(map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
	iter != controlTaintMap.end(); iter++) {
      
      sep = "";
      log << "\t\t" << std::hex << iter->first << " - [";
      for(int i = 0; i < (int)iter->second->nbits; i++) {
      if(bitset_test_bit(iter->second, i)) {
          log << sep << i;
          sep = ", ";
      }
      }
      log << "]\n";
      log.flush();
    }
  }
#endif
}

// ####TODO: check this function when controlflow is needed
static void Controlflow(RTN rtn, void *v)
{
  string rtn_name = RTN_Name(rtn);

  IMG img = SEC_Img(RTN_Sec(rtn));

  if(LEVEL_CORE::IMG_TYPE_SHAREDLIB == IMG_Type(img)) return;

  RTN_Open(rtn);

  RoutineGraph *rtnGraph = new RoutineGraph(rtn);

  map<ADDRINT, set<ADDRINT> > controls;

  for(INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {

    if(XED_CATEGORY_COND_BR == INS_Category(ins)) {

      ADDRINT addr = INS_Address(ins);
      BasicBlock *block = rtnGraph->addressMap[addr];
      if(NULL == block) {
	printf("block is null\n");
	fflush(stdout);
      }

      BasicBlock *ipdomBlock = block->getPostDominator();
      if(NULL == ipdomBlock) {
	printf("ipdomBlock is null in %s\n", rtn_name.c_str());
	fflush(stdout);
      }
      ADDRINT ipdomAddr = ipdomBlock->startingAddress;
   
      if(controls.find(ipdomAddr) == controls.end()) {
	controls[ipdomAddr] = set<ADDRINT>();
      }

      controls[ipdomAddr].insert(addr);

      //      printf("placing push call: %#x - %s\n", addr,
      //     INS_Disassemble(ins).c_str());
      
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(PushControl),
		     IARG_PTR, addr,
		     IARG_END);
    }
  }
  
  for(INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {

    ADDRINT addr = INS_Address(ins);
   
    if(controls.end() == controls.find(addr)) continue;

    IARGLIST args = IARGLIST_Alloc();

    for(set<ADDRINT>::iterator iter = controls[addr].begin();
	iter != controls[addr].end(); iter++) {
      IARGLIST_AddArguments(args, IARG_ADDRINT, *iter, IARG_END);
      //      printf("\t%#x\n", *iter);
    }
      

    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(PopControl),
		   IARG_UINT32, controls[addr].size(),
		   IARG_IARGLIST, args,
		   IARG_END);
    IARGLIST_Free(args);
  }
  
  delete rtnGraph;

  RTN_Close(rtn);
}

static void Dataflow(INS ins, void *v)
{
    xed_iclass_enum_t opcode = (xed_iclass_enum_t) INS_Opcode(ins);

    (*instrument_functions[opcode])(ins, v);
}

static void Trace(INS ins, void *v)
{
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(Print),
            IARG_ADDRINT, INS_Address(ins),
            IARG_PTR, new string(INS_Disassemble(ins)),
            IARG_END);
}

/* 
 * Dytan function for tainting memory range.
 * @param addr - starting address of the memory to be tainted
 * @param size - size of the memory range to be tainted
 * @param taint_mark - OPTIONAL user supplied taint mark. -1 by default
 */
void SetNewTaintForMemory(ADDRINT addr, ADDRINT size, int taint_mark)
{

	printf("input %#x\n", addr);

    if (taint_mark == -1) {
        assert(taintGen);
        taint_mark = taintGen->nextTaintMark();
    } else if (taint_mark >= NUMBER_OF_TAINT_MARKS) {
        // check if the user assigned taint mark is legal
        fprintf(stderr, "Illegal taint mark: User supplied taint mark %d is "\
                "bigger than max taint mark (%d)", taint_mark, NUMBER_OF_TAINT_MARKS);
        abort();
    }

    bitset *tmp = bitset_init(NUMBER_OF_TAINT_MARKS);
    bitset_set_bit(tmp, taint_mark);

    for(ADDRINT address = (ADDRINT) addr;
            address < (ADDRINT) addr + size; address++) {
        memTaintMap[address] = bitset_copy(tmp);
    }

#ifdef TRACE
    const char *sep = "";
    //log << "@" << std::hex << addr << "-" << std::hex <<  addr + size -1 << "[";
    for(int i = 0; i < (int)tmp->nbits; i++) {
        if(bitset_test_bit(tmp, i)) {
            //log << sep << i;
            sep = ", ";
        }
    }
    //log << "]\n";
    //log.flush();
#endif
    
    //taintAssignmentLog << taint_mark << " ->" << std::hex << addr << "-" << std::hex << addr + size - 1 << "\n";
    //taintAssignmentLog.flush();

    bitset_free(tmp);
}

void SetNewTaintStart(){
	int new_start = taintGen->getCurrent();
	taintGen->setStart(new_start);
}

PathTaintSource *path_source;
NetworkTaintSource *network_source;
FunctionTaintSource *func_source;

int set_framework_options(config *conf, SyscallMonitor *monitor)
{

	//initialization
	if(conf->sources.size()==0){
		//basic set up
		path_source = new PathTaintSource(monitor, false);
		network_source = new NetworkTaintSource(monitor, false);
		func_source = new FunctionTaintSource();
        if (!conf->num_markings.compare("1")) {
            taintGen = new ConstantTaintGenerator(5);
        } else if (conf->num_markings.compare("")) {
            int num = convertTo<int>(conf->num_markings);
            if (num < 1 || num > NUMBER_OF_TAINT_MARKS) {
                cout << "Incorrect number of taint marks specified";
                exit(1);
            }
            NUMBER_OF_TAINT_MARKS = num;
            taintGen = new TaintGenerator(0, num);
        }
	}

	//case in which I have moultiple sources
    vector<source>::iterator itr = conf->sources.begin();
    // Iterate through all the taint sources
    while (itr != conf->sources.end()) {
        source src = *itr;
        itr++;
        // Specifies whether to taint per byte or per read for IO sources
        taint_range_t taint_granularity = PerRead;
        if (!src.granularity.compare("PerRead")) {
            taint_granularity = PerRead;
        } else if (!src.granularity.compare("PerByte")) {
            taint_granularity = PerByte;
        }


        if (!conf->num_markings.compare("1")) {
            taintGen = new ConstantTaintGenerator(5);
        } else if (conf->num_markings.compare("")) { 
            int num = convertTo<int>(conf->num_markings);
            if (num < 1 || num > NUMBER_OF_TAINT_MARKS) {
                cout << "Incorrect number of taint marks specified";
                exit(1);
            }
            NUMBER_OF_TAINT_MARKS = num;
            taintGen = new TaintGenerator(0, num);
        }


        if (!src.type.compare("path")) {
            if(!src.details[0].compare("*")) {
                path_source = new PathTaintSource(monitor, true);
                path_source->addObserverForAll(taint_granularity);
            }else {
                path_source = new PathTaintSource(monitor, false);
                for (unsigned int i = 0; i < src.details.size(); i++) {
                    string actual_path = src.details[i];
                    path_source->addPathSource(actual_path, taint_granularity); 
                }
            }
        } else if (!src.type.compare("network")) {
            if(!src.details[0].compare("*")) {
                network_source = new NetworkTaintSource(monitor, true);
                network_source->addObserverForAll(taint_granularity);
            }else {
                network_source = new NetworkTaintSource(monitor, false);
                // add network source
                for (unsigned int i = 0; i < src.details.size(); i+=2) {
                    string host_ip = src.details[i];
                    string host_port = src.details[i+1];
                    network_source->addNetworkSource(host_ip, host_port, taint_granularity);
                }
            }
        } else if (!src.type.compare("function")) {
            func_source = new FunctionTaintSource();
            //mattia return value of function is tainted
            //func_source->addFunctionSource("funcname", num_taint_marks);  //TODO: remove hard coding
        } else {
            std::cout << "Invalid source type";
        }
    }

    propagation prop = conf->prop;

    return 0;
}

/* TODO : memory leak. call this function */
void cleanup(void)
{
    delete taintGen;
    delete func_source;
    delete path_source;
    delete network_source;
    log.close();
    //taintAssignmentLog.close();
//    delete conf;

}

void PrintProfilingResult(INT32 code, void *v){
	int count=0;
	for(map<string, int>::iterator profiling_it = profilingMap.begin(); profiling_it != profilingMap.end(); profiling_it++) {
		//cout << profiling_it->second << "\t" << profiling_it->first << "\n";
		prof_log << profiling_it->second << "\t" << profiling_it->first << "\n";
		prof_log.flush();
		count++;
	}
	log << "Total number of keys:" << std::dec << count << "\n";
}

int main(int argc, char **argv)
{
    config *conf = NULL;
    conf = new config;

    log.open("out.log");
    //taintAssignmentLog.open("taint-log.log");

    /**
     * read configuration file and populate conf with
     * the configuration details
     */
    int result = parseConfig(argc, argv, conf);

    if (result == -1) {
        //error in parsing config file
        exit(1);
    }

    PIN_InitSymbols();
    PIN_Init(argc, argv);

    
    /*
     SyscallMonitor takes care of the dirty work of handling system calls
     all that you need to do it give it the system call number of monitor
     and a callback that will be called after the system call and give the
     arguments and the return value.  See syscall_monitor.H for the system 
     call monitor and also syscall_functions.c for the call back functions.
     */
    
    SyscallMonitor *monitor = new SyscallMonitor();
    
    set_framework_options(conf, monitor);

    dest = bitset_init(NUMBER_OF_TAINT_MARKS);
    src = bitset_init(NUMBER_OF_TAINT_MARKS);
    eax = bitset_init(NUMBER_OF_TAINT_MARKS);
    edx = bitset_init(NUMBER_OF_TAINT_MARKS);
    base = bitset_init(NUMBER_OF_TAINT_MARKS);
    idx = bitset_init(NUMBER_OF_TAINT_MARKS);
    eflags = bitset_init(NUMBER_OF_TAINT_MARKS);
    cnt = bitset_init(NUMBER_OF_TAINT_MARKS);

    tracing = false;
    profiling_marks = conf->prof.marks;
    profiling_markop = conf->prof.markop;
    if(profiling_marks){
        prof_log.open("prof.log");
    }
    if(profiling_markop){
    	prof_log.open("prof.log");
    	PIN_AddFiniFunction	(PrintProfilingResult,0);
    	word = true;
    	word_size=4;
    }
    
    /*
     * Set up taint sinks
     */
/*    vector<sink>::iterator itr2 = conf->sinks.begin();
    // Iterate over taint sinks
    while (itr2 != conf->sinks.end()) {
        sink snk = *itr2;
        itr2++;
        //TODO
    } */

    IMG_AddInstrumentFunction(ReplaceUserFunctions, 0);

    //this is how to mark inputs to the program
    RTN_AddInstrumentFunction(taint_routines, 0);

#ifdef TRACE
    INS_AddInstrumentFunction(Trace, 0);
#endif

    /* Setup taint propagation */
    if (conf->prop.dataflow)
        INS_AddInstrumentFunction(Dataflow, 0);
    if (conf->prop.controlflow) {
        RTN_AddInstrumentFunction(Controlflow, 0);
        controlFlowTainting = true;
    }else
        controlFlowTainting = false;

/*
   This the large dispatch table that associated a dataflow instrumentation
   function with an instruction opcode.  See instrument_functions.c for
   the actualy instrumentation functions.  
   */

    // set a default handling function that aborts.  This makes sure I don't
    // miss instructions in new applications
    for(int i = 0; i < XED_ICLASS_LAST; i++) {
        instrument_functions[i] = &UnimplementedInstruction;
    }

    instrument_functions[XED_ICLASS_ADD] = &Instrument_ADD; // 1
    instrument_functions[XED_ICLASS_PUSH] = &Instrument_PUSH; // 2
    instrument_functions[XED_ICLASS_POP] = &Instrument_POP; // 3
    instrument_functions[XED_ICLASS_OR] = &Instrument_OR; // 4

    instrument_functions[XED_ICLASS_ADC] = &Instrument_ADC; // 6
    instrument_functions[XED_ICLASS_SBB] = &Instrument_SBB; // 7
    instrument_functions[XED_ICLASS_AND] = &Instrument_AND; // 8

    //  instrument_functions[XED_ICLASS_DAA] = &Instrument_DAA; // 11
    instrument_functions[XED_ICLASS_SUB] = &Instrument_SUB; // 12

    //  instrument_functions[XED_ICLASS_DAS] = &Instrument_DAS; // 14
    instrument_functions[XED_ICLASS_XOR] = &Instrument_XOR; // 15

    //  instrument_functions[XED_ICLASS_AAA] = &Instrument_AAA; // 17
    instrument_functions[XED_ICLASS_CMP] = &Instrument_CMP; // 18

    //  instrument_functions[XED_ICLASS_AAS] = &Instrument_AAS; // 20
    instrument_functions[XED_ICLASS_INC] = &Instrument_INC; // 21
    instrument_functions[XED_ICLASS_DEC] = &Instrument_DEC; // 22

    //  instrument_functions[XED_ICLASS_PUSHAD] = &Instrument_PUSHAD; // 25
    //  instrument_functions[XED_ICLASS_POPAD] = &Instrument_POPAD; // 27
    //  instrument_functions[XED_ICLASS_BOUND] = &Instrument_BOUND; // 28
    //  instrument_functions[XED_ICLASS_ARPL] = &Instrument_ARPL; // 29

    instrument_functions[XED_ICLASS_IMUL] = &Instrument_IMUL; // 35
    //  instrument_functions[XED_ICLASS_INSB] = &Instrument_INSB; // 36

    //  instrument_functions[XED_ICLASS_INSD] = &Instrument_INSD; // 38
    //  instrument_functions[XED_ICLASS_OUTSB] = &Instrument_OUTSB; // 39

    //  instrument_functions[XED_ICLASS_OUTSD] = &Instrument_OUTSD; // 41
    instrument_functions[XED_ICLASS_JO] = &Instrument_Jcc; //42
    instrument_functions[XED_ICLASS_JNO] = &Instrument_Jcc; //43
    instrument_functions[XED_ICLASS_JB] = &Instrument_Jcc; //43
    instrument_functions[XED_ICLASS_JNB] = &Instrument_Jcc; //45
    instrument_functions[XED_ICLASS_JZ] = &Instrument_Jcc; //46
    instrument_functions[XED_ICLASS_JNZ] = &Instrument_Jcc; //47
    instrument_functions[XED_ICLASS_JBE] = &Instrument_Jcc; //48
    instrument_functions[XED_ICLASS_JNBE] = &Instrument_Jcc; //49
    instrument_functions[XED_ICLASS_JS] = &Instrument_Jcc; //50
    instrument_functions[XED_ICLASS_JNS] = &Instrument_Jcc; //51
    instrument_functions[XED_ICLASS_JP] = &Instrument_Jcc; //52
    instrument_functions[XED_ICLASS_JNP] = &Instrument_Jcc; //53
    instrument_functions[XED_ICLASS_JL] = &Instrument_Jcc; //54
    instrument_functions[XED_ICLASS_JNL] = &Instrument_Jcc; //55
    instrument_functions[XED_ICLASS_JLE] = &Instrument_Jcc; //56
    instrument_functions[XED_ICLASS_JNLE] = &Instrument_Jcc; //57
    instrument_functions[XED_ICLASS_CMOVNLE] = &Instrument_CMOVcc; //58
    instrument_functions[XED_ICLASS_TEST] = &Instrument_TEST; //59
    instrument_functions[XED_ICLASS_XCHG] = &Instrument_XCHG; //60
    instrument_functions[XED_ICLASS_MOV] = &Instrument_MOV; //61
    instrument_functions[XED_ICLASS_LEA] = &Instrument_LEA; //62

    instrument_functions[XED_ICLASS_PAUSE] = &Instrument_PAUSE; //64

    instrument_functions[XED_ICLASS_CWDE] = &Instrument_CWDE; //67

    instrument_functions[XED_ICLASS_CDQ] = &Instrument_CDQ; //70
    //  instrument_functions[XED_ICLASS_CALL_FAR] = &Instrument_CALL_FAR; //71
    //  instrument_functions[XED_ICLASS_WAIT] = &Instrument_WAIT; //72
    instrument_functions[XED_ICLASS_CMPSW] = &Instrument_CMPSW;//73

    instrument_functions[XED_ICLASS_PUSHFD] = &Instrument_PUSHFD; //74

    instrument_functions[XED_ICLASS_POPFD] = &Instrument_POPFD; //77

    instrument_functions[XED_ICLASS_SAHF] = &Instrument_SAHF; //79
    instrument_functions[XED_ICLASS_LAHF] = &Instrument_LAHF; //80
    instrument_functions[XED_ICLASS_MOVSB] = &Instrument_MOVSB; //81
    instrument_functions[XED_ICLASS_MOVSW] = &Instrument_MOVSW; //82

    instrument_functions[XED_ICLASS_CMPSB] = &Instrument_CMPSB; //85

    //  instrument_functions[XED_ICLASS_CMPSD] = &Instrument_CMPSD; //87

    instrument_functions[XED_ICLASS_STOSB] = &Instrument_STOSB; //89
    //  instrument_functions[XED_ICLASS_STOSW] = &Instrument_STOSW; //90
    instrument_functions[XED_ICLASS_STOSD] = &Instrument_STOSD; //91

    //  instrument_functions[XED_ICLASS_LODSB] = &Instrument_LODSB; //93

    //  instrument_functions[XED_ICLASS_LODSD] = &Instrument_LODSD; //95

    instrument_functions[XED_ICLASS_SCASB] = &Instrument_SCASB; //97

    //  instrument_functions[XED_ICLASS_SCASD] = &Instrument_SCASD; //99

    instrument_functions[XED_ICLASS_RET_NEAR] = &Instrument_RET_NEAR; //102
    //  instrument_functions[XED_ICLASS_LES] = &Instrument_LES; //103
    //  instrument_functions[XED_ICLASS_LDS] = &Instrument_LDS; //104

    //  instrument_functions[XED_ICLASS_ENTER] = &Instrument_ENTER; //106
    instrument_functions[XED_ICLASS_LEAVE] = &Instrument_LEAVE; //107
    //  instrument_functions[XED_ICLASS_RET_FAR] = &Instrument_RET_FAR; //108
    //  instrument_functions[XED_ICLASS_INT3] = &Instrument_INT3; //109
    instrument_functions[XED_ICLASS_INT] = &Instrument_INT; //110
    //  instrument_functions[XED_ICLASS_INT0] = &Instrument_INT0; //111

    //  instrument_functions[XED_ICLASS_IRETD] = &Instrument_IRETD; //113

    //  instrument_functions[XED_ICLASS_AAM] = &Instrument_AAM; //115
    //  instrument_functions[XED_ICLASS_AAD] = &Instrument_AAD; //116
    //  instrument_functions[XED_ICLASS_SALC] = &Instrument_SALC; //117
    //  instrument_functions[XED_ICLASS_XLAT] = &Instrument_XLAT; //118

    //  instrument_functions[XED_ICLASS_LOOPNE] = &Instrument_LOOPNE; //120
    //  instrument_functions[XED_ICLASS_LOOPE] = &Instrument_LOOPE; //121
    //  instrument_functions[XED_ICLASS_LOOP] = &Instrument_LOOP; //122
    instrument_functions[XED_ICLASS_JRCXZ] = &Instrument_Jcc; //123
    //  instrument_functions[XED_ICLASS_IN] = &Instrument_IN; //124
    //  instrument_functions[XED_ICLASS_OUT] = &Instrument_OUT; //125
    instrument_functions[XED_ICLASS_CALL_NEAR] = &Instrument_CALL_NEAR; //126
    instrument_functions[XED_ICLASS_JMP] = &Instrument_JMP; //127
    //  instrument_functions[XED_ICLASS_JMP_FAR] = &Instrument_JMP_FAR; //128

    //  instrument_functions[XED_ICLASS_INT_l] = &Instrument_INT_l; //130

    instrument_functions[XED_ICLASS_HLT] = &Instrument_HLT; //133
    //  instrument_functions[XED_ICLASS_CMC] = &Instrument_CMC; //134

    //  instrument_functions[XED_ICLASS_CLC] = &Instrument_CLC; //136
    //  instrument_functions[XED_ICLASS_STC] = &Instrument_STC; //137
    //  instrument_functions[XED_ICLASS_CLI] = &Instrument_CLI; //138
    //  instrument_functions[XED_ICLASS_STI] = &Instrument_STI; //139
    instrument_functions[XED_ICLASS_CLD] = &Instrument_CLD; //140
    instrument_functions[XED_ICLASS_STD] = &Instrument_STD; //141

    instrument_functions[XED_ICLASS_RDTSC] = &Instrument_RDTSC; //169

    instrument_functions[XED_ICLASS_CMOVB] = &Instrument_CMOVcc; //177
    instrument_functions[XED_ICLASS_CMOVNB] = &Instrument_CMOVcc; //178
    instrument_functions[XED_ICLASS_CMOVZ] = &Instrument_CMOVcc; //179
    instrument_functions[XED_ICLASS_CMOVNZ] = &Instrument_CMOVcc; //180
    instrument_functions[XED_ICLASS_CMOVBE] = &Instrument_CMOVcc; //181
    instrument_functions[XED_ICLASS_CMOVNBE] = &Instrument_CMOVcc; //182

    //  instrument_functions[XED_ICLASS_EMMS] = &Instrument_EMMS; //216

    instrument_functions[XED_ICLASS_SETB] = &Instrument_SETcc; //222
    instrument_functions[XED_ICLASS_SETNB] = &Instrument_SETcc; //223
    instrument_functions[XED_ICLASS_SETZ] = &Instrument_SETcc; //224
    instrument_functions[XED_ICLASS_SETNZ] = &Instrument_SETcc; //225
    instrument_functions[XED_ICLASS_SETBE] = &Instrument_SETcc; //226
    instrument_functions[XED_ICLASS_SETNBE] = &Instrument_SETcc; //227
    instrument_functions[XED_ICLASS_CPUID] = &Instrument_CPUID; //228
    instrument_functions[XED_ICLASS_BT] = &Instrument_BT; //229
    instrument_functions[XED_ICLASS_SHLD] = &Instrument_SHLD; //230
    instrument_functions[XED_ICLASS_CMPXCHG] = &Instrument_CMPXCHG; //231

    //  instrument_functions[XED_ICLASS_BTR] = &Instrument_BTR; //233

    instrument_functions[XED_ICLASS_NOP] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP2] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP3] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP4] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP5] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP6] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP7] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP8] = &Instrument_NOP;
    instrument_functions[XED_ICLASS_NOP9] = &Instrument_NOP;

    instrument_functions[XED_ICLASS_MOVZX] = &Instrument_MOVZX; //236
    instrument_functions[XED_ICLASS_XADD] = &Instrument_XADD; //237

    //  instrument_functions[XED_ICLASS_PSRLQ] = &Instrument_PSRLQ; //250
    //  instrument_functions[XED_ICLASS_PADDQ] = &Instrument_PADDQ; //251

    //  instrument_functions[XED_ICLASS_MOVQ] = &Instrument_MOVQ; //255

    //  instrument_functions[XED_ICLASS_MOVQ2Q] = &Instrument_MOVDQ2Q; //258

    //  instrument_functions[XED_ICLASS_PSLLQ] = &Instrument_PSLLQ; //272
    //  instrument_functions[XED_ICLASS_PMULUDQ] = &Instrument_PMULUDQ; //273

    //  instrument_functions[XED_ICLASS_UD2] = &Instrument_UD2; //281
    instrument_functions[XED_ICLASS_MOVAPS] = &Instrument_MOVAPS;//301
    instrument_functions[XED_ICLASS_MOVD] = &Instrument_MOVD;//303
    instrument_functions[XED_ICLASS_MOVDQA] = &Instrument_MOVDQA;//306
    instrument_functions[XED_ICLASS_MOVDQU] = &Instrument_MOVDQU;//307
    instrument_functions[XED_ICLASS_CMOVS] = &Instrument_CMOVcc;
    instrument_functions[XED_ICLASS_CMOVNS] = &Instrument_CMOVcc; //308
    instrument_functions[XED_ICLASS_MOVHPD] = &Instrument_MOVHPD;//311
    instrument_functions[XED_ICLASS_CMOVL] = &Instrument_CMOVcc;
    instrument_functions[XED_ICLASS_CMOVNL] = &Instrument_CMOVcc; //312
    instrument_functions[XED_ICLASS_CMOVLE] = &Instrument_CMOVcc; //313
    instrument_functions[XED_ICLASS_MOVLPD] = &Instrument_MOVLPD; //314
    instrument_functions[XED_ICLASS_MOVQ] = &Instrument_MOVQ; //324

    instrument_functions[XED_ICLASS_MOVSD] = &Instrument_MOVSD; //mattia 327

    instrument_functions[XED_ICLASS_MOVSD_XMM] = &Instrument_MOVSD_XMM; //mattia 328

    instrument_functions[XED_ICLASS_MUL] = &Instrument_MUL; //mattia 342

    //instrument_functions[XED_ICLASS_MOVD] = &Instrument_MOVD; //350
    //instrument_functions[XED_ICLASS_MOVDQU] = &Instrument_MOVDQU; //351

    //instrument_functions[XED_ICLASS_MOVDQA] = &Instrument_MOVDQA; //354

    instrument_functions[XED_ICLASS_SETS] = &Instrument_SETcc; //361

    instrument_functions[XED_ICLASS_SETL] = &Instrument_SETcc; //365
    instrument_functions[XED_ICLASS_SETNL] = &Instrument_SETcc; //366
    instrument_functions[XED_ICLASS_SETLE] = &Instrument_SETcc; //367
    instrument_functions[XED_ICLASS_SETNLE] = &Instrument_SETcc; //368

    //instrument_functions[XED_ICLASS_BTS] = &Instrument_BTS; //370
    instrument_functions[XED_ICLASS_SHRD] = &Instrument_SHRD; //371

    instrument_functions[XED_ICLASS_BSF] = &Instrument_BSF; //376
    instrument_functions[XED_ICLASS_BSR] = &Instrument_BSR; //377
    instrument_functions[XED_ICLASS_MOVSX] = &Instrument_MOVSX; //378
    instrument_functions[XED_ICLASS_BSWAP] = &Instrument_BSWAP; //379
    instrument_functions[XED_ICLASS_PALIGNR] = &Instrument_PALIGNR;//381

    //instrument_functions[XED_ICLASS_PAND] = &Instrument_PAND; //383

    //instrument_functions[XED_ICLASS_PSUBSW] = &Instrument_PSUBSW; //389



    instrument_functions[XED_ICLASS_PCMPEQB] = &Instrument_PCMPEQB; //mattia 391

    instrument_functions[XED_ICLASS_PMOVMSKB] = &Instrument_PMOVMSKB; // mattia 453

    instrument_functions[XED_ICLASS_ROL] = &Instrument_ROL; //472
    instrument_functions[XED_ICLASS_ROR] = &Instrument_ROR; //473
    //instrument_functions[XED_ICLASS_RCL] = &Instrument_RCL; //474
    //instrument_functions[XED_ICLASS_RCR] = &Instrument_RCR; //475
    instrument_functions[XED_ICLASS_SHL] = &Instrument_SHL; //476
    instrument_functions[XED_ICLASS_SHR] = &Instrument_SHR; //477
    instrument_functions[XED_ICLASS_SAR] = &Instrument_SAR; //478
    instrument_functions[XED_ICLASS_NOT] = &Instrument_NOT; //479
    instrument_functions[XED_ICLASS_NEG] = &Instrument_NEG; //480
    //instrument_functions[XED_ICLASS_POR] = &Instrument_POR; //mattia 481
    instrument_functions[XED_ICLASS_DIV] = &Instrument_DIV; //482
    instrument_functions[XED_ICLASS_PREFETCHT0] = &Instrument_PREFETCHT0;//483
    instrument_functions[XED_ICLASS_IDIV] = &Instrument_IDIV;
    instrument_functions[XED_ICLASS_PSHUFD] = &Instrument_PSHUFD; //491

    instrument_functions[XED_ICLASS_LDMXCSR] = &Instrument_LDMXCSR; //507
    instrument_functions[XED_ICLASS_STMXCSR] = &Instrument_STMXCSR;
    instrument_functions[XED_ICLASS_PSUBB] = &Instrument_PSUBB; //508

    instrument_functions[XED_ICLASS_FDIV] = &Instrument_FDIV;
    instrument_functions[XED_ICLASS_FLD] = &Instrument_FLD; //523
    instrument_functions[XED_ICLASS_FST] = &Instrument_FST; //524
    instrument_functions[XED_ICLASS_FSTP] = &Instrument_FSTP; //525
    instrument_functions[XED_ICLASS_FLDCW] = &Instrument_FLDCW; //527

    instrument_functions[XED_ICLASS_FNSTCW] = &Instrument_FNSTCW; //529
    instrument_functions[XED_ICLASS_FXCH] = &Instrument_FXCH; //530

    instrument_functions[XED_ICLASS_PXOR] = &Instrument_PXOR; //mattia 532

    instrument_functions[XED_ICLASS_FLDZ] = &Instrument_FLDZ; //542
    instrument_functions[XED_ICLASS_FILD] = &Instrument_FILD; //572
    instrument_functions[XED_ICLASS_FISTP] = &Instrument_FISTP; //575

    instrument_functions[XED_ICLASS_FNSTSW] = &Instrument_FNSTSW; //587
    instrument_functions[XED_ICLASS_FUCOM] = &Instrument_FUCOM; //589
    instrument_functions[XED_ICLASS_FADDP] = &Instrument_FADDP; //591
    instrument_functions[XED_ICLASS_FDIVRP] = &Instrument_FDIVRP; //596
    instrument_functions[XED_ICLASS_FDIVP] = &EmptyHandler; //597
    instrument_functions[XED_ICLASS_FIDIVR] = &EmptyHandler; //566
    instrument_functions[XED_ICLASS_FIADD] = &EmptyHandler; //559
    instrument_functions[XED_ICLASS_FIST] = &EmptyHandler; //574
    instrument_functions[XED_ICLASS_XORPS] = &Instrument_XORPS; //644


    // instructions that compare real numbers and set the eflags register
    instrument_functions[XED_ICLASS_FCOMI] = &Instrument_Eflags;
    instrument_functions[XED_ICLASS_FCOMIP] = &Instrument_Eflags;
    instrument_functions[XED_ICLASS_FUCOMI] = &Instrument_Eflags;
    instrument_functions[XED_ICLASS_FUCOMIP] = &Instrument_Eflags;

    
    //Add instrumentation functions for syscall handling
    PIN_AddSyscallEntryFunction(SysBefore, (VOID *)monitor);
    PIN_AddSyscallExitFunction(SysAfter, (VOID *)monitor);

    // set a default observer that aborts when a program uses a system
    // call that we don't provide a handling function for.
    monitor->setDefaultObserver(UnimplementedSystemCall);

    monitor->addObserver(SYS_access, Handle_ACCESS, 0);
    monitor->addObserver(SYS_alarm, Handle_ALARM, 0);
    monitor->addObserver(SYS_brk, Handle_BRK, 0);
    monitor->addObserver(SYS_chmod, Handle_CHMOD, 0);
    monitor->addObserver(SYS_close, Handle_CLOSE, 0);
    monitor->addObserver(SYS_dup, Handle_DUP, 0);
    monitor->addObserver(SYS_fcntl64, Handle_FSTAT64, 0);
    monitor->addObserver(SYS_flock, Handle_FLOCK, 0);
    monitor->addObserver(SYS_fstat64, Handle_FSTAT64, 0);
    monitor->addObserver(SYS_fsync, Handle_FSYNC, 0);
    monitor->addObserver(SYS_ftruncate, Handle_FTRUNCATE, 0);
    monitor->addObserver(SYS_getdents64, Handle_GETDENTS64, 0);
    monitor->addObserver(SYS_getpid, Handle_GETPID, 0);
    monitor->addObserver(SYS_gettimeofday, Handle_GETTIMEOFDAY, 0);
    monitor->addObserver(SYS_getuid32, Handle_GETUID32, 0);
    monitor->addObserver(SYS_ioctl, Handle_IOCTL, 0);
    monitor->addObserver(SYS_link, Handle_LINK, 0);
    monitor->addObserver(SYS__llseek, Handle_LLSEEK, 0);
    monitor->addObserver(SYS_lseek, Handle_LSTAT64, 0);
    monitor->addObserver(SYS_lstat64, Handle_LSTAT64, 0);
    monitor->addObserver(SYS_mmap, Handle_MMAP, 0);
    monitor->addObserver(SYS_mmap2, Handle_MMAP2, 0);
    monitor->addObserver(SYS_mprotect, Handle_MPROTECT, 0);
    monitor->addObserver(SYS_munmap, Handle_MUNMAP, 0);

    /* Pin has problems instrumenting the nanosleep system call so we skip it */
    // monitor->addObserver(SYS_nanosleep, Handle_NANOSLEEP, 0);
    monitor->addObserver(SYS_open, Handle_OPEN, 0);
    monitor->addObserver(SYS_read, Handle_READ, 0);
    monitor->addObserver(SYS_readlink, Handle_READLINK, 0);
    monitor->addObserver(SYS_rename, Handle_RENAME, 0);
    monitor->addObserver(SYS_rt_sigaction, Handle_RT_SIGACTION, 0);
    monitor->addObserver(SYS_rt_sigprocmask, Handle_RT_SIGPROCMASK, 0);
    monitor->addObserver(SYS_set_thread_area, Handle_SET_THREAD_AREA, 0);
    monitor->addObserver(SYS_stat64, Handle_STAT64, 0);
    monitor->addObserver(SYS_socketcall, Handle_SOCKETCALL, 0);
    monitor->addObserver(SYS_time, Handle_TIME, 0);
    monitor->addObserver(SYS_uname, Handle_UNAME, 0);
    monitor->addObserver(SYS_unlink, Handle_UNLINK, 0);
    monitor->addObserver(SYS_utime, Handle_UTIME, 0);
    monitor->addObserver(SYS_write, Handle_WRITE, 0);
    monitor->addObserver(SYS_writev, Handle_WRITEV, 0);
    monitor->addObserver(SYS_poll, Handle_POLL, 0);
    monitor->addObserver(SYS_gettid, Handle_GETTID, 0);
    monitor->addObserver(SYS_tgkill, Handle_TGKILL, 0);
    monitor->addObserver(SYS_getgid32, Handle_GETGID32, 0);
    monitor->addObserver(SYS_geteuid32, Handle_GETEUID32, 0);
    monitor->addObserver(SYS_getegid32, Handle_GETEGID32, 0);
    monitor->addObserver(SYS_getdents, Handle_GETDENTS, 0);
    monitor->addObserver(SYS_clone, Handle_CLONE, 0);
    monitor->addObserver(SYS_dup2, Handle_DUP2, 0);
    monitor->addObserver(SYS_waitpid, Handle_WAITPID, 0);
    monitor->addObserver(SYS_set_tid_address, Handle_SET_TID_ADDRESS, 0);
    monitor->addObserver(SYS_chown32, Handle_CHOWN32, 0);

    // never returns
    PIN_StartProgram();

}
