/*******************************************************************************
*									       *
* interpret.c -- Nirvana Editor macro interpreter			       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
* 									       *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* April, 1997								       *
*									       *
* Written by Mark Edel							       *
*									       *
*******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "interpret.h"
#include "textBuf.h"
#include "nedit.h"
#include "menu.h"
#include "text.h"
#include "../util/rbTree.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "window.h"

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

#define PROGRAM_SIZE  4096	/* Maximum program size */
#define MAX_ERR_MSG_LEN 256	/* Max. length for error messages */
#define LOOP_STACK_SIZE 200	/* (Approx.) Number of break/continue stmts
    	    	    	    	   allowed per program */
#define INSTRUCTION_LIMIT 100 	/* Number of instructions the interpreter is
    	    	    	    	   allowed to execute before preempting and
    	    	    	    	   returning to allow other things to run */

/* Temporary markers placed in a branch address location to designate
   which loop address (break or continue) the location needs */
#define NEEDS_BREAK 1
#define NEEDS_CONTINUE 2

#define N_ARGS_ARG_SYM -1   	/* special arg number meaning $n_args value */

enum opStatusCodes {STAT_OK=2, STAT_DONE, STAT_ERROR, STAT_PREEMPT};

static void addLoopAddr(Inst *addr);
static void saveContext(RestartData *context);
static void restoreContext(RestartData *context);
static int returnNoVal(void);
static int returnVal(void);
static int returnValOrNone(int valOnStack);
static int pushSymVal(void);
static int pushArgVal(void);
static int pushArgCount(void);
static int pushArgArray(void);
static int pushArraySymVal(void);
static int dupStack(void);
static int add(void);
static int subtract(void);
static int multiply(void);
static int divide(void);
static int modulo(void);
static int negate(void);
static int increment(void);
static int decrement(void);
static int gt(void);
static int lt(void);
static int ge(void);
static int le(void);
static int eq(void);
static int ne(void);
static int bitAnd(void);
static int bitOr(void);
static int and(void);
static int or(void);
static int not(void);
static int power(void);
static int concat(void);
static int assign(void);
static int callSubroutine(void);
static int fetchRetVal(void);
static int branch(void);
static int branchTrue(void);
static int branchFalse(void);
static int branchNever(void);
static int arrayRef(void);
static int arrayAssign(void);
static int arrayRefAndAssignSetup(void);
static int beginArrayIter(void);
static int arrayIter(void);
static int inArray(void);
static int deleteArrayElement(void);
static void freeSymbolTable(Symbol *symTab);
static int errCheck(const char *s);
static int execError(const char *s1, const char *s2);
static rbTreeNode *arrayEmptyAllocator(void);
static rbTreeNode *arrayAllocateNode(rbTreeNode *src);
static int arrayEntryCopyToNode(rbTreeNode *dst, rbTreeNode *src);
static int arrayEntryCompare(rbTreeNode *left, rbTreeNode *right);
static void arrayDisposeNode(rbTreeNode *src);
static SparseArrayEntry *allocateSparseArrayEntry(void);

/*#define DEBUG_ASSEMBLY*/
/*#define DEBUG_STACK*/

#if defined(DEBUG_ASSEMBLY) || defined(DEBUG_STACK)
#define DEBUG_DISASSEMBLER
static void disasm(Inst *inst, int nInstr);
#endif /* #if defined(DEBUG_ASSEMBLY) || defined(DEBUG_STACK) */

#ifdef DEBUG_ASSEMBLY   /* for disassembly */
#define DISASM(i, n)    disasm(i, n)
#else /* #ifndef DEBUG_ASSEMBLY */
#define DISASM(i, n)
#endif /* #ifndef DEBUG_ASSEMBLY */

#ifdef DEBUG_STACK      /* for run-time instruction and stack trace */
static void stackdump(int n, int extra);
#define STACKDUMP(n, x) stackdump(n, x)
#define DISASM_RT(i, n) disasm(i, n)
#else /* #ifndef DEBUG_STACK */
#define STACKDUMP(n, x)
#define DISASM_RT(i, n)
#endif /* #ifndef DEBUG_STACK */

/* Global symbols and function definitions */
static Symbol *GlobalSymList = NULL;

/* List of all memory allocated for strings */
static char *AllocatedStrings = NULL;

typedef struct SparseArrayEntryWrapperTag {
    SparseArrayEntry 	data; /* LEAVE this as top entry */
    int inUse;              /* we use pointers to the data to refer to the entire struct */
    struct SparseArrayEntryWrapperTag *next;
} SparseArrayEntryWrapper;

static SparseArrayEntryWrapper *AllocatedSparseArrayEntries = NULL; 

/* Message strings used in macros (so they don't get repeated every time
   the macros are used */
static const char *StackOverflowMsg = "macro stack overflow";
static const char *StackUnderflowMsg = "macro stack underflow";
static const char *StringToNumberMsg = "string could not be converted to number";

/* Temporary global data for use while accumulating programs */
static Symbol *LocalSymList = NULL;	 /* symbols local to the program */
static Inst Prog[PROGRAM_SIZE]; 	 /* the program */
static Inst *ProgP;			 /* next free spot for code gen. */
static Inst *LoopStack[LOOP_STACK_SIZE]; /* addresses of break, cont stmts */
static Inst **LoopStackPtr = LoopStack;  /*  to fill at the end of a loop */

/* Global data for the interpreter */
static DataValue *TheStack;	    /* the stack */
static DataValue *StackP;	    /* next free spot on stack */
static DataValue *FrameP;   	    /* frame pointer (start of local variables
    	    	    	    	       for the current subroutine invocation) */
static Inst *PC;		    /* program counter during execution */
static char *ErrMsg;		    /* global for returning error messages
    	    	    	    	       from executing functions */
static WindowInfo
	*InitiatingWindow = NULL;   /* window from which macro was run */
static WindowInfo *FocusWindow;	    /* window on which macro commands operate */
static int PreemptRequest;  	    /* passes preemption requests from called
    	    	    	    	       routines back up to the interpreter */

/* Array for mapping operations to functions for performing the operations
   Must correspond to the enum called "operations" in interpret.h */
static int (*OpFns[N_OPS])() = {returnNoVal, returnVal, pushSymVal, dupStack,
    add, subtract, multiply, divide, modulo, negate, increment, decrement,
    gt, lt, ge, le, eq, ne, bitAnd, bitOr, and, or, not, power, concat,
    assign, callSubroutine, fetchRetVal, branch, branchTrue, branchFalse,
    branchNever, arrayRef, arrayAssign, beginArrayIter, arrayIter, inArray,
    deleteArrayElement, pushArraySymVal,
    arrayRefAndAssignSetup, pushArgVal, pushArgCount, pushArgArray};

/* Stack-> symN-sym0(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ... */
#define FP_ARG_ARRAY_CACHE_INDEX (-1)
#define FP_ARG_COUNT_INDEX (-2)
#define FP_OLD_FP_INDEX (-3)
#define FP_RET_PC_INDEX (-4)
#define FP_TO_ARGS_DIST (4) /* should be 0 - (above index) */
#define FP_GET_ITEM(xFrameP,xIndex) (*(xFrameP + xIndex))
#define FP_GET_ARG_ARRAY_CACHE(xFrameP) (FP_GET_ITEM(xFrameP, FP_ARG_ARRAY_CACHE_INDEX))
#define FP_GET_ARG_COUNT(xFrameP) (FP_GET_ITEM(xFrameP, FP_ARG_COUNT_INDEX).val.n)
#define FP_GET_OLD_FP(xFrameP) ((FP_GET_ITEM(xFrameP, FP_OLD_FP_INDEX)).val.dataval)
#define FP_GET_RET_PC(xFrameP) ((FP_GET_ITEM(xFrameP, FP_RET_PC_INDEX)).val.inst)
#define FP_ARG_START_INDEX(xFrameP) (-(FP_GET_ARG_COUNT(xFrameP) + FP_TO_ARGS_DIST))
#define FP_GET_ARG_N(xFrameP,xN) (FP_GET_ITEM(xFrameP, xN + FP_ARG_START_INDEX(xFrameP)))
#define FP_GET_SYM_N(xFrameP,xN) (FP_GET_ITEM(xFrameP, xN))
#define FP_GET_SYM_VAL(xFrameP,xSym) (FP_GET_SYM_N(xFrameP, xSym->value.val.n))

/*
** Initialize macro language global variables.  Must be called before
** any macros are even parsed, because the parser uses action routine
** symbols to comprehend hyphenated names.
*/
void InitMacroGlobals(void)
{
    XtActionsRec *actions;
    int i, nActions;
    static char argName[3] = "$x";
    static DataValue dv = {NO_TAG, {0}};

    /* Add action routines from NEdit menus and text widget */
    actions = GetMenuActions(&nActions);
    for (i=0; i<nActions; i++) {
    	dv.val.xtproc = actions[i].proc;
    	InstallSymbol(actions[i].string, ACTION_ROUTINE_SYM, dv);
    }
    actions = TextGetActions(&nActions);
    for (i=0; i<nActions; i++) {
    	dv.val.xtproc = actions[i].proc;
    	InstallSymbol(actions[i].string, ACTION_ROUTINE_SYM, dv);
    }
    
    /* Add subroutine argument symbols ($1, $2, ..., $9) */
    for (i=0; i<9; i++) {
	argName[1] = '1' + i;
	dv.val.n = i;
	InstallSymbol(argName, ARG_SYM, dv);
    }
    
    /* Add special symbol $n_args */
    dv.val.n = N_ARGS_ARG_SYM;
    InstallSymbol("$n_args", ARG_SYM, dv);
}

/*
** To build a program for the interpreter, call BeginCreatingProgram, to
** begin accumulating the program, followed by calls to AddOp, AddSym,
** and InstallSymbol to add symbols and operations.  When the new program
** is finished, collect the results with FinishCreatingProgram.  This returns
** a self contained program that can be run with ExecuteMacro.
*/

/*
** Start collecting instructions for a program. Clears the program
** and the symbol table.
*/
void BeginCreatingProgram(void)
{ 
    LocalSymList = NULL;
    ProgP = Prog;
    LoopStackPtr = LoopStack;
}

/*
** Finish up the program under construction, and return it (code and
** symbol table) as a package that ExecuteMacro can execute.  This
** program must be freed with FreeProgram.
*/
Program *FinishCreatingProgram(void)
{
    Program *newProg;
    int progLen, fpOffset = 0;
    Symbol *s;
    
    newProg = (Program *)NEditMalloc(sizeof(Program));
    progLen = ((char *)ProgP) - ((char *)Prog);
    newProg->code = (Inst *)NEditMalloc(progLen);
    memcpy(newProg->code, Prog, progLen);
    newProg->localSymList = LocalSymList;
    LocalSymList = NULL;
    
    /* Local variables' values are stored on the stack.  Here we assign
       frame pointer offsets to them. */
    for (s = newProg->localSymList; s != NULL; s = s->next)
	s->value.val.n = fpOffset++;
    
    DISASM(newProg->code, ProgP - Prog);
    
    return newProg;
}

void FreeProgram(Program *prog)
{
    freeSymbolTable(prog->localSymList);
    NEditFree(prog->code);
    NEditFree(prog);    
}

/*
** Add an operator (instruction) to the end of the current program
*/
int AddOp(int op, char **msg)
{
    if (ProgP >= &Prog[PROGRAM_SIZE]) {
	*msg = "macro too large";
	return 0;
    }
    ProgP->func = OpFns[op];
    ProgP++;
    return 1;
}

