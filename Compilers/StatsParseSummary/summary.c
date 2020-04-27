/*
	Michael Scherban - ECE 566 - Spring 2019
*/

/*
 * File: summary.c
 *
 * Description:
 *   This is where you implement your project 3 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"
#include "valmap.h"

/* Header file global to this project */
#include "summary.h"

typedef struct Stats_def {
  int functions;
  int globals;
  int bbs;

  int insns;
  int insns_nearby_dep;
  
  int allocas;

  int loads;
  int loads_alloca;
  int loads_globals;

  int stores;
  int stores_alloca;
  int stores_globals;
  
  int conditional_branches;
  int calls;
  int calls_with_pointers;
  int calls_w_func;

  int gep;
  int gep_load;
  int gep_alloca;
  int gep_globals;
  int gep_gep;

  int loops; //approximated by backedges
  int floats;
} Stats;

void pretty_print_stats(FILE *f, Stats s, int spaces)
{
  char spc[128];
  int i;

  // insert spaces before each line
  for(i=0; i<spaces; i++)
    spc[i] = ' ';
  spc[i] = '\0';
    
  fprintf(f,"%sFunctions.......................%d\n",spc,s.functions);
  fprintf(f,"%sGlobal Vars.....................%d\n",spc,s.globals);
  fprintf(f,"%sBasic Blocks....................%d\n",spc,s.bbs);
  fprintf(f,"%sInstructions....................%d\n",spc,s.insns);
  fprintf(f,"%sInstructions - insns_nearby_dep.%d\n",spc,s.insns_nearby_dep);

  fprintf(f,"%sInstructions - Cond. Branches...%d\n",spc,s.conditional_branches);
  fprintf(f,"%sInstructions - Calls............%d\n",spc,s.calls);
  fprintf(f,"%sInstructions - Calls.w.ptr......%d\n",spc,s.calls_with_pointers);
  fprintf(f,"%sInstructions - Calls.w.func.....%d\n",spc,s.calls_w_func);

  fprintf(f,"%sInstructions - Allocas..........%d\n",spc,s.allocas);
  fprintf(f,"%sInstructions - Loads............%d\n",spc,s.loads);
  fprintf(f,"%sInstructions - Loads (alloca)...%d\n",spc,s.loads_alloca);
  fprintf(f,"%sInstructions - Loads (globals)..%d\n",spc,s.loads_globals);


  fprintf(f,"%sInstructions - Stores...........%d\n",spc,s.stores);
  fprintf(f,"%sInstructions - Stores (alloca)..%d\n",spc,s.stores_alloca);
  fprintf(f,"%sInstructions - Stores (globals).%d\n",spc,s.stores_globals);


  fprintf(f,"%sInstructions - gep..............%d\n",spc,s.gep);
  fprintf(f,"%sInstructions - gep (load).......%d\n",spc,s.gep_load);
  fprintf(f,"%sInstructions - gep (alloca).....%d\n",spc,s.gep_alloca);
  fprintf(f,"%sInstructions - gep (globals)....%d\n",spc,s.gep_globals);
  fprintf(f,"%sInstructions - gep (gep)........%d\n",spc,s.gep_gep);

  fprintf(f,"%sInstructions - Other............%d\n",spc,
	  s.insns-s.conditional_branches-s.loads-s.stores-s.gep-s.calls);
  fprintf(f,"%sLoops...........................%d\n",spc,s.loops);
  fprintf(f,"%sFloats..........................%d\n",spc,s.floats);
}

