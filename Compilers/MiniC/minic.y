%{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"

#include "list.h"
#include "symbol.h"

int num_errors;

extern int yylex();   /* lexical analyzer generated from lex.l */

int yyerror();
int parser_error(const char*);

void minic_abort();
char *get_filename();
int get_lineno();

int loops_found=0;

extern LLVMModuleRef Module;
extern LLVMContextRef Context;
 LLVMBuilderRef Builder;

LLVMValueRef Function=NULL;
LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params);

%}

/* Data structure for parse tree nodes */

%union {
  int inum;
  float fnum;
  char * id;
  LLVMTypeRef  type;
  LLVMValueRef value;
  LLVMBasicBlockRef bb;
  paramlist_t *params;
}

/* these tokens are simply their corresponding int values, more terminals*/

%token SEMICOLON COMMA COLON
%token LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET
%token ASSIGN PLUS MINUS STAR DIV MOD 
%token LT GT LTE GTE EQ NEQ NOT
%token LOGICAL_AND LOGICAL_OR
%token BITWISE_OR BITWISE_XOR LSHIFT RSHIFT BITWISE_INVERT

%token DOT ARROW AMPERSAND QUESTION_MARK

%token FOR WHILE IF ELSE DO STRUCT SIZEOF RETURN SWITCH
%token BREAK CONTINUE CASE
%token INT VOID FLOAT

/* no meaning, just placeholders */
%token STATIC AUTO EXTERN TYPEDEF CONST VOLATILE ENUM UNION REGISTER
/* NUMBER and ID have values associated with them returned from lex*/

%token <inum> CONSTANT_INTEGER /*data type of NUMBER is num union*/
%token <fnum> CONSTANT_FLOAT /*data type of NUMBER is num union*/
%token <id>  ID

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* values created by parser*/

%type <id> declarator
%type <params> param_list param_list_opt
%type <value> expression
%type <value> expr_opt
%type <value> assignment_expression
%type <value> conditional_expression
%type <value> constant_expression
%type <value> logical_OR_expression
%type <value> logical_AND_expression
%type <value> inclusive_OR_expression
%type <value> exclusive_OR_expression
%type <value> AND_expression
%type <value> equality_expression
%type <value> relational_expression
%type <value> shift_expression
%type <value> additive_expression
%type <value> multiplicative_expression
%type <value> cast_expression
%type <value> unary_expression
%type <value> lhs_expression
%type <value> postfix_expression
%type <value> primary_expression
%type <value> constant
%type <type> type_specifier
%type <value> opt_initializer
/* 
   The grammar used here is largely borrowed from Kernighan and Ritchie's "The C
   Programming Language," 2nd Edition, Prentice Hall, 1988. 

   But, some modifications have been made specifically for MiniC!
 */

%%

/* 
   Beginning of grammar: Rules
*/

translation_unit:	  external_declaration
			| translation_unit external_declaration
;

external_declaration:	  function_definition
{
  /* finish compiling function */
  if(num_errors>100)
    {
      minic_abort();
    }
  else if(num_errors==0)
    {
      
    }
}
                        | declaration 
{ 
  /* nothing to be done here */
}
;

function_definition:	  type_specifier ID LPAREN param_list_opt RPAREN 
{
  symbol_push_scope();
  /* This is a mid-rule action */
  BuildFunction($1,$2,$4);  
} 
                          compound_stmt 
{ 
  /* This is the rule completion */
  LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  if(!LLVMGetBasicBlockTerminator(BB))
    {
      if($1==LLVMInt32Type())	
	{
	  LLVMBuildRet(Builder,LLVMConstInt(LLVMInt32TypeInContext(Context),
					    0,(LLVMBool)1));
	}
      else if($1==LLVMFloatType()) 
	{
	  LLVMBuildRet(Builder,LLVMConstReal(LLVMFloatType(),0.0));
					    
	}
      else
	{
	  LLVMBuildRetVoid(Builder);
	  
	}
    }

  symbol_pop_scope();
  /* make sure basic block has a terminator (a return statement) */
}
                        | type_specifier STAR ID LPAREN param_list_opt RPAREN 
{
  symbol_push_scope();
  BuildFunction(LLVMPointerType($1,0),$3,$5);
} 
                          compound_stmt 
{ 
  /* This is the rule completion */


  /* make sure basic block has a terminator (a return statement) */

  LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  if(!LLVMGetBasicBlockTerminator(BB))
    {
      LLVMBuildRet(Builder,LLVMConstPointerNull(LLVMPointerType($1,0)));
    }

  symbol_pop_scope();
}
;