/*
** Add a symbol operand to the current program
*/
int AddSym(Symbol *sym, char **msg)
{
    if (ProgP >= &Prog[PROGRAM_SIZE]) {
	*msg = "macro too large";
	return 0;
    }
    ProgP->sym = sym;
    ProgP++;
    return 1;
}

/*
** Add an immediate value operand to the current program
*/
int AddImmediate(int value, char **msg)
{
    if (ProgP >= &Prog[PROGRAM_SIZE]) {
	*msg = "macro too large";
	return 0;
    }
    ProgP->value = value;
    ProgP++;
    return 1;
}

/*
** Add a branch offset operand to the current program
*/
int AddBranchOffset(Inst *to, char **msg)
{
    if (ProgP >= &Prog[PROGRAM_SIZE]) {
	*msg = "macro too large";
	return 0;
    }
    /* Should be ptrdiff_t for branch offsets */
    ProgP->value = to - ProgP;
    ProgP++;
    
    return 1;
}

/*
** Return the address at which the next instruction will be stored
*/
Inst *GetPC(void)
{
    return ProgP;
}

/*
** Swap the positions of two contiguous blocks of code.  The first block
** running between locations start and boundary, and the second between
** boundary and end.
*/
void SwapCode(Inst *start, Inst *boundary, Inst *end)
{
#define reverseCode(L, H) \
    do { register Inst t, *l = L, *h = H - 1; \
         while (l < h) { t = *h; *h-- = *l; *l++ = t; } } while (0)
    /* double-reverse method: reverse elements of both parts then whole lot */
    /* eg abcdefABCD -1-> edcbaABCD -2-> edcbaDCBA -3-> DCBAedcba */
    reverseCode(start, boundary);   /* 1 */
    reverseCode(boundary, end);     /* 2 */
    reverseCode(start, end);        /* 3 */
}

/*
** Maintain a stack to save addresses of branch operations for break and
** continue statements, so they can be filled in once the information
** on where to branch is known.
**
** Call StartLoopAddrList at the beginning of a loop, AddBreakAddr or
** AddContinueAddr to register the address at which to store the branch
** address for a break or continue statement, and FillLoopAddrs to fill
** in all the addresses and return to the level of the enclosing loop.
*/
void StartLoopAddrList(void)
{
    addLoopAddr(NULL);
}

int AddBreakAddr(Inst *addr)
{
    if (LoopStackPtr == LoopStack) return 1;
    addLoopAddr(addr);
    addr->value = NEEDS_BREAK;
    return 0;
}

int AddContinueAddr(Inst *addr)
{   
    if (LoopStackPtr == LoopStack) return 1;
    addLoopAddr(addr);
    addr->value = NEEDS_CONTINUE;
    return 0;
}

static void addLoopAddr(Inst *addr)
{
    if (LoopStackPtr > &LoopStack[LOOP_STACK_SIZE-1]) {
    	fprintf(stderr, "NEdit: loop stack overflow in macro parser");
    	return;
    }
    *LoopStackPtr++ = addr;
}

void FillLoopAddrs(Inst *breakAddr, Inst *continueAddr)
{
    while (True) {
    	LoopStackPtr--;
    	if (LoopStackPtr < LoopStack) {
    	    fprintf(stderr, "NEdit: internal error (lsu) in macro parser\n");
    	    return;
    	}
    	if (*LoopStackPtr == NULL)
    	    break;
    	if ((*LoopStackPtr)->value == NEEDS_BREAK)
            (*LoopStackPtr)->value = breakAddr - *LoopStackPtr;
    	else if ((*LoopStackPtr)->value == NEEDS_CONTINUE)
            (*LoopStackPtr)->value = continueAddr - *LoopStackPtr;
    	else
    	    fprintf(stderr, "NEdit: internal error (uat) in macro parser\n");
    }
}

/*
** Execute a compiled macro, "prog", using the arguments in the array
** "args".  Returns one of MACRO_DONE, MACRO_PREEMPT, or MACRO_ERROR.
** if MACRO_DONE is returned, the macro completed, and the returned value
** (if any) can be read from "result".  If MACRO_PREEMPT is returned, the
** macro exceeded its alotted time-slice and scheduled...
*/
int ExecuteMacro(WindowInfo *window, Program *prog, int nArgs, DataValue *args,
    	DataValue *result, RestartData **continuation, char **msg)
{
    RestartData *context;
    static DataValue noValue = {NO_TAG, {0}};
    Symbol *s;
    int i;
    
    /* Create an execution context (a stack, a stack pointer, a frame pointer,
       and a program counter) which will retain the program state across
       preemption and resumption of execution */
    context = (RestartData *)NEditMalloc(sizeof(RestartData));
    context->stack = (DataValue *)NEditMalloc(sizeof(DataValue) * STACK_SIZE);
    *continuation = context;
    context->stackP = context->stack;
    context->pc = prog->code;
    context->runWindow = window;
    context->focusWindow = window;

    /* Push arguments and call information onto the stack */
    for (i=0; i<nArgs; i++)
    	*(context->stackP++) = args[i];

    context->stackP->val.subr = NULL; /* return PC */
    context->stackP->tag = NO_TAG;
    context->stackP++;
    
    *(context->stackP++) = noValue; /* old FrameP */
    
    context->stackP->tag = NO_TAG; /* nArgs */
    context->stackP->val.n = nArgs;
    context->stackP++;
    
    *(context->stackP++) = noValue; /* cached arg array */
    
    context->frameP = context->stackP;
    
    /* Initialize and make room on the stack for local variables */
    for (s = prog->localSymList; s != NULL; s = s->next) {
    	FP_GET_SYM_VAL(context->frameP, s) = noValue;
    	context->stackP++;
    }
    
    /* Begin execution, return on error or preemption */
    return ContinueMacro(context, result, msg);
}

/*
** Continue the execution of a suspended macro whose state is described in
** "continuation"
*/
int ContinueMacro(RestartData *continuation, DataValue *result, char **msg)
{
    register int status, instCount = 0;
    register Inst *inst;
    RestartData oldContext;
    
    /* To allow macros to be invoked arbitrarily (such as those automatically
       triggered within smart-indent) within executing macros, this call is
       reentrant. */
    saveContext(&oldContext);
    
    /*
    ** Execution Loop:  Call the succesive routine addresses in the program
    ** until one returns something other than STAT_OK, then take action
    */
    restoreContext(continuation);
    ErrMsg = NULL;
    for (;;) {
    	
    	/* Execute an instruction */
    	inst = PC++;
	status = (inst->func)();
    	
    	/* If error return was not STAT_OK, return to caller */
    	if (status != STAT_OK) {
    	    if (status == STAT_PREEMPT) {
    		saveContext(continuation);
    		restoreContext(&oldContext);
    		return MACRO_PREEMPT;
    	    } else if (status == STAT_ERROR) {
		*msg = ErrMsg;
		FreeRestartData(continuation);
		restoreContext(&oldContext);
		return MACRO_ERROR;
	    } else if (status == STAT_DONE) {
		*msg = "";
		*result = *--StackP;
		FreeRestartData(continuation);
		restoreContext(&oldContext);
		return MACRO_DONE;
	    }
    	}
	
	/* Count instructions executed.  If the instruction limit is hit,
	   preempt, store re-start information in continuation and give
	   X, other macros, and other shell scripts a chance to execute */
    	instCount++;
	if (instCount >= INSTRUCTION_LIMIT) {
    	    saveContext(continuation);
    	    restoreContext(&oldContext);
    	    return MACRO_TIME_LIMIT;
	}
    }
}

/*
** If a macro is already executing, and requests that another macro be run,
** this can be called instead of ExecuteMacro to run it in the same context
** as if it were a subroutine.  This saves the caller from maintaining
** separate contexts, and serializes processing of the two macros without
** additional work.
*/
void RunMacroAsSubrCall(Program *prog)
{
    Symbol *s;
    static DataValue noValue = {NO_TAG, {0}};

    /* See subroutine "callSubroutine" for a description of the stack frame
       for a subroutine call */
    StackP->tag = NO_TAG;
    StackP->val.inst = PC; /* return PC */
    StackP++;
    
    StackP->tag = NO_TAG;
    StackP->val.dataval = FrameP; /* old FrameP */
    StackP++;
    
    StackP->tag = NO_TAG; /* nArgs */
    StackP->val.n = 0;
    StackP++;
    
    *(StackP++) = noValue; /* cached arg array */
    
    FrameP = StackP;
    PC = prog->code;
    for (s = prog->localSymList; s != NULL; s = s->next) {
	FP_GET_SYM_VAL(FrameP, s) = noValue;
	StackP++;
    }
}

void FreeRestartData(RestartData *context)
{
    NEditFree(context->stack);
    NEditFree(context);
}

/*
** Cause a macro in progress to be preempted (called by commands which take
** a long time, or want to return to the event loop.  Call ResumeMacroExecution
** to resume.
*/
void PreemptMacro(void)
{
    PreemptRequest = True;
}

/*
** Reset the return value for a subroutine which caused preemption (this is
** how to return a value from a routine which preempts instead of returning
** a value directly).
*/
void ModifyReturnedValue(RestartData *context, DataValue dv)
{
    if ((context->pc-1)->func == fetchRetVal)
	*(context->stackP-1) = dv;
}

/*
** Called within a routine invoked from a macro, returns the window in
** which the macro is executing (where the banner is, not where it is focused)
*/
WindowInfo *MacroRunWindow(void)
{
    return InitiatingWindow;
}

/*
** Called within a routine invoked from a macro, returns the window to which
** the currently executing macro is focused (the window which macro commands
** modify, not the window from which the macro is being run)
*/
WindowInfo *MacroFocusWindow(void)
{
    return FocusWindow;
}

/*
** Set the window to which macro subroutines and actions which operate on an
** implied window are directed.
*/
void SetMacroFocusWindow(WindowInfo *window)
{
    FocusWindow = window;
}

/*
** install an array iteration symbol
** it is tagged as an integer but holds an array node pointer
*/
#define ARRAY_ITER_SYM_PREFIX "aryiter "
Symbol *InstallIteratorSymbol(void)
{
    char symbolName[sizeof(ARRAY_ITER_SYM_PREFIX) + TYPE_INT_STR_SIZE(int)];
    DataValue value;
    static int interatorNameIndex = 0;

    sprintf(symbolName, ARRAY_ITER_SYM_PREFIX "#%d", interatorNameIndex);
    ++interatorNameIndex;
    value.tag = INT_TAG;
    value.val.arrayPtr = NULL;
    return(InstallSymbol(symbolName, LOCAL_SYM, value));
}

/*
** Lookup a constant string by its value. This allows reuse of string
** constants and fixing a leak in the interpreter.
*/
Symbol *LookupStringConstSymbol(const char *value)
{
    Symbol *s;

    for (s = GlobalSymList; s != NULL; s = s->next) {
        if (s->type == CONST_SYM &&
            s->value.tag == STRING_TAG &&
            !strcmp(s->value.val.str.rep, value)) {
            return(s);
        }
    }
    return(NULL);
}

