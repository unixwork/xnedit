/*------------------------------------------------------------------------*
 * `CompileRE', `ExecRE', and `ConvertSubstituteRE' -- regular expression parsing
 *
 * This is a HIGHLY ALTERED VERSION of Henry Spencer's `regcomp'
 * code adapted for NEdit.
 *
 * .-------------------------------------------------------------------.
 * | ORIGINAL COPYRIGHT NOTICE:                                        |
 * |                                                                   |
 * | Copyright (c) 1986 by University of Toronto.                      |
 * | Written by Henry Spencer.  Not derived from licensed software.    |
 * |                                                                   |
 * | Permission is granted to anyone to use this software for any      |
 * | purpose on any computer system, and to redistribute it freely,    |
 * | subject to the following restrictions:                            |
 * |                                                                   |
 * | 1. The author is not responsible for the consequences of use of   |
 * |      this software, no matter how awful, even if they arise       |
 * |      from defects in it.                                          |
 * |                                                                   |
 * | 2. The origin of this software must not be misrepresented, either |
 * |      by explicit claim or by omission.                            |
 * |                                                                   |
 * | 3. Altered versions must be plainly marked as such, and must not  |
 * |      be misrepresented as being the original software.            |
 * `-------------------------------------------------------------------'
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. In addition, you may distribute version of this program linked to
 * Motif or Open Motif. See README for details.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "regexConvert.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <X11/Intrinsic.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif


/* Utility definitions. */

#define NSUBEXP 50

#define CONVERT_FAIL(m)  {*Error_Ptr = (m); return 0;}
#define IS_QUANTIFIER(c) ((c) == '*' || (c) == '+' || (c) == '?')
#define U_CHAR_AT(p)     ((unsigned int) *(unsigned char *)(p))

/* Flags to be passed up and down via function parameters during compile. */

#define WORST             0  /* Worst case. No assumptions can be made.*/
#define HAS_WIDTH         1  /* Known never to match null string. */
#define SIMPLE            2  /* Simple enough to be STAR/PLUS operand. */

#define NO_PAREN          0  /* Only set by initial call to "chunk". */
#define PAREN             1  /* Used for normal capturing parentheses. */

#define REG_ZERO        0UL
#define REG_ONE         1UL

/* Global work variables for `ConvertRE'. */

static unsigned char *Reg_Parse;       /* Input scan ptr (scans user's regex) */
static int            Total_Paren;     /* Parentheses, (),  counter. */
static unsigned long  Convert_Size;    /* Address of this used as flag. */
static unsigned char *Code_Emit_Ptr;   /* When Code_Emit_Ptr is set to
                                          &Compute_Size no code is emitted.
                                          Instead, the size of code that WOULD
                                          have been generated is accumulated in
                                          Convert_Size.  Otherwise,
                                          Code_Emit_Ptr points to where compiled
                                          regex code is to be written. */
static unsigned char  Compute_Size;
static char         **Error_Ptr;       /* Place to store error messages so
                                          they can be returned by `ConvertRE' */
static char           Error_Text [128];/* Sting to build error messages in. */

static unsigned char  Meta_Char [] = ".*+?[(|)^<>$";

static unsigned char *Convert_Str;

/* Forward declarations for functions used by `ConvertRE'. */

static int            alternative       (int *flag_param);
static int            chunk             (int paren, int *flag_param);
static void           emit_convert_byte (unsigned char c);
static unsigned char  literal_escape    (unsigned char c, int);
static int            atom              (int *flag_param);
static void           reg_error         (char *str);
static int            piece             (int *flag_param);

/*----------------------------------------------------------------------*
 * ConvertRE
 *
 * Compiles a regular expression into the internal format used by
 * `ExecRE'.
 *
 * Beware that the optimization and preparation code in here knows about
 * some of the structure of the compiled regexp.
 *----------------------------------------------------------------------*/