declaration:    type_specifier STAR ID opt_initializer SEMICOLON
{
  if (is_global_scope())
    {
      /*LLVMValueRef g = */ LLVMAddGlobal(Module,LLVMPointerType($1,0),$3);

      //Do some sanity checks .. if okay, then do this:
      //LLVMSetInitializer(g,$4);
    } 
  else
    {
      symbol_insert($3,  /* map name to alloca */
		    LLVMBuildAlloca(Builder,LLVMPointerType($1,0),$3)); /* build alloca */
      
      // Store initial value!
      // LLVMBuildStore(Builder, ...
    }

} 
              | type_specifier ID opt_initializer SEMICOLON
{
  if (is_global_scope())
    {
      LLVMAddGlobal(Module,$1,$2);

      // Do some checks... if it's okay:
      //LLVMSetInitializer(g,$4);
    }
  else
    {
		//printf("building alloca %s\n", $2);
		LLVMValueRef alloca = LLVMBuildAlloca(Builder,$1,$2);
      symbol_insert($2,  /* map name to alloca */
		    alloca); /* build alloca */
		      /* not an arg */

      // Store initial value if there is one
      // LLVMBuildStore(Builder, ...
	  if ($3 != NULL) {
		  LLVMBuildStore(Builder, $3, alloca);
	  }
    }
} 
;

declaration_list:	   declaration
{

}
                         | declaration_list declaration  
{

}
;


type_specifier:		  INT 
{
  $$ = LLVMInt32Type();
}
|                         FLOAT
{
  $$ = LLVMFloatType();
}
|                         VOID
{
  $$ = LLVMVoidType();
}
;

declarator: ID
{
  $$ = $1;
}
;

opt_initializer: ASSIGN constant_expression	      
{
  $$ = $2;
}
| // nothing
{
  // indicate there is none
  $$ = NULL;
}
;

param_list_opt:           
{ 
  $$ = NULL;
}
                        | param_list
{ 
  $$ = $1;
}
;

param_list:	
			  param_list COMMA type_specifier declarator
{
  $$ = push_param($1,$4,$3);
}
			| param_list COMMA type_specifier STAR declarator
{
  $$ = push_param($1,$5,LLVMPointerType($3,0));
}
                        | param_list COMMA type_specifier
{
  $$ = push_param($1,NULL,$3);
}
			|  type_specifier declarator
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, $2, $1);
}
			| type_specifier STAR declarator
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, $3, LLVMPointerType($1,0));
}
                        | type_specifier
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, NULL, $1);
}
;


statement:		  expr_stmt            
			| compound_stmt        
			| selection_stmt       
			| iteration_stmt       
			| jump_stmt            
                        | break_stmt
                        | continue_stmt
                        | case_stmt
;

expr_stmt:	           SEMICOLON            
{ 

}
			|  expression SEMICOLON       
{ 

}
;

compound_stmt:		  LBRACE declaration_list_opt statement_list_opt RBRACE 
{

}
;

declaration_list_opt:	
{

}
			| declaration_list
{

}
;

statement_list_opt:	
{

}
			| statement_list
{

}
;

statement_list:		statement
{

}
			| statement_list statement
{

}
;

break_stmt:               BREAK SEMICOLON
{

};

case_stmt:                CASE constant_expression COLON
{
  // BONUS: part of switch implementation
};

continue_stmt:            CONTINUE SEMICOLON
{

};

