%{
/*
	Michael Scherban - ECE 561 - Mscherb
 */
#include <stdio.h>
#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"
#include <string.h>

#include "uthash.h"

#include <errno.h>
  //#include <search.h>

extern FILE *yyin;
int yylex(void);
int yyerror(const char *);

extern char *fileNameOut;

extern LLVMModuleRef Module;
extern LLVMContextRef Context;

LLVMValueRef Function;
LLVMBasicBlockRef BasicBlock;
LLVMBuilderRef Builder;

int params_cnt=0;

struct TmpMap{
  char *key;                /* key */
  void *val;                /* data */
  UT_hash_handle hh;        /* makes this structure hashable */
};
 

struct TmpMap *map = NULL;    /* important! initialize to NULL */

void map_insert(char *key, void* val) { 
  struct TmpMap *s; 
  s = malloc(sizeof(struct TmpMap)); 
  s->key = strdup(key); 
  s->val = val; 
  HASH_ADD_KEYPTR( hh, map, s->key, strlen(s->key), s ); 
}

void * map_find(char *tmp) {
  struct TmpMap *s;
  HASH_FIND_STR( map, tmp, s );  /* s: output pointer */
  if (s) 
    return s->val;
  else 
    return NULL; // returns NULL if not found
}

%}

%union {
  char *tmp;
  int num;
  char *id;
  LLVMValueRef val;
}

%token DEF ASSIGN SEMI COMMA MINUS PLUS LBRACKET RBRACKET SUM TMP NUM ID RAISE HASH LESSTHAN
%type <num> NUM
%type <id> ID
%type <val> expr stmt stmtlist


//%nonassoc QUESTION COLON
%left LESSTHAN
%left PLUS MINUS
%left MULTIPLY DIVIDE RAISE

%start program

%%

program: define stmtlist 
{ 
  /* 
    IMPLEMENT: return value, program is over

    Hint: the following code is not sufficient
  */  

  LLVMBuildRet(Builder,$2); //construct the return type and value
}
;

define: DEF ID LBRACKET NUM RBRACKET SEMI 
{  
  /* NO NEED TO CHANGE ANYTHING IN THIS RULE */

  /* Now we know how many parameters we need.  Create a function type
     and add it to the Module */

  params_cnt = $4;

  LLVMTypeRef Integer = LLVMInt64TypeInContext(Context);

  LLVMTypeRef *IntRefArray = malloc(sizeof(LLVMTypeRef)*params_cnt);
  int i;
  
  /* Build type for function */
  for(i=0; i<params_cnt; i++)
    IntRefArray[i] = Integer;

  LLVMBool var_arg = 0; /* false */
  LLVMTypeRef FunType = LLVMFunctionType(Integer,IntRefArray,params_cnt,var_arg);

  /* Found in LLVM-C -> Core -> Modules */
  Function = LLVMAddFunction(Module,$2,FunType);

  /* Add a new entry basic block to the function */
  BasicBlock = LLVMAppendBasicBlock(Function,"entry");

  /* Create an instruction builder class */
  Builder = LLVMCreateBuilder();

  /* Insert new instruction at the end of entry block */
  LLVMPositionBuilderAtEnd(Builder,BasicBlock);
}
;

/*
   IMPLMENT ALL THE RULES BELOW HERE!
 */

/* In both cases assign stmtlist from the input */
stmtlist:  stmtlist stmt { $$ = $2; }
		 |  stmt{ $$ = $1; }
	
;         

stmt: ID ASSIGN expr SEMI
	{  map_insert($1, $3); //Associate a LLVMValueRef to an identifier in the hash table
	   $$ = $3; }		   //set stmt to the expr
;


expr:   expr MINUS expr { $$ = LLVMBuildSub(Builder, $1, $3, ""); } //Minus
      | expr PLUS expr {  $$ = LLVMBuildAdd(Builder, $1, $3, ""); } //Addition
      | expr RAISE expr {  
							int cnt;
							LLVMValueRef vr;
							//error out if expr is not a constant
							if (!LLVMIsConstant($3)) {
								yyerror("Exponent is not a constant!");
							}
							else {
								//it is a constant:
								cnt = LLVMConstIntGetZExtValue($3); //get the count
								vr = LLVMBuildICmp(Builder, LLVMIntSLT, LLVMConstInt(LLVMInt64Type(), cnt, 0), LLVMConstInt(LLVMInt64Type(), 0, 0), ""); //compare the count with negative
								vr = LLVMBuildSelect(Builder, vr, LLVMConstInt(LLVMInt64Type(), 0, 0), LLVMConstInt(LLVMInt64Type(), 1, 0), ""); //if its negative, we want to use a 0 multipler, else use a 1
								//calculate value:
								while (cnt-- > 0) {
									vr = LLVMBuildMul(Builder, $1, vr, "");
								}
								$$ = vr;
							}
						}
      | MINUS expr { $$ = LLVMBuildNeg(Builder, $2, ""); } //negate the value
      | expr MULTIPLY expr { $$ = LLVMBuildMul(Builder, $1, $3, ""); } //Multiply
      | expr DIVIDE expr { $$ = LLVMBuildSDiv(Builder, $1, $3, ""); } //Divide
	  | expr LESSTHAN expr  {
								LLVMValueRef cmp;
								cmp = LLVMBuildICmp(Builder, LLVMIntSLT, $1, $3, ""); // do a comparison if first expr is less than second
								//feed the comparison into the select instruction and set the output to 1 or 0, depending on the result of the comparison
								$$ = LLVMBuildSelect(Builder, cmp, LLVMConstInt(LLVMInt64Type(), 1, 0), LLVMConstInt(LLVMInt64Type(), 0, 0), "");
							}
      | LBRACKET expr RBRACKET {
									LLVMValueRef cmp, arg;
									int c = 0;
									$$ = LLVMConstInt(LLVMInt64Type(), 0, 0); //set the output to a constant 0 for now
									//Get the first parameter in the parameter field
									arg = LLVMGetFirstParam(Function);
									while (arg != NULL) { //go through each parameter
										//compare the parameter num with the value in expr
										cmp = LLVMBuildICmp(Builder, LLVMIntEQ, $2, LLVMConstInt(LLVMInt64Type(), c, 0), "");
										$$ = LLVMBuildSelect(Builder, cmp, arg, $$, ""); //if they equal, set the arg as the output
										arg = LLVMGetNextParam(arg); //check the next arg, till we run out of args (even if we already matched)
										c++;
									}
								}
      | ID {
				if (map_find($1) != NULL) //check if the identifier is in the hash table
					$$ = map_find($1); //found the identifier, push the LLVMValueRef structure through as expr
				else
					yyerror("Undefined identifier"); //error out if we tried to access an identifier that has not been defined before
			}
      | NUM { $$ = LLVMConstInt(LLVMInt64Type(), $1, 0); } //convert the NUM to a constant
      | HASH { $$ = LLVMConstInt(LLVMInt64Type(), params_cnt, 0); } //a hash is just the total number of parameters
;

%%


void initialize()
{
  /* IMPLEMENT: add something here if needed */
}

int line;

int yyerror(const char *msg)
{
  printf("%s at line %d.\n",msg,line);
  return 0;
}
