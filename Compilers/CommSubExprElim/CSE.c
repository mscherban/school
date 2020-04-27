/*
 * File: CSE.c
 *
 * Description:
 *   This is where you implement the C version of project 4 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"
#include "transform.h"
//#include "worklist.h"

/* Header file global to this project */
#include "cfg.h"
#include "CSE.h"

#define for_each_function(__F, __M)		\
	for (LLVMValueRef (__F) = LLVMGetFirstFunction(__M); \
		(__F) != NULL; (__F) = LLVMGetNextFunction(__F))

#define for_each_basicblock(__BB, __F)	\
	for (LLVMBasicBlockRef (__BB) = LLVMGetFirstBasicBlock(__F); \
		(__BB) != NULL; (__BB) = LLVMGetNextBasicBlock(__BB))

#define for_each_instruction(__I, __BB)	\
	for (LLVMValueRef (__I) = LLVMGetFirstInstruction(__BB);	\
		(__I) != NULL; (__I) = LLVMGetNextInstruction(__I))

#define for_each_child_block(__C, __BB)	\
	for (LLVMBasicBlockRef (__C) = LLVMFirstDomChild(__BB);	\
		(__C) != NULL; (__C) = LLVMNextDomChild(__BB,__C))
		
#define for_each_instruction_following(__I, __II)	\
	for (LLVMValueRef (__I) = LLVMGetNextInstruction(__II);	\
		(__I) != NULL; (__I) = LLVMGetNextInstruction(__I))
		
int CSEDead=0;
int CSEElim=0;
int CSESimplify=0;
int CSELdElim=0;
int CSELdStElim=0;
int CSERStElim=0;

static LLVMBool HasSideEffect(LLVMValueRef I) {
	LLVMOpcode O = LLVMGetInstructionOpcode(I);
	switch(O) {
		case LLVMCall:
		case LLVMBr:
		case LLVMRet:
		case LLVMStore:
			return 1;
		case LLVMLoad:
			if (LLVMGetVolatile(I))
				return 1;
		default:
		break;
	}
	
	return 0;
}

static LLVMBool SkippableCSE(LLVMValueRef I) {
	/* skip over certain types */
  LLVMOpcode O = LLVMGetInstructionOpcode(I); /* Opcode */
  /* list of opcodes that should not be checked */
  switch (O) {
	case LLVMLoad:
	case LLVMStore:
	case LLVMVAArg:
	case LLVMCall:
	case LLVMRet:
	case LLVMBr:
	case LLVMAlloca:
	case LLVMFCmp:
	case LLVMSwitch:
	case LLVMUnreachable:
	case LLVMInvoke:
	case LLVMExtractValue: //causes error in bh test
		return 1;
	break;
	default:
	break;
  }
  return 0;
}


static LLVMBool InstructionsIdentical(LLVMValueRef I, LLVMValueRef Ic) {
	LLVMOpcode O = LLVMGetInstructionOpcode(I);
	LLVMOpcode Oc = LLVMGetInstructionOpcode(Ic);
	int i;

	/* check of the opcode types match */
	if (O != Oc)
		return 0;
	
	/* if it's an iCmp, make sure the predicate is the same... */
	if (O == LLVMICmp && (LLVMGetICmpPredicate(I) != LLVMGetICmpPredicate(Ic)))
		return 0;
	
	/* check if same type */
	if (LLVMTypeOf(I) != LLVMTypeOf(Ic))
		return 0;

	/* check if they have the same number of opcodes */
	if (LLVMGetNumOperands(I) != LLVMGetNumOperands(Ic))
		return 0;

	for (i = 0; i < LLVMGetNumOperands(I); i++) {
		LLVMValueRef Op = LLVMGetOperand(I, i);
		LLVMValueRef Opc = LLVMGetOperand(Ic, i);
		
		if (Op != Opc) {
			/* if operands aren't the same jump out */
			return 0;
		}
		
#if 0 //its enough to just check if the operands are the same LLVMValueRef
		/* operands should be of the same type */
		if (LLVMTypeOf(Op) != LLVMTypeOf(Opc))
			return 0;
		
		/* are they constant? */
		if (LLVMIsConstant(Op)) {
			if (LLVMIsConstant(Opc) && LLVMIsAInstruction(Op)) {
				/* both are constant, see if they match */
				if (LLVMGetTypeKind(LLVMTypeOf(Op)) == LLVMIntegerTypeKind) {
					/* integers */
					if (LLVMConstIntGetZExtValue(Op) != LLVMConstIntGetZExtValue(Opc)) {
						return 0; /* if they arent the same constant value they aren't identical */
					} else {
						/* same constant in operand slot */
					}
				} else if (LLVMGetTypeKind(LLVMTypeOf(Op)) == LLVMFloatTypeKind || LLVMGetTypeKind(LLVMTypeOf(Op)) == LLVMDoubleTypeKind) {
					/* floats */
					LLVMBool x;
					if (LLVMConstRealGetDouble(Op, &x) != LLVMConstRealGetDouble(Opc, &x)) {
						return 0; /* if they arent the same constant value they aren't identical */
					} else {
						/* same constant in operand slot */
					}
				} else {
					/* all others */
					if (Op != Opc) {
						return 0;
					}
				}
				

			} else {
				return 0;
			}
		} else { /* operand points to another instruction */
			/* are they the same instruction? */
			if (Op != Opc) {
				/* nope */
				return 0;
			}
		}
#endif

	}

	/* if we got here it's a match */
	return 1;
}