selection_stmt:		  
		          IF LPAREN expression RPAREN {
					  LLVMBasicBlockRef blockif = LLVMAppendBasicBlock(Function, "block.if");
					  LLVMBasicBlockRef blockthen = LLVMAppendBasicBlock(Function, "block.else");
					  
					  LLVMValueRef zero;
						if (LLVMTypeOf($3) == LLVMInt32Type()) {
							zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
						} else if (LLVMTypeOf($3) == LLVMFloatType()) {
							zero = LLVMConstReal(LLVMFloatType(), 0);
						} else {
							zero = LLVMConstPointerNull(LLVMPointerType(LLVMInt32Type(),0));
						}
					  LLVMValueRef cond = LLVMBuildICmp(Builder, LLVMIntNE, $3, zero, "");
					  LLVMBuildCondBr(Builder, cond, blockif, blockthen);

					  LLVMPositionBuilderAtEnd(Builder, blockif);
					  $<bb>$ = blockthen;
					  
				  } statement {
					  LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "block.join");
					  LLVMBuildBr(Builder, join);

					  LLVMPositionBuilderAtEnd(Builder, $<bb>5);
					  $<bb>$ = join;
				  } ELSE statement {
					  LLVMBuildBr(Builder, $<bb>7);

					  LLVMPositionBuilderAtEnd(Builder, $<bb>7);
				  }

| 		          SWITCH LPAREN expression RPAREN statement 
{
  // +10 BONUS POINTS for a fully correct implementation
}
;

iteration_stmt:		  WHILE LPAREN { 
  /* set up header basic block
     make it the new insertion point */
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function, "while.cond");
	LLVMBuildBr(Builder, cond);
	LLVMPositionBuilderAtEnd(Builder, cond);

	$<bb>$ = cond;
} expression RPAREN { 
  /* set up loop body */

  /* create new body and exit blocks */
  LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function, "while.body");
  LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "while.join");
  
  LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
  LLVMValueRef condx = LLVMBuildICmp(Builder, LLVMIntNE, $4, zero, "");
  LLVMBuildCondBr(Builder, condx, body, join);

  LLVMPositionBuilderAtEnd(Builder, body);
  $<bb>$ = join;

  /* to support nesting: */
  /*push_loop(expr,body,body,after);*/
  //push_loop($<bb>3, body, NULL, join);
} 
  statement
{
  /* finish loop */
  /*loop_info_t info = get_loop();*/

  /*pop_loop();*/
  LLVMMoveBasicBlockAfter($<bb>6, LLVMGetLastBasicBlock(Function));
  LLVMBuildBr(Builder, $<bb>3);
  LLVMPositionBuilderAtEnd(Builder, $<bb>6);
}

| FOR LPAREN expr_opt 
{
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function, "for.cond");
	LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function, "for.body");
	LLVMBasicBlockRef reinit = LLVMAppendBasicBlock(Function, "for.reinit");
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "for.join");
	
	LLVMBuildBr(Builder, cond);
	LLVMPositionBuilderAtEnd(Builder, cond);
	push_loop(cond, body, reinit, join);
} 
SEMICOLON expr_opt 
{
	loop_info_t info = get_loop();
	
	LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	LLVMValueRef condx = LLVMBuildICmp(Builder, LLVMIntNE, $6, zero, "");
	LLVMBuildCondBr(Builder, condx, info.body, info.exit);
	LLVMPositionBuilderAtEnd(Builder, info.reinit);
} 
SEMICOLON expr_opt 
{
	loop_info_t info = get_loop();
	LLVMBuildBr(Builder, info.expr);
	LLVMPositionBuilderAtEnd(Builder, info.body);
}
RPAREN statement
{
	loop_info_t info = get_loop();
	pop_loop();
	LLVMBuildBr(Builder, info.reinit);
	LLVMPositionBuilderAtEnd(Builder, info.exit);
	LLVMMoveBasicBlockAfter(info.reinit, LLVMGetLastBasicBlock(Function));
	LLVMMoveBasicBlockAfter(info.exit, LLVMGetLastBasicBlock(Function));
}
| DO {
	LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function, "do.body");
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function, "do.cond");
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "do.join");
	
	LLVMBuildBr(Builder, body);
	LLVMPositionBuilderAtEnd(Builder, body);
	push_loop(cond, body, NULL, join);
} statement {
	loop_info_t info = get_loop();
	LLVMBuildBr(Builder, info.expr);
	LLVMPositionBuilderAtEnd(Builder, info.expr);
} WHILE LPAREN expression {
	loop_info_t info = get_loop();
	
	LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	LLVMValueRef condx = LLVMBuildICmp(Builder, LLVMIntNE, $7, zero, "");
	LLVMBuildCondBr(Builder, condx, info.body, info.exit);
	LLVMPositionBuilderAtEnd(Builder, info.exit);
	LLVMMoveBasicBlockAfter(info.expr, LLVMGetLastBasicBlock(Function));
	LLVMMoveBasicBlockAfter(info.exit, LLVMGetLastBasicBlock(Function));
	pop_loop();
} RPAREN SEMICOLON
{
  // 566: implement
}
;

