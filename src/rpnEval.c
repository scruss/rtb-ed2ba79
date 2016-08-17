/*
 * returnToBasic:
 *	A new implementation of BASIC
 *	Copyright (c) 2012 Gordon Henderson
 *********************************************************************************
 * rpnEval.c:
 *	Evaluate the output stack as generated by the shunting yard
 *********************************************************************************
 * This file is part of RTB:
 *	Return To Basic
 *	http://projects.drogon.net/return-to-basic
 *
 *    RTB is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RTB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RTB.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************
 */

#undef	DEBUG_STACK

#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

//#include <unistd.h>

#include "rtb.h"
#include "bomb.h"

#include "bool.h"
#include "keywords.h"
#include "symbolTable.h"
#include "goto.h"
#include "procFn.h"

#include "array.h"
#include "shuntingYard.h"
#include "rpnEval.h"

#ifdef	DUMP_RPN_STACK
//#include "screenKeyboard.h"
#endif


/*
 * Our stack only holds symbols of either numeric or string
 *	type. (constants or variables)
 *********************************************************************************
 */

double    numStack    [RPN_STACK_SIZE / 2] ;
char     *strStack    [RPN_STACK_SIZE / 2] ;
uint16_t  numArrStack [RPN_STACK_SIZE / 2] ;
uint16_t  strArrStack [RPN_STACK_SIZE / 2] ;
uint8_t   stackOrder  [RPN_STACK_SIZE] ;

int    numStackPtr = 0 ;
int    strStackPtr = 0 ;
int numArrStackPtr = 0 ;
int strArrStackPtr = 0 ;
int  stackOrderPtr = 0;


#ifdef	DUMP_RPN_STACK
/*
 * dumpRpnStack:
 *	Some debugging
 *********************************************************************************
 */

void dumpRpnStack (char *prefix)
{
  int i, j ;
  int num, str, arrN, arrS ;

  num  = numStackPtr ;
  str  = strStackPtr ;
  arrN = numArrStackPtr ;
  arrS = strArrStackPtr ;

  if ((stackOrderPtr + num + str + arrN + arrS) == 0)
  {
    printf ("%s: dumpRpnStack: All stacks empty\n", prefix) ;
    return ;
  }
  else
    printf ("%s: rpnStack: Size: %d, num: %d, str: %d, arrN: %d, arrS: %d\n",
	prefix, stackOrderPtr, num, str, arrN, arrS) ;

  --num ;
  --str ;
  --arrN ;
  --arrS ;

  for (i = stackOrderPtr - 1 ; i >= 0 ; --i)
  {
    j = stackOrder [i] ;
    printf ("%s: ", prefix) ;
    /**/ if (j == EVAL_STACK_NUM)
    {
      printf ("%3d: (%2d) %lf\n",    i, num, numStack [num]) ;
      --num ;
    }
    else if (j == EVAL_STACK_STR)
    {
      printf ("%3d: (%2d) \"%s\"\n", i, str, strStack [str]) ;
      --str ;
    }
    else if (j == EVAL_STACK_NUM_ARR)
    {
      printf ("%3d: (%2d) 0x%04X\n", i, arrN, numArrStack [arrN]) ;
      --arrN ;
    }
    else if (j == EVAL_STACK_STR_ARR)
    {
      printf ("%3d: (%2d) 0x%04X\n", i, arrS, strArrStack [arrS]) ;
      --arrS ;
    }
    else
      printf ("Unknown stack type: %d\n", j) ;
  }
}
#endif


/*
 * popN: popS: popA:
 *	Pop a number, string or array off the appropriate stack
 *********************************************************************************
 */

double popN (void)
{
  --stackOrderPtr ;
  return numStack [--numStackPtr] ;
}

char *popS (void)
{
  --stackOrderPtr ;
  return strStack [--strStackPtr] ;
}

uint16_t popAn (void)
{
  --stackOrderPtr ;
  return numArrStack [--numArrStackPtr] ;
}

uint16_t popAs (void)
{
  --stackOrderPtr ;
  return strArrStack [--strArrStackPtr] ;
}


/*
 * pushN: pushS:
 *	Push a number or string on to it's stack
 *********************************************************************************
 */

void pushN (double number)
{
  stackOrder [stackOrderPtr++] = EVAL_STACK_NUM ;
  numStack   [numStackPtr++]   = number ;
}