/*
** install string str in the global symbol table with a string name
*/
Symbol *InstallStringConstSymbol(const char *str)
{
    static int stringConstIndex = 0;
    char stringName[35];
    DataValue value;
    Symbol *sym = LookupStringConstSymbol(str);
    if (sym) {
        return sym;
    }

    sprintf(stringName, "string #%d", stringConstIndex++);
    value.tag = STRING_TAG;
    AllocNStringCpy(&value.val.str, str);
    return(InstallSymbol(stringName, CONST_SYM, value));
}

/*
** find a symbol in the symbol table
*/
Symbol *LookupSymbol(const char *name)
{
    Symbol *s;

    for (s = LocalSymList; s != NULL; s = s->next)
	if (strcmp(s->name, name) == 0)
	    return s;
    for (s = GlobalSymList; s != NULL; s = s->next)
	if (strcmp(s->name, name) == 0)
	    return s;
    return NULL;
}

/*
** install symbol name in symbol table
*/
Symbol *InstallSymbol(const char *name, enum symTypes type, DataValue value)
{
    Symbol *s;

    s = (Symbol *)NEditMalloc(sizeof(Symbol));
    s->name = NEditStrdup(name);
    s->type = type;
    s->value = value;
    if (type == LOCAL_SYM) {
    	s->next = LocalSymList;
    	LocalSymList = s;
    } else {
    	s->next = GlobalSymList;
    	GlobalSymList = s;
    }
    return s;
}

/*
** Promote a symbol from local to global, removing it from the local symbol
** list.
**
** This is used as a forward declaration feature for macro functions.
** If a function is called (ie while parsing the macro) where the
** function isn't defined yet, the symbol is put into the GlobalSymList
** so that the function definition uses the same symbol.
**
*/
Symbol *PromoteToGlobal(Symbol *sym)
{
    Symbol *s;

    if (sym->type != LOCAL_SYM)
	return sym;

    /* Remove sym from the local symbol list */
    if (sym == LocalSymList)
	LocalSymList = sym->next;
    else {
	for (s = LocalSymList; s != NULL; s = s->next) {
	    if (s->next == sym) {
		s->next = sym->next;
		break;
	    }
	}
    }
    
    /* There are two scenarios which could make this check succeed:
       a) this sym is in the GlobalSymList as a LOCAL_SYM symbol
       b) there is another symbol as a non-LOCAL_SYM in the GlobalSymList
       Both are errors, without question.
       We currently just print this warning, but we should error out the
       parsing process. */
    s = LookupSymbol(sym->name);
    if (sym == s) {
        /* case a)
           just make this symbol a GLOBAL_SYM symbol and return */
        fprintf(stderr,
                "nedit: To boldly go where no local sym has gone before: %s\n",
                sym->name);
        sym->type = GLOBAL_SYM;
        return sym;
    } else if (NULL != s) {
        /* case b)
           sym will shadow the old symbol from the GlobalSymList */
        fprintf(stderr,
                "nedit: duplicate symbol in LocalSymList and GlobalSymList: %s\n",
                sym->name);
    }

    /* Add the symbol directly to the GlobalSymList, because InstallSymbol()
       will allocate a new Symbol, which results in a memory leak of sym.
       Don't use MACRO_FUNCTION_SYM as type, because in
       macro.c:readCheckMacroString() we use ProgramFree() for the .val.prog,
       but this symbol has no program attached and ProgramFree() is not NULL
       pointer safe */
    sym->type = GLOBAL_SYM;
    sym->next = GlobalSymList;
    GlobalSymList = sym;

    return sym;
}

/*
** Allocate memory for a string, and keep track of it, such that it
** can be recovered later using GarbageCollectStrings.  (A linked list
** of pointers is maintained by threading through the memory behind
** the returned pointers).  Length does not include the terminating null
** character, so to allocate space for a string of strlen == n, you must
** use AllocString(n+1).
*/

/*#define TRACK_GARBAGE_LEAKS*/
#ifdef TRACK_GARBAGE_LEAKS
static int numAllocatedStrings = 0;
static int numAllocatedSparseArrayElements = 0;
#endif

/* Allocate a new string buffer of length chars */
char *AllocString(int length)
{
    char *mem;
    
    mem = (char*)NEditMalloc(length + sizeof(char *) + 1);
    *((char **)mem) = AllocatedStrings;
    AllocatedStrings = mem;
#ifdef TRACK_GARBAGE_LEAKS
    ++numAllocatedStrings;
#endif
    return mem + sizeof(char *) + 1;
}

/* 
 * Allocate a new NString buffer of length chars (terminating \0 included), 
 * The buffer length is initialized to length-1 and the terminating \0 is 
 * filled in. 
 */
int AllocNString(NString *string, int length)
{
    char *mem;
    
    mem = (char*)NEditMalloc(length + sizeof(char *) + 1);
    if (!mem) {
        string->rep = 0;
        string->len = 0;
        return False;
    }
      
    *((char **)mem) = AllocatedStrings;
    AllocatedStrings = mem;
#ifdef TRACK_GARBAGE_LEAKS
    ++numAllocatedStrings;
#endif
    string->rep = mem + sizeof(char *) + 1;
    string->rep[length-1] = '\0';                /* forced \0 */
    string->len = length-1;
    return True;
}

/* Allocate a new string buffer of length chars, and copy in the string s */
char *AllocStringNCpy(const char *s, int length)
{
    char *p = AllocString(length + 1);  /* add extra char for forced \0 */
    if (!p)
        return p;
    if (!s)
        s = "";
    p[length] = '\0';                   /* forced \0 */
    return strncpy(p, s, length);
}

/* 
 * Allocate a new NString buffer of length chars (terminating \0 NOT included),
 * and copy at most length characters of the given string.
 * The buffer length is properly set and the buffer is guaranteed to be 
 * \0-terminated.
 */
int AllocNStringNCpy(NString *string, const char *s, int length)
{
    if (!AllocNString(string, length + 1)) /* add extra char for forced \0 */
      return False;
    if (!s)
        s = "";
    strncpy(string->rep, s, length);
    string->len = strlen(string->rep); /* re-calculate! */
    return True;
}

/* Allocate a new copy of string s */
char *AllocStringCpy(const char *s)
{
    return AllocStringNCpy(s, s ? strlen(s) : 0);
}

/* 
 * Allocate a new NString buffer, containing a copy of the given string.
 * The length is set to the length of the string and resulting string is
 * guaranteed to be \0-terminated.
 */
int AllocNStringCpy(NString *string, const char *s)
{
    size_t length = s ? strlen(s) : 0;
    if (!AllocNString(string, length + 1))
        return False;
    if (s)
        strncpy(string->rep, s, length);
    return True;
}

static SparseArrayEntry *allocateSparseArrayEntry(void)
{
    SparseArrayEntryWrapper *mem;

    mem = (SparseArrayEntryWrapper *)NEditMalloc(sizeof(SparseArrayEntryWrapper));
    mem->next = AllocatedSparseArrayEntries;
    AllocatedSparseArrayEntries = mem;
#ifdef TRACK_GARBAGE_LEAKS
    ++numAllocatedSparseArrayElements;
#endif
    return(&(mem->data));
}

static void MarkArrayContentsAsUsed(SparseArrayEntry *arrayPtr)
{
    SparseArrayEntry *globalSEUse;

    if (arrayPtr) {
        ((SparseArrayEntryWrapper *)arrayPtr)->inUse = 1;
        for (globalSEUse = (SparseArrayEntry *)rbTreeBegin((rbTreeNode *)arrayPtr);
            globalSEUse != NULL;
            globalSEUse = (SparseArrayEntry *)rbTreeNext((rbTreeNode *)globalSEUse)) {

            ((SparseArrayEntryWrapper *)globalSEUse)->inUse = 1;
            /* test first because it may be read-only static string */
            if (!(*(globalSEUse->key - 1))) {
                *(globalSEUse->key - 1) = 1;
            }
            if (globalSEUse->value.tag == STRING_TAG) {
                /* test first because it may be read-only static string */
                if (!(*(globalSEUse->value.val.str.rep - 1))) {
                    *(globalSEUse->value.val.str.rep - 1) = 1;
                }
            }
            else if (globalSEUse->value.tag == ARRAY_TAG) {
                MarkArrayContentsAsUsed(globalSEUse->value.val.arrayPtr);
            }
        }
    }
}

/*
** Collect strings that are no longer referenced from the global symbol
** list.  THIS CAN NOT BE RUN WHILE ANY MACROS ARE EXECUTING.  It must
** only be run after all macro activity has ceased.
*/

void GarbageCollectStrings(void)
{
    SparseArrayEntryWrapper *nextAP, *thisAP;
    char *p, *next;
    Symbol *s;

    /* mark all strings as unreferenced */
    for (p = AllocatedStrings; p != NULL; p = *((char **)p)) {
    	*(p + sizeof(char *)) = 0;
    }
    
    for (thisAP = AllocatedSparseArrayEntries;
        thisAP != NULL; thisAP = thisAP->next) {
        thisAP->inUse = 0;
    }

    /* Sweep the global symbol list, marking which strings are still
       referenced */
    for (s = GlobalSymList; s != NULL; s = s->next) {
    	if (s->value.tag == STRING_TAG) {
            /* test first because it may be read-only static string */
            if (!(*(s->value.val.str.rep - 1))) {
    	        *(s->value.val.str.rep - 1) = 1;
            }
        }
        else if (s->value.tag == ARRAY_TAG) {
            MarkArrayContentsAsUsed(s->value.val.arrayPtr);
        }
    }

    /* Collect all of the strings which remain unreferenced */
    next = AllocatedStrings;
    AllocatedStrings = NULL;
    while (next != NULL) {
    	p = next;
    	next = *((char **)p);
    	if (*(p + sizeof(char *)) != 0) {
    	    *((char **)p) = AllocatedStrings;
    	    AllocatedStrings = p;
    	}
        else {
#ifdef TRACK_GARBAGE_LEAKS
            --numAllocatedStrings;
#endif
    	    NEditFree(p);
    	}
    }
    
    nextAP = AllocatedSparseArrayEntries;
    AllocatedSparseArrayEntries = NULL;
    while (nextAP != NULL) {
        thisAP = nextAP;
        nextAP = nextAP->next;
        if (thisAP->inUse != 0) {
            thisAP->next = AllocatedSparseArrayEntries;
            AllocatedSparseArrayEntries = thisAP;
        }
        else {
#ifdef TRACK_GARBAGE_LEAKS
            --numAllocatedSparseArrayElements;
#endif
            NEditFree(thisAP);
        }
    }

#ifdef TRACK_GARBAGE_LEAKS
    printf("str count = %d\nary count = %d\n", numAllocatedStrings, numAllocatedSparseArrayElements);
#endif
}

/*
** Save and restore execution context to data structure "context"
*/
static void saveContext(RestartData *context)
{
    context->stack = TheStack;
    context->stackP = StackP;
    context->frameP = FrameP;
    context->pc = PC;
    context->runWindow = InitiatingWindow;
    context->focusWindow = FocusWindow;
}

static void restoreContext(RestartData *context)
{
    TheStack = context->stack;
    StackP = context->stackP;
    FrameP = context->frameP;
    PC = context->pc;
    InitiatingWindow = context->runWindow;
    FocusWindow = context->focusWindow;
}

static void freeSymbolTable(Symbol *symTab)
{
    Symbol *s;
    
    while(symTab != NULL) {
    	s = symTab;
    	free(s->name);
    	symTab = s->next;
    	NEditFree(s);
    }    
}

#define POP(dataVal) \
    if (StackP == TheStack) \
	return execError(StackUnderflowMsg, ""); \
    dataVal = *--StackP;
   