expr_opt:		
{ 
	
}
			| expression
{ 
	
}
;

jump_stmt:		  RETURN SEMICOLON
{ 
  LLVMBuildRetVoid(Builder);

}
			| RETURN expression SEMICOLON
{
  LLVMBuildRet(Builder,$2);
}
;

expression:               assignment_expression
{ 
  $$=$1;
}
;

assignment_expression:    conditional_expression
{
  $$=$1;
}
                        | lhs_expression ASSIGN assignment_expression
{
  //printf("lhs_expression ASSIGN assignment_expression\n");
  /* Implement */
  LLVMBuildStore(Builder,$3,$1);
}
;


conditional_expression:   logical_OR_expression
{
  $$=$1;
}
                        | logical_OR_expression QUESTION_MARK expression COLON conditional_expression
{
  /* Implement */
}
;

constant_expression:       conditional_expression
{ $$ = $1; }
;

logical_OR_expression:    logical_AND_expression
{
  $$ = $1;
}
                        | logical_OR_expression LOGICAL_OR 
{
	LLVMValueRef zero, cond;
	LLVMBasicBlockRef or = LLVMAppendBasicBlock(Function, "logl.or");
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "logl.or.join");
	zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	cond = LLVMBuildICmp(Builder, LLVMIntNE, $1, zero, "");
	LLVMBuildCondBr(Builder, cond, join, or);
	LLVMPositionBuilderAtEnd(Builder, or);
	
	$<bb>$ = join;
} 
logical_AND_expression
{
	LLVMValueRef zero, cond, phi;
	zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	cond = LLVMBuildICmp(Builder, LLVMIntNE, $4, zero, "");
	LLVMValueRef val = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
	LLVMBasicBlockRef or = LLVMGetInstructionParent(cond);
	LLVMBasicBlockRef entry = LLVMGetPreviousBasicBlock(or);
	LLVMBuildBr(Builder, $<bb>3);
	LLVMPositionBuilderAtEnd(Builder, $<bb>3);
	
	phi = LLVMBuildPhi(Builder, LLVMInt32Type(), "logl.or.phi");
	LLVMValueRef phi_vals[] = { LLVMConstInt(LLVMInt32Type(), 1, 0), val };
	LLVMBasicBlockRef phi_blocks[] = { entry, or };
	LLVMAddIncoming(phi, phi_vals, phi_blocks, 2);
	
	$$ = phi;
};

