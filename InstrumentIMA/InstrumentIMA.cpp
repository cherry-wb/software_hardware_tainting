#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
	struct InstrumentIMA : public FunctionPass {
		static char ID;
		InstrumentIMA() : FunctionPass(ID) {}
	    virtual bool runOnFunction(Function &function) {
	    	bool return_value = false;
	    	//getting pointer to function
	    	Function *func= &function;
	    	//iterating over basic blocks
	    	for (Function::iterator bb_it = func->begin(), bb_end = func->end(); bb_it != bb_end; ++bb_it){
	    		//basic block pointer
	    		BasicBlock *bb =  bb_it;
	    		//iterating over instructions
	    		for (BasicBlock::iterator instr_it = bb->begin(), instr_end = bb->end(); instr_it != instr_end; ++instr_it){
	    			Instruction *curr_instr = instr_it;
	    			//taint initialization and cancellation
	    			if (isa<CallInst>(curr_instr)){
	    				CallInst* ci = (CallInst*) curr_instr;
	    				if(ci->getCalledFunction()==NULL){
	    					continue;
	    				}
	    				if(ci->getCalledFunction()->getName().compare("malloc")==0){
	    					if(instrumentMalloc(bb, ci)){
	    						return_value = true;
	    					}
	    				}
	    				if(ci->getCalledFunction()->getName().compare("realloc")==0){
	    					if(instrumentRealloc(bb, ci)){
	    						return_value = true;
	    					}
	    				}
	    				if(ci->getCalledFunction()->getName().compare("calloc")==0){
	    					if(instrumentCalloc(bb, ci)){
	    						return_value = true;
	    					}
	    				}
	    				if(ci->getCalledFunction()->getName().compare("free")==0){
	    					if(instrumentFree(bb, ci)){
	    						return_value = true;
	    					}
	    				}
	    			}
	    		}
	    	}

	    	return return_value;
	    }

	    virtual bool doInitialization(Module &module){
	    	Module *mod = &module;
	    	LLVMContext &context = mod->getContext();
	    	GlobalVariable *ima_tag_count = new GlobalVariable(*mod, Type::getInt32Ty(context), false, GlobalValue::ExternalLinkage, 0, "ima_tag_count");
	    	ima_tag_count->setAlignment(4);
	    	ConstantInt* const_int32_0 = ConstantInt::get(context, APInt(32, StringRef("0"), 10));
	    	ima_tag_count->setInitializer(const_int32_0);
	    	return true;
	    }

	    bool instrumentMalloc(BasicBlock* bb, CallInst *ci){
	    	//allocating vector of store instruction of allocated pointer
	    	std::vector<StoreInst*> storeinst_vector;

	    	//checking number of uses of malloc
	    	if(ci->getNumUses()!=1){
	    		errs() << "Result of malloc is used by a strange number of users\n";
	    		return false;
	    	}

	    	//iterating over uses of malloc
	    	for(Value::use_iterator ci_use_it = ci->use_begin(), ci_use_end = ci->use_end(); ci_use_it != ci_use_end; ++ci_use_it){
	    		User *ci_user = *ci_use_it;
	    		if(isa<StoreInst>(ci_user)){
	    			StoreInst *si = (StoreInst*) ci_user;
	    			storeinst_vector.push_back(si);
	    			continue;
	    		}
	    		if(isa<BitCastInst>(ci_user)){
	    			BitCastInst *bci = (BitCastInst*) ci_user;

	    			for(Value::use_iterator bci_use_it = bci->use_begin(), bci_use_end = bci->use_end(); bci_use_it != bci_use_end; ++bci_use_it){
	    				User *bci_user = *bci_use_it;
	    	    		if(isa<StoreInst>(bci_user)){
	    	    			StoreInst *si = (StoreInst*) bci_user;
	    	    			storeinst_vector.push_back(si);
	    	    			continue;
	    	    		}
	    				errs() << "Having a new type of user for bitcast of malloc\n";
	    				return false;
	    			}
	    			continue;
	    		}
	    		if(isa<ReturnInst>(ci_user)){
	    			//do not know how to handle
	    			errs() << "Do not know how I can handle return of malloc\n";
	    			return false;
	    		}

	    		errs() << "Having a new type of user for malloc\n";
	    		return false;
	    	}

	    	if(storeinst_vector.size()==0){
	    		errs() << "No store instructions for pointer of malloc\n";
	    		return false;
	    	}

			//getting module and context references
			Module* m = bb->getParent()->getParent();
			LLVMContext &context = bb->getContext();

	    	for(unsigned int i=0; i<storeinst_vector.size();i++){
	    		StoreInst *si = storeinst_vector[i];
				Instruction *next = si->getNextNode();

				//getting global counter to have different counter for new allocated memory
				GlobalVariable *ima_tag_count = m->getGlobalVariable("ima_tag_count",true);
				if(ima_tag_count==NULL){
					errs() << "Could not get global tag count for IMA\n";
					return false;
				}
				//load counter
				LoadInst* load_ima_tag_count = new LoadInst(ima_tag_count, "", false, next);

				//tagging pointer memory of the pointer returned by malloc
				CastInst* bitcast_inst = new BitCastInst(si->getOperand(1), Type::getInt8PtrTy(context), "", next);
				Constant* function_for_ptr = NULL;
				std::vector<Value*> arguments_for_ptr;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_ptr = m->getOrInsertFunction("DYTAN_tag_pointer", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_ptr = dyn_cast<Function>(function_for_ptr);
				Value *second_for_ptr = ConstantInt::get(Type::getInt32Ty(context), 4);
				arguments_for_ptr.push_back(bitcast_inst);
				arguments_for_ptr.push_back(second_for_ptr);
				arguments_for_ptr.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_ptr, arguments_for_ptr, "", next);


				//tagging memory returned by malloc
				Constant* function_for_memory = NULL;
				std::vector<Value*> arguments_for_memory;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_memory = m->getOrInsertFunction("DYTAN_tag_memory", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_memory = dyn_cast<Function>(function_for_memory);
				Value *first = ci;
				Value *second = ci->getArgOperand(0);
				arguments_for_memory.push_back(first);
				arguments_for_memory.push_back(second);
				arguments_for_memory.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_memory, arguments_for_memory, "", next);


				//increment counter
				BinaryOperator* inc_ima_tag_count = BinaryOperator::Create(Instruction::Add, load_ima_tag_count, ConstantInt::get(Type::getInt32Ty(context), 1), "inc", next);
				StoreInst* store_ima_tag_count = new StoreInst(inc_ima_tag_count, ima_tag_count, false, next);
				store_ima_tag_count->setAlignment(4);
	    	}

	    	//storeinst_vector.clear();

	    	return true;
	    }

	    bool instrumentRealloc(BasicBlock* bb, CallInst* ci){
	    	//allocating vector of store instruction of allocated pointer
	    	std::vector<StoreInst*> storeinst_vector;

	    	//checking number of uses of realloc
	    	if(ci->getNumUses()!=1){
	    		errs() << "Result of realloc is used by a strange number of users\n";
	    		return false;
	    	}

	    	//iterating over uses of realloc
	    	for(Value::use_iterator ci_use_it = ci->use_begin(), ci_use_end = ci->use_end(); ci_use_it != ci_use_end; ++ci_use_it){
	    		User *ci_user = *ci_use_it;
	    		if(isa<StoreInst>(ci_user)){
	    			StoreInst *si = (StoreInst*) ci_user;
	    			storeinst_vector.push_back(si);
	    			continue;
	    		}
	    		if(isa<BitCastInst>(ci_user)){
	    			BitCastInst *bci = (BitCastInst*) ci_user;

	    			for(Value::use_iterator bci_use_it = bci->use_begin(), bci_use_end = bci->use_end(); bci_use_it != bci_use_end; ++bci_use_it){
	    				User *bci_user = *bci_use_it;
	    	    		if(isa<StoreInst>(bci_user)){
	    	    			StoreInst *si = (StoreInst*) bci_user;
	    	    			storeinst_vector.push_back(si);
	    	    			continue;
	    	    		}
	    				errs() << "Having a new type of user for bitcast of realloc\n";
	    				return false;
	    			}
	    			continue;
	    		}
	    		if(isa<ReturnInst>(ci_user)){
	    			//do not know how to handle
	    			errs() << "Do not know how I can handle return of realloc\n";
	    			return false;
	    		}

	    		errs() << "Having a new type of user for realloc\n";
	    		return false;
	    	}

	    	if(storeinst_vector.size()==0){
	    		errs() << "No store instructions for pointer of realloc\n";
	    		return false;
	    	}

	      	Module* m = bb->getParent()->getParent();
	      	LLVMContext &context = bb->getContext();

	  		//before calling realloc free the tags for previous chunk of memory
	  		Constant* function_free = NULL;
	  		std::vector<Value*> arguments_free;
	  		//DYTAN_free(i8* %addr)
	  		function_free = m->getOrInsertFunction("DYTAN_free", Type::getVoidTy(context),  Type::getInt8PtrTy(context), (Type *)0 );
	  		Function *tag_function_free = dyn_cast<Function>(function_free);
	  		Value *first_free = ci->getArgOperand(0);
	  		arguments_free.push_back(first_free);
	  		CallInst::Create(tag_function_free, arguments_free, "", ci->getNextNode());

	    	for(unsigned int i=0; i<storeinst_vector.size();i++){
				StoreInst *si = storeinst_vector[i];
				Instruction *next = si->getNextNode();

				//getting global counter to have different counter for new allocated memory
				GlobalVariable *ima_tag_count = m->getGlobalVariable("ima_tag_count",true);
				if(ima_tag_count==NULL){
					errs() << "Could not get global tag count for IMA\n";
					return false;
				}
				//load counter
				LoadInst* load_ima_tag_count = new LoadInst(ima_tag_count, "", false, next);

				//tagging pointer memory of the pointer returned by realloc
				CastInst* bitcast_inst = new BitCastInst(si->getOperand(1), Type::getInt8PtrTy(context), "", next);
				Constant* function_for_ptr = NULL;
				std::vector<Value*> arguments_for_ptr;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_ptr = m->getOrInsertFunction("DYTAN_tag_pointer", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_ptr = dyn_cast<Function>(function_for_ptr);
				Value *second_for_ptr = ConstantInt::get(Type::getInt32Ty(context), 4);
				arguments_for_ptr.push_back(bitcast_inst);
				arguments_for_ptr.push_back(second_for_ptr);
				arguments_for_ptr.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_ptr, arguments_for_ptr, "", next);

				//tag new chunk of memory
				Constant* function_for_memory = NULL;
				std::vector<Value*> arguments_for_memory;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_memory = m->getOrInsertFunction("DYTAN_tag_memory", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_memory = dyn_cast<Function>(function_for_memory);
				Value *first = ci;
				Value *second = ci->getArgOperand(1);
				arguments_for_memory.push_back(first);
				arguments_for_memory.push_back(second);
				arguments_for_memory.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_memory, arguments_for_memory, "", next);

				//increment counter
				BinaryOperator* inc_ima_tag_count = BinaryOperator::Create(Instruction::Add, load_ima_tag_count, ConstantInt::get(Type::getInt32Ty(context), 1), "inc", next);
				StoreInst* store_ima_tag_count = new StoreInst(inc_ima_tag_count, ima_tag_count, false, next);
				store_ima_tag_count->setAlignment(4);
	    	}

	    	//storeinst_vector.clear();

	  		return true;
	    }

	    bool instrumentCalloc(BasicBlock* bb, CallInst* ci){
	    	//allocating vector of store instruction of allocated pointer
	    	std::vector<StoreInst*> storeinst_vector;

	    	//checking number of uses of calloc
	    	if(ci->getNumUses()!=1){
	    		errs() << "Result of calloc is used by a strange number of users\n";
	    		return false;
	    	}

	    	//iterating over uses of calloc
	    	for(Value::use_iterator ci_use_it = ci->use_begin(), ci_use_end = ci->use_end(); ci_use_it != ci_use_end; ++ci_use_it){
	    		User *ci_user = *ci_use_it;
	    		if(isa<StoreInst>(ci_user)){
	    			StoreInst *si = (StoreInst*) ci_user;
	    			storeinst_vector.push_back(si);
	    			continue;
	    		}
	    		if(isa<BitCastInst>(ci_user)){
	    			BitCastInst *bci = (BitCastInst*) ci_user;

	    			for(Value::use_iterator bci_use_it = bci->use_begin(), bci_use_end = bci->use_end(); bci_use_it != bci_use_end; ++bci_use_it){
	    				User *bci_user = *bci_use_it;
	    	    		if(isa<StoreInst>(bci_user)){
	    	    			StoreInst *si = (StoreInst*) bci_user;
	    	    			storeinst_vector.push_back(si);
	    	    			continue;
	    	    		}
	    				errs() << "Having a new type of user for bitcast of calloc\n";
	    				return false;
	    			}
	    			continue;
	    		}
	    		if(isa<ReturnInst>(ci_user)){
	    			//do not know how to handle
	    			errs() << "Do not know how I can handle return of calloc\n";
	    			return false;
	    		}

	    		errs() << "Having a new type of user for calloc\n";
	    		return false;
	    	}

	    	if(storeinst_vector.size()==0){
	    		errs() << "No store instructions for pointer of calloc\n";
	    		return false;
	    	}

			//getting module and context references
			Module* m = bb->getParent()->getParent();
			LLVMContext &context = bb->getContext();

	    	for(unsigned int i=0; i<storeinst_vector.size();i++){
				StoreInst *si = storeinst_vector[i];
				Instruction *next = si->getNextNode();

				//getting global counter to have different counter for new allocated memory
				GlobalVariable *ima_tag_count = m->getGlobalVariable("ima_tag_count",true);
				if(ima_tag_count==NULL){
					errs() << "Could not get global tag count for IMA\n";
					return false;
				}
				//load counter
				LoadInst* load_ima_tag_count = new LoadInst(ima_tag_count, "", false, next);

				//tagging pointer memory of the pointer returned by calloc
				CastInst* bitcast_inst = new BitCastInst(si->getOperand(1), Type::getInt8PtrTy(context), "", next);
				Constant* function_for_ptr = NULL;
				std::vector<Value*> arguments_for_ptr;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_ptr = m->getOrInsertFunction("DYTAN_tag_pointer", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_ptr = dyn_cast<Function>(function_for_ptr);
				Value *second_for_ptr = ConstantInt::get(Type::getInt32Ty(context), 4);
				arguments_for_ptr.push_back(bitcast_inst);
				arguments_for_ptr.push_back(second_for_ptr);
				arguments_for_ptr.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_ptr, arguments_for_ptr, "", next);

				//tag new chunk of memory
				Constant* function_for_memory = NULL;
				std::vector<Value*> arguments_for_memory;
				//DYTAN_tag(i8* %addr, i32 %size, i8* %name)
				function_for_memory = m->getOrInsertFunction("DYTAN_tag_memory", Type::getVoidTy(context),  Type::getInt8PtrTy(context),Type::getInt32Ty(context), Type::getInt32Ty(context), (Type *)0 );
				Function *tag_function_for_memory = dyn_cast<Function>(function_for_memory);
				Value *first = ci;
				Value *num = ci->getArgOperand(0);
				Value *size = ci->getArgOperand(1);
				Value *second = BinaryOperator::CreateMul(num, size, "", next);
				arguments_for_memory.push_back(first);
				arguments_for_memory.push_back(second);
				arguments_for_memory.push_back(load_ima_tag_count);
				CallInst::Create(tag_function_for_memory, arguments_for_memory, "", next);

				//increment counter
				BinaryOperator* inc_ima_tag_count = BinaryOperator::Create(Instruction::Add, load_ima_tag_count, ConstantInt::get(Type::getInt32Ty(context), 1), "inc", next);
				StoreInst* store_ima_tag_count = new StoreInst(inc_ima_tag_count, ima_tag_count, false, next);
				store_ima_tag_count->setAlignment(4);
	    	}

	  		return true;
	    }

	    bool instrumentFree(BasicBlock* bb, CallInst* ci){
	    	Instruction *next = ci->getNextNode();
	      	Module* m = bb->getParent()->getParent();
	      	LLVMContext &context = bb->getContext();
	  	    Constant* function = NULL;
	  		std::vector<Value*> arguments;

	  		//DYTAN_free(i8* %addr)
	  		function = m->getOrInsertFunction("DYTAN_free", Type::getVoidTy(context),  Type::getInt8PtrTy(context), (Type *)0 );
	  		Function *tag_function = dyn_cast<Function>(function);
	  		Value *first = ci->getArgOperand(0);
	  		arguments.push_back(first);
	  		CallInst::Create(tag_function, arguments, "", next);
	  		return true;
	    }


	};
}

char InstrumentIMA::ID = 0;
//static RegisterPass<InstrumentIMA> X("instrument-ima", "Instrumentation for Illegal Memory Access", false, false);
static RegisterPass<InstrumentIMA> X("instrument-ima", "Instrumentation for Illegal Memory Access", false, false);