static void CseThroughDomTree(LLVMBasicBlockRef BB, LLVMValueRef I) {
	LLVMBasicBlockRef Child = LLVMFirstDomChild(BB);
	
	/* go through dominated BBs */
	while (Child) {
		LLVMValueRef Ic = LLVMGetFirstInstruction(Child);
		while (Ic) {
			if (InstructionsIdentical(I, Ic)) {
				/* remove the instruction w/o breaking iterator */
				LLVMValueRef Rm = Ic;
				LLVMReplaceAllUsesWith(Ic, I);
				Ic = LLVMGetNextInstruction(Ic);
				LLVMInstructionEraseFromParent(Rm);
				CSEElim++;
				continue;
			}
			
			Ic = LLVMGetNextInstruction(Ic);
		}

		CseThroughDomTree(Child, I); //recursively go through dom tree
		Child = LLVMNextDomChild(BB, Child);
	}
}

static LLVMBool IsDead(LLVMValueRef I) {
	/* dead instructions have no uses and no side effects */
	if (!LLVMGetFirstUse(I) && !HasSideEffect(I) && LLVMIsAInstruction(I)) {
		/* unreachables and switches count as terminators, dont eliminate those */
		if (LLVMGetInstructionOpcode(I) != LLVMUnreachable
					&& LLVMGetInstructionOpcode(I) != LLVMSwitch)
			return 1;
	}
	return 0;
}

#if 0 //not using worklist anymore
static void EliminateDeadInstructions(worklist_t W) {
	LLVMValueRef I;
	int i;
	
	while ((I = worklist_pop(W)) != NULL) {
		if (!LLVMGetFirstUse(I)) {
			/* The instruction has no uses, it is dead.
				add its operands to the work list then delete it */
			for (i = 0; i < LLVMGetNumOperands(I); i++) {
				LLVMValueRef Op = LLVMGetOperand(I, i);
				if (!HasSideEffect(Op) && LLVMIsAInstruction(Op)) {
					worklist_insert(W, Op);
				}
			}
			
			/* removing switches and unreachables causes errors compiling */
			if (LLVMGetInstructionOpcode(I) != LLVMUnreachable
					&& LLVMGetInstructionOpcode(I) != LLVMSwitch) {
				LLVMInstructionEraseFromParent(I);
				CSEDead++;
			}
		}
	}
}
#endif