logical_AND_expression:   inclusive_OR_expression
{
  $$ = $1;
}
| logical_AND_expression LOGICAL_AND
{

	LLVMValueRef zero, cond;
	LLVMBasicBlockRef and = LLVMAppendBasicBlock(Function, "logl.and");
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function, "logl.and.join");
	if (LLVMTypeOf($1) == LLVMInt32Type()) {
		zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	} else if (LLVMTypeOf($1) == LLVMFloatType()) {
		zero = LLVMConstReal(LLVMFloatType(), 0);
	} else {
		zero = LLVMConstPointerNull(LLVMPointerType(LLVMInt32Type(),0));
	}
	
	cond = LLVMBuildICmp(Builder, LLVMIntNE, $1, zero, "");
	LLVMBuildCondBr(Builder, cond, and, join);
	LLVMPositionBuilderAtEnd(Builder, and);
	
	$<bb>$ = join;
} inclusive_OR_expression
{
  /* --Implement */

  	LLVMValueRef zero, cond, phi;
	if (LLVMTypeOf($1) == LLVMInt32Type()) {
		zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	} else if (LLVMTypeOf($1) == LLVMFloatType()) {
		zero = LLVMConstReal(LLVMFloatType(), 0);
	} else {
		zero = LLVMConstPointerNull(LLVMPointerType(LLVMInt32Type(),0));
	}
	cond = LLVMBuildICmp(Builder, LLVMIntNE, $4, zero, "");
	LLVMValueRef val = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
	LLVMBasicBlockRef and = LLVMGetInstructionParent(cond);
	LLVMBasicBlockRef entry = LLVMGetPreviousBasicBlock(and);
	LLVMBuildBr(Builder, $<bb>3);
	LLVMPositionBuilderAtEnd(Builder, $<bb>3);
	
	phi = LLVMBuildPhi(Builder, LLVMInt32Type(), "logl.and.phi");
	LLVMValueRef phi_vals[] = { LLVMConstInt(LLVMInt32Type(), 0, 0), val };
	LLVMBasicBlockRef phi_blocks[] = {entry, and };
	LLVMAddIncoming(phi, phi_vals, phi_blocks, 2);
	
	$$ = phi;
}
;

inclusive_OR_expression:  exclusive_OR_expression
{
    $$=$1;
}
                        | inclusive_OR_expression BITWISE_OR exclusive_OR_expression
{
  /* Implement */
}
;

exclusive_OR_expression:  AND_expression
{
  $$ = $1;
}
                        | exclusive_OR_expression BITWISE_XOR AND_expression
{
  /* Implement */
}
;

AND_expression:           equality_expression
{
  $$ = $1;
}
                        | AND_expression AMPERSAND equality_expression
{
  /* Implement */
}
;

equality_expression:      relational_expression
{
  $$ = $1;
}
                        | equality_expression EQ relational_expression
{
  /* Implement: use icmp */
  LLVMValueRef cond;
  if (LLVMTypeOf($1) == LLVMInt32Type()) {
	  cond = LLVMBuildICmp(Builder, LLVMIntEQ, $1, $3, "cond.lt");
  }
  else {
	  cond = LLVMBuildFCmp(Builder, LLVMRealOEQ, $1, $3, "condf.lt");
  }
  
  $$ = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
}
                        | equality_expression NEQ relational_expression
{
  /* Implement: use icmp */
}
;

relational_expression:    shift_expression
{
    $$=$1;
}
                        | relational_expression LT shift_expression
{
  /* --Implement: use icmp */
  LLVMValueRef zero, one, cond;
  if (LLVMTypeOf($1) == LLVMInt32Type()) {
	  zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	  one = LLVMConstInt(LLVMInt32Type(), 1, 0);
	  cond = LLVMBuildICmp(Builder, LLVMIntSLT, $1, $3, "cond.lt");
  }
  else {
	  zero = LLVMConstReal(LLVMFloatType(), 0);
	  one = LLVMConstReal(LLVMFloatType(), 1);
	  cond = LLVMBuildFCmp(Builder, LLVMRealOLT, $1, $3, "condf.lt");
  }
  
  $$ = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
}
                        | relational_expression GT shift_expression
{
  /* Implement: use icmp */
    LLVMValueRef zero, one, cond;
  if (LLVMTypeOf($1) == LLVMInt32Type()) {
	  zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
	  one = LLVMConstInt(LLVMInt32Type(), 1, 0);
	  cond = LLVMBuildICmp(Builder, LLVMIntSGT, $1, $3, "cond.gt");
  }
  else {
	  zero = LLVMConstReal(LLVMFloatType(), 0);
	  one = LLVMConstReal(LLVMFloatType(), 1);
	  cond = LLVMBuildFCmp(Builder, LLVMRealOGT, $1, $3, "condf.gt");
  }
  
  $$ = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
}
                        | relational_expression LTE shift_expression
{
  /* Implement: use icmp */
}
                        | relational_expression GTE shift_expression
{
  /* Implement: use icmp */
}
;

shift_expression:         additive_expression
{
    $$=$1;
}
                        | shift_expression LSHIFT additive_expression
{
  /* Implement */
}
                        | shift_expression RSHIFT additive_expression
{
  /* Implement */
}
;

