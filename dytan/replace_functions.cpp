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

#include "replace_functions.h"
#include "dytan.h"
/*
  This file sets of convience functions that can be called from an
  application to interact with the tool


  Current we can assigned taint marks, dump taint marks, and control
  if logging is enabled
 */


void SetTrace(int trace)
{
  tracing = trace;
}

map<string, int> tagMap;

void AssignTagToByteRange(ADDRINT start, size_t size, size_t id)
{

  std::ostringstream os;
  os << id;
  string name = os.str();

  assert(taintGen);
  if(tagMap.end() == tagMap.find(name)) {
    tagMap[name] = taintGen->nextTaintMark();
  }
  
  bitset *s = bitset_init(NUMBER_OF_TAINT_MARKS);
  bitset_set_bit(s, tagMap[name]);
  
  const char *sep = "";
  cout << "assigned location " <<std::hex << start << "-" << std::hex << start + size -1 << " taint marks [";

  for(unsigned int i = 0; i < s->nbits; i++) {
    if(bitset_test_bit(s, i)) {
      printf("%s%d", sep, i);
      sep = ", ";
    }
  }
  printf("]\n");

  for(ADDRINT addr = start; addr < start + size; addr++) {
    memTaintMap[addr] = bitset_copy(s);
  }
  bitset_free(s);
}

void AssignPointerTagToByteRange(ADDRINT start, size_t size, size_t id)
{

  std::ostringstream os;
  os << id;
  string name = os.str();

  assert(taintGen);
  if(tagMap.end() == tagMap.find(name)) {
    tagMap[name] = taintGen->nextTaintMark();
  }

  bitset *s = bitset_init(NUMBER_OF_TAINT_MARKS);
  bitset_set_bit(s, tagMap[name]);

  const char *sep = "";
  cout << "assigned tag to pointer " <<std::hex << start << "-" << std::hex << start + size -1 << " taint marks [";

  for(unsigned int i = 0; i < s->nbits; i++) {
    if(bitset_test_bit(s, i)) {
      printf("%s%d", sep, i);
      sep = ", ";
    }
  }
  printf("]\n");

  for(ADDRINT addr = start; addr < start + size; addr++) {
    memTaintMap[addr] = bitset_copy(s);
  }
  bitset_free(s);
}

void AssignMemoryTagToByteRange(ADDRINT start, size_t size, size_t id)
{

  std::ostringstream os;
  os << id;
  string name = os.str();

  assert(taintGen);
  if(tagMap.end() == tagMap.find(name)) {
    tagMap[name] = taintGen->nextTaintMark();
  }

  bitset *s = bitset_init(NUMBER_OF_TAINT_MARKS);
  bitset_set_bit(s, tagMap[name]);

  const char *sep = "";
  cout << "assigned tag to memory " <<std::hex << start << "-" << std::hex << start + size -1 << " taint marks [";

  for(unsigned int i = 0; i < s->nbits; i++) {
    if(bitset_test_bit(s, i)) {
      printf("%s%d", sep, i);
      sep = ", ";
    }
  }
  printf("]\n");

  for(ADDRINT addr = start; addr < start + size; addr++) {
    memTaintMap[addr] = bitset_copy(s);
  }
  bitset_free(s);
}

void DisplayTagsForByteRange(ADDRINT start, size_t size, char *fmt, ...)
{
  bitset *tmp = bitset_init(NUMBER_OF_TAINT_MARKS);
  for(ADDRINT addr = start; addr < start + size; addr++) {
    map<ADDRINT, bitset *>::iterator iter = memTaintMap.find(addr);
    if(memTaintMap.end() != iter) {
      bitset_union(tmp, iter->second);  
    }
  }

  bitset *controlTaint = bitset_init(NUMBER_OF_TAINT_MARKS);
  for(map<ADDRINT, bitset *>::iterator iter = controlTaintMap.begin();
      iter != controlTaintMap.end(); iter++) {
    bitset_union(controlTaint, iter->second);
  }
  bitset_union(tmp, controlTaint);

  va_list ap;
  va_start(ap, fmt);
  
  vprintf(fmt, ap);
  va_end(ap);

  const char *sep = "";
  printf(" at a location %#x-%#x has taint marks: [", start, start + size - 1);
  for(unsigned int i = 0; i < tmp->nbits; i++) {
    if(bitset_test_bit(tmp, i)) {
      printf("%s%d", sep, i);
      sep = ", ";
    }
  }
  printf("] with active control flow taint marks[");
  sep = "";
  for(unsigned int i = 0; i < controlTaint->nbits; i++) {
    if(bitset_test_bit(controlTaint, i)) {
      printf("%s%d", sep, i);
      sep = ", ";
    }
  }
  printf("]\n");
 
  bitset_free(tmp);
  bitset_free(controlTaint);
}

