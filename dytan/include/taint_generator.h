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

#ifndef _TAINT_GENERATOR_H_
#define _TAINT_GENERATOR_H_

#include <iostream>
#include <cstdlib>

using namespace std;

class TaintGenerator
{
	int _start;
    int _current;
    int _max;
 public:
    TaintGenerator() {
    }

    TaintGenerator(int start, int max) {
    	_start = start;
        _current = start;
        _max = max;
    }

    virtual int nextTaintMark() {
    	int result = _current;
    	if(((_current+1)%_max)==0){
    		_current = _start;
    	}
    	else{
    		_current = _current+1;
    	}
    	return result;
        //int result = (_current)%_max;
        //_current = _current + 1;
        //return result;
    }

    virtual void setStart(int new_start){
    	_start = new_start;
    }

    //current gives the lowest available mark
    virtual int getCurrent(){
    	return _current;
    }

    virtual ~TaintGenerator() {}

};

class ConstantTaintGenerator: public TaintGenerator
{
 private:
  int _seed;

 public:
  ConstantTaintGenerator(int seed) {
    _seed = seed;
  }

  int nextTaintMark() {
    return _seed;
  } 
};


class RandomSetTaintGenerator: public TaintGenerator
{
 private:
  int _start;
  int _num;

 public:
  RandomSetTaintGenerator(int start, int num) {
    _start = start;
    _num = num;
  }

  int nextTaintMark() {
    return (rand() % _num) + _start;
  } 
};

#endif