additive_expression:      multiplicative_expression
{
  $$ = $1;
}
                        | additive_expression PLUS multiplicative_expression
{
  /* --Implement */
  if(LLVMTypeOf($3) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildAdd(Builder, $1, $3, "");
	}
  else if (LLVMTypeOf($3) == LLVMFloatType())
    {
		$$ = LLVMBuildFAdd(Builder, $1, $3, "");
	}
  

}
                        | additive_expression MINUS multiplicative_expression
{
  /* --Implement */
  if(LLVMTypeOf($3) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildSub(Builder, $1, $3, "");
	}
  else if (LLVMTypeOf($3) == LLVMFloatType())
    {
		$$ = LLVMBuildFSub(Builder, $1, $3, "");
	}
}
;

multiplicative_expression:  cast_expression
{
  $$ = $1;
}
                        | multiplicative_expression STAR cast_expression
{
  /* --Implement */
   if(LLVMTypeOf($3) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildMul(Builder, $1, $3, "");
	}
  else if (LLVMTypeOf($3) == LLVMFloatType())
    {
		$$ = LLVMBuildFMul(Builder, $1, $3, "");
	}
}
                        | multiplicative_expression DIV cast_expression
{
  /* --Implement */
  if(LLVMTypeOf($3) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildSDiv(Builder, $1, $3, "");
	}
  else if (LLVMTypeOf($3) == LLVMFloatType())
    {
		$$ = LLVMBuildFDiv(Builder, $1, $3, "");
	}
}
                        | multiplicative_expression MOD cast_expression
{
  /* --Implement */
  if(LLVMTypeOf($3) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildSRem(Builder, $1, $3, "");
	}
  else if (LLVMTypeOf($3) == LLVMFloatType())
    {
		$$ = LLVMBuildFRem(Builder, $1, $3, "");
	}
}
;

cast_expression:          unary_expression
{ $$ = $1; }
                        | LPAREN type_specifier RPAREN cast_expression
{
  // --Implement
  if ($2 == LLVMInt32Type()) {
	  //(int)float, need to convert float to int
	  $$ = LLVMBuildFPToSI(Builder, $4, $2, "");
  }
  else {
	  //(float)int, need to convert int to float
	  $$ = LLVMBuildSIToFP(Builder, $4, $2, "");
  } 
}
                        | LPAREN type_specifier STAR RPAREN cast_expression
{ 
  // Implement
  if ($2 == LLVMInt32Type()) {
	  //(int *)XXXXXXX), convert an integer into a pointer
	  $$ = LLVMBuildIntToPtr(Builder, $5, LLVMPointerType(LLVMInt32Type(),0), "");
  }
  else {
	  
  }
}
;

lhs_expression:           ID 
{
  //printf("lhs_expression: %s\n", $1);
  LLVMValueRef val = symbol_find($1);
  if (val == NULL) { printf("problem!\n");}
  $$ = val;
}
                        | STAR ID
{
  LLVMValueRef val = symbol_find($2);
  $$ = LLVMBuildLoad(Builder,val,"");
}
;

unary_expression:         postfix_expression
{
  $$ = $1;
}
                        | AMPERSAND primary_expression
{
  /* --Implement ???? */
  $$ = LLVMGetOperand($2, 0);
}
                        | STAR primary_expression
{
  /*  */
  $$ = LLVMBuildLoad(Builder,$2,"");
}
                        | MINUS unary_expression
{
  /* --Implement */
    if(LLVMTypeOf($2) == LLVMInt32Type())	
	{
	  $$ = LLVMBuildSub(Builder, LLVMConstInt(LLVMInt32Type(), 0, 0), $2, "");
	}
  else if (LLVMTypeOf($2) == LLVMFloatType())
    {
		$$ = LLVMBuildFSub(Builder, LLVMConstReal(LLVMFloatType(), 0), $2, "");
	}
}
                        | PLUS unary_expression
{
  $$ = $2;
}
                        | BITWISE_INVERT unary_expression
{
  /* Implement */
}
                        | NOT unary_expression
{
  /* --Implement */

  LLVMValueRef zero, cond = LLVMConstInt(LLVMInt32Type(), 0, 0);;
	if(LLVMTypeOf($2) == LLVMInt32Type())	
	{
		zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
		cond = LLVMBuildICmp(Builder, LLVMIntNE, $2, zero, "");
	}
	else if (LLVMTypeOf($2) == LLVMFloatType())
    {
		zero = LLVMConstReal(LLVMFloatType(), 0);
		cond = LLVMBuildFCmp(Builder, LLVMRealONE, $2, zero, "");
	} else {
		zero = LLVMConstPointerNull(LLVMPointerType(LLVMInt32Type(),0));
		cond = LLVMBuildICmp(Builder, LLVMIntNE, $2, zero, "");
	}
	$$ = LLVMBuildZExt(Builder, cond, LLVMInt32Type(), "");
}
;