void pushS (char *str)
{
  char *s = malloc (strlen (str) + 1) ;

  if (s == NULL)
    bomb ("Out of memory", TRUE) ;

  strcpy (s, str) ;
  stackOrder [stackOrderPtr++] = EVAL_STACK_STR ;
  strStack   [strStackPtr++]   = s ;
}

void pushAn (uint16_t symbol)
{
  stackOrder [stackOrderPtr++] = EVAL_STACK_NUM_ARR ;
  numArrStack   [numArrStackPtr++]   = symbol ;
}

void pushAs (uint16_t symbol)
{
  stackOrder [stackOrderPtr++] = EVAL_STACK_STR_ARR ;
  strArrStack   [strArrStackPtr++]   = symbol ;
}


/*
 * pushSym:
 *	Push a symbol onto the stack, but expand it first and put the value
 *	on the appropriate stack, updating the type stack in the process.
 *********************************************************************************
 */

static int pushSym (uint16_t symbol)
{
  struct   symbolTableStruct *st ;
  uint16_t index = symbol & ~TK_SYM_MASK ;
  uint16_t  type = symbol &  TK_SYM_MASK ;
  int idx ;

  st = &symbolTable [index] ;

  if (type == TK_SYM_VAR_NUM)
  {
    if (st->writeCount == 0)
      return FALSE ;
    pushN (st->value.realVal) ;
    return TRUE ;
  }

  if (type == TK_SYM_CONST_NUM)
  {
    pushN (st->value.realVal) ;
    return TRUE ;
  }

  if (type == TK_SYM_VAR_STR)
  {
    if (st->value.stringVal == NULL)
      return FALSE ;
    pushS (st->value.stringVal) ;
    return TRUE ;
  }

  if (type == TK_SYM_CONST_STR)
  {
    pushS (st->name) ;
    return TRUE ;
  }

// Arrays:
//	If there is at least one element on the stack, then evaluate the array+index, else
//	just push the array

  if (type == TK_SYM_VAR_NUM_ARR)
  {
    if (stackOrderPtr >= 1)
    { 
      if (!popArrayIndex (st, &idx))
	return FALSE ;

      pushN (st->value.realArray [idx]) ;
    }
    else
      pushAn (symbol) ;
    return TRUE ;
  }

  if (type == TK_SYM_VAR_STR_ARR)
  {
    if (stackOrderPtr >= 1)
    { 
      if (!popArrayIndex (st, &idx))
	return FALSE ;

      if (st->value.stringArray [idx] == NULL)
	return syntaxError ("Eval: Unassigned string") ;

      pushS (st->value.stringArray [idx]) ;
    }
    else
      pushAs (symbol) ;
    return TRUE ;
  }

  return syntaxError ("Eval: Wrong type of symbol (help!)") ;
}



/*
 * oneNumber:
 * twoNumbers:
 * oneString:
 * twoStrings
 *	Test for one or 2 items on the stack and both being numbers or strings
 *********************************************************************************
 */

int oneNumber (void)
{
  if (stackOrderPtr < 1)
    return FALSE ;

  return (stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) ;
}

int twoNumbers (void)
{
  if (stackOrderPtr < 2)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_NUM)) ;
}

int threeNumbers (void)
{
  if (stackOrderPtr < 3)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 3] == EVAL_STACK_NUM)) ;
}

int fourNumbers (void)
{
  if (stackOrderPtr < 4)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 3] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 4] == EVAL_STACK_NUM)) ;
}

int fiveNumbers (void)
{
  if (stackOrderPtr < 5)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 3] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 4] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 5] == EVAL_STACK_NUM)) ;
}

int sevenNumbers (void)
{
  if (stackOrderPtr < 7)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 3] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 4] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 5] == EVAL_STACK_NUM) && (stackOrder [stackOrderPtr - 6] == EVAL_STACK_NUM) &&
	  (stackOrder [stackOrderPtr - 7] == EVAL_STACK_NUM)) ;
}

int oneString (void)
{
  if (stackOrderPtr < 1)
    return FALSE ;

  return (stackOrder [stackOrderPtr - 1] == EVAL_STACK_STR) ;
}

int twoStrings (void)
{
  if (stackOrderPtr < 2)
    return FALSE ;

  return ((stackOrder [stackOrderPtr - 1] == EVAL_STACK_STR) && (stackOrder [stackOrderPtr - 2] == EVAL_STACK_STR)) ;
}

/*
 * rpnEval:
 *	Evaluate the queue created by the shunting yard
 *********************************************************************************
 */