void print_csv_file(const char *filename, Stats s, const char *id)
{
  FILE *f = fopen(filename,"w");
  fprintf(f,"id,%s\n",id);
  fprintf(f,"functions,%d\n",s.functions);
  fprintf(f,"globals,%d\n",s.globals);
  fprintf(f,"bbs,%d\n",s.bbs);
  fprintf(f,"insns,%d\n",s.insns);
  fprintf(f,"insns_nearby_dep,%d\n",s.insns_nearby_dep);
  fprintf(f,"allocas,%d\n",s.allocas);
  fprintf(f,"branches,%d\n",s.conditional_branches);
  fprintf(f,"calls,%d\n",s.calls);
  fprintf(f,"calls_w_ptr,%d\n",s.calls_with_pointers);
  fprintf(f,"calls_w_func,%d\n",s.calls_w_func);
  fprintf(f,"loads,%d\n",s.loads);
  fprintf(f,"loads_alloca,%d\n",s.loads_alloca);
  fprintf(f,"loads_globals,%d\n",s.loads_globals);
  fprintf(f,"stores,%d\n",s.stores);
  fprintf(f,"stores_alloca,%d\n",s.stores_alloca);
  fprintf(f,"stores_global,%d\n",s.stores_globals);
  fprintf(f,"gep,%d\n",s.gep);
  fprintf(f,"gep_load,%d\n",s.gep_load);
  fprintf(f,"gep_alloca,%d\n",s.gep_alloca);
  fprintf(f,"gep_globals,%d\n",s.gep_globals);
  fprintf(f,"gep_gep,%d\n",s.gep_gep);
  fprintf(f,"loops,%d\n",s.loops);
  fprintf(f,"floats,%d\n",s.floats);
  fclose(f);
}

Stats MyStats;

LLVMBool IsBackEdge(LLVMBasicBlockRef BB, LLVMBasicBlockRef BB2) {
	/* The terminator of BB2, if it is a loop, it will point to BB */
	LLVMValueRef Term = LLVMGetBasicBlockTerminator(BB2);
	LLVMBasicBlockRef Edge;
	
	if (LLVMGetInstructionOpcode(Term) == LLVMBr) {
		/* Only care about branches */
		for (unsigned int i = 0; i < LLVMGetNumSuccessors(Term); i++) {
			Edge = LLVMGetSuccessor(Term, i); /* Edge is the block that BB2 points to */
			if (Edge == BB) {
				return 1; /* there is a back edge */
			}
		}
	}
	
	return 0; /* no back edge */
}

void GetLoopStats(LLVMValueRef F) {
	/* Go through each basic block in the module */
	for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(F);
		BB != NULL; BB = LLVMGetNextBasicBlock(BB))
	{
		/* go through each basic block including and following the one being checked */
		for (LLVMBasicBlockRef BB2 = BB; BB2 != NULL; BB2 = LLVMGetNextBasicBlock(BB2))
		{
			/* Loop is defined by (BB dom BB2), and, there is a back edge from BB2 -> BB */
			if (LLVMDominates(F, BB, BB2) && IsBackEdge(BB, BB2)) {
				MyStats.loops++;
				break; /* break out because we found a loop header */
			}
		}
	}
}

void GetOperandStats(LLVMValueRef I, LLVMModuleRef Module) {
	int i;
	int n = LLVMGetNumOperands(I);
	LLVMValueRef O;
	LLVMBasicBlockRef P, IP;
	
	/* check for operand defined in same BB */
	for (i = 0; i < n; i++) {
		O = LLVMGetOperand(I, i); /* Operand */
		if (LLVMIsAInstruction(O)) {
			P = LLVMGetInstructionParent(O); /* Operands BB */
			IP = LLVMGetInstructionParent(I); /* Instructions BB */
			if (P == IP) {
				MyStats.insns_nearby_dep++;
				break;
			}
		} 
	}
	
	/* check for f32, f64 operand */
	for (i = 0; i < n; i++) {
		O = LLVMGetOperand(I, i); /* Operand */
		if (LLVMGetTypeKind(LLVMTypeOf(O)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(O)) == LLVMDoubleTypeKind) {
			MyStats.floats++;
			break;
		} 
	}
}