postfix_expression:       primary_expression
{
  $$ = $1;
}
;

primary_expression:       ID 
{
  //printf("primary_expression: ID: %s\n", $1);
  LLVMValueRef val = symbol_find($1);
  $$ = LLVMBuildLoad(Builder,val,"");
}
                        | constant
{
  $$ = $1;
}
                        | LPAREN expression RPAREN
{
  $$ = $2;
}
;

constant:	          CONSTANT_INTEGER  
{ 
  /* -- Implement */
  //printf("const val: %i\n", $1);
  $$ = LLVMConstInt(LLVMInt32Type(), $1, 0);
} 
|                         CONSTANT_FLOAT
{
  //LLVMBool b;
  //printf("floatval: %f\n", $1);
  $$ = LLVMConstReal(LLVMFloatType(), $1);
  //printf("did i get here? %f\n", LLVMConstRealGetDouble($$, &b));
}
;

%%

LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params)
{
  int i;
  int size = paramlist_size(params);
  LLVMTypeRef *ParamArray = malloc(sizeof(LLVMTypeRef)*size);
  LLVMTypeRef FunType;
  LLVMBasicBlockRef BasicBlock;

  paramlist_t *tmp = params;
  /* Build type for function */
  for(i=size-1; i>=0; i--) 
    {
      ParamArray[i] = tmp->type;
      tmp = next_param(tmp);
    }
  
  FunType = LLVMFunctionType(RetType,ParamArray,size,0);

  Function = LLVMAddFunction(Module,name,FunType);
  
  /* Add a new entry basic block to the function */
  BasicBlock = LLVMAppendBasicBlock(Function,"entry");

  /* Create an instruction builder class */
  Builder = LLVMCreateBuilder();

  /* Insert new instruction at the end of entry block */
  LLVMPositionBuilderAtEnd(Builder,BasicBlock);

  tmp = params;
  for(i=size-1; i>=0; i--)
    {
      LLVMValueRef alloca = LLVMBuildAlloca(Builder,tmp->type,tmp->name);
      LLVMBuildStore(Builder,LLVMGetParam(Function,i),alloca);
      symbol_insert(tmp->name,alloca);
      tmp=next_param(tmp);
    }

  return Function;
}

extern int line_num;
extern char *infile[];
static int   infile_cnt=0;
extern FILE * yyin;

int parser_error(const char *msg)
{
  printf("%s (%d): Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int internal_error(const char *msg)
{
  printf("%s (%d): Internal Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int yywrap() {
  static FILE * currentFile = NULL;

  if ( (currentFile != 0) ) {
    fclose(yyin);
  }
  
  if(infile[infile_cnt]==NULL)
    return 1;

  currentFile = fopen(infile[infile_cnt],"r");
  if(currentFile!=NULL)
    yyin = currentFile;
  else
    printf("Could not open file: %s",infile[infile_cnt]);

  infile_cnt++;
  
  return (currentFile)?0:1;
}

int yyerror()
{
  parser_error("Un-resolved syntax error.");
  return 1;
}

char * get_filename()
{
  return infile[infile_cnt-1];
}

int get_lineno()
{
  return line_num;
}


void minic_abort()
{
  parser_error("Too many errors to continue.");
  exit(1);
}