int rpnEval (void)
{
  struct keywordStruct *kw ;
  int    ptr = 0 ;
  int    marker ;
  int    oStackTop = oStackPtr ;
  uint16_t token, type ;
  int   removed = 0 ;

#ifdef	DEBUG_STACK
  printf ("rpnEval: START: oStackPtr: %d\n", oStackPtr) ;
  dumpRpnStack     ("rpnEval") ;
  dumpShuntingYard ("rpnEval") ;
#endif

// It's an error if the stack is empty

  if (oStackPtr == 0)
    return syntaxError ("Eval: No data to work with") ;

// Work out where the marker is

  marker = oStackPtr - 1 ;
  while (oStack [marker] != TK_SYM_EOL)
    --marker ;

  ptr = marker + 1 ;

/*
printf ("marker: %2d, ptr: %2d, oStackPtr: %2d, stackOrderPtr: %2d\n",
	marker, ptr, oStackPtr, stackOrderPtr) ;
*/

#ifdef	DEBUG_STACK
  printf ("rpnEval: marker: %d, ptr: %d, oStackPtr: %d\n", marker, ptr, oStackPtr) ;
#endif

// Now work up from the marker

  while (ptr != oStackTop)
  {
    token = oStack [ptr++] ;
    ++removed ;

#ifdef	DEBUG_STACK
    printf ("rpnEval: Taken 0x%04X from the oStack\n", token) ;
#endif

    type = token & TK_SYM_MASK ;

// Built-in function?

    if (type == 0)				// Built-in Function
    {
      kw = &keywords [token] ;
      if (kw->function == NULL)
	return syntaxError ("Eval: Call to undefined function: %s", kw->keyword) ;

#ifdef	DEBUG_STACK
      printf ("rpnEval: built-in: \"%s\"\n", keywords [token].keyword) ;
      dumpRpnStack ("rpnEval") ;
#endif
      if (kw->function (NULL))
	continue ;
      else
      {
	if (escapePressed)
	  return FALSE ;

	return syntaxError ("Eval: Call to \"%s\" failed", keywords [token].keyword) ;
      }
    }

// Variable or Constant?

    if ( (type == TK_SYM_CONST_NUM)   || (type == TK_SYM_CONST_STR)  ||
         (type == TK_SYM_VAR_NUM)     || (type == TK_SYM_VAR_STR)    ||
         (type == TK_SYM_VAR_NUM_ARR) || (type == TK_SYM_VAR_STR_ARR) )
    {
      if (!pushSym (token))
	return syntaxError ("Eval: Unassigned variable") ;
      continue ;
    }

// User-defined function?

    if (type == TK_SYM_FUNC)
    {
#ifdef	DEBUG_STACK
      printf ("rpnEval: START user-FN: %d\n", stackOrderPtr) ;
dumpRpnStack ("rpnEval") ;
#endif
      if (doFunc (token))
      {
#ifdef	DEBUG_STACK
	printf ("rpnEval: END user-FN - OK: %d\n", stackOrderPtr) ;
	dumpRpnStack ("rpnEval") ;
#endif
	continue ;
      }
      else
      {
	if (escapePressed)
	  return FALSE ;
	else
	  return syntaxError ("Eval: Failure of User-defined Function") ;
      }
    }

// Unknown...

    return syntaxError ("Eval: Unsupported SYMBOL/Token on stack") ;
  }

#ifdef	DEBUG_STACK
  printf ("rpnEval: taking oStackPtr from %d to %d (%d removed)\n", oStackPtr, removed + 1, removed) ;
#endif
  oStackPtr -= (removed + 1) ;

#ifdef	DEBUG_STACK
  dumpRpnStack ("rpnEval") ;
#endif

/*
printf ("marker: %2d, ptr: %2d, oStackPtr: %2d, stackOrderPtr: %2d\n",
	marker, ptr, oStackPtr, stackOrderPtr) ;
*/

  return TRUE ;
}


/*
 * rpnEvalNum:
 *	Called from the main code when it's expecting a numeric result
 *********************************************************************************
 */

int rpnEvalNum (double *result)
{
  if (!rpnEval ())
    return FALSE ;

  if (!oneNumber ())
    return syntaxError ("Number expected") ;

  *result = popN () ;
  return TRUE ;
}


/*
 * rpnEvalStr:
 *	Called from the main code when it's expecting a string result
 *********************************************************************************
 */

int rpnEvalStr (char **result)
{
  if (!rpnEval ())
    return FALSE ;

  if (!oneString ())
    return syntaxError ("String expected") ;

  *result = popS () ;
  return TRUE ;
}