void GetInstructionStats(LLVMValueRef I, LLVMModuleRef Module) {
	LLVMOpcode O = LLVMGetInstructionOpcode(I);
	LLVMValueRef Op, /*Arg,*/ F;
	LLVMOpcode Ol;
	
	/* opcode specific stats */
	switch (O) {
		case LLVMAlloca:
			MyStats.allocas++;
			break;
		case LLVMLoad:
			MyStats.loads++; /* load */
			Op = LLVMGetOperand(I, 0); /* get the operand of the load */
			Ol = LLVMGetInstructionOpcode(Op); /* get the opcode of the operand */
			if (Ol == LLVMAlloca)
				MyStats.loads_alloca++; /* alloca */
			else if (LLVMGetValueKind(Op) == LLVMGlobalVariableValueKind)
				MyStats.loads_globals++; /* global */
			break;
		case LLVMStore:
			MyStats.stores++; /* store */
			Op = LLVMGetOperand(I, 1); /* get the operand of the store address */
			Ol = LLVMGetInstructionOpcode(Op); /* get the opcode of the operand */
			if (Ol == LLVMAlloca)
				MyStats.stores_alloca++;
			else if (LLVMGetValueKind(Op) == LLVMGlobalVariableValueKind) {
				MyStats.stores_globals++;
			}
				
			break;
		case LLVMBr:
			if (LLVMGetNumOperands(I) > 1) /* conditionals have more than 1 target */
				MyStats.conditional_branches++;
			break;
		case LLVMCall:
			MyStats.calls++;
			Op = LLVMGetCalledValue(I); /* function call is referencing */

			/* The last operand is the called function */
			for (int i = 0; i < LLVMGetNumOperands(I)-1; i++) {
				Op = LLVMGetOperand(I, i);
				if (LLVMGetTypeKind(LLVMTypeOf(Op)) == LLVMPointerTypeKind) {
					MyStats.calls_with_pointers++;
					break;
				}				
			}
			
/*	bad try here, seg faults when looking at function pointers
			if (LLVMGetValueKind(Op) == LLVMFunctionValueKind) {
				for (Arg = LLVMGetFirstParam(Op); Arg != NULL; Arg = LLVMGetNextParam(Arg)) {
					if (LLVMGetTypeKind(LLVMTypeOf(Arg)) == LLVMPointerTypeKind) {
						MyStats.calls_with_pointers++;
						break;
					}
				}
			}
*/			

			/* go through each function in module to find if it is defined within it */
			for (F = LLVMGetFirstFunction(Module); F != NULL; F = LLVMGetNextFunction(F)) {
				if (LLVMIsDeclaration(F)) {
					/* skip "declared" external functions */
					continue;
				}
				
				/* last operand is the called function */
				Op = LLVMGetOperand(I, LLVMGetNumOperands(I) - 1);
				if (Op == F) {
					MyStats.calls_w_func++;
					break;
				}
			}

			break;
		case LLVMGetElementPtr:
			MyStats.gep++;
			Op = LLVMGetOperand(I, 0); /* the gep base */
			Ol = LLVMGetInstructionOpcode(Op); /* opcode of the gep base */
			if (Ol == LLVMAlloca)
				MyStats.gep_alloca++;
			else if (Ol == LLVMLoad)
				MyStats.gep_load++;
			else if (Ol == LLVMGetElementPtr)
				MyStats.gep_gep++;
			else if (LLVMGetValueKind(Op) == LLVMGlobalVariableValueKind)
				MyStats.gep_globals++;
			break;
		default:
			break;
	}
	
	GetOperandStats(I, Module); /* check operands */
}

void GetStats(LLVMModuleRef Module) {
	/* Get all the global variables */
	for (LLVMValueRef Gl = LLVMGetFirstGlobal(Module);
		Gl != NULL; Gl = LLVMGetNextGlobal(Gl))
		{
			/* Extern globals will not have an initializer */
			if (LLVMGetInitializer(Gl)) {
				MyStats.globals++;
			}
		}
	
	/* Each function... */
	for (LLVMValueRef F = LLVMGetFirstFunction(Module);
		F != NULL; F = LLVMGetNextFunction(F))
	{
		if (LLVMIsDeclaration(F)) {
			/* External functions are "declared", skip these */
			continue;
		}
		
		MyStats.functions++;
		for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(F);
			BB != NULL; BB = LLVMGetNextBasicBlock(BB))
		{
			/* Each basic block */
			MyStats.bbs++;
			for (LLVMValueRef I = LLVMGetFirstInstruction(BB);
				I != NULL; I = LLVMGetNextInstruction(I))
			{
				/* Each instruction */
				MyStats.insns++;
				GetInstructionStats(I, Module);
			}
		}
		
		GetLoopStats(F);
	}
}

void
Summarize(LLVMModuleRef Module, const char *id, const char* filename)
{
  GetStats(Module);
  //pretty_print_stats(stdout,MyStats,0);
  print_csv_file(filename,MyStats,id);
}