char * ConvertRE (const char *exp, char **errorText) {

   int  flags_local, pass;

   /* Set up `errorText' to receive failure reports. */

    Error_Ptr = errorText;
   *Error_Ptr = "";

   if (exp == NULL) CONVERT_FAIL ("NULL argument to `ConvertRE\'");

   Code_Emit_Ptr = &Compute_Size;
   Convert_Size  = 0UL;

   /* We can't allocate space until we know how big the compiled form will be,
      but we can't compile it (and thus know how big it is) until we've got a
      place to put the code.  So we cheat: we compile it twice, once with code
      generation turned off and size counting turned on, and once "for real".
      This also means that we don't allocate space until we are sure that the
      thing really will compile successfully, and we never have to move the
      code and thus invalidate pointers into it.  (Note that it has to be in
      one piece because free() must be able to free it all.) */

   for (pass = 1; pass <= 2; pass++) {
      /*-------------------------------------------*
       * FIRST  PASS: Determine size and legality. *
       * SECOND PASS: Emit converted code.         *
       *-------------------------------------------*/

      Reg_Parse   = (unsigned char *) exp;
      Total_Paren = 1;

      if (chunk (NO_PAREN, &flags_local) == 0) return (NULL); /* Something
                                                                 went wrong */
      emit_convert_byte ('\0');

      if (pass == 1) {
         /* Allocate memory. */

         Convert_Str =
            (unsigned char *) NEditMalloc(sizeof (unsigned char) * Convert_Size);

         if (Convert_Str == NULL) {
            CONVERT_FAIL ("out of memory in `ConvertRE\'");
         }

         Code_Emit_Ptr = Convert_Str;
      }
   }

   return (char *) Convert_Str;
}

/*----------------------------------------------------------------------*
 * chunk                                                                *
 *                                                                      *
 * Process main body of regex or process a parenthesized "thing".       *
 *                                                                      *
 * Caller must absorb opening parenthesis.
 *----------------------------------------------------------------------*/

static int chunk (int paren, int *flag_param) {

   register int   this_branch;
            int   flags_local;

   *flag_param = HAS_WIDTH;  /* Tentatively. */

   /* Make an OPEN node, if parenthesized. */

   if (paren == PAREN) {
      if (Total_Paren >= NSUBEXP) {
         sprintf (Error_Text, "number of ()'s > %d", (int) NSUBEXP);
         CONVERT_FAIL (Error_Text);
      }

      Total_Paren++;
   }

   /* Pick up the branches, linking them together. */

   do {
      this_branch = alternative (&flags_local);

      if (this_branch == 0) return 0;

      /* If any alternative could be zero width, consider the whole
         parenthisized thing to be zero width. */

      if (!(flags_local & HAS_WIDTH)) *flag_param &= ~HAS_WIDTH;

      /* Are there more alternatives to process? */

      if (*Reg_Parse != '|') break;

      emit_convert_byte ('|');

      Reg_Parse++;
   } while (1);

   /* Check for proper termination. */

   if (paren != NO_PAREN && *Reg_Parse != ')') {
      CONVERT_FAIL ("missing right parenthesis \')\'");

   } else if (paren != NO_PAREN) {
      emit_convert_byte (')');
      Reg_Parse++;

   } else if (paren == NO_PAREN && *Reg_Parse != '\0') {
      if (*Reg_Parse == ')') {
         CONVERT_FAIL ("missing left parenthesis \'(\'");
      } else {
         CONVERT_FAIL ("junk on end");  /* "Can't happen" - NOTREACHED */
      }
   }

   return 1;
}

/*----------------------------------------------------------------------*
 * alternative - Processes one alternative of an '|' operator.
 *----------------------------------------------------------------------*/

static int alternative (int *flag_param) {

   int  ret_val;
   int  flags_local;

   *flag_param = WORST;  /* Tentatively. */

   /* Loop until we hit the start of the next alternative, the end of this set
      of alternatives (end of parentheses), or the end of the regex. */

   while (*Reg_Parse != '|' && *Reg_Parse != ')' && *Reg_Parse != '\0') {
      ret_val = piece (&flags_local);

      if (ret_val == 0) return 0; /* Something went wrong. */

      *flag_param |= flags_local & HAS_WIDTH;
   }

   return 1;
}