void LLVMCommonSubexpressionElimination(LLVMModuleRef Module)
{
	LLVMValueRef New;
	LLVMBool DontGetNextI = 0;
	
	//LLVMDumpModule(Module);
	
  /* Implement here! */
	for_each_function(F, Module) {
	 for_each_basicblock(BB, F) {
		LLVMValueRef I = LLVMGetFirstInstruction(BB);
		while (I) {
			
		
		/* CSE */
		if (!SkippableCSE(I)) {
			/* check the rest of the BB first */
			LLVMValueRef Ic = LLVMGetNextInstruction(I);
			while (Ic) {
				if (InstructionsIdentical(I, Ic)) {
					/* found a subexpression to remove */
					LLVMValueRef Rm = Ic;
					LLVMReplaceAllUsesWith(Ic, I);
					Ic = LLVMGetNextInstruction(Ic);
					LLVMInstructionEraseFromParent(Rm);
					CSEElim++;
					continue;
				}
				Ic = LLVMGetNextInstruction(Ic);
			}
			/* go through the rest of the dominator tree */
			CseThroughDomTree(BB, I);
		}
#if 0
		/* Dead Code Elimination */
		if (!HasSideEffect(I) && LLVMIsAInstruction(I)) {
			worklist_insert(DCEWl, I);
		}
#endif
		/* dead code elimination */
		if (IsDead(I)) {
			LLVMValueRef Rm = I;
			I = LLVMGetNextInstruction(I);
			LLVMInstructionEraseFromParent(Rm);
			CSEDead++;
			continue;
		}

		/* Simplify */
		if ((New = InstructionSimplify(I)) != NULL) {
			/* instruction resulted in a better one. use that one */
			LLVMValueRef Rm = I;
			LLVMReplaceAllUsesWith(I, New);
			I = LLVMGetNextInstruction(I);
			LLVMInstructionEraseFromParent(Rm);
			CSESimplify++;
			continue;
		}

		
		/* Redundant loads */
		if (LLVMGetInstructionOpcode(I) == LLVMLoad) {
			LLVMValueRef R = LLVMGetNextInstruction(I);
			while (R) {
				LLVMOpcode O = LLVMGetInstructionOpcode(R);
				if (O == LLVMStore) {
					/* seeing a store breaks the optimization */
					break;
				} else if (O == LLVMLoad && !LLVMGetVolatile(R) &&
							(LLVMGetOperand(I, 0) == LLVMGetOperand(R,0)) &&
							(LLVMTypeOf(I) == LLVMTypeOf(R))) {
					/* a non-volatile load, with same address and address type */
					LLVMValueRef Rm = R;
					LLVMReplaceAllUsesWith(R, I);
					R = LLVMGetNextInstruction(R);
					LLVMInstructionEraseFromParent(Rm);
					CSELdElim++;
					continue;
				}
				
				
				R = LLVMGetNextInstruction(R);
			}
		}
		 
		/* Redundant Stores */
		if (LLVMGetInstructionOpcode(I) == LLVMStore) {
			/* visit each next instruction in the basic block */
			LLVMValueRef R = LLVMGetNextInstruction(I);
			while (R) {
				 LLVMOpcode O = LLVMGetInstructionOpcode(R);
				 if (O == LLVMLoad && !LLVMGetVolatile(R) &&
					(LLVMGetOperand(I, 1) == LLVMGetOperand(R, 0)) &&
					(LLVMTypeOf(R) == LLVMTypeOf(LLVMGetOperand(I,0)))) {
					/* a non-volatile load, whose address matches the store address, and type */
					/* unnessessary load, remove it and replace with stores data */
					LLVMValueRef Rm = R;
					LLVMReplaceAllUsesWith(R, LLVMGetOperand(I,0));
					R = LLVMGetNextInstruction(R);
					LLVMInstructionEraseFromParent(Rm);
					CSELdStElim++;
					continue;
				 } else if (O == LLVMStore && !LLVMGetVolatile(I) && 
							(LLVMGetOperand(I,1) == LLVMGetOperand(R,1)) &&
							(LLVMTypeOf(LLVMGetOperand(I,0)) == LLVMTypeOf(LLVMGetOperand(R,0))) &&
							(LLVMTypeOf(LLVMGetOperand(I,1)) == LLVMTypeOf(LLVMGetOperand(R,1)))) {
					/* a non-volatile store, whose addresses and data match */
					/* this store is unnecessary and does nothing, remove it */
					LLVMValueRef Rm = I;
					I = LLVMGetNextInstruction(I);
					LLVMInstructionEraseFromParent(Rm);
					CSERStElim++;
					DontGetNextI = 1;
					break;
				} else if (O == LLVMStore) {
					break; /* any other store breaks it */
				} else if (O == LLVMLoad) {
					break; /* any other load breaks it */
				}
				
				R = LLVMGetNextInstruction(R);
			}
		}
			
			
			
		/* to the next */
		if (DontGetNextI) {
			DontGetNextI = 0;
		} else {
			I = LLVMGetNextInstruction(I);
		}

	  }
	 }
	 
	 //EliminateDeadInstructions(DCEWl);
	}

	
	
	//LLVMDumpModule(Module);
  fprintf(stderr,"CSE_Dead.....%d\n", CSEDead);
  fprintf(stderr,"CSE_Basic.....%d\n", CSEElim);
  fprintf(stderr,"CSE_Simplify..%d\n", CSESimplify);
  fprintf(stderr,"CSE_RLd.......%d\n", CSELdElim);
  fprintf(stderr,"CSE_RSt.......%d\n", CSERStElim);
  fprintf(stderr,"CSE_LdSt......%d\n", CSELdStElim);  
}