#define PUSH(dataVal) \
    if (StackP >= &TheStack[STACK_SIZE]) \
    	return execError(StackOverflowMsg, ""); \
    *StackP++ = dataVal;

#define PEEK(dataVal, peekIndex) \
    dataVal = *(StackP - peekIndex - 1);

#define POP_INT(number) \
    if (StackP == TheStack) \
	return execError(StackUnderflowMsg, ""); \
    --StackP; \
    if (StackP->tag == STRING_TAG) { \
    	if (!StringToNum(StackP->val.str.rep, &number)) \
    	    return execError(StringToNumberMsg, ""); \
    } else if (StackP->tag == INT_TAG) \
        number = StackP->val.n; \
    else \
        return(execError("can't convert array to integer", NULL));

#define POP_STRING(string) \
    if (StackP == TheStack) \
	return execError(StackUnderflowMsg, ""); \
    --StackP; \
    if (StackP->tag == INT_TAG) { \
    	string = AllocString(TYPE_INT_STR_SIZE(int)); \
    	sprintf(string, "%d", StackP->val.n); \
    } else if (StackP->tag == STRING_TAG) \
        string = StackP->val.str.rep; \
    else \
        return(execError("can't convert array to string", NULL));
   
#define PEEK_STRING(string, peekIndex) \
    if ((StackP - peekIndex - 1)->tag == INT_TAG) { \
        string = AllocString(TYPE_INT_STR_SIZE(int)); \
        sprintf(string, "%d", (StackP - peekIndex - 1)->val.n); \
    } \
    else if ((StackP - peekIndex - 1)->tag == STRING_TAG) { \
        string = (StackP - peekIndex - 1)->val.str.rep; \
    } \
    else { \
        return(execError("can't convert array to string", NULL)); \
    }

#define PEEK_INT(number, peekIndex) \
    if ((StackP - peekIndex - 1)->tag == STRING_TAG) { \
        if (!StringToNum((StackP - peekIndex - 1)->val.str.rep, &number)) { \
    	    return execError(StringToNumberMsg, ""); \
        } \
    } else if ((StackP - peekIndex - 1)->tag == INT_TAG) { \
        number = (StackP - peekIndex - 1)->val.n; \
    } \
    else { \
        return(execError("can't convert array to string", NULL)); \
    }

#define PUSH_INT(number) \
    if (StackP >= &TheStack[STACK_SIZE]) \
    	return execError(StackOverflowMsg, ""); \
    StackP->tag = INT_TAG; \
    StackP->val.n = number; \
    StackP++;
    
#define PUSH_STRING(string, length) \
    if (StackP >= &TheStack[STACK_SIZE]) \
    	return execError(StackOverflowMsg, ""); \
    StackP->tag = STRING_TAG; \
    StackP->val.str.rep = string; \
    StackP->val.str.len = length; \
    StackP++;

#define BINARY_NUMERIC_OPERATION(operator) \
    int n1, n2; \
    DISASM_RT(PC-1, 1); \
    STACKDUMP(2, 3); \
    POP_INT(n2) \
    POP_INT(n1) \
    PUSH_INT(n1 operator n2) \
    return STAT_OK;

#define UNARY_NUMERIC_OPERATION(operator) \
    int n; \
    DISASM_RT(PC-1, 1); \
    STACKDUMP(1, 3); \
    POP_INT(n) \
    PUSH_INT(operator n) \
    return STAT_OK;

/*
** copy a symbol's value onto the stack
** Before: Prog->  [Sym], next, ...
**         TheStack-> next, ...
** After:  Prog->  Sym, [next], ...
**         TheStack-> [symVal], next, ...
*/
static int pushSymVal(void)
{
    Symbol *s;
    int nArgs, argNum;
    DataValue symVal;

    DISASM_RT(PC-1, 2);
    STACKDUMP(0, 3);

    s = PC->sym;
    PC++;

    if (s->type == LOCAL_SYM) {
        symVal = FP_GET_SYM_VAL(FrameP, s);
    } else if (s->type == GLOBAL_SYM || s->type == CONST_SYM) {
        symVal = s->value;
    } else if (s->type == ARG_SYM) {
    	nArgs = FP_GET_ARG_COUNT(FrameP);
    	argNum = s->value.val.n;
    	if (argNum >= nArgs) {
    	    return execError("referenced undefined argument: %s",  s->name);
        }
    	if (argNum == N_ARGS_ARG_SYM) {
            symVal.tag = INT_TAG;
            symVal.val.n = nArgs;
    	}
        else {
            symVal = FP_GET_ARG_N(FrameP, argNum);
        }
    } else if (s->type == PROC_VALUE_SYM) {
	char *errMsg;
	if (!(s->value.val.subr)(FocusWindow, NULL, 0,
	    	&symVal, &errMsg)) {
	    return execError(errMsg, s->name);
        }
    } else
    	return execError("reading non-variable: %s", s->name);
    if (symVal.tag == NO_TAG) {
    	return execError("variable not set: %s", s->name);
    }

    PUSH(symVal)

    return STAT_OK;
}

static int pushArgVal(void)
{
    int nArgs, argNum;

    DISASM_RT(PC-1, 1);
    STACKDUMP(1, 3);

    POP_INT(argNum)
    --argNum;
    nArgs = FP_GET_ARG_COUNT(FrameP);
    if (argNum >= nArgs || argNum < 0) {
        char argStr[TYPE_INT_STR_SIZE(argNum)];
        sprintf(argStr, "%d", argNum + 1);
    	return execError("referenced undefined argument: $args[%s]", argStr);
    }
    PUSH(FP_GET_ARG_N(FrameP, argNum));
    return STAT_OK;
}

static int pushArgCount(void)
{
    DISASM_RT(PC-1, 1);
    STACKDUMP(0, 3);

    PUSH_INT(FP_GET_ARG_COUNT(FrameP));
    return STAT_OK;
}

static int pushArgArray(void)
{
    int nArgs, argNum;
    DataValue argVal, *resultArray;

    DISASM_RT(PC-1, 1);
    STACKDUMP(0, 3);

    nArgs = FP_GET_ARG_COUNT(FrameP);
    resultArray = &FP_GET_ARG_ARRAY_CACHE(FrameP);
    if (resultArray->tag != ARRAY_TAG) {
        resultArray->tag = ARRAY_TAG;
        resultArray->val.arrayPtr = ArrayNew();

        for (argNum = 0; argNum < nArgs; ++argNum) {
            char intStr[TYPE_INT_STR_SIZE(argNum)];

            sprintf(intStr, "%d", argNum + 1);
            argVal = FP_GET_ARG_N(FrameP, argNum);
            if (!ArrayInsert(resultArray, AllocStringCpy(intStr), &argVal)) {
                return(execError("array insertion failure", NULL));
            }
        }
    }
    PUSH(*resultArray);
    return STAT_OK;
}

/*
** Push an array (by reference) onto the stack
** Before: Prog->  [ArraySym], makeEmpty, next, ...
**         TheStack-> next, ...
** After:  Prog->  ArraySym, makeEmpty, [next], ...
**         TheStack-> [elemValue], next, ...
** makeEmpty is either true (1) or false (0): if true, and the element is not
** present in the array, create it.
*/
static int pushArraySymVal(void)
{
    Symbol *sym;
    DataValue *dataPtr;
    int initEmpty;
    
    DISASM_RT(PC-1, 3);
    STACKDUMP(0, 3);

    sym = PC->sym;
    PC++;
    initEmpty = PC->value;
    PC++;
    
    if (sym->type == LOCAL_SYM) {
    	dataPtr = &FP_GET_SYM_VAL(FrameP, sym);
    }
    else if (sym->type == GLOBAL_SYM) {
    	dataPtr = &sym->value;
    }
    else {
    	return execError("assigning to non-lvalue array or non-array: %s", sym->name);
    }

    if (initEmpty && dataPtr->tag == NO_TAG) {
        dataPtr->tag = ARRAY_TAG;
        dataPtr->val.arrayPtr = ArrayNew();
    }

    if (dataPtr->tag == NO_TAG) {
        return execError("variable not set: %s", sym->name);
    }

    PUSH(*dataPtr)

    return STAT_OK;
}

/*
** assign top value to next symbol
**
** Before: Prog->  [symbol], next, ...
**         TheStack-> [value], next, ...
** After:  Prog->  symbol, [next], ...
**         TheStack-> next, ...
*/
static int assign(void)
{
    Symbol *sym;
    DataValue *dataPtr;
    DataValue value;
    
    DISASM_RT(PC-1, 2);
    STACKDUMP(1, 3);

    sym = PC->sym;
    PC++;

    if (sym->type != GLOBAL_SYM && sym->type != LOCAL_SYM) {
        if (sym->type == ARG_SYM) {
            return execError("assignment to function argument: %s",  sym->name);
        }
        else if (sym->type == PROC_VALUE_SYM) {
            return execError("assignment to read-only variable: %s", sym->name);
        }
        else {
            return execError("assignment to non-variable: %s", sym->name);
        }
    }

    if (sym->type == LOCAL_SYM) {
        dataPtr = &FP_GET_SYM_VAL(FrameP, sym);
    }
    else {
        dataPtr = &sym->value;
    }

    POP(value)

    if (value.tag == ARRAY_TAG) {
       return ArrayCopy(dataPtr, &value);
    }

    *dataPtr = value;
    return STAT_OK;
}