/*----------------------------------------------------------------------*
 * piece - something followed by possible '*', '+', or '?'.
 *----------------------------------------------------------------------*/

static int piece (int *flag_param) {

   register int            ret_val;
   register unsigned char  op_code;
            unsigned long  min_val = REG_ZERO;
            int            flags_local;

   ret_val = atom (&flags_local);

   if (ret_val == 0) return 0;  /* Something went wrong. */

   op_code = *Reg_Parse;

   if (!IS_QUANTIFIER (op_code)) {
      *flag_param = flags_local;

      return (ret_val);
   }

   Reg_Parse++;

   if (op_code == '+') min_val = REG_ONE;

   /* It is dangerous to apply certain quantifiers to a possibly zero width
      item. */

   if (!(flags_local & HAS_WIDTH) && min_val > REG_ZERO) {
      sprintf (Error_Text, "%c operand could be empty", op_code);

      CONVERT_FAIL (Error_Text);
   }

   *flag_param = (min_val > REG_ZERO) ? (WORST | HAS_WIDTH) : WORST;

   if ( !((op_code == '*') || (op_code == '+') || (op_code == '?')) ) {
      /* We get here if the IS_QUANTIFIER macro is not coordinated properly
         with this function. */

      CONVERT_FAIL ("internal error #2, `piece\'");
   }

   if (IS_QUANTIFIER (*Reg_Parse)) {
      sprintf (Error_Text, "nested quantifiers, %c%c", op_code, *Reg_Parse);

      CONVERT_FAIL (Error_Text);
   }

   emit_convert_byte (op_code);

   return (ret_val);
}

/*----------------------------------------------------------------------*
 * atom - Process one regex item at the lowest level
 *----------------------------------------------------------------------*/

