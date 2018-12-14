/*******************************************************************************
*                                                                              *
* interpret.h -- Nirvana Editor Interpreter Header File                        *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef NEDIT_INTERPRET_H_INCLUDED
#define NEDIT_INTERPRET_H_INCLUDED

#include "nedit.h"
#include "../util/rbTree.h"

#define STACK_SIZE 1024		/* Maximum stack size */
#define MAX_SYM_LEN 100 	/* Max. symbol name length */
#define MACRO_EVENT_MARKER 2 	/* Special value for the send_event field of
    	    	    	    	   events passed to action routines.  Tells
    	    	    	    	   them that they were called from a macro */

enum symTypes {CONST_SYM, GLOBAL_SYM, LOCAL_SYM, ARG_SYM, PROC_VALUE_SYM,
    	C_FUNCTION_SYM, MACRO_FUNCTION_SYM, ACTION_ROUTINE_SYM};
#define N_OPS 43
enum operations {OP_RETURN_NO_VAL, OP_RETURN, OP_PUSH_SYM, OP_DUP, OP_ADD,
    OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEGATE, OP_INCR, OP_DECR, OP_GT, OP_LT,
    OP_GE, OP_LE, OP_EQ, OP_NE, OP_BIT_AND, OP_BIT_OR, OP_AND, OP_OR, OP_NOT,
    OP_POWER, OP_CONCAT, OP_ASSIGN, OP_SUBR_CALL, OP_FETCH_RET_VAL, OP_BRANCH,
    OP_BRANCH_TRUE, OP_BRANCH_FALSE, OP_BRANCH_NEVER, OP_ARRAY_REF,
    OP_ARRAY_ASSIGN, OP_BEGIN_ARRAY_ITER, OP_ARRAY_ITER, OP_IN_ARRAY,
    OP_ARRAY_DELETE, OP_PUSH_ARRAY_SYM, OP_ARRAY_REF_ASSIGN_SETUP, OP_PUSH_ARG,
    OP_PUSH_ARG_COUNT, OP_PUSH_ARG_ARRAY};

enum typeTags {NO_TAG, INT_TAG, STRING_TAG, ARRAY_TAG};

enum execReturnCodes {MACRO_TIME_LIMIT, MACRO_PREEMPT, MACRO_DONE, MACRO_ERROR};

#define ARRAY_DIM_SEP "\034"

struct DataValueTag;
struct SparseArrayEntryTag;
struct ProgramTag;
struct SymbolRec;

typedef union InstTag {
    int (*func)(void);
    int value;
    struct SymbolRec *sym;
} Inst;

typedef int (*BuiltInSubr)(WindowInfo *window, struct DataValueTag *argList, 
        int nArgs, struct DataValueTag *result, char **errMsg);

typedef struct NStringTag {
  char *rep;
  size_t len;
} NString;

typedef struct DataValueTag {
    enum typeTags tag;
    union {
        int n;
        struct NStringTag str;
        BuiltInSubr subr;
        struct ProgramTag* prog;
        XtActionProc xtproc;
        Inst* inst;
        struct DataValueTag* dataval;
        struct SparseArrayEntryTag *arrayPtr;
    } val;
} DataValue;

typedef struct SparseArrayEntryTag {
    rbTreeNode nodePtrs; /* MUST BE FIRST ENTRY */
    char *key;
    DataValue value;
} SparseArrayEntry;

/* symbol table entry */
typedef struct SymbolRec {
    char *name;
    enum symTypes type;
    DataValue value;
    struct SymbolRec *next;     /* to link to another */  
} Symbol;

typedef struct ProgramTag {
    Symbol *localSymList;
    Inst *code;
} Program;

/* Information needed to re-start a preempted macro */
typedef struct {
    DataValue *stack;
    DataValue *stackP;
    DataValue *frameP;
    Inst *pc;
    WindowInfo *runWindow;
    WindowInfo *focusWindow;
} RestartData;

void InitMacroGlobals(void);

SparseArrayEntry *arrayIterateFirst(DataValue *theArray);
SparseArrayEntry *arrayIterateNext(SparseArrayEntry *iterator);
SparseArrayEntry *ArrayNew(void);
Boolean ArrayInsert(DataValue* theArray, char* keyStr, DataValue* theValue);
void ArrayDelete(DataValue *theArray, char *keyStr);
void ArrayDeleteAll(DataValue *theArray);
unsigned ArraySize(DataValue *theArray);
Boolean ArrayGet(DataValue* theArray, char* keyStr, DataValue* theValue);
int ArrayCopy(DataValue *dstArray, DataValue *srcArray);

/* Routines for creating a program, (accumulated beginning with
   BeginCreatingProgram and returned via FinishCreatingProgram) */
void BeginCreatingProgram(void);
int AddOp(int op, char **msg);
int AddSym(Symbol *sym, char **msg);
int AddImmediate(int value, char **msg);
int AddBranchOffset(Inst *to, char **msg);
Inst *GetPC(void);
Symbol *InstallIteratorSymbol(void);
Symbol *LookupStringConstSymbol(const char *value);
Symbol *InstallStringConstSymbol(const char *str);
Symbol *LookupSymbol(const char *name);
Symbol *InstallSymbol(const char *name, enum symTypes type, DataValue value);
Program *FinishCreatingProgram(void);
void SwapCode(Inst *start, Inst *boundary, Inst *end);
void StartLoopAddrList(void);
int AddBreakAddr(Inst *addr);
int AddContinueAddr(Inst *addr);
void FillLoopAddrs(Inst *breakAddr, Inst *continueAddr);

/* create a permanently allocated static string (only for use with static strings) */
#define PERM_ALLOC_STR(xStr) (((char *)("\001" xStr)) + 1)

/* Routines for executing programs */
int ExecuteMacro(WindowInfo *window, Program *prog, int nArgs, DataValue *args,
    	DataValue *result, RestartData **continuation, char **msg);
int ContinueMacro(RestartData *continuation, DataValue *result, char **msg);
void RunMacroAsSubrCall(Program *prog);
void PreemptMacro(void);
char *AllocString(int length);
char *AllocStringNCpy(const char *s, int length);
char *AllocStringCpy(const char *s);
int AllocNString(NString *string, int length);
int AllocNStringNCpy(NString *string, const char *s, int length);
int AllocNStringCpy(NString *string, const char *s);
void GarbageCollectStrings(void);
void FreeRestartData(RestartData *context);
Symbol *PromoteToGlobal(Symbol *sym);
void FreeProgram(Program *prog);
void ModifyReturnedValue(RestartData *context, DataValue dv);
WindowInfo *MacroRunWindow(void);
WindowInfo *MacroFocusWindow(void);
void SetMacroFocusWindow(WindowInfo *window);
/* function used for implicit conversion from string to number */
int StringToNum(const char *string, int *number);

#endif /* NEDIT_INTERPRET_H_INCLUDED */