/*
** copy the top value of the stack
** Before: TheStack-> value, next, ...
** After:  TheStack-> value, value, next, ...
*/
static int dupStack(void)
{
    DataValue value;

    DISASM_RT(PC-1, 1);
    STACKDUMP(1, 3);

    PEEK(value, 0)
    PUSH(value)

    return STAT_OK;
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which all the keys from both the right and left are copied
** the values from the right array are used in the result array when the
** keys are the same
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int add(void)
{
    DataValue leftVal, rightVal, resultArray;
    int n1, n2;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    PEEK(rightVal, 0)
    if (rightVal.tag == ARRAY_TAG) {
        PEEK(leftVal, 1)
        if (leftVal.tag == ARRAY_TAG) {
            SparseArrayEntry *leftIter, *rightIter;
            resultArray.tag = ARRAY_TAG;
            resultArray.val.arrayPtr = ArrayNew();
            
            POP(rightVal)
            POP(leftVal)
            leftIter = arrayIterateFirst(&leftVal);
            rightIter = arrayIterateFirst(&rightVal);
            while (leftIter || rightIter) {
                Boolean insertResult = True;
                
                if (leftIter && rightIter) {
                    int compareResult = arrayEntryCompare((rbTreeNode *)leftIter, (rbTreeNode *)rightIter);
                    if (compareResult < 0) {
                        insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                        leftIter = arrayIterateNext(leftIter);
                    }
                    else if (compareResult > 0) {
                        insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                        rightIter = arrayIterateNext(rightIter);
                    }
                    else {
                        insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                        leftIter = arrayIterateNext(leftIter);
                        rightIter = arrayIterateNext(rightIter);
                    }
                }
                else if (leftIter) {
                    insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                    leftIter = arrayIterateNext(leftIter);
                }
                else {
                    insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                    rightIter = arrayIterateNext(rightIter);
                }
                if (!insertResult) {
                    return(execError("array insertion failure", NULL));
                }
            }
            PUSH(resultArray)
        }
        else {
            return(execError("can't mix math with arrays and non-arrays", NULL));
        }
    }
    else {
        POP_INT(n2)
        POP_INT(n1)
        PUSH_INT(n1 + n2)
    }
    return(STAT_OK);
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in the left array but not in the right
** are copied
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int subtract(void)
{
    DataValue leftVal, rightVal, resultArray;
    int n1, n2;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    PEEK(rightVal, 0)
    if (rightVal.tag == ARRAY_TAG) {
        PEEK(leftVal, 1)
        if (leftVal.tag == ARRAY_TAG) {
            SparseArrayEntry *leftIter, *rightIter;
            resultArray.tag = ARRAY_TAG;
            resultArray.val.arrayPtr = ArrayNew();
            
            POP(rightVal)
            POP(leftVal)
            leftIter = arrayIterateFirst(&leftVal);
            rightIter = arrayIterateFirst(&rightVal);
            while (leftIter) {
                Boolean insertResult = True;
                
                if (leftIter && rightIter) {
                    int compareResult = arrayEntryCompare((rbTreeNode *)leftIter, (rbTreeNode *)rightIter);
                    if (compareResult < 0) {
                        insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                        leftIter = arrayIterateNext(leftIter);
                    }
                    else if (compareResult > 0) {
                        rightIter = arrayIterateNext(rightIter);
                    }
                    else {
                        leftIter = arrayIterateNext(leftIter);
                        rightIter = arrayIterateNext(rightIter);
                    }
                }
                else if (leftIter) {
                    insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                    leftIter = arrayIterateNext(leftIter);
                }
                if (!insertResult) {
                    return(execError("array insertion failure", NULL));
                }
            }
            PUSH(resultArray)
        }
        else {
            return(execError("can't mix math with arrays and non-arrays", NULL));
        }
    }
    else {
        POP_INT(n2)
        POP_INT(n1)
        PUSH_INT(n1 - n2)
    }
    return(STAT_OK);
}

/*
** Other binary operators
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
**
** Other unary operators
** Before: TheStack-> value, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int multiply(void)
{
    BINARY_NUMERIC_OPERATION(*)
}

static int divide(void)
{
    int n1, n2;

    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP_INT(n2)
    POP_INT(n1)
    if (n2 == 0) {
	return execError("division by zero", "");
    }
    PUSH_INT(n1 / n2)
    return STAT_OK;
}

static int modulo(void)
{
    int n1, n2;

    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP_INT(n2)
    POP_INT(n1)
    if (n2 == 0) {
	return execError("modulo by zero", "");
    }
    PUSH_INT(n1 % n2)
    return STAT_OK;
}

static int negate(void)
{
    UNARY_NUMERIC_OPERATION(-)
}

static int increment(void)
{
    UNARY_NUMERIC_OPERATION(++)
}

static int decrement(void)
{
    UNARY_NUMERIC_OPERATION(--)
}

static int gt(void)
{
    BINARY_NUMERIC_OPERATION(>)
}

static int lt(void)
{
    BINARY_NUMERIC_OPERATION(<)
}

static int ge(void)
{
    BINARY_NUMERIC_OPERATION(>=)
}

static int le(void)
{
    BINARY_NUMERIC_OPERATION(<=)
}

/*
** verify that compares are between integers and/or strings only
** Before: TheStack-> value1, value2, next, ...
** After:  TheStack-> resValue, next, ...
** where resValue is 1 for true, 0 for false
*/
static int eq(void)
{
    DataValue v1, v2;

    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP(v1)
    POP(v2)
    if (v1.tag == INT_TAG && v2.tag == INT_TAG) {
        v1.val.n = v1.val.n == v2.val.n;
    }
    else if (v1.tag == STRING_TAG && v2.tag == STRING_TAG) {
        v1.val.n = !strcmp(v1.val.str.rep, v2.val.str.rep);
    }
    else if (v1.tag == STRING_TAG && v2.tag == INT_TAG) {
        int number;
        if (!StringToNum(v1.val.str.rep, &number)) {
            v1.val.n = 0;
        }
        else {
            v1.val.n = number == v2.val.n;
        }
    }
    else if (v2.tag == STRING_TAG && v1.tag == INT_TAG) {
        int number;
        if (!StringToNum(v2.val.str.rep, &number)) {
            v1.val.n = 0;
        }
        else {
            v1.val.n = number == v1.val.n;
        }
    }
    else {
        return(execError("incompatible types to compare", NULL));
    }
    v1.tag = INT_TAG;
    PUSH(v1)
    return(STAT_OK);
}

/* negated eq() call */
static int ne(void)
{
    eq();
    return not();
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in both the right or left are copied
** the values from the right array are used in the result array
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int bitAnd(void)
{ 
    DataValue leftVal, rightVal, resultArray;
    int n1, n2;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    PEEK(rightVal, 0)
    if (rightVal.tag == ARRAY_TAG) {
        PEEK(leftVal, 1)
        if (leftVal.tag == ARRAY_TAG) {
            SparseArrayEntry *leftIter, *rightIter;
            resultArray.tag = ARRAY_TAG;
            resultArray.val.arrayPtr = ArrayNew();
            
            POP(rightVal)
            POP(leftVal)
            leftIter = arrayIterateFirst(&leftVal);
            rightIter = arrayIterateFirst(&rightVal);
            while (leftIter && rightIter) {
                Boolean insertResult = True;
                int compareResult = arrayEntryCompare((rbTreeNode *)leftIter, (rbTreeNode *)rightIter);

                if (compareResult < 0) {
                    leftIter = arrayIterateNext(leftIter);
                }
                else if (compareResult > 0) {
                    rightIter = arrayIterateNext(rightIter);
                }
                else {
                    insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                    leftIter = arrayIterateNext(leftIter);
                    rightIter = arrayIterateNext(rightIter);
                }
                if (!insertResult) {
                    return(execError("array insertion failure", NULL));
                }
            }
            PUSH(resultArray)
        }
        else {
            return(execError("can't mix math with arrays and non-arrays", NULL));
        }
    }
    else {
        POP_INT(n2)
        POP_INT(n1)
        PUSH_INT(n1 & n2)
    }
    return(STAT_OK);
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in either the right or left but not both
** are copied
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int bitOr(void)
{ 
    DataValue leftVal, rightVal, resultArray;
    int n1, n2;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    PEEK(rightVal, 0)
    if (rightVal.tag == ARRAY_TAG) {
        PEEK(leftVal, 1)
        if (leftVal.tag == ARRAY_TAG) {
            SparseArrayEntry *leftIter, *rightIter;
            resultArray.tag = ARRAY_TAG;
            resultArray.val.arrayPtr = ArrayNew();
            
            POP(rightVal)
            POP(leftVal)
            leftIter = arrayIterateFirst(&leftVal);
            rightIter = arrayIterateFirst(&rightVal);
            while (leftIter || rightIter) {
                Boolean insertResult = True;
                
                if (leftIter && rightIter) {
                    int compareResult = arrayEntryCompare((rbTreeNode *)leftIter, (rbTreeNode *)rightIter);
                    if (compareResult < 0) {
                        insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                        leftIter = arrayIterateNext(leftIter);
                    }
                    else if (compareResult > 0) {
                        insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                        rightIter = arrayIterateNext(rightIter);
                    }
                    else {
                        leftIter = arrayIterateNext(leftIter);
                        rightIter = arrayIterateNext(rightIter);
                    }
                }
                else if (leftIter) {
                    insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
                    leftIter = arrayIterateNext(leftIter);
                }
                else {
                    insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
                    rightIter = arrayIterateNext(rightIter);
                }
                if (!insertResult) {
                    return(execError("array insertion failure", NULL));
                }
            }
            PUSH(resultArray)
        }
        else {
            return(execError("can't mix math with arrays and non-arrays", NULL));
        }
    }
    else {
        POP_INT(n2)
        POP_INT(n1)
        PUSH_INT(n1 | n2)
    }
    return(STAT_OK);
}

static int and(void)
{ 
    BINARY_NUMERIC_OPERATION(&&)
}

static int or(void)
{
    BINARY_NUMERIC_OPERATION(||)
}
    
static int not(void)
{
    UNARY_NUMERIC_OPERATION(!)
}

/*
** raise one number to the power of another
** Before: TheStack-> raisedBy, number, next, ...
** After:  TheStack-> result, next, ...
*/
static int power(void)
{
    int n1, n2, n3;

    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP_INT(n2)
    POP_INT(n1)
    /*  We need to round to deal with pow() giving results slightly above
        or below the real result since it deals with floating point numbers.
        Note: We're not really wanting rounded results, we merely
        want to deal with this simple issue. So, 2^-2 = .5, but we
        don't want to round this to 1. This is mainly intended to deal with
        4^2 = 15.999996 and 16.000001.
    */
    if (n2 < 0 && n1 != 1 && n1 != -1) {
        if (n1 != 0) {
            /* since we're integer only, nearly all negative exponents result in 0 */
            n3 = 0;
        }
        else {
            /* allow error to occur */
            n3 = (int)pow((double)n1, (double)n2);
        }
    }
    else {
        if ((n1 < 0) && (n2 & 1)) {
            /* round to nearest integer for negative values*/
            n3 = (int)(pow((double)n1, (double)n2) - (double)0.5);
        }
        else {
            /* round to nearest integer for positive values*/
            n3 = (int)(pow((double)n1, (double)n2) + (double)0.5);
        }
    }
    PUSH_INT(n3)
    return errCheck("exponentiation");
}

/*
** concatenate two top items on the stack
** Before: TheStack-> str2, str1, next, ...
** After:  TheStack-> result, next, ...
*/
static int concat(void)
{
    char *s1, *s2, *out;
    int len1, len2;

    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP_STRING(s2)
    POP_STRING(s1)
    len1 = strlen(s1);
    len2 = strlen(s2);
    out = AllocString(len1 + len2 + 1);
    strncpy(out, s1, len1);
    strcpy(&out[len1], s2);
    PUSH_STRING(out, len1 + len2)
    return STAT_OK;
}

/*
** Call a subroutine or function (user defined or built-in).  Args are the
** subroutine's symbol, and the number of arguments which have been pushed
** on the stack.
**
** For a macro subroutine, the return address, frame pointer, number of
** arguments and space for local variables are added to the stack, and the
** PC is set to point to the new function. For a built-in routine, the
** arguments are popped off the stack, and the routine is just called.
**
** Before: Prog->  [subrSym], nArgs, next, ...
**         TheStack-> argN-arg1, next, ...
** After:  Prog->  next, ...            -- (built-in called subr)
**         TheStack-> retVal?, next, ...
**    or:  Prog->  (in called)next, ... -- (macro code called subr)
**         TheStack-> symN-sym1(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
*/
static int callSubroutine(void)
{
    Symbol *sym, *s;
    int i, nArgs;
    static DataValue noValue = {NO_TAG, {0}};
    Program *prog;
    char *errMsg;
    
    sym = PC->sym;
    PC++;
    nArgs = PC->value;
    PC++;
    
    DISASM_RT(PC-3, 3);
    STACKDUMP(nArgs, 3);

    /*
    ** If the subroutine is built-in, call the built-in routine
    */
    if (sym->type == C_FUNCTION_SYM) {
    	DataValue result;

        /* "pop" stack back to the first argument in the call stack */
    	StackP -= nArgs;

    	/* Call the function and check for preemption */
    	PreemptRequest = False;
	if (!sym->value.val.subr(FocusWindow, StackP,
	    	nArgs, &result, &errMsg))
	    return execError(errMsg, sym->name);
    	if (PC->func == fetchRetVal) {
    	    if (result.tag == NO_TAG) {
    	    	return execError("%s does not return a value", sym->name);
            }
    	    PUSH(result);
	    PC++;
    	}
    	return PreemptRequest ? STAT_PREEMPT : STAT_OK;
    }
    
    /*
    ** Call a macro subroutine:
    **
    ** Push all of the required information to resume, and make space on the
    ** stack for local variables (and initialize them), on top of the argument
    ** values which are already there.
    */
    if (sym->type == MACRO_FUNCTION_SYM) {
    	StackP->tag = NO_TAG; /* return PC */
    	StackP->val.inst = PC;
    	StackP++;
        
    	StackP->tag = NO_TAG; /* old FrameP */
    	StackP->val.dataval = FrameP;
    	StackP++;
        
    	StackP->tag = NO_TAG; /* nArgs */
    	StackP->val.n = nArgs;
    	StackP++;
        
        *(StackP++) = noValue; /* cached arg array */
        
    	FrameP = StackP;
    	prog = sym->value.val.prog;
    	PC = prog->code;
	for (s = prog->localSymList; s != NULL; s = s->next) {
	    FP_GET_SYM_VAL(FrameP, s) = noValue;
	    StackP++;
	}
   	return STAT_OK;
    }
    
    /*
    ** Call an action routine
    */
    if (sym->type == ACTION_ROUTINE_SYM) {
        String *argList;
    	Cardinal numArgs = nArgs;
    	XKeyEvent key_event;
	Display *disp;
	Window win;
    
	/* Create a fake event with a timestamp suitable for actions which need
	   timestamps, a marker to indicate that the call was from a macro
	   (to stop shell commands from putting up their own separate banner) */
        disp=XtDisplay(InitiatingWindow->shell);
	win=XtWindow(InitiatingWindow->shell);

	key_event.type = KeyPress;
	key_event.send_event = MACRO_EVENT_MARKER;
	key_event.time=XtLastTimestampProcessed(XtDisplay(InitiatingWindow->shell));
	
	/* The following entries are just filled in to avoid problems
	   in strange cases, like calling "self_insert()" directly from the
	   macro menu. In fact the display was sufficient to cure this crash. */
        key_event.display=disp;
        key_event.window=key_event.root=key_event.subwindow=win;
    
        argList = (String *)NEditCalloc(nArgs, sizeof(*argList));
	/* pop arguments off the stack and put them in the argument list */
	for (i=nArgs-1; i>=0; i--) {
    	    POP_STRING(argList[i])
	}

    	/* Call the action routine and check for preemption */
    	PreemptRequest = False;
    	sym->value.val.xtproc(FocusWindow->lastFocus,
    	    	(XEvent *)&key_event, argList, &numArgs);
        NEditFree(argList);
    	if (PC->func == fetchRetVal) {
    	    return execError("%s does not return a value", sym->name);
        }
    	return PreemptRequest ? STAT_PREEMPT : STAT_OK;
    }

    /* Calling a non subroutine symbol */
    return execError("%s is not a function or subroutine", sym->name);
}

/*
** This should never be executed, returnVal checks for the presence of this
** instruction at the PC to decide whether to push the function's return
** value, then skips over it without executing.
*/
static int fetchRetVal(void)
{
    return execError("internal error: frv", NULL);
}

/* see comments for returnValOrNone() */
static int returnNoVal(void)
{
    return returnValOrNone(False);
}
static int returnVal(void)
{
    return returnValOrNone(True);
}

/*
** Return from a subroutine call
** Before: Prog->  [next], ...
**         TheStack-> retVal?, ...(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
** After:  Prog->  next, ..., (in caller)[FETCH_RET_VAL?], ...
**         TheStack-> retVal?, next, ...
*/
static int returnValOrNone(int valOnStack)
{
    DataValue retVal;
    static DataValue noValue = {NO_TAG, {0}};
    DataValue *newFrameP;
    int nArgs;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(StackP - FrameP + FP_GET_ARG_COUNT(FrameP) + FP_TO_ARGS_DIST, 3);

    /* return value is on the stack */
    if (valOnStack) {
    	POP(retVal);
    }
    
    /* get stored return information */
    nArgs = FP_GET_ARG_COUNT(FrameP);
    newFrameP = FP_GET_OLD_FP(FrameP);
    PC = FP_GET_RET_PC(FrameP);
    
    /* pop past local variables */
    StackP = FrameP;
    /* pop past function arguments */
    StackP -= (FP_TO_ARGS_DIST + nArgs);
    FrameP = newFrameP;
    
    /* push returned value, if requsted */
    if (PC == NULL) {
	if (valOnStack) {
    	    PUSH(retVal);
	} else {
	    PUSH(noValue);
	}
    } else if (PC->func == fetchRetVal) {
	if (valOnStack) {
    	    PUSH(retVal);
	    PC++;
	} else {
	    return execError(
	    	"using return value of %s which does not return a value",
	    	((PC-2)->sym->name));
	}
    }
    
    /* NULL return PC indicates end of program */
    return PC == NULL ? STAT_DONE : STAT_OK;
}

/*
** Unconditional branch offset by immediate operand
**
** Before: Prog->  [branchDest], next, ..., (branchdest)next
** After:  Prog->  branchDest, next, ..., (branchdest)[next]
*/
static int branch(void)
{
    DISASM_RT(PC-1, 2);
    STACKDUMP(0, 3);

    PC += PC->value;
    return STAT_OK;
}

/*
** Conditional branches if stack value is True/False (non-zero/0) to address
** of immediate operand (pops stack)
**
** Before: Prog->  [branchDest], next, ..., (branchdest)next
** After:  either: Prog->  branchDest, [next], ...
** After:  or:     Prog->  branchDest, next, ..., (branchdest)[next]
*/
static int branchTrue(void)
{
    int value;
    Inst *addr;
    
    DISASM_RT(PC-1, 2);
    STACKDUMP(1, 3);

    POP_INT(value)
    addr = PC + PC->value;
    PC++;
    
    if (value)
    	PC = addr;
    return STAT_OK;
}
static int branchFalse(void)
{
    int value;
    Inst *addr;
    
    DISASM_RT(PC-1, 2);
    STACKDUMP(1, 3);

    POP_INT(value)
    addr = PC + PC->value;
    PC++;
    
    if (!value)
    	PC = addr;
    return STAT_OK;
}

/*
** Ignore the address following the instruction and continue.  Why? So
** some code that uses conditional branching doesn't have to figure out
** whether to store a branch address.
**
** Before: Prog->  [branchDest], next, ...
** After:  Prog->  branchDest, [next], ...
*/
static int branchNever(void)
{
    DISASM_RT(PC-1, 2);
    STACKDUMP(0, 3);

    PC++;
    return STAT_OK;
}

/*
** recursively copy(duplicate) the sparse array nodes of an array
** this does not duplicate the key/node data since they are never
** modified, only replaced
*/
int ArrayCopy(DataValue *dstArray, DataValue *srcArray)
{
    SparseArrayEntry *srcIter;
    
    dstArray->tag = ARRAY_TAG;
    dstArray->val.arrayPtr = ArrayNew();
    
    srcIter = arrayIterateFirst(srcArray);
    while (srcIter) {
        if (srcIter->value.tag == ARRAY_TAG) {
            int errNum;
            DataValue tmpArray;
            
            errNum = ArrayCopy(&tmpArray, &srcIter->value);
            if (errNum != STAT_OK) {
                return(errNum);
            }
            if (!ArrayInsert(dstArray, srcIter->key, &tmpArray)) {
                return(execError("array copy failed", NULL));
            }
        }
        else {
            if (!ArrayInsert(dstArray, srcIter->key, &srcIter->value)) {
                return(execError("array copy failed", NULL));
            }
        }
        srcIter = arrayIterateNext(srcIter);
    }
    return(STAT_OK);
}

/*
** creates an allocated string of a single key for all the sub-scripts
** using ARRAY_DIM_SEP as a separator
** this function uses the PEEK macros in order to remove most limits on
** the number of arguments to an array
** I really need to optimize the size approximation rather than assuming
** a worst case size for every integer argument
*/
static int makeArrayKeyFromArgs(int nArgs, char **keyString, int leaveParams)
{
    DataValue tmpVal;
    int sepLen = strlen(ARRAY_DIM_SEP);
    int keyLength = 0;
    int i;

    keyLength = sepLen * (nArgs - 1);
    for (i = nArgs - 1; i >= 0; --i) {
        PEEK(tmpVal, i)
        if (tmpVal.tag == INT_TAG) {
            keyLength += TYPE_INT_STR_SIZE(tmpVal.val.n);
        }
        else if (tmpVal.tag == STRING_TAG) {
            keyLength += tmpVal.val.str.len;
        }
        else {
            return(execError("can only index array with string or int.", NULL));
        }
    }
    *keyString = AllocString(keyLength + 1);
    (*keyString)[0] = 0;
    for (i = nArgs - 1; i >= 0; --i) {
        if (i != nArgs - 1) {
            strcat(*keyString, ARRAY_DIM_SEP);
        }
        PEEK(tmpVal, i)
        if (tmpVal.tag == INT_TAG) {
            sprintf(&((*keyString)[strlen(*keyString)]), "%d", tmpVal.val.n);
        }
        else if (tmpVal.tag == STRING_TAG) {
            strcat(*keyString, tmpVal.val.str.rep);
        }
        else {
            return(execError("can only index array with string or int.", NULL));
        }
    }
    if (!leaveParams) {
        for (i = nArgs - 1; i >= 0; --i) {
            POP(tmpVal)
        }
    }
    return(STAT_OK);
}

/*
** allocate an empty array node, this is used as the root node and never
** contains any data, only refernces to other nodes
*/
static rbTreeNode *arrayEmptyAllocator(void)
{
    SparseArrayEntry *newNode = allocateSparseArrayEntry();
    if (newNode) {
        newNode->key = NULL;
        newNode->value.tag = NO_TAG;
    }
    return((rbTreeNode *)newNode);
}

/*
** create and copy array node and copy contents, we merely copy pointers
** since they are never modified, only replaced
*/
static rbTreeNode *arrayAllocateNode(rbTreeNode *src)
{
    SparseArrayEntry *newNode = allocateSparseArrayEntry();
    if (newNode) {
        newNode->key = ((SparseArrayEntry *)src)->key;
        newNode->value = ((SparseArrayEntry *)src)->value;
    }
    return((rbTreeNode *)newNode);
}

/*
** copy array node data, we merely copy pointers since they are never
** modified, only replaced
*/
static int arrayEntryCopyToNode(rbTreeNode *dst, rbTreeNode *src)
{
    ((SparseArrayEntry *)dst)->key = ((SparseArrayEntry *)src)->key;
    ((SparseArrayEntry *)dst)->value = ((SparseArrayEntry *)src)->value;
    return(1);
}

/*
** compare two array nodes returning an integer value similar to strcmp()
*/
static int arrayEntryCompare(rbTreeNode *left, rbTreeNode *right)
{
    return(strcmp(((SparseArrayEntry *)left)->key, ((SparseArrayEntry *)right)->key));
}

/*
** dispose an array node, garbage collection handles this, so we mark it
** to allow iterators in macro language to determine they have been unlinked
*/
static void arrayDisposeNode(rbTreeNode *src)
{
    /* Let garbage collection handle this but mark it so iterators can tell */
    src->left = NULL;
    src->right = NULL;
    src->parent = NULL;
    src->color = -1;
}

SparseArrayEntry *ArrayNew(void)
{
	return((SparseArrayEntry *)rbTreeNew(arrayEmptyAllocator));
}

/*
** insert a DataValue into an array, allocate the array if needed
** keyStr must be a string that was allocated with AllocString()
*/
Boolean ArrayInsert(DataValue* theArray, char* keyStr, DataValue* theValue)
{
    SparseArrayEntry tmpEntry;
    rbTreeNode *insertedNode;

    tmpEntry.key = keyStr;
    tmpEntry.value = *theValue;

    if (theArray->val.arrayPtr == NULL) {
        theArray->val.arrayPtr = ArrayNew();
    }

    if (theArray->val.arrayPtr != NULL) {
        insertedNode = rbTreeInsert((rbTreeNode*) (theArray->val.arrayPtr),
                (rbTreeNode *)&tmpEntry, arrayEntryCompare, arrayAllocateNode,
                arrayEntryCopyToNode);

        if (insertedNode) {
            return True;
        } else {
            return False;
        }
    }

    return False;
}

/*
** remove a node from an array whose key matches keyStr
*/
void ArrayDelete(DataValue *theArray, char *keyStr)
{
    SparseArrayEntry searchEntry;

    if (theArray->val.arrayPtr) {
        searchEntry.key = keyStr;
        rbTreeDelete((rbTreeNode *)theArray->val.arrayPtr, (rbTreeNode *)&searchEntry,
                    arrayEntryCompare, arrayDisposeNode);
    }
}

/*
** remove all nodes from an array
*/
void ArrayDeleteAll(DataValue *theArray)
{
    if (theArray->val.arrayPtr) {
        rbTreeNode *iter = rbTreeBegin((rbTreeNode *)theArray->val.arrayPtr);
        while (iter) {
            rbTreeNode *nextIter = rbTreeNext(iter);
            rbTreeDeleteNode((rbTreeNode *)theArray->val.arrayPtr,
                        iter, arrayDisposeNode);

            iter = nextIter;
        }
    }
}

/*
** returns the number of elements (nodes containing values) of an array
*/
unsigned ArraySize(DataValue* theArray)
{
    if (theArray->val.arrayPtr) {
        return rbTreeSize((rbTreeNode *)theArray->val.arrayPtr);
    } else {
        return 0;
    }
}

/*
** retrieves an array node whose key matches
** returns 1 for success 0 for not found
*/
Boolean ArrayGet(DataValue* theArray, char* keyStr, DataValue* theValue)
{
    SparseArrayEntry searchEntry;
    rbTreeNode *foundNode;

    if (theArray->val.arrayPtr) {
        searchEntry.key = keyStr;
        foundNode = rbTreeFind((rbTreeNode*) theArray->val.arrayPtr,
                (rbTreeNode*) &searchEntry, arrayEntryCompare);
        if (foundNode) {
            *theValue = ((SparseArrayEntry*) foundNode)->value;
            return True;
        }
    }

    return False;
}

/*
** get pointer to start iterating an array
*/
SparseArrayEntry *arrayIterateFirst(DataValue *theArray)
{
    SparseArrayEntry *startPos;
    if (theArray->val.arrayPtr) {
        startPos = (SparseArrayEntry *)rbTreeBegin((rbTreeNode *)theArray->val.arrayPtr);
    }
    else {
        startPos = NULL;
    }
    return(startPos);
}

/*
** move iterator to next entry in array
*/
SparseArrayEntry *arrayIterateNext(SparseArrayEntry *iterator)
{
    SparseArrayEntry *nextPos;
    if (iterator) {
        nextPos = (SparseArrayEntry *)rbTreeNext((rbTreeNode *)iterator);
    }
    else {
        nextPos = NULL;
    }
    return(nextPos);
}

/*
** evaluate an array element and push the result onto the stack
**
** Before: Prog->  [nDim], next, ...
**         TheStack-> indnDim, ... ind1, ArraySym, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> indexedArrayVal, next, ...
*/
static int arrayRef(void)
{
    int errNum;
    DataValue srcArray, valueItem;
    char *keyString = NULL;
    int nDim;
    
    nDim = PC->value;
    PC++;

    DISASM_RT(PC-2, 2);
    STACKDUMP(nDim, 3);

    if (nDim > 0) {
        errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
        if (errNum != STAT_OK) {
            return(errNum);
        }

        POP(srcArray)
        if (srcArray.tag == ARRAY_TAG) {
            if (!ArrayGet(&srcArray, keyString, &valueItem)) {
                return(execError("referenced array value not in array: %s", keyString));
            }
            PUSH(valueItem)
            return(STAT_OK);
        }
        else {
            return(execError("operator [] on non-array", NULL));
        }
    }
    else {
        POP(srcArray)
        if (srcArray.tag == ARRAY_TAG) {
            PUSH_INT(ArraySize(&srcArray))
            return(STAT_OK);
        }
        else {
            return(execError("operator [] on non-array", NULL));
        }
    }
}

/*
** assign to an array element of a referenced array on the stack
**
** Before: Prog->  [nDim], next, ...
**         TheStack-> rhs, indnDim, ... ind1, ArraySym, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> next, ...
*/
static int arrayAssign(void)
{
    char *keyString = NULL;
    DataValue srcValue, dstArray;
    int errNum;
    int nDim;
    
    nDim = PC->value;
    PC++;

    DISASM_RT(PC-2, 1);
    STACKDUMP(nDim, 3);

    if (nDim > 0) {
        POP(srcValue)

        errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
        if (errNum != STAT_OK) {
            return(errNum);
        }
        
        POP(dstArray)

        if (dstArray.tag != ARRAY_TAG && dstArray.tag != NO_TAG) {
            return(execError("cannot assign array element of non-array", NULL));
        }
        if (srcValue.tag == ARRAY_TAG) {
            DataValue arrayCopyValue;
            
            errNum = ArrayCopy(&arrayCopyValue, &srcValue);
            srcValue = arrayCopyValue;
            if (errNum != STAT_OK) {
                return(errNum);
            }
        }
        if (ArrayInsert(&dstArray, keyString, &srcValue)) {
            return(STAT_OK);
        }
        else {
            return(execError("array member allocation failure", NULL));
        }
    }
    return(execError("empty operator []", NULL));
}

/*
** for use with assign-op operators (eg a[i,j] += k
**
** Before: Prog->  [binOp], nDim, next, ...
**         TheStack-> [rhs], indnDim, ... ind1, next, ...
** After:  Prog->  binOp, nDim, [next], ...
**         TheStack-> [rhs], arrayValue, next, ...
*/
static int arrayRefAndAssignSetup(void)
{
    int errNum;
    DataValue srcArray, valueItem, moveExpr;
    char *keyString = NULL;
    int binaryOp, nDim;
    
    binaryOp = PC->value;
    PC++;
    nDim = PC->value;
    PC++;

    DISASM_RT(PC-3, 3);
    STACKDUMP(nDim + 1, 3);

    if (binaryOp) {
        POP(moveExpr)
    }
    
    if (nDim > 0) {
        errNum = makeArrayKeyFromArgs(nDim, &keyString, 1);
        if (errNum != STAT_OK) {
            return(errNum);
        }

        PEEK(srcArray, nDim)
        if (srcArray.tag == ARRAY_TAG) {
            if (!ArrayGet(&srcArray, keyString, &valueItem)) {
                return(execError("referenced array value not in array: %s", keyString));
            }
            PUSH(valueItem)
            if (binaryOp) {
                PUSH(moveExpr)
            }
            return(STAT_OK);
        }
        else {
            return(execError("operator [] on non-array", NULL));
        }
    }
    else {
        return(execError("array[] not an lvalue", NULL));
    }
}

/*
** setup symbol values for array iteration in interpreter
**
** Before: Prog->  [iter], ARRAY_ITER, iterVar, iter, endLoopBranch, next, ...
**         TheStack-> [arrayVal], next, ...
** After:  Prog->  iter, [ARRAY_ITER], iterVar, iter, endLoopBranch, next, ...
**         TheStack-> [next], ...
** Where: 
**      iter is a symbol which gives the position of the iterator value in
**              the stack frame
**      arrayVal is the data value holding the array in question
*/
static int beginArrayIter(void)
{
    Symbol *iterator;
    DataValue *iteratorValPtr;
    DataValue arrayVal;

    DISASM_RT(PC-1, 2);
    STACKDUMP(1, 3);

    iterator = PC->sym;
    PC++;

    POP(arrayVal)
    
    if (iterator->type == LOCAL_SYM) {
        iteratorValPtr = &FP_GET_SYM_VAL(FrameP, iterator);
    }
    else {
        return(execError("bad temporary iterator: %s",  iterator->name));
    }

    iteratorValPtr->tag = INT_TAG;
    if (arrayVal.tag != ARRAY_TAG) {
        return(execError("can't iterate non-array", NULL));
    }

    iteratorValPtr->val.arrayPtr = arrayIterateFirst(&arrayVal);
    return(STAT_OK);
}

/*
** copy key to symbol if node is still valid, marked bad by a color of -1
** then move iterator to next node
** this allows iterators to progress even if you delete any node in the array
** except the item just after the current key
**
** Before: Prog->  iter, ARRAY_ITER, [iterVar], iter, endLoopBranch, next, ...
**         TheStack-> [next], ...
** After:  Prog->  iter, ARRAY_ITER, iterVar, iter, endLoopBranch, [next], ...
**         TheStack-> [next], ...      (unchanged)
** Where: 
**      iter is a symbol which gives the position of the iterator value in
**              the stack frame (set up by BEGIN_ARRAY_ITER); that value refers
**              to the array and a position within it
**      iterVal is the programmer-visible symbol which will take the current
**              key value
**      endLoopBranch is the instruction offset to the instruction following the
**              loop (measured from itself)
**      arrayVal is the data value holding the array in question
** The return-to-start-of-loop branch (at the end of the loop) should address
** the ARRAY_ITER instruction
*/
static int arrayIter(void)
{
    Symbol *iterator;
    Symbol *item;
    DataValue *iteratorValPtr;
    DataValue *itemValPtr;
    SparseArrayEntry *thisEntry;
    Inst *branchAddr;

    DISASM_RT(PC-1, 4);
    STACKDUMP(0, 3);

    item = PC->sym;
    PC++;
    iterator = PC->sym;
    PC++;
    branchAddr = PC + PC->value;
    PC++;

    if (item->type == LOCAL_SYM) {
        itemValPtr = &FP_GET_SYM_VAL(FrameP, item);
    }
    else if (item->type == GLOBAL_SYM) {
        itemValPtr = &(item->value);
    }
    else {
        return(execError("can't assign to: %s",  item->name));
    }
    itemValPtr->tag = NO_TAG;

    if (iterator->type == LOCAL_SYM) {
        iteratorValPtr = &FP_GET_SYM_VAL(FrameP, iterator);
    }
    else {
        return(execError("bad temporary iterator: %s",  iterator->name));
    }

    thisEntry = iteratorValPtr->val.arrayPtr;
    if (thisEntry && thisEntry->nodePtrs.color != -1) {
        itemValPtr->tag = STRING_TAG;
        itemValPtr->val.str.rep = thisEntry->key;
        itemValPtr->val.str.len = strlen(thisEntry->key);
        
        iteratorValPtr->val.arrayPtr = arrayIterateNext(thisEntry);
    }
    else {
        PC = branchAddr;
    }
    return(STAT_OK);
}

/*
** determine if a key or keys exists in an array
** if the left argument is a string or integer a single check is performed
** if the key exists, 1 is pushed onto the stack, otherwise 0
** if the left argument is an array 1 is pushed onto the stack if every key
** in the left array exists in the right array, otherwise 0
**
** Before: Prog->  [next], ...
**         TheStack-> [ArraySym], inSymbol, next, ...
** After:  Prog->  [next], ...      -- (unchanged)
**         TheStack-> next, ...
*/
static int inArray(void)
{
    DataValue theArray, leftArray, theValue;
    char *keyStr;
    int inResult = 0;
    
    DISASM_RT(PC-1, 1);
    STACKDUMP(2, 3);

    POP(theArray)
    if (theArray.tag != ARRAY_TAG) {
        return(execError("operator in on non-array", NULL));
    }
    PEEK(leftArray, 0)
    if (leftArray.tag == ARRAY_TAG) {
        SparseArrayEntry *iter;
        
        POP(leftArray)
        inResult = 1;
        iter = arrayIterateFirst(&leftArray);
        while (inResult && iter) {
            inResult = inResult && ArrayGet(&theArray, iter->key, &theValue);
            iter = arrayIterateNext(iter);
        }
    }
    else {
        POP_STRING(keyStr)
        if (ArrayGet(&theArray, keyStr, &theValue)) {
            inResult = 1;
        }
    }
    PUSH_INT(inResult)
    return(STAT_OK);
}

/*
** remove a given key from an array unless nDim is 0, then all keys are removed
**
** for use with assign-op operators (eg a[i,j] += k
** Before: Prog->  [nDim], next, ...
**         TheStack-> [indnDim], ... ind1, arrayValue, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> next, ...
*/
static int deleteArrayElement(void)
{
    DataValue theArray;
    char *keyString = NULL;
    int nDim;

    nDim = PC->value;
    PC++;

    DISASM_RT(PC-2, 2);
    STACKDUMP(nDim + 1, 3);

    if (nDim > 0) {
        int errNum;

        errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
        if (errNum != STAT_OK) {
            return(errNum);
        }
    }

    POP(theArray)
    if (theArray.tag == ARRAY_TAG) {
        if (nDim > 0) {
            ArrayDelete(&theArray, keyString);
        }
        else {
            ArrayDeleteAll(&theArray);
        }
    }
    else {
        return(execError("attempt to delete from non-array", NULL));
    }
    return(STAT_OK);
}

/*
** checks errno after operations which can set it.  If an error occured,
** creates appropriate error messages and returns false
*/
static int errCheck(const char *s)
{
    if (errno == EDOM)
	return execError("%s argument out of domain", s);
    else if (errno == ERANGE)
	return execError("%s result out of range", s);
    else
        return STAT_OK;
}

/*
** combine two strings in a static area and set ErrMsg to point to the
** result.  Returns false so a single return execError() statement can
** be used to both process the message and return.
*/
static int execError(const char *s1, const char *s2)
{
    static char msg[MAX_ERR_MSG_LEN];
    
    sprintf(msg, s1, s2);
    ErrMsg = msg;
    return STAT_ERROR;
}

int StringToNum(const char *string, int *number)
{
    const char *c = string;
    
    while (*c == ' ' || *c == '\t') {
        ++c;
    }
    if (*c == '+' || *c == '-') {
        ++c;
    }
    while (isdigit((unsigned char)*c)) {
        ++c;
    }
    while (*c == ' ' || *c == '\t') {
        ++c;
    }
    if (*c) {
        /* if everything went as expected, we should be at end, but we're not */
        return False;
    }
    if (number) {
        if (sscanf(string, "%d", number) != 1) {
            /* This case is here to support old behavior */
    	    *number = 0;
        }
    }
    return True;
}

#ifdef DEBUG_DISASSEMBLER   /* dumping values in disassembly or stack dump */
static void dumpVal(DataValue dv)
{
    switch (dv.tag) {
        case INT_TAG:
            printf("i=%d", dv.val.n);
            break;
        case STRING_TAG:
            {
                int k;
                char s[21];
                char *src = dv.val.str.rep;
                if (!src) {
                    printf("s=<NULL>");
                }
                else {
                    for (k = 0; src[k] && k < sizeof s - 1; k++) {
                        s[k] = isprint(src[k]) ? src[k] : '?';
                    }
                    s[k] = 0;
                    printf("s=\"%s\"%s[%d]", s,
                           src[k] ? "..." : "", strlen(src));
                }
            }
            break;
        case ARRAY_TAG:
            printf("<array>");
            break;
        case NO_TAG:
            if (!dv.val.inst) {
                printf("<no value>");
            }
            else {
                printf("?%8p", dv.val.inst);
            }
            break;
        default:
            printf("UNKNOWN DATA TAG %d ?%8p", dv.tag, dv.val.inst);
            break;
    }
}
#endif /* #ifdef DEBUG_DISASSEMBLER */

#ifdef DEBUG_DISASSEMBLER /* For debugging code generation */
static void disasm(Inst *inst, int nInstr)
{
    static const char *opNames[N_OPS] = {
        "RETURN_NO_VAL",                /* returnNoVal */
        "RETURN",                       /* returnVal */
        "PUSH_SYM",                     /* pushSymVal */
        "DUP",                          /* dupStack */
        "ADD",                          /* add */
        "SUB",                          /* subtract */
        "MUL",                          /* multiply */
        "DIV",                          /* divide */
        "MOD",                          /* modulo */
        "NEGATE",                       /* negate */
        "INCR",                         /* increment */
        "DECR",                         /* decrement */
        "GT",                           /* gt */
        "LT",                           /* lt */
        "GE",                           /* ge */
        "LE",                           /* le */
        "EQ",                           /* eq */
        "NE",                           /* ne */
        "BIT_AND",                      /* bitAnd */
        "BIT_OR",                       /* bitOr */
        "AND",                          /* and */
        "OR",                           /* or */
        "NOT",                          /* not */
        "POWER",                        /* power */
        "CONCAT",                       /* concat */
        "ASSIGN",                       /* assign */
        "SUBR_CALL",                    /* callSubroutine */
        "FETCH_RET_VAL",                /* fetchRetVal */
        "BRANCH",                       /* branch */
        "BRANCH_TRUE",                  /* branchTrue */
        "BRANCH_FALSE",                 /* branchFalse */
        "BRANCH_NEVER",                 /* branchNever */
        "ARRAY_REF",                    /* arrayRef */
        "ARRAY_ASSIGN",                 /* arrayAssign */
        "BEGIN_ARRAY_ITER",             /* beginArrayIter */
        "ARRAY_ITER",                   /* arrayIter */
        "IN_ARRAY",                     /* inArray */
        "ARRAY_DELETE",                 /* deleteArrayElement */
        "PUSH_ARRAY_SYM",               /* pushArraySymVal */
        "ARRAY_REF_ASSIGN_SETUP",       /* arrayRefAndAssignSetup */
        "PUSH_ARG",                     /* $arg[expr] */
        "PUSH_ARG_COUNT",               /* $arg[] */
        "PUSH_ARG_ARRAY"                /* $arg */
    };
    int i, j;
    
    printf("\n");
    for (i = 0; i < nInstr; ++i) {
        printf("Prog %8p ", &inst[i]);
        for (j = 0; j < N_OPS; ++j) {
            if (inst[i].func == OpFns[j]) {
                printf("%22s ", opNames[j]);
                if (j == OP_PUSH_SYM || j == OP_ASSIGN) {
                    Symbol *sym = inst[i+1].sym;
                    printf("%s", sym->name);
                    if (sym->value.tag == STRING_TAG &&
                        strncmp(sym->name, "string #", 8) == 0) {
                        dumpVal(sym->value);
                    }
                    ++i;
                }
                else if (j == OP_BRANCH || j == OP_BRANCH_FALSE ||
                        j == OP_BRANCH_NEVER || j == OP_BRANCH_TRUE) {
                    printf("to=(%d) %p", inst[i+1].value,
                            &inst[i+1] + inst[i+1].value);
                    ++i;
                }
                else if (j == OP_SUBR_CALL) {
                    printf("%s (%d arg)", inst[i+1].sym->name, inst[i+2].value);
                    i += 2;
                }
                else if (j == OP_BEGIN_ARRAY_ITER) {
                    printf("%s in", inst[i+1].sym->name);
                    ++i;
                }
                else if (j == OP_ARRAY_ITER) {
                    printf("%s = %s++ end-loop=(%d) %p",
                            inst[i+1].sym->name,
                            inst[i+2].sym->name,
                            inst[i+3].value, &inst[i+3] + inst[i+3].value);
                    i += 3;
                }
                else if (j == OP_ARRAY_REF || j == OP_ARRAY_DELETE ||
                            j == OP_ARRAY_ASSIGN) {
                    printf("nDim=%d", inst[i+1].value);
                    ++i;
                }
                else if (j == OP_ARRAY_REF_ASSIGN_SETUP) {
                    printf("binOp=%s ", inst[i+1].value ? "true" : "false");
                    printf("nDim=%d", inst[i+2].value);
                    i += 2;
                }
                else if (j == OP_PUSH_ARRAY_SYM) {
                    printf("%s", inst[++i].sym->name);
                    printf(" %s", inst[i+1].value ? "createAndRef" : "refOnly");
                    ++i;
                }

                printf("\n");
                break;
            }
        }
        if (j == N_OPS) {
            printf("%x\n", inst[i].value);
        }
    }
}
#endif /* #ifdef DEBUG_DISASSEMBLER */

#ifdef DEBUG_STACK  /* for run-time stack dumping */
#define STACK_DUMP_ARG_PREFIX "Arg"
static void stackdump(int n, int extra)
{
    /* TheStack-> symN-sym1(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ... */
    int nArgs = FP_GET_ARG_COUNT(FrameP);
    int i, offset;
    char buffer[sizeof(STACK_DUMP_ARG_PREFIX) + TYPE_INT_STR_SIZE(int)];
    printf("Stack ----->\n");
    for (i = 0; i < n + extra; i++) {
        char *pos = "";
        DataValue *dv = &StackP[-i - 1];
        if (dv < TheStack) {
            printf("--------------Stack base--------------\n");
            break;
        }
        offset = dv - FrameP;

        printf("%4.4s", i < n ? ">>>>" : "");
        printf("%8p ", dv);
        switch (offset) {
            case 0:                         pos = "FrameP"; break;  /* first local symbol value */
            case FP_ARG_ARRAY_CACHE_INDEX:  pos = "args";   break;  /* arguments array */
            case FP_ARG_COUNT_INDEX:        pos = "NArgs";  break;  /* number of arguments */
            case FP_OLD_FP_INDEX:           pos = "OldFP";  break;
            case FP_RET_PC_INDEX:           pos = "RetPC";  break;
            default:
                if (offset < -FP_TO_ARGS_DIST && offset >= -FP_TO_ARGS_DIST - nArgs) {
                    sprintf(pos = buffer, STACK_DUMP_ARG_PREFIX "%d",
                            offset + FP_TO_ARGS_DIST + nArgs + 1);
                }
                break;
        }
        printf("%-6s ", pos);
        dumpVal(*dv);
        printf("\n");
    }
}
#endif /* ifdef DEBUG_STACK */