static int atom (int *flag_param) {
   int            ret_val = 1;
   unsigned char  test;
   int            flags_local;

   *flag_param = WORST;  /* Tentatively. */

   switch (*Reg_Parse++) {
      case '^':
         emit_convert_byte ('^');
         break;

      case '$':
         emit_convert_byte ('$');
         break;

      case '<':
         emit_convert_byte ('<');
         break;

      case '>':
         emit_convert_byte ('>');
         break;

      case '.':
         emit_convert_byte ('.');

         *flag_param |= (HAS_WIDTH | SIMPLE); break;

      case '(':
         emit_convert_byte ('(');

         ret_val = chunk (PAREN, &flags_local);

         if (ret_val == 0) return 0;  /* Something went wrong. */

         /* Add HAS_WIDTH flag if it was set by call to chunk. */

         *flag_param |= flags_local & HAS_WIDTH;

         break;

      case '\0':
      case '|':
      case ')':
         CONVERT_FAIL ("internal error #3, `atom\'");  /* Supposed to be  */
                                                       /* caught earlier. */
      case '?':
      case '+':
      case '*':
         sprintf (Error_Text, "%c follows nothing", *(Reg_Parse - 1));
         CONVERT_FAIL (Error_Text);

      case '{':
         emit_convert_byte ('\\'); /* Quote braces. */
         emit_convert_byte ('{');

         break;

      case '[':
         {
            register unsigned int  last_value;
                     unsigned char last_emit = 0;
                     unsigned char buffer [500];
                              int  head = 0;
                              int  negated = 0;
                              int  do_brackets  = 1;
                              int  a_z_flag     = 0;
                              int  A_Z_flag     = 0;
                              int  zero_nine    = 0;
                              int  u_score_flag = 0;

            buffer [0]  = '\0';

            /* Handle characters that can only occur at the start of a class. */

            if (*Reg_Parse == '^') { /* Complement of range. */
               negated = 1;

               Reg_Parse++;
            }

            if (*Reg_Parse == ']' || *Reg_Parse == '-') {
               /* If '-' or ']' is the first character in a class,
                  it is a literal character in the class. */

               last_emit = *Reg_Parse;

               if (head >= 498) {
                  CONVERT_FAIL ("too much data in [] to convert.");
               }

               buffer [head++] = '\\'; /* Escape `]' and '-' for clarity. */
               buffer [head++] = *Reg_Parse;

               Reg_Parse++;
            }

            /* Handle the rest of the class characters. */

            while (*Reg_Parse != '\0' && *Reg_Parse != ']') {
               if (*Reg_Parse == '-') { /* Process a range, e.g [a-z]. */
                  Reg_Parse++;

                  if (*Reg_Parse == ']' || *Reg_Parse == '\0') {
                     /* If '-' is the last character in a class it is a literal
                        character.  If `Reg_Parse' points to the end of the
                        regex string, an error will be generated later. */

                     last_emit = '-';

                     if (head >= 498) {
                        CONVERT_FAIL ("too much data in [] to convert.");
                     }

                     buffer [head++] = '\\'; /* Escape '-' for clarity. */
                     buffer [head++] = '-';

                  } else {
                     if (*Reg_Parse == '\\') {
                        /* Handle escaped characters within a class range. */

                        Reg_Parse++;

                        if ((test = literal_escape (*Reg_Parse, 0))) {

                           buffer [head++] = '-';

                           if (*Reg_Parse != '\"') {
                              emit_convert_byte ('\\');
                           }

                           buffer [head++] = *Reg_Parse;
                           last_value = (unsigned int) test;
                        } else {
                           sprintf (
                              Error_Text,
                              "\\%c is an invalid escape sequence(3)",
                              *Reg_Parse);

                           CONVERT_FAIL (Error_Text);
                        }
                     } else {
                        last_value = U_CHAR_AT (Reg_Parse);

                        if (last_emit == '0' && last_value == '9') {
                           zero_nine = 1;
                           head--;
                        } else if (last_emit == 'a' && last_value == 'z') {
                           a_z_flag  = 1;
                           head--;
                        } else if (last_emit == 'A' && last_value == 'Z') {
                           A_Z_flag = 1;
                           head--;
                        } else {
                           buffer [head++] = '-';

                           if ((test = literal_escape (*Reg_Parse, 1))) {
                              /* Ordinary character matches an escape sequence;
                                 convert it to the escape sequence. */

                              if (head >= 495) {
                                 CONVERT_FAIL (
                                    "too much data in [] to convert.");
                              }

                              buffer [head++] = '\\';

                              if (test == '0') { /* Make octal escape. */
                                 test = *Reg_Parse;
                                 buffer [head++] = '0';
                                 buffer [head++] = ('0' + (test / 64));
                                 test -= (test / 64) * 64;
                                 buffer [head++] = ('0' + (test / 8));
                                 test -= (test / 8) * 8;
                                 buffer [head++] = ('0' +  test);
                              } else {
                                 buffer [head++] = test;
                              }
                           } else {
                              buffer [head++] = last_value;
                           }
                        }
                     }

                     if (last_emit > last_value) {
                        CONVERT_FAIL ("invalid [] range");
                     }

                     last_emit = (unsigned char) last_value;

                     Reg_Parse++;

                  } /* End class character range code. */
               } else if (*Reg_Parse == '\\') {
                  Reg_Parse++;

                  if ((test = literal_escape (*Reg_Parse, 0)) != '\0') {
                     last_emit = test;

                     if (head >= 498) {
                        CONVERT_FAIL ("too much data in [] to convert.");
                     }

                     if (*Reg_Parse != '\"') {
                        buffer [head++] = '\\';
                     }

                     buffer [head++] = *Reg_Parse;

                  } else {
                     sprintf (Error_Text,
                              "\\%c is an invalid escape sequence(1)",
                              *Reg_Parse);

                     CONVERT_FAIL (Error_Text);
                  }

                  Reg_Parse++;

                  /* End of class escaped sequence code */
               } else {
                  last_emit = *Reg_Parse;

                  if (*Reg_Parse == '_') {
                     u_score_flag = 1; /* Emit later if we can't do `\w'. */

                  } else if ((test = literal_escape (*Reg_Parse, 1))) {
                     /* Ordinary character matches an escape sequence;
                        convert it to the escape sequence. */

                     if (head >= 495) {
                        CONVERT_FAIL ("too much data in [] to convert.");
                     }

                     buffer [head++] = '\\';

                     if (test == '0') {  /* Make octal escape. */
                        test = *Reg_Parse;
                        buffer [head++] = '0';
                        buffer [head++] = ('0' + (test / 64));
                        test -= (test / 64) * 64;
                        buffer [head++] = ('0' + (test / 8));
                        test -= (test / 8) * 8;
                        buffer [head++] = ('0' +  test);
                     } else {
                        if (head >= 499) {
                           CONVERT_FAIL ("too much data in [] to convert.");
                        }

                        buffer [head++] = test;
                     }
                  } else {
                     if (head >= 499) {
                        CONVERT_FAIL ("too much data in [] to convert.");
                     }

                     buffer [head++] = *Reg_Parse;
                  }

                  Reg_Parse++;
               }
            } /* End of while (*Reg_Parse != '\0' && *Reg_Parse != ']') */

            if (*Reg_Parse != ']') CONVERT_FAIL ("missing right \']\'");

            buffer [head] = '\0';

            /* NOTE: it is impossible to specify an empty class.  This is
               because [] would be interpreted as "begin character class"
               followed by a literal ']' character and no "end character class"
               delimiter (']').  Because of this, it is always safe to assume
               that a class HAS_WIDTH. */

            Reg_Parse++; *flag_param |= HAS_WIDTH | SIMPLE;

            if (head == 0) {
               if (( a_z_flag &&  A_Z_flag &&  zero_nine &&  u_score_flag) ||
                   ( a_z_flag &&  A_Z_flag && !zero_nine && !u_score_flag) ||
                   (!a_z_flag && !A_Z_flag &&  zero_nine && !u_score_flag)) {

                   do_brackets = 0;
               }
            }

            if (do_brackets) {
               emit_convert_byte ('[');
               if (negated) emit_convert_byte ('^');
            }

            /* Output any shortcut escapes if we can. */

            while (a_z_flag || A_Z_flag || zero_nine || u_score_flag) {
               if (a_z_flag && A_Z_flag && zero_nine && u_score_flag) {
                  emit_convert_byte ('\\');

                  if (negated && !do_brackets) {
                     emit_convert_byte ('W');
                  } else {
                     emit_convert_byte ('w');
                  }

                  a_z_flag = A_Z_flag = zero_nine = u_score_flag = 0;
               } else if (a_z_flag && A_Z_flag) {
                  emit_convert_byte ('\\');

                  if (negated && !do_brackets) {
                     emit_convert_byte ('L');
                  } else {
                     emit_convert_byte ('l');
                  }

                  a_z_flag = A_Z_flag = 0;
               } else if (zero_nine) {
                  emit_convert_byte ('\\');

                  if (negated && !do_brackets) {
                     emit_convert_byte ('D');
                  } else {
                     emit_convert_byte ('d');
                  }

                  zero_nine = 0;
               } else if (a_z_flag) {
                  emit_convert_byte ('a');
                  emit_convert_byte ('-');
                  emit_convert_byte ('z');

                  a_z_flag = 0;
               } else if (A_Z_flag) {
                  emit_convert_byte ('A');
                  emit_convert_byte ('-');
                  emit_convert_byte ('Z');

                  A_Z_flag = 0;
               } else if (u_score_flag) {
                  emit_convert_byte ('_');

                  u_score_flag = 0;
               }
            }

            /* Output our buffered class characters. */

            for (head = 0; buffer [head] != '\0'; head++) {
               emit_convert_byte (buffer [head]);
            }

            if (do_brackets) {
               emit_convert_byte (']');
            }
         }

         break; /* End of character class code. */

         /* Fall through to Default case to handle literal escapes. */

      default:
         Reg_Parse--; /* If we fell through from the above code, we are now
                         pointing at the back slash (\) character. */
         {
            unsigned char *parse_save, *emit_save;
                     int   emit_diff, len = 0;

            /* Loop until we find a meta character or end of regex string. */

            for (; *Reg_Parse != '\0' &&
                   !strchr ((char *) Meta_Char, (int) *Reg_Parse);
                 len++) {

               /* Save where we are in case we have to back
                  this character out. */

               parse_save = Reg_Parse;
               emit_save  = Code_Emit_Ptr;

               if (*Reg_Parse == '\\') {
                  if ((test = literal_escape (*(Reg_Parse + 1), 0))) {
                     if (*(Reg_Parse + 1) != '\"') {
                        emit_convert_byte ('\\');
                     }

                     Reg_Parse++; /* Point to escaped character */
                     emit_convert_byte (*Reg_Parse);

                  } else {
                     sprintf (Error_Text,
                              "\\%c is an invalid escape sequence(2)",
                              *(Reg_Parse + 1));

                     CONVERT_FAIL (Error_Text);
                  }

                  Reg_Parse++;
               } else {
                  /* Ordinary character */

                  if ((test = literal_escape (*Reg_Parse, 1))) {
                     /* Ordinary character matches an escape sequence;
                        convert it to the escape sequence. */

                     emit_convert_byte ('\\');

                     if (test == '0') {
                        test = *Reg_Parse;
                        emit_convert_byte ('0');
                        emit_convert_byte ('0' + (test / 64));
                        test -= (test / 64) * 64;
                        emit_convert_byte ('0' + (test / 8));
                        test -= (test / 8) * 8;
                        emit_convert_byte ('0' +  test);
                     } else {
                        emit_convert_byte (test);
                     }
                  } else {
                     emit_convert_byte (*Reg_Parse);
                  }

                  Reg_Parse++;
               }

               /* If next regex token is a quantifier (?, +. *, or {m,n}) and
                  our EXACTLY node so far is more than one character, leave the
                  last character to be made into an EXACTLY node one character
                  wide for the multiplier to act on.  For example 'abcd* would
                  have an EXACTLY node with an 'abc' operand followed by a STAR
                  node followed by another EXACTLY node with a 'd' operand. */

               if (IS_QUANTIFIER (*Reg_Parse) && len > 0) {
                  Reg_Parse = parse_save; /* Point to previous regex token. */
                  emit_diff = (Code_Emit_Ptr - emit_save);

                  if (Code_Emit_Ptr == &Compute_Size) {
                     Convert_Size -= emit_diff;
                  } else { /* Write over previously emitted byte. */
                     Code_Emit_Ptr = emit_save;
                  }

                  break;
               }
            }

            if (len <= 0) CONVERT_FAIL ("internal error #4, `atom\'");

            *flag_param |= HAS_WIDTH;

            if (len == 1) *flag_param |= SIMPLE;
         }
      } /* END switch (*Reg_Parse++) */

   return (ret_val);
}

