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

#include "taint_source_path.h"
#include "dytan.h"

void pathSourceReadDefault(string, syscall_arguments, void *);
void pathSourceReadCallbackPerByte(string, syscall_arguments, void *);
void pathSourceReadCallbackPerRead(string, syscall_arguments, void *);

PathTaintSource::PathTaintSource(SyscallMonitor *syscallMonitor, bool observeEverything)
{
  monitor = new PathMonitor(syscallMonitor, observeEverything);
  monitor->activate();

  monitor->registerDefault(pathSourceReadDefault, this);
}  

PathTaintSource::~PathTaintSource()
{
  delete monitor;
}

void PathTaintSource::addObserverForAll(taint_range_t type)
{
    switch (type) {
        case PerByte:
            monitor->registerCallbackForAll(pathSourceReadCallbackPerByte, NULL);
            break;
        case PerRead:
            monitor->registerCallbackForAll(pathSourceReadCallbackPerRead, NULL);
            break;
        default:
            printf("Missing case\n");
            abort();
    }
}

void PathTaintSource::addPathSource(string pathname, taint_range_t type)
{
  switch(type) {
  case PerByte: {
    monitor->observePath(pathname, pathSourceReadCallbackPerByte, NULL);
    break;
  }
  case PerRead: {
    monitor->observePath(pathname, pathSourceReadCallbackPerRead, NULL);
    break;
  }
  default:
    printf("Missing case\n");
    abort();
  }

}

/**************************************************************/

// ssize_t read(int fd, void *buf, size_t count);

void pathSourceReadCallbackPerByte(string pathname,
				   syscall_arguments args,
				   void *v)
{
//  TaintGenerator *gen = static_cast<TaintGenerator *>(v);
  
  char *buf = (char *) args.arg1;
  int ret = args.ret;
  int tag = -1;

  ADDRINT start = (ADDRINT) buf;
  ADDRINT end = start + ret;

  //bail if nothing was actually assigned to memory
  if(ret <= 0) return;
  
  assert(taintGen);
  bitset *s = bitset_init(NUMBER_OF_TAINT_MARKS);

  for(ADDRINT addr = start; addr < end; addr++) {
      tag = taintGen->nextTaintMark();
      bitset_set_bit(s, tag);
      memTaintMap[addr] = bitset_copy(s);
      bitset_reset(s);
  }
  bitset_free(s);
  
  lseek(args.arg0, 0, SEEK_CUR);
  //mattia
  //off_t currentOffset = lseek(args.arg0, 0, SEEK_CUR);
  //off_t curr = currentOffset - args.ret;
  //ADDRINT currAddress = start;
  //while (curr != currentOffset) {
  //    taintAssignmentLog << tag << " - " << pathname << "[" << curr++ << "] -> " << std::hex << currAddress++ << "\n";
  //}
  //taintAssignmentLog.flush();
  
  
#ifdef TRACE
  if(tracing) {
      log << "\t" << std::hex << start << "-" << std::hex << end - 1 << " <- read\n";
      log.flush();
  }
#endif
} 

void pathSourceReadCallbackPerRead(string pathname, 
				   syscall_arguments args,
				   void *v)
{
//  TaintGenerator *gen = static_cast<TaintGenerator *>(v);
 
  char *buf = (char *) args.arg1;
  int ret = args.ret;
  int tag;

  ADDRINT start = (ADDRINT) buf;
  ADDRINT end = start + ret;

  //bail if nothing was actually assigned to memory
  if(ret <= 0) return;
  
  assert(taintGen);
  bitset *s = bitset_init(NUMBER_OF_TAINT_MARKS);
  tag = taintGen->nextTaintMark();
  bitset_set_bit(s, tag);

  for(ADDRINT addr = start; addr < end; addr++) {
      memTaintMap[addr] = bitset_copy(s);
  }
  bitset_free(s);
  
  lseek(args.arg0, 0, SEEK_CUR);
  //mattia
  //off_t currentOffset = lseek(args.arg0, 0, SEEK_CUR);
  //taintAssignmentLog << tag << " - " << pathname << "[" << currentOffset - args.ret << "-" << currentOffset << "] -> " << std::hex << start << "-" << std::hex << end -1 << "\n";
  //taintAssignmentLog.flush();
  
#ifdef TRACE
  if(tracing) {
      log << "\t" << std::hex << start << "-" << std::hex << end - 1 << " <- read(" << tag <<")\n";
      log.flush();
  }
#endif
 
}

void pathSourceReadDefault(string pathname, 
			   syscall_arguments args,
			   void *v)
{
}
  