void ClearTagsForByteRange(ADDRINT start)
{
	//delete taint marks for contiguous chunk of memory
	ADDRINT addr = start;
	ADDRINT finish = start;
	while(true){
		map<ADDRINT, bitset *>::iterator iter = memTaintMap.find(addr);
		if(memTaintMap.end() != iter) {
			memTaintMap.erase(addr);
			finish = addr;
			addr++;
		}
		else{
			break;
		}
	}

	cout << "cleared tag for memory " <<std::hex << start << "-" << std::hex << finish << "\n";
}

void TagsCheck(ADDRINT pointer, ADDRINT memory)
{
//TODO implement check, not needed for profiling
//	std::vector<int> pointer_taint_marks;
//	bitset *tmp_pointer = bitset_init(NUMBER_OF_TAINT_MARKS);
//	map<ADDRINT, bitset *>::iterator iter_pointer = memTaintMap.find(pointer);
//	if(memTaintMap.end() != iter_pointer) {
//		bitset_union(tmp_pointer, iter_pointer->second);
//		const char *sep = "";
//		printf("pointer %#x has taint marks: [", pointer);
//		for(unsigned int i = 0; i < tmp_pointer->nbits; i++) {
//			if(bitset_test_bit(tmp_pointer, i)) {
//				printf("%s%d", sep, i);
//				sep = ", ";
//				pointer_taint_marks.push_back(i);
//			}
//		}
//		printf("]\n");
//	}
//	else{
//		//printf("location %#x has problems\n", pointer);
//	}
//
//	std::vector<int> memory_taint_marks;
//	bitset *tmp = bitset_init(NUMBER_OF_TAINT_MARKS);
//	map<ADDRINT, bitset *>::iterator iter = memTaintMap.find(memory);
//	if(memTaintMap.end() != iter) {
//		bitset_union(tmp, iter->second);
//		const char *sep = "";
//		printf("memory %#x has taint marks: [", memory);
//		for(unsigned int i = 0; i < tmp->nbits; i++) {
//			if(bitset_test_bit(tmp, i)) {
//				printf("%s%d", sep, i);
//				sep = ", ";
//				memory_taint_marks.push_back(i);
//			}
//		}
//		printf("]\n");
//	}
//	else{
//		//printf("location %#x has problems\n", memory);
//	}
//
//	bool found = false;
//	for(unsigned int i=0; i<pointer_taint_marks.size();i++){
//		for(unsigned int k=0; k<memory_taint_marks.size();k++){
//			if(pointer_taint_marks[i]==memory_taint_marks[k]){
//				found = true;
//				break;
//			}
//		}
//	}
//	if(!found){
//		printf("IMA\n");
//	}
}

void ReplaceUserFunctions(IMG img, void *v)
{
  const char *func_names[] = {
    "DYTAN_set_trace",
    "DYTAN_display",
    "DYTAN_tag",
    "DYTAN_tag_pointer",
    "DYTAN_tag_memory",
    "DYTAN_free",
    "DYTAN_check",
  };
  
  AFUNPTR functions[] = {
    AFUNPTR(SetTrace),
    AFUNPTR(DisplayTagsForByteRange),
    AFUNPTR(AssignTagToByteRange),
    AFUNPTR(AssignPointerTagToByteRange),
    AFUNPTR(AssignMemoryTagToByteRange),
    AFUNPTR(ClearTagsForByteRange),
    AFUNPTR(TagsCheck),
  };
  
  if(IMG_TYPE_SHAREDLIB == IMG_Type(img)) return;
  
  printf("Looking at img: %s\n", IMG_Name(img).c_str());
  
  RTN rtn;
  
  for(int i = 0; i < 7; i++) {
    rtn = RTN_FindByName(img, func_names[i]);
    if(RTN_Valid(rtn)) {
      RTN_Replace(rtn, functions[i]);
      printf("Replaced: %s\n", func_names[i]);
    }
    else {
      printf("Could not replace: %s\n", func_names[i]);
    }
  }
}