/*----------------------------------------------------------------------*
 * emit_convert_byte
 *
 * Emit (if appropriate) a byte of converted code.
 *----------------------------------------------------------------------*/

static void emit_convert_byte (unsigned char c) {

   if (Code_Emit_Ptr == &Compute_Size) {
      Convert_Size++;
   } else {
      *Code_Emit_Ptr++ = c;
   }
}

/*--------------------------------------------------------------------*
 * literal_escape
 *
 * Recognize escaped literal characters (prefixed with backslash),
 * and translate them into the corresponding character.
 *
 * Returns the proper character value or NULL if not a valid literal
 * escape.
 *--------------------------------------------------------------------*/

static unsigned char literal_escape (unsigned char c, int action) {

   static unsigned char control_escape [] =  {
      'a', 'b',
      'e',
      'f', 'n', 'r', 't', 'v', '\0'
   };

   static unsigned char control_actual [] =  {
      '\a', '\b',
#ifdef EBCDIC_CHARSET
      0x27,  /* Escape character in IBM's EBCDIC character set. */
#else
      0x1B,  /* Escape character in ASCII character set. */
#endif
      '\f', '\n', '\r', '\t', '\v', '\0'
   };

   static unsigned char valid_escape [] =  {
      'a',   'b',   'f',   'n',   'r',   't',   'v',   '(',    ')',   '[',
      ']',   '<',   '>',   '.',   '\\',  '|',   '^',   '$',   '*',   '+',
      '?',   '&',   '\"',  '\0'
   };

   static unsigned char value [] = {
      '\a',  '\b',  '\f',  '\n',  '\r',  '\t',  '\v',  '(',   ')',   '[',
      ']',   '<',   '>',   '.',   '\\',   '|',  '^',   '$',   '*',   '+',
      '?',   '&',   '\"',  '\0'
   };

   int i;

   if (action == 0) {
      for (i = 0; valid_escape [i] != '\0'; i++) {
         if (c == valid_escape [i]) return value [i];
      }
   } else if (action == 1) {
      for (i = 0; control_actual [i] != '\0'; i++) {
         if (c == control_actual [i]) {
            return control_escape [i];
         }
      }
   }

   if (action == 1) {
      if (!isprint (c)) {
         /* Signal to generate an numeric (octal) escape. */
         return '0';
      }
   }

   return 0;
}

/*----------------------------------------------------------------------*
 * ConvertSubstituteRE - Perform substitutions after a `regexp' match.
 *----------------------------------------------------------------------*/

void ConvertSubstituteRE (
   const char   *source,
   char   *dest,
   int     max) {

   register unsigned char *src;
   register unsigned char *dst;
   register unsigned char  c;
   register unsigned char  test;

   if (source == NULL || dest == NULL) {
      reg_error ("NULL parm to `ConvertSubstituteRE\'");

      return;
   }

   src = (unsigned char *) source;
   dst = (unsigned char *) dest;

   while ((c = *src++) != '\0') {

      if (c == '\\') {
         /* Process any case altering tokens, i.e \u, \U, \l, \L. */

         if (*src == 'u' || *src == 'U' || *src == 'l' || *src == 'L') {
            *dst++ = '\\';
             c     = *src++;
            *dst++ = c;

            if (c == '\0') {
               break;
            } else {
               c = *src++;
            }
         }
      }

      if (c == '&') {
         *dst++ = '&';

      } else if (c == '\\') {
         if (*src == '0') {
            /* Convert `\0' to `&' */

            *dst++ = '&'; src++;

         } else if ('1' <= *src && *src <=  '9') {
            *dst++ = '\\';
            *dst++ = *src++;

         } else if ((test = literal_escape (*src, 0)) != '\0') {
            *dst++ = '\\';
            *dst++ = *src++;

         } else if (*src == '\0') {
            /* If '\' is the last character of the replacement string, it is
               interpreted as a literal backslash. */

            *dst++ = '\\';
         } else {
            /* Old regex's allowed any escape sequence.  Convert these to
               unescaped characters that replace themselves; i.e. they don't
               need to be escaped. */

            *dst++ = *src++;
         }
      } else {
         /* Ordinary character. */

         if (((char *) dst - (char *) dest) >= (max - 1)) {
            break;
         } else {
            if ((test = literal_escape (c, 1))) {
               /* Ordinary character matches an escape sequence;
                  convert it to the escape sequence. */

               *dst++ = '\\';

               if (test == '0') { /* Make octal escape. */
                  test   = c;
                  *dst++ = '0';
                  *dst++ = ('0' + (test / 64));
                  test  -= (test / 64) * 64;
                  *dst++ = ('0' + (test / 8));
                  test  -= (test / 8) * 8;
                  *dst++ = ('0' +  test);
               } else {
                  *dst++ = test;
               }

            } else {
               *dst++ = c;
            }
         }
      }
   }

   *dst = '\0';
}

/*----------------------------------------------------------------------*
 * reg_error
 *----------------------------------------------------------------------*/

static void reg_error (char *str) {

   fprintf (
      stderr,
      "NEdit: Internal error processing regular expression (%s)\n",
      str);
}
