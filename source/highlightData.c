/*******************************************************************************
*									       *
* highlightData.c -- Maintain, and allow user to edit, highlight pattern list  *
*		     used for syntax highlighting			       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
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

#include "highlightData.h"
#include "textBuf.h"
#include "nedit.h"
#include "highlight.h"
#include "regularExp.h"
#include "preferences.h"
#include "help.h"
#include "window.h"
#include "regexConvert.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/managedList.h"
#include "../util/nedit_malloc.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef VMS
#include "../util/VMSparam.h"
#else
#ifndef __MVS__
#include <sys/param.h>
#endif
#endif /*VMS*/

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>

#ifdef HAVE_DEBUG_H
#include "../debug.h"
#endif

/* Maximum allowed number of styles (also limited by representation of
   styles as a byte - 'b') */
#define MAX_HIGHLIGHT_STYLES 128

/* Maximum number of patterns allowed in a pattern set (regular expression
   limitations are probably much more restrictive).  */
#define MAX_PATTERNS 127

/* Names for the fonts that can be used for syntax highlighting */
#define N_FONT_TYPES 4
enum fontTypes {PLAIN_FONT, ITALIC_FONT, BOLD_FONT, BOLD_ITALIC_FONT};
static const char *FontTypeNames[N_FONT_TYPES] =
   {"Plain", "Italic", "Bold", "Bold Italic"};

typedef struct {
    char *name;
    char *color;
    char *bgColor;
    int font;
} highlightStyleRec;

static int styleError(const char *stringStart, const char *stoppedAt,
       const char *message);
#if 0
static int lookupNamedPattern(patternSet *p, char *patternName);
#endif
static int lookupNamedStyle(const char *styleName);
static highlightPattern *readHighlightPatterns(char **inPtr, int withBraces,
       char **errMsg, int *nPatterns);
static int readHighlightPattern(char **inPtr, char **errMsg,
    	highlightPattern *pattern);
static patternSet *readDefaultPatternSet(const char *langModeName);
static int isDefaultPatternSet(patternSet *patSet);
static patternSet *readPatternSet(char **inPtr, int convertOld);
static patternSet *highlightError(char *stringStart, char *stoppedAt,
    	const char *message);
static char *intToStr(int i);
static char *createPatternsString(patternSet *patSet, char *indentStr);
static void setStyleByName(const char *style);
static void hsDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsOkCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static highlightStyleRec *copyHighlightStyleRec(highlightStyleRec *hs);
static void *hsGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg);
static void hsSetDisplayedCB(void *item, void *cbArg);
static highlightStyleRec *readHSDialogFields(int silent);
static void hsFreeItemCB(void *item);
static void freeHighlightStyleRec(highlightStyleRec *hs);
static int hsDialogEmpty(void);
static int updateHSList(void);
static void updateHighlightStyleMenu(void);
static void convertOldPatternSet(patternSet *patSet);
static void convertPatternExpr(char **patternRE, char *patSetName,
	char *patName, int isSubsExpr);
static Widget createHighlightStylesMenu(Widget parent);
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void langModeCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void styleDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void patTypeCB(Widget w, XtPointer clientData, XtPointer callData);
static void matchTypeCB(Widget w, XtPointer clientData, XtPointer callData);
static int checkHighlightDialogData(void);
static void updateLabels(void);
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void helpCB(Widget w, XtPointer clientData, XtPointer callData);
static void *getDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg);
static void setDisplayedCB(void *item, void *cbArg);
static void setStyleMenu(const char *styleName);
static highlightPattern *readDialogFields(int silent);
static int dialogEmpty(void);
static int updatePatternSet(void);
static patternSet *getDialogPatternSet(void);
static int patternSetsDiffer(patternSet *patSet1, patternSet *patSet2);
static highlightPattern *copyPatternSrc(highlightPattern *pat,
    	highlightPattern *copyTo);
static void freeItemCB(void *item);
static void freePatternSrc(highlightPattern *pat, int freeStruct);
static void freePatternSet(patternSet *p);

/* list of available highlight styles */
static int NHighlightStyles = 0;
static highlightStyleRec *HighlightStyles[MAX_HIGHLIGHT_STYLES];

/* Highlight styles dialog information */
static struct {
    Widget shell;
    Widget nameW;
    Widget colorW;
    Widget bgColorW;
    Widget plainW, boldW, italicW, boldItalicW;
    Widget managedListW;
    highlightStyleRec **highlightStyleList;
    int nHighlightStyles;
} HSDialog = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0};

/* Highlight dialog information */
static struct {
    Widget shell;
    Widget lmOptMenu;
    Widget lmPulldown;
    Widget styleOptMenu;
    Widget stylePulldown;
    Widget nameW;
    Widget topLevelW;
    Widget deferredW;
    Widget subPatW;
    Widget colorPatW;
    Widget simpleW;
    Widget rangeW;
    Widget parentW;
    Widget startW;
    Widget endW;
    Widget errorW;
    Widget lineContextW;
    Widget charContextW;
    Widget managedListW;
    Widget parentLbl;
    Widget startLbl;
    Widget endLbl;
    Widget errorLbl;
    Widget matchLbl;
    char *langModeName;
    int nPatterns;
    highlightPattern **patterns;
} HighlightDialog = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                     NULL, NULL, NULL, NULL, NULL, 0, NULL };

/* Pattern sources loaded from the .nedit file or set by the user */
static int NPatternSets = 0;
static patternSet *PatternSets[MAX_LANGUAGE_MODES];

static char *DefaultPatternSets[] = {
     "Ada:1:0{\n\
	Comments:\"--\":\"$\"::Comment::\n\
	String Literals:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	Character Literals:\"'(?:[^\\\\]|\\\\.)'\":::Character Const::\n\
	Ada Attributes:\"(?i'size\\s+(use)>)|'\\l[\\l\\d]*(?:_[\\l\\d]+)*\":::Ada Attributes::\n\
	Size Attribute:\"\\1\":\"\"::Keyword:Ada Attributes:C\n\
	Based Numeric Literals:\"<(?:\\d+(?:_\\d+)*)#(?:[\\da-fA-F]+(?:_[\\da-fA-F]+)*)(?:\\.[\\da-fA-F]+(?:_[\\da-fA-F]+)*)?#(?iE[+\\-]?(?:\\d+(?:_\\d+)*))?(?!\\Y)\":::Numeric Const::\n\
	Numeric Literals:\"<(?:\\d+(?:_\\d+)*)(?:\\.\\d+(?:_\\d+)*)?(?iE[+\\-]?(?:\\d+(?:_\\d+)*))?>\":::Numeric Const::\n\
	Pragma:\"(?n(?ipragma)\\s+\\l[\\l\\d]*(?:_\\l[\\l\\d]*)*\\s*\\([^)]*\\)\\s*;)\":::Preprocessor::\n\
	Withs Use:\"(?#Make \\s work across newlines)(?n(?iwith|use)(?#Leading W/S)\\s+(?#First package name)(?:\\l[\\l\\d]*(?:(_|\\.\\l)[\\l\\d]*)*)(?#Additional package names [optional])(?:\\s*,\\s*(?:\\l[\\l\\d]*(?:(_|\\.\\l)[\\l\\d]+)*))*(?#Trailing W/S)\\s*;)+\":::Preprocessor::\n\
	Predefined Types:\"(?i(?=[bcdfilps]))<(?iboolean|character|count|duration|float|integer|long_float|long_integer|priority|short_float|short_integer|string)>\":::Storage Type::D\n\
	Predefined Subtypes:\"(?i(?=[fnp]))<(?ifield|natural|number_base|positive|priority)>\":::Storage Type::D\n\
	Reserved Words:\"(?i(?=[a-gil-pr-uwx]))<(?iabort|abs|accept|access|and|array|at|begin|body|case|constant|declare|delay|delta|digits|do|else|elsif|end|entry|exception|exit|for|function|generic|goto|if|in|is|limited|loop|mod|new|not|null|of|or|others|out|package|pragma|private|procedure|raise|range|record|rem|renames|return|reverse|select|separate|subtype|task|terminate|then|type|use|when|while|with|xor)>\":::Keyword::D\n\
	Dot All:\"\\.(?iall)>\":::Storage Type::\n\
	Ada 95 Only:\"(?i(?=[aprtu]))<(?iabstract|tagged|all|protected|aliased|requeue|until)>\":::Keyword::\n\
	Labels Parent:\"<(\\l[\\l\\d]*(?:_[\\l\\d]+)*)(?n\\s*:\\s*)(?ifor|while|loop|declare|begin)>\":::Keyword::D\n\
	Labels subpattern:\"\\1\":\"\"::Label:Labels Parent:DC\n\
	Endloop labels:\"<(?nend\\s+loop\\s+(\\l[\\l\\d]*(?:_[\\l\\d]+)*\\s*));\":::Keyword::\n\
	Endloop labels subpattern:\"\\1\":\"\"::Label:Endloop labels:C\n\
	Goto labels:\"\\<\\<\\l[\\l\\d]*(?:_[\\l\\d]+)*\\>\\>\":::Flag::\n\
	Exit parent:\"((?iexit))\\s+(\\l\\w*)(?i\\s+when>)?\":::Keyword::\n\
	Exit subpattern:\"\\2\":\"\"::Label:Exit parent:C\n\
	Identifiers:\"<(?:\\l[\\l\\d]*(?:_[\\l\\d]+)*)>\":::Identifier::D}",
    "Awk:2:0{\n\
	Comment:\"#\":\"$\"::Comment::\n\
	Pattern:\"/(\\\\.|([[][]]?[^]]+[]])|[^/])+/\":::Preprocessor::\n\
	Keyword:\"<(return|print|printf|if|else|while|for|in|do|break|continue|next|exit|close|system|getline)>\":::Keyword::D\n\
	String:\"\"\"\":\"\"\"\":\"\\n\":String1::\n\
	String escape:\"\\\\(.|\\n)\":::String1:String:\n\
	Builtin functions:\"<(atan2|cos|exp|int|log|rand|sin|sqrt|srand|gsub|index|length|match|split|sprintf|sub|substr)>\":::Keyword::D\n\
	Gawk builtin functions:\"<(fflush|gensub|tolower|toupper|systime|strftime)>\":::Text Key1::D\n\
	Builtin variables:\"<(ARGC|ARGV|FILENAME|FNR|FS|NF|NR|OFMT|OFS|ORS|RLENGTH|RS|RSTART|SUBSEP)>\":::Storage Type::D\n\
	Gawk builtin variables:\"\"\"<(ARGIND|ERRNO|RT|IGNORECASE|FIELDWIDTHS)>\"\"\":::Storage Type::D\n\
	Field:\"\\$[0-9a-zA-Z_]+|\\$[ \\t]*\\([^,;]*\\)\":::Storage Type::D\n\
	BeginEnd:\"<(BEGIN|END)>\":::Preprocessor1::D\n\
	Numeric constant:\"(?<!\\Y)((0(x|X)[0-9a-fA-F]*)|[0-9.]+((e|E)(\\+|-)?)?[0-9]*)(L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::D\n\
	String pattern:\"~[ \\t]*\"\"\":\"\"\"\":\"\\n\":Preprocessor::\n\
	String pattern escape:\"\\\\(.|\\n)\":::Preprocessor:String pattern:\n\
	newline escape:\"\\\\$\":::Preprocessor1::\n\
	Function:\"function\":::Preprocessor1::D}",
    "C++:1:0{\n\
	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	cplus comment:\"//\":\"(?<!\\\\)$\"::Comment::\n\
	string:\"L?\"\"\":\"\"\"\":\"\\n\":String::\n\
	preprocessor line:\"^\\s*#\\s*(?:include|define|if|ifn?def|line|error|else|endif|elif|undef|pragma)>\":\"$\"::Preprocessor::\n\
	string escape chars:\"\\\\(?:.|\\n)\":::String1:string:\n\
	preprocessor esc chars:\"\\\\(?:.|\\n)\":::Preprocessor1:preprocessor line:\n\
	preprocessor comment:\"/\\*\":\"\\*/\"::Comment:preprocessor line:\n\
	preproc cplus comment:\"//\":\"$\"::Comment:preprocessor line:\n\
    	preprocessor string:\"L?\"\"\":\"\"\"\":\"\\n\":Preprocessor1:preprocessor line:\n\
    	prepr string esc chars:\"\\\\(?:.|\\n)\":::String1:preprocessor string:\n\
	preprocessor keywords:\"<__(?:LINE|FILE|DATE|TIME|STDC)__>\":::Preprocessor::\n\
	preprocessor keywords c++11:\"<__func__|__STDC_HOSTED__|_Pragma>\":::Preprocessor::\n\
	character constant:\"L?'\":\"'\":\"[^\\\\][^']\":Character Const::\n\
	numeric constant:\"(?<!\\Y)(?:(?:0(?:x|X)[0-9a-fA-F]*)|(?:(?:[0-9]+\\.?[0-9]*)|(?:\\.[0-9]+))(?:(?:e|E)(?:\\+|-)?[0-9]+)?)(?:L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::D\n\
	storage keyword:\"<(?:class|typename|typeid|template|friend|virtual|inline|explicit|operator|public|private|protected|const|extern|auto|register|static|mutable|unsigned|signed|volatile|char|double|float|int|long|short|bool|wchar_t|void|typedef|struct|union|enum|asm|export)>\":::Storage Type::D\n\
	storage keyword c++11:\"<(?:override|final|decltype|constexpr|noexcept)>\":::Storage Type::D\n\
	keyword:\"<(?:new|delete|this|return|goto|if|else|case|default|switch|break|continue|while|do|for|try|catch|throw|sizeof|true|false|namespace|using|dynamic_cast|static_cast|reinterpret_cast|const_cast)>\":::Keyword::D\n\
	keyword c++11:\"<(?:nullptr|static_assert|alignof)>\":::Keyword::D\n\
	braces:\"[{}]\":::Keyword::D}",
    "C:1:0 {\n\
    	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	string:\"L?\"\"\":\"\"\"\":\"\\n\":String::\n\
	preprocessor line:\"^\\s*#\\s*(?:include|define|if|ifn?def|line|error|else|endif|elif|undef|pragma)>\":\"$\"::Preprocessor::\n\
    	string escape chars:\"\\\\(?:.|\\n)\":::String1:string:\n\
    	preprocessor esc chars:\"\\\\(?:.|\\n)\":::Preprocessor1:preprocessor line:\n\
    	preprocessor comment:\"/\\*\":\"\\*/\"::Comment:preprocessor line:\n\
    	preprocessor string:\"L?\"\"\":\"\"\"\":\"\\n\":Preprocessor1:preprocessor line:\n\
    	prepr string esc chars:\"\\\\(?:.|\\n)\":::String1:preprocessor string:\n\
	preprocessor keywords:\"<__(?:LINE|FILE|DATE|TIME|STDC)__>\":::Preprocessor::\n\
	character constant:\"L?'\":\"'\":\"[^\\\\][^']\":Character Const::\n\
	numeric constant:\"(?<!\\Y)(?:(?:0(?:x|X)[0-9a-fA-F]*)|(?:(?:[0-9]+\\.?[0-9]*)|(?:\\.[0-9]+))(?:(?:e|E)(?:\\+|-)?[0-9]+)?)(?:L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::D\n\
    	storage keyword:\"<(?:const|extern|auto|register|static|unsigned|signed|volatile|char|double|float|int|long|short|void|typedef|struct|union|enum)>\":::Storage Type::D\n\
    	keyword:\"<(?:return|goto|if|else|case|default|switch|break|continue|while|do|for|sizeof)>\":::Keyword::D\n\
    	braces:\"[{}]\":::Keyword::D}",
    "CSS:1:0{\n\
	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	import rule:\"@import\\s+(url\\([^)]+\\))\\s*\":\";\"::Warning::\n\
	import delim:\"&\":\"&\"::Preprocessor:import rule:C\n\
	import url:\"\\1\":::Subroutine1:import rule:C\n\
	import media:\"(all|screen|print|projection|aural|braille|embossed|handheld|tty|tv|,)\":::Preprocessor1:import rule:\n\
	media rule:\"(@media)\\s+\":\"(?=\\{)\"::Warning::\n\
	media delim:\"&\":\"&\"::Preprocessor:media rule:C\n\
	media type:\"(all|screen|print|projection|aural|braille|embossed|handheld|tty|tv|,)\":::Preprocessor1:media rule:\n\
	charset rule:\"@charset\\s+(\"\"[^\"\"]+\"\")\\s*;\":::Preprocessor::\n\
	charset name:\"\\1\":::String:charset rule:C\n\
	font-face rule:\"@font-face\":::Preprocessor::\n\
	page rule:\"@page\":\"(?=\\{)\"::Preprocessor1::\n\
	page delim:\"&\":\"&\"::Preprocessor:page rule:C\n\
	page pseudo class:\":(first|left|right)\":::Storage Type:page rule:\n\
	declaration:\"\\{\":\"\\}\"::Warning::\n\
	declaration delims:\"&\":\"&\"::Keyword:declaration:C\n\
	declaration comment:\"/\\*\":\"\\*/\"::Comment:declaration:\n\
	property:\"<(azimuth|background(-(attachment|color|image|position|repeat))?|border(-(bottom(-(color|style|width))?|-(color|style|width)|collapse|color|left(-(color|style|width))?|right(-(color|style|width))?|spacing|style|top(-(color|style|width))?|width))?|bottom|caption-side|clear|clip|color|content|counter-(increment|reset)|cue(-(after|before))?|cursor|direction|display|elevation|empty-cells|float|font(-(family|size|size-adjust|stretch|style|variant|weight))?|height|left|letter-spacing|line-height|list-style(-(image|position|type))?|margin(-(bottom|left|right|top))?|marker-offset|marks|max-(height|width)|min-(height|width)|orphans|outline(-(color|style|width))?|overflow|padding(-(bottom|left|right|top))?|page(-break-(after|before|inside))?|pause(-(after|before))?|pitch(-range)?|play-during|position|quotes|richness|right|size|speak(-(header|numeral|punctuation))?|speech-rate|stress|table-layout|text(-(align|decoration|indent|shadow|transform))|top|unicode-bidi|vertical-align|visibility|voice-family|volume|white-space|widows|width|word-spacing|z-index)>\":::Identifier1:declaration:\n\
	value:\":\":\";\":\"\\}\":Warning:declaration:\n\
	value delims:\"&\":\"&\"::Keyword:value:C\n\
	value modifier:\"!important|inherit\":::Keyword:value:\n\
	uri value:\"<url\\([^)]+\\)\":::Subroutine1:value:\n\
	clip value:\"<rect\\(\\s*([+-]?\\d+(?:\\.\\d*)?)(in|cm|mm|pt|pc|em|ex|px)\\s*(,|\\s)\\s*([+-]?\\d+(?:\\.\\d*)?)(in|cm|mm|pt|pc|em|ex|px)\\s*(,|\\s)\\s*([+-]?\\d+(?:\\.\\d*)?)(in|cm|mm|pt|pc|em|ex|px)\\s*(,|\\s)\\s*([+-]?\\d+(?:\\.\\d*)?)(in|cm|mm|pt|pc|em|ex|px)\\s*\\)\":::Subroutine:value:\n\
	function value:\"<attr\\([^)]+\\)|<counter\\((\\l|\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?))([-\\l\\d]|\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?))*\\s*(,\\s*<(disc|circle|square|decimal|decimal-leading-zero|lower-roman|upper-roman|lower-greek|lower-alpha|lower-latin|upper-alpha|upper-latin|hebrew|armenian|georgian|cjk-ideographic|hiragana|katakana|hiragana-iroha|katakana-iroha|none)>)?\\)|<counters\\((\\l|\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?))([-\\l\\d]|\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?))*\\s*,\\s*(\"\"[^\"\"]*\"\"|'[^']*')\\s*(,\\s*<(disc|circle|square|decimal|decimal-leading-zero|lower-roman|upper-roman|lower-greek|lower-alpha|lower-latin|upper-alpha|upper-latin|hebrew|armenian|georgian|cjk-ideographic|hiragana|katakana|hiragana-iroha|katakana-iroha|none)>)?\\)\":::Subroutine:value:\n\
	color value:\"(#[A-Fa-f\\d]{6}>|#[A-Fa-f\\d]{3}>|rgb\\(([+-]?\\d+(\\.\\d*)?)\\s*,\\s*([+-]?\\d+(\\.\\d*)?)\\s*,\\s*([+-]?\\d+(\\.\\d*)?)\\)|rgb\\(([+-]?\\d+(\\.\\d*)?%)\\s*,\\s*([+-]?\\d+(\\.\\d*)?%)\\s*,\\s*([+-]?\\d+(\\.\\d*)?%)\\)|<(?iaqua|black|blue|fuchsia|gray|green|lime|maroon|navy|olive|purple|red|silver|teal|white|yellow)>|<transparent>)\":::Text Arg2:value:\n\
	dimension value:\"[+-]?(\\d*\\.\\d+|\\d+)(in|cm|mm|pt|pc|em|ex|px|deg|grad|rad|s|ms|hz|khz)>\":::Numeric Const:value:\n\
	percentage value:\"[+-]?(\\d*\\.\\d+|\\d+)%\":::Numeric Const:value:\n\
	named value:\"<(100|200|300|400|500|600|700|800|900|above|absolute|always|armenian|auto|avoid|baseline|behind|below|bidi-override|blink|block|bold|bolder|both|bottom|capitalize|caption|center(?:-left|-right)?|child|circle|cjk-ideographic|close-quote|code|collapse|compact|condensed|continuous|crop|cross(?:hair)?|cursive|dashed|decimal(?:-leading-zero)?|default|digits|disc|dotted|double|e-resize|embed|expanded|extra(?:-condensed|-expanded)|fantasy|far(?:-left|-right)|fast(?:er)?|female|fixed|georgian|groove|hebrew|help|hidden|hide|high(?:er)?|hiragana(?:-iroha)?|icon|inherit|inline(?:-table)?|inset|inside|italic|justify|katakana(?:-iroha)?|landscape|larger?|left(?:-side|wards)?|level|lighter|line-through|list-item|loud|low(?:er(?:-alpha|-greek|-latin|-roman|case)?)?|ltr|male|marker|medium|menu|message-box|middle|mix|monospace|move|n-resize|narrower|ne-resize|no(?:-close-quote|-open-quote|-repeat)|none|normal|nowrap|nw-resize|oblique|once|open-quote|out(?:set|side)|overline|pointer|portrait|pre|relative|repeat(?:-x|-y)?|ridge|right(?:-side|wards)?|rtl|run-in|s-resize|sans-serif|scroll|se-resize|semi(?:-condensed|-expanded)|separate|serif|show|silent|slow(?:er)?|small(?:-caps|-caption|er)?|soft|solid|spell-out|square|static|status-bar|sub|super|sw-resize|table(?:-caption|-cell|-column(?:-group)?|-footer-group|-header-group|-row(?:-group)?)?|text(?:-bottom|-top)?|thick|thin|top|ultra(?:-condensed|-expanded)|underline|upper(?:-alpha|-latin|-roman|case)|visible|w-resize|wait|wider|x-(?:fast|high|large|loud|low|slow|small|soft)|xx-(large|small))>\":::Text Arg2:value:\n\
	integer value:\"<\\d+>\":::Numeric Const:value:\n\
	font family:\"(?iarial|courier|impact|helvetica|lucida|symbol|times|verdana)\":::String:value:\n\
	dq string value:\"\"\"\":\"\"\"\":\"\\n\":String:value:\n\
	dq string escape:\"\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?)\":::Text Escape:dq string value:\n\
	dq string continuation:\"\\\\\\n\":::Text Escape:dq string value:\n\
	sq string value:\"'\":\"'\":\"\\n\":String:value:\n\
	sq string escape:\"\\\\([ -~\\0200-\\0377]|[\\l\\d]{1,6}\\s?)\":::Text Escape:sq string value:\n\
	sq string continuation:\"\\\\\\n\":::Text Escape:sq string value:\n\
	operators:\"[,/]\":::Keyword:value:\n\
	selector id:\"#[-\\w]+>\":::Pointer::\n\
	selector class:\"\\.[-\\w]+>\":::Storage Type::\n\
	selector pseudo class:\":(first-child|link|visited|hover|active|focus|lang(\\([\\-\\w]+\\))?)(?!\\Y)\":::Text Arg1::\n\
	selector attribute:\"\\[[^\\]]+\\]\":::Ada Attributes::\n\
	selector operators:\"[,>*+]\":::Keyword::\n\
	selector pseudo element:\":(first-letter|first-line|before|after)>\":::Text Arg::\n\
	type selector:\"<[\\l_][-\\w]*>\":::Plain::\n\
	free text:\".\":::Warning::\n\
	info:\"(?# version 1.31; author/maintainer: Joor Loohuis, joor@loohuis-consulting.nl)\":::Plain::D}",
    "Csh:1:0{\n\
	Comment:\"#\":\"$\"::Comment::\n\
	Single Quote String:\"'\":\"([^\\\\]'|^')\":\"\\n\":String::\n\
	SQ String Esc Char:\"\\\\([bcfnrt$\\n\\\\]|[0-9][0-9]?[0-9]?)\":::String1:Single Quote String:\n\
	Double Quote String:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	DQ String Esc Char:\"\\\\([bcfnrt\\n\\\\]|[0-9][0-9]?[0-9]?)\":::String1:Double Quote String:\n\
	Keywords:\"(^|[`;()])[ 	]*(return|if|endif|then|else|switch|endsw|while|end|foreach|do|done)>\":::Keyword::D\n\
	Variable Ref:\"\\$([<$0-9\\*]|[#a-zA-Z_?][0-9a-zA-Z_[\\]]*(:([ehqrtx]|gh|gt|gr))?|\\{[#0-9a-zA-Z_?][a-zA-Z0-9_[\\]]*(:([ehqrtx]|gh|gt|gr))?})\":::Identifier1::\n\
	Variable in String:\"\\$([<$0-9\\*]|[#a-zA-Z_?][0-9a-zA-Z_[\\]]*(:([ehqrtx]|gh|gt|gr))?|\\{[#0-9a-zA-Z_?][a-zA-Z0-9_[\\]]*(:([ehqrtx]|gh|gt|gr))?})\":::Identifier1:Double Quote String:\n\
	Naked Variable Cmds:\"<(unset|set|setenv|shift)[ \\t]+[0-9a-zA-Z_]*(\\[.+\\])?\":::Identifier1::\n\
	Recolor Naked Cmd:\"\\1\":::Keyword:Naked Variable Cmds:C\n\
	Built In Cmds:\"(^|\\|&|[\\|`;()])[ 	]*(alias|bg|break|breaksw|case|cd|chdir|continue|default|echo|eval|exec|exit|fg|goto|glob|hashstat|history|jobs|kill|limit|login|logout|nohup|notify|nice|onintr|popd|pushd|printenv|read|rehash|repeat|set|setenv|shift|source|suspend|time|umask|unalias|unhash|unlimit|unset|unsetenv|wait)>\":::Keyword::D\n\
	Tcsh Built In Cmds:\"(^|\\|&|[\\|`;()])[ 	]*(alloc|bindkey|builtins|complete|echotc|filetest|hup|log|sched|settc|setty|stop|telltc|uncomplete|where|which|dirs|ls-F)>\":::Keyword::D\n\
	Special Chars:\"([-{};.,<>&~=!|^%[\\]\\+\\*\\|()])\":::Keyword::D}",
    "Fortran:2:0{\n\
	Comment:\"^[Cc*!]\":\"$\"::Comment::\n\
	Bang Comment:\"!\":\"$\"::Comment::\n\
	Debug Line:\"^D\":\"$\"::Preprocessor::\n\
	String:\"'\":\"'\":\"\\n([^ \\t]| [^ \\t]|  [^ \\t]|   [^ \\t]|    [^ \\t]|     [ \\t0]| *\\t[^1-9])\":String::\n\
	Keywords:\"<(?iaccept|automatic|backspace|block|call|close|common|continue|data|decode|delete|dimension|do|else|elseif|encode|enddo|end *file|endif|end|entry|equivalence|exit|external|format|function|go *to|if|implicit|include|inquire|intrinsic|logical|map|none|on|open|parameter|pause|pointer|print|program|read|record|return|rewind|save|static|stop|structure|subroutine|system|then|type|union|unlock|virtual|volatile|while|write)>\":::Keyword::D\n\
	Data Types:\"<(?ibyte|character|complex|double *complex|double *precision|double|integer|real)(\\*[0-9]+)?>\":::Keyword::D\n\
	F90 Keywords:\"<(?iallocatable|allocate|case|case|cycle|deallocate|elsewhere|namelist|recursive|rewrite|select|where|intent|optional)>\":::Keyword::D\n\
	Continuation:\"^(     [^ \\t0]|( |  |   |    )?\\t[1-9])\":::Flag::\n\
	Continuation in String:\"\\n(     [^ \\t0]|( |  |   |    )?\\t[1-9])\":::Flag:String:}",
    "Java:3:0{\n\
	README:\"Java highlighting patterns for NEdit 5.1. Version 1.5 Author/maintainer: Joachim Lous - jlous at users.sourceforge.net\":::Flag::D\n\
	doccomment:\"/\\*\\*\":\"\\*/\"::Text Comment::\n\
	doccomment tag:\"@\\l*\":::Text Key1:doccomment:\n\
	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	cplus comment:\"//\":\"$\"::Comment::\n\
	string:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	string escape:\"(?:\\\\u[\\dA-Faf]{4}|\\\\[0-7]{1,3}|\\\\[btnfr'\"\"\\\\])\":::String1:string:\n\
	single quoted:\"'\":\"'\":\"\\n\":String::\n\
	single quoted escape:\"(?:\\\\u[\\dA-Faf]{4}|\\\\[0-7]{1,3}|\\\\[btnfr'\"\"\\\\])(?=')\":::String1:single quoted:\n\
	single quoted char:\".(?=')\":::String:single quoted:\n\
	single quoted error:\".\":::Flag:single quoted:\n\
	hex const:\"<(?i0[X][\\dA-F]+)>\":::Numeric Const::\n\
	long const:\"<(?i[\\d]+L)>\":::Numeric Const::\n\
	decimal const:\"(?<!\\Y)(?i\\d+(?:\\.\\d*)?(?:E[+\\-]?\\d+)?[FD]?|\\.\\d+(?:E[+\\-]?\\d+)?[FD]?)(?!\\Y)\":::Numeric Const::\n\
	include:\"<(?:import|package)>\":\";\":\"\\n\":Preprocessor::\n\
	classdef:\"<(?:class|interface)>\\s*\\n?\\s*([\\l_]\\w*)\":::Keyword::\n\
	classdef name:\"\\1\":\"\"::Storage Type:classdef:C\n\
	extends:\"<(?:extends)>\":\"(?=(?:<implements>|[{;]))\"::Keyword::\n\
	extends argument:\"<[\\l_][\\w\\.]*(?=\\s*(?:/\\*.*\\*/)?(?://.*)?\\n?\\s*(?:[,;{]|<implements>))\":::Storage Type:extends:\n\
	extends comma:\",\":::Keyword:extends:\n\
	extends comment:\"/\\*\":\"\\*/\"::Comment:extends:\n\
	extends cpluscomment:\"//\":\"$\"::Comment:extends:\n\
	extends error:\".\":::Flag:extends:\n\
	impl_throw:\"<(?:implements|throws)>\":\"(?=[{;])\"::Keyword::\n\
	impl_throw argument:\"<[\\l_][\\w\\.]*(?=\\s*(?:/\\*.*\\*/)?(?://.*)?\\n?\\s*[,;{])\":::Storage Type:impl_throw:\n\
	impl_throw comma:\",\":::Keyword:impl_throw:\n\
	impl_throw comment:\"/\\*\":\"\\*/\"::Comment:impl_throw:\n\
	impl_throw cpluscomment:\"//\":\"$\"::Comment:impl_throw:\n\
	impl_throw error:\".\":::Flag:impl_throw:\n\
	case:\"<case>\":\":\"::Label::\n\
	case single quoted:\"'\\\\?[^']'\":::Character Const:case:\n\
	case numeric const:\"(?<!\\Y)(?i0[X][\\dA-F]+|\\d+(:?\\.\\d*)?(?:E[+\\-]?\\d+)?F?|\\.\\d+(?:E[+\\-]?\\d+)?F?|\\d+L)(?!\\Y)\":::Numeric Const:case:\n\
	case cast:\"\\(\\s*([\\l_][\\w.]*)\\s*\\)\":::Keyword:case:\n\
	case cast type:\"\\1\":\"\"::Storage Type:case cast:C\n\
	case variable:\"[\\l_][\\w.]*\":::Identifier1:case:\n\
	case signs:\"[-+*/<>^&|%()]\":::Keyword:case:\n\
	case error:\".\":::Flag:case:\n\
	label:\"([;{}:])\":\"[\\l_]\\w*\\s*:\":\"[^\\s\\n]\":Label::\n\
	label qualifier:\"\\1\":\"\"::Keyword:label:C\n\
	labelref:\"<(?:break|continue)>\\s*\\n?\\s*([\\l_]\\w*)?(?=\\s*\\n?\\s*;)\":::Keyword::\n\
	labelref name:\"\\1\":\"\"::Label:labelref:C\n\
	instanceof:\"<instanceof>\\s*\\n?\\s*([\\l_][\\w.]*)\":::Keyword::\n\
	instanceof class:\"\\1\":\"\"::Storage Type:instanceof:C\n\
	newarray:\"new\\s*[\\n\\s]\\s*([\\l_][\\w\\.]*)\\s*\\n?\\s*(?=\\[)\":::Keyword::\n\
	newarray type:\"\\1\":\"\"::Storage Type:newarray:C\n\
	constructor def:\"<(abstract|final|native|private|protected|public|static|synchronized)\\s*[\\n|\\s]\\s*[\\l_]\\w*\\s*\\n?\\s*(?=\\()\":::Subroutine::\n\
	constructor def modifier:\"\\1\":\"\"::Keyword:constructor def:C\n\
	keyword - modifiers:\"<(?:abstract|final|native|private|protected|public|static|transient|synchronized|volatile)>\":::Keyword::\n\
	keyword - control flow:\"<(?:catch|do|else|finally|for|if|return|switch|throw|try|while)>\":::Keyword::\n\
	keyword - calc value:\"<(?:new|super|this)>\":::Keyword::\n\
	keyword - literal value:\"<(?:false|null|true)>\":::Numeric Const::\n\
	function def:\"<([\\l_][\\w\\.]*)>((?:\\s*\\[\\s*\\])*)\\s*[\\n|\\s]\\s*<[\\l_]\\w*>\\s*\\n?\\s*(?=\\()\":::Plain::\n\
	function def type:\"\\1\":\"\"::Storage Type:function def:C\n\
	function def type brackets:\"\\2\":\"\"::Keyword:function def:C\n\
	function call:\"<[\\l_]\\w*>\\s*\\n?\\s*(?=\\()\":::Plain::\n\
	cast:\"[^\\w\\s]\\s*\\n?\\s*\\(\\s*([\\l_][\\w\\.]*)\\s*\\)\":::Keyword::\n\
	cast type:\"\\1\":\"\"::Storage Type:cast:C\n\
	declaration:\"<[\\l_][\\w\\.]*>((:?\\s*\\[\\s*\\]\\s*)*)(?=\\s*\\n?\\s*(?!instanceof)[\\l_]\\w*)\":::Storage Type::\n\
	declaration brackets:\"\\1\":\"\"::Keyword:declaration:C\n\
	variable:\"<[\\l_]\\w*>\":::Identifier1::D\n\
	braces and parens:\"[(){}[\\]]\":::Keyword::D\n\
	signs:\"[-+*/%=,.;:<>!|&^?]\":::Keyword::D\n\
	error:\".\":::Flag::D}",
#ifndef VMS
/* The VAX C compiler cannot compile this definition */
    "JavaScript:1:0{\n\
	DSComment:\"//\":\"$\"::Comment::\n\
	MLComment:\"/\\*\":\"\\*/\"::Comment::\n\
	DQColors:\"aliceblue|antiquewhite|aqua|aquamarine|azure|beige|bisque|black|blanchedalmond|blue|blueviolet|brown|burlywood|cadetblue|chartreuse|chocolate|coral|cornflowerblue|cornsilk|crimson|cyan|darkblue|darkcyan|darkgoldenrod|darkgray|darkgreen|darkkhaki|darkmagenta|darkolivegreen|darkorange|darkorchid|darkred|darksalmon|darkseagreen|darkslateblue|darkslategray|darkturquoise|darkviolet|deeppink|deepskyblue|dimgray|dodgerblue|firebrick|floralwhite|forestgreen|fuchsia|gainsboro|ghostwhite|gold|goldenrod|gray|green|greenyellow|honeydew|hotpink|indianred|indigo|ivory|khaki|lavender|lavenderblush|lawngreen|lemonchiffon|lightblue|lightcoral|lightcyan|lightgoldenrodyellow|lightgreen|lightgrey|lightpink|lightsalmon|lightseagreen|lightskyblue|lightslategray|lightsteelblue|lightyellow|lime|limegreen|linen|magenta|maroon|mediumaquamarine|mediumblue|mediumorchid|mediumpurple|mediumseagreen|mediumslateblue|mediumspringgreen|mediumturquoise|mediumvioletred|midnightblue|mintcream|mistyrose|moccasin|navajowhite|navy|oldlace|olive|olivedrab|orange|orangered|orchid|palegoldenrod|palegreen|paleturquoise|palevioletred|papayawhip|peachpuff|peru|pink|plum|powderblue|purple|red|rosybrown|royalblue|saddlebrown|salmon|sandybrown|seagreen|seashell|sienna|silver|skyblue|slateblue|slategray|snow|springgreen|steelblue|tan|teal|thistle|tomato|turquoise|violet|wheat|white|whitesmoke|yellow|yellowgreen|#[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]\":::Text Arg1:DQStrings:\n\
	SQColors:\"aliceblue|antiquewhite|aqua|aquamarine|azure|beige|bisque|black|blanchedalmond|blue|blueviolet|brown|burlywood|cadetblue|chartreuse|chocolate|coral|cornflowerblue|cornsilk|crimson|cyan|darkblue|darkcyan|darkgoldenrod|darkgray|darkgreen|darkkhaki|darkmagenta|darkolivegreen|darkorange|darkorchid|darkred|darksalmon|darkseagreen|darkslateblue|darkslategray|darkturquoise|darkviolet|deeppink|deepskyblue|dimgray|dodgerblue|firebrick|floralwhite|forestgreen|fuchsia|gainsboro|ghostwhite|gold|goldenrod|gray|green|greenyellow|honeydew|hotpink|indianred|indigo|ivory|khaki|lavender|lavenderblush|lawngreen|lemonchiffon|lightblue|lightcoral|lightcyan|lightgoldenrodyellow|lightgreen|lightgrey|lightpink|lightsalmon|lightseagreen|lightskyblue|lightslategray|lightsteelblue|lightyellow|lime|limegreen|linen|magenta|maroon|mediumaquamarine|mediumblue|mediumorchid|mediumpurple|mediumseagreen|mediumslateblue|mediumspringgreen|mediumturquoise|mediumvioletred|midnightblue|mintcream|mistyrose|moccasin|navajowhite|navy|oldlace|olive|olivedrab|orange|orangered|orchid|palegoldenrod|palegreen|paleturquoise|palevioletred|papayawhip|peachpuff|peru|pink|plum|powderblue|purple|red|rosybrown|royalblue|saddlebrown|salmon|sandybrown|seagreen|seashell|sienna|silver|skyblue|slateblue|slategray|snow|springgreen|steelblue|tan|teal|thistle|tomato|turquoise|violet|wheat|white|whitesmoke|yellow|yellowgreen|(#)[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-F-af0-9]\":::Text Arg1:SQStrings:\n\
	Numeric:\"(?<!\\Y)((0(x|X)[0-9a-fA-F]*)|[0-9.]+((e|E)(\\+|-)?)?[0-9]*)(L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::\n\
	Events:\"<(onAbort|onBlur|onClick|onChange|onDblClick|onDragDrop|onError|onFocus|onKeyDown|onKeyPress|onLoad|onMouseDown|onMouseMove|onMouseOut|onMouseOver|onMouseUp|onMove|onResize|onSelect|onSubmit|onUnload)>\":::Keyword::\n\
	Braces:\"[{}]\":::Keyword::\n\
	Statements:\"<(break|continue|else|for|if|in|new|return|this|typeof|var|while|with)>\":::Keyword::\n\
	Function:\"function[\\t ]+([a-zA-Z0-9_]+)[\\t \\(]+\":\"[\\n{]\"::Keyword::\n\
	FunctionName:\"\\1\":\"\"::Storage Type:Function:C\n\
	FunctionArgs:\"\\(\":\"\\)\"::Text Arg:Function:\n\
	Parentheses:\"[\\(\\)]\":::Plain::\n\
	BuiltInObjectType:\"<(anchor|Applet|Area|Array|button|checkbox|Date|document|elements|FileUpload|form|frame|Function|hidden|history|Image|link|location|Math|navigator|Option|password|Plugin|radio|reset|select|string|submit|text|textarea|window)>\":::Storage Type::\n\
	SQStrings:\"'\":\"'\":\"\\n\":String::\n\
	DQStrings:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	EventCapturing:\"captureEvents|releaseEvents|routeEvent|handleEvent\":\"\\)\":\"\\n\":Keyword::\n\
	PredefinedMethods:\"<(abs|acos|alert|anchor|asin|atan|atan2|back|big|blink|blur|bold|ceil|charAt|clear|clearTimeout|click|close|confirm|cos|escape|eval|exp|fixed|floor|focus|fontcolor|fontsize|forward|getDate|getDay|getHours|getMinutes|getMonth|getSeconds|getTime|getTimezoneOffset|getYear|go|indexOf|isNaN|italics|javaEnabled|join|lastIndexOf|link|log|max|min|open|parse|parseFloat|parseInt|pow|prompt|random|reload|replace|reset|reverse|round|scroll|select|setDate|setHours|setMinutes|setMonth|setSeconds|setTimeout|setTime|setYear|sin|small|sort|split|sqrt|strike|sub|submit|substring|sup|taint|tan|toGMTString|toLocaleString|toLowerCase|toString|toUpperCase|unescape|untaint|UTC|write|writeln)>\":::Keyword::\n\
	Properties:\"<(action|alinkColor|anchors|appCodeName|appName|appVersion|bgColor|border|checked|complete|cookie|defaultChecked|defaultSelected|defaultStatus|defaultValue|description|E|elements|enabledPlugin|encoding|fgColor|filename|forms|frames|hash|height|host|hostname|href|hspace|index|lastModified|length|linkColor|links|LN2|LN10|LOG2E|LOG10E|lowsrc|method|name|opener|options|parent|pathname|PI|port|protocol|prototype|referrer|search|selected|selectedIndex|self|SQRT1_2|SQRT2|src|status|target|text|title|top|type|URL|userAgent|value|vlinkColor|vspace|width|window)>\":::Storage Type::\n\
	Operators:\"[= ; ->]|[/]|&|\\|\":::Preprocessor::}",
#endif /*VMS*/
    "LaTeX:1:0{\n\
	Comment:\"%\":\"$\"::Text Comment::\n\
	Parameter:\"#[0-9]*\":::Text Arg::\n\
	Special Chars:\"[{}&]\":::Keyword::\n\
	Escape Chars:\"\\\\[$&%#_{}]\":::Text Escape::\n\
	Super Sub 1 Char:\"(?:\\^|_)(?:\\\\\\l+|#\\d|[^{\\\\])\":::Text Arg2::\n\
	Verbatim Begin End:\"\\\\begin\\{verbatim\\*?}\":\"\\\\end\\{verbatim\\*?}\"::Plain::\n\
	Verbatim BG Color:\"&\":\"&\"::Keyword:Verbatim Begin End:C\n\
	Verbatim:\"(\\\\verb\\*?)([^\\l\\s\\*]).*?(\\2)\":::Plain::\n\
	Verbatim Color:\"\\1\\2\\3\":\"\"::Keyword:Verbatim:C\n\
	Inline Math:\"(?<!#\\d)(?:\\$|\\\\\\()\":\"\\$|\\\\\\)\":\"\\\\\\(|(?n[^\\\\]%)\":LaTeX Math::\n\
	Math Color:\"&\":\"&\"::Keyword:Inline Math:C\n\
	Math Escape Chars:\"\\\\\\$\":::Text Escape:Inline Math:\n\
	No Arg Command:\"\\\\(?:left|right)[\\[\\]{}()]\":::Text Key::\n\
	Command:\"[_^]|[\\\\@](?:a'|a`|a=|[A-Za-z]+\\*?|\\\\\\*|[-@_='`^\"\"|\\[\\]*:!+<>/~.,\\\\ ])\":\"nevermatch\":\"[^{[(]\":Text Key::\n\
	Cmd Brace Args:\"\\{\":\"}\":\"(?<=^%)|\\\\]|\\$\\$|\\\\end\\{equation\\}\":Text Arg2:Command:\n\
	Brace Color:\"&\":\"&\"::Text Arg:Cmd Brace Args:C\n\
	Cmd Paren Args:\"\\(\":\"\\)\":\"$\":Text Arg2:Command:\n\
	Paren Color:\"&\":\"&\"::Text Arg:Cmd Paren Args:C\n\
	Cmd Bracket Args:\"\\[\":\"\\]\":\"$|\\\\\\]\":Text Arg2:Command:\n\
	Bracket Color:\"&\":\"&\"::Text Arg:Cmd Bracket Args:C\n\
	Sub Cmd Bracket Args Esc:\"\\\\\\}\":::Plain:Sub Cmd Bracket Args:\n\
	Sub Cmd Bracket Args:\"\\{\":\"\\}\":\"$|\\\\\\]\":Preprocessor1:Cmd Bracket Args:\n\
	Sub Command:\"(?:[_^]|(?:[\\\\@](?:[A-Za-z]+\\*?|[^A-Za-z$&%#{}~\\\\ \\t])))\":::Text Key1:Cmd Brace Args:\n\
	Sub Brace:\"\\{\":\"}\"::Text Arg2:Cmd Brace Args:\n\
	Sub Sub Brace:\"\\{\":\"}\"::Text Arg2:Sub Brace:\n\
	Sub Sub Sub Brace:\"\\{\":\"}\"::Text Arg2:Sub Sub Brace:\n\
	Sub Sub Sub Sub Brace:\"\\{\":\"}\"::Text Arg2:Sub Sub Sub Brace:\n\
	Sub Paren:\"\\(\":\"\\)\":\"$\":Text Arg2:Cmd Paren Args:\n\
	Sub Sub Paren:\"\\(\":\"\\)\":\"$\":Text Arg2:Sub Paren:\n\
	Sub Sub Sub Paren:\"\\(\":\"\\)\":\"$\":Text Arg2:Sub Sub Paren:\n\
	Sub Parameter:\"#[0-9]*\":::Text Arg:Cmd Brace Args:\n\
	Sub Spec Chars:\"[{}$&]\":::Text Arg:Cmd Brace Args:\n\
	Sub Esc Chars:\"\\\\[$&%#_{}~^\\\\]\":::Text Arg1:Cmd Brace Args:}",
    "Lex:1:0{\n\
	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	string:\"L?\"\"\":\"\"\"\":\"\\n\":String::\n\
	meta string:\"\\\\\"\".*\\\\\"\"\":::String::\n\
	preprocessor line:\"^\\s*#\\s*(include|define|if|ifn?def|line|error|else|endif|elif|undef|pragma)>\":\"$\"::Preprocessor::\n\
	string escape chars:\"\\\\(.|\\n)\":::String1:string:\n\
	preprocessor esc chars:\"\\\\(.|\\n)\":::Preprocessor1:preprocessor line:\n\
	preprocessor comment:\"/\\*\":\"\\*/\"::Comment:preprocessor line:\n\
    	preprocessor string:\"L?\"\"\":\"\"\"\":\"\\n\":Preprocessor1:preprocessor line:\n\
    	prepr string esc chars:\"\\\\(?:.|\\n)\":::String1:preprocessor string:\n\
	character constant:\"'\":\"'\":\"[^\\\\][^']\":Character Const::\n\
	numeric constant:\"(?<!\\Y)((0(x|X)[0-9a-fA-F]*)|(([0-9]+\\.?[0-9]*)|(\\.[0-9]+))((e|E)(\\+|-)?[0-9]+)?)(L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::D\n\
	storage keyword:\"<(const|extern|auto|register|static|unsigned|signed|volatile|char|double|float|int|long|short|void|typedef|struct|union|enum)>\":::Storage Type::D\n\
	keyword:\"<(return|goto|if|else|case|default|switch|break|continue|while|do|for|sizeof)>\":::Keyword::D\n\
	lex keyword:\"<(yylval|yytext|input|unput|output|lex_input|lex_output|yylex|yymore|yyless|yyin|yyout|yyleng|yywtext|yywleng|yyterminate|REJECT|ECHO|BEGIN|YY_NEW_FILE|yy_create_buffer|yy_switch_to_buffer|yy_delete_buffer|YY_CURRENT_BUFFER|YY_BUFFER_STATE|YY_DECL|YY_INPUT|yywrap|YY_USER_ACTION|YY_USER_INIT|YY_BREAK)>\":::Text Arg::D\n\
	stdlib:\"<(BUFSIZ|CHAR_BIT|CHAR_MAX|CHAR_MIN|CLOCKS_PER_SEC|DBL_DIG|DBL_EPSILON|DBL_MANT_DIG|DBL_MAX|DBL_MAX_10_EXP|DBL_MAX_EXP|DBL_MIN|DBL_MIN_10_EXP|DBL_MIN_EXP|EDOM|EOF|ERANGE|EXIT_FAILURE|EXIT_SUCCESS|FILE|FILENAME_MAX|FLT_DIG|FLT_EPSILON|FLT_MANT_DIG|FLT_MAX|FLT_MAX_10_EXP|FLT_MAX_EXP|FLT_MIN|FLT_MIN_10_EXP|FLT_MIN_EXP|FLT_RADIX|FLT_ROUNDS|FOPEN_MAX|HUGE_VAL|INT_MAX|INT_MIN|LC_ALL|LC_COLLATE|LC_CTYPE|LC_MONETARY|LC_NUMERIC|LC_TIME|LDBL_DIG|LDBL_EPSILON|LDBL_MANT_DIG|LDBL_MAX|LDBL_MAX_10_EXP|LDBL_MAX_EXP|LDBL_MIN|LDBL_MIN_10_EXP|LDBL_MIN_EXP|LONG_MAX|LONG_MIN|L_tmpnam|MB_CUR_MAX|MB_LEN_MAX|NULL|RAND_MAX|SCHAR_MAX|SCHAR_MIN|SEEK_CUR|SEEK_END|SEEK_SET|SHRT_MAX|SHRT_MIN|SIGABRT|SIGFPE|SIGILL|SIGINT|SIGSEGV|SIGTERM|SIG_DFL|SIG_ERR|SIG_IGN|TMP_MAX|UCHAR_MAX|UINT_MAX|ULONG_MAX|USHRT_MAX|WCHAR_MAX|WCHAR_MIN|WEOF|_IOFBF|_IOLBF|_IONBF|abort|abs|acos|asctime|asin|assert|atan|atan2|atexit|atof|atoi|atol|bsearch|btowc|calloc|ceil|clearerr|clock|clock_t|cos|cosh|ctime|difftime|div|div_t|errno|exit|exp|fabs|fclose|feof|ferror|fflush|fgetc|fgetpos|fgets|fgetwc|fgetws|floor|fmod|fopen|fpos_t|fprintf|fputc|fputs|fputwc|fputws|fread|free|freopen|frexp|fscanf|fseek|fsetpos|ftell|fwide|fwprintf|fwrite|fwscanf|getc|getchar|getenv|gets|getwc|getwchar|gmtime|isalnum|isalpha|iscntrl|isdigit|isgraph|islower|isprint|ispunct|isspace|isupper|iswalnum|iswalpha|iswcntrl|iswctype|iswdigit|iswgraph|iswlower|iswprint|iswpunct|iswspace|iswupper|iswxdigit|isxdigit|jmp_buf|labs|lconv|ldexp|ldiv|ldiv_t|localeconv|localtime|log|log10|longjmp|malloc|mblen|mbrlen|mbrtowc|mbsinit|mbsrtowcs|mbstate_t|mbstowcs|mbtowc|memchr|memcmp|memcpy|memmove|memset|mktime|modf|offsetof|perror|pow|printf|ptrdiff_t|putc|puts|putwc|putwchar|qsort|raise|rand|realloc|remove|rename|rewind|scanf|setbuf|setjmp|setlocale|setvbuf|sig_atomic_t|signal|sin|sinh|size_t|sprintf|sqrt|srand|sscanf|stderr|stdin|stdout|strcat|strchr|strcmp|strcoll|strcpy|strcspn|strerror|strftime|strlen|strncat|strncmp|strncpy|stroul|strpbrk|strrchr|strspn|strstr|strtod|strtok|strtol|strxfrm|swprintf|swscanf|system|tan|tanh|time|time_t|tm|tmpfile|tmpnam|tolower|toupper|towctrans|towlower|towupper|ungetc|ungetwc|va_arg|va_end|va_list|va_start|vfwprintf|vprintf|vsprintf|vswprintf|vwprintf|wint_t|wmemchr|wmemcmp|wmemcpy|wmemmove|wmemset|wprintf|wscanf)>\":::Subroutine::D\n\
	label:\"<goto>|(^[ \\t]*[A-Za-z_][A-Za-z0-9_]*[ \\t]*:)\":::Flag::D\n\
	braces:\"[{}]\":::Keyword::D\n\
	markers:\"(?<!\\Y)(%\\{|%\\}|%%)(?!\\Y)\":::Flag::D}",
    "Makefile:8:0{\n\
	Comment:\"#\":\"$\"::Comment::\n\
	Comment Continuation:\"\\\\\\n\":::Keyword:Comment:\n\
	Assignment:\"^( *| [ \\t]*)[A-Za-z0-9_+][^ \\t]*[ \\t]*(\\+|:)?=\":\"$\"::Preprocessor::\n\
	Assignment Continuation:\"\\\\\\n\":::Keyword:Assignment:\n\
	Assignment Comment:\"#\":\"$\"::Comment:Assignment:\n\
	Dependency Line:\"^( *| [ \\t]*)(.DEFAULT|.DELETE_ON_ERROR|.EXPORT_ALL_VARIABLES.IGNORE|.INTERMEDIATE|.PHONY|.POSIX|.PRECIOUS|.SECONDARY|.SILENT|.SUFFIXES)*(([A-Za-z0-9./$(){} _@^<*?%+-]*(\\\\\\n)){,8}[A-Za-z0-9./$(){} _@^<*?%+-]*)::?\":\"$|;\"::Text Key1::\n\
	Dep Target Special:\"\\2\":\"\"::Text Key1:Dependency Line:C\n\
	Dep Target:\"\\3\":\"\"::Text Key:Dependency Line:C\n\
	Dep Continuation:\"\\\\\\n\":::Keyword:Dependency Line:\n\
	Dep Comment:\"#\":\"$\"::Comment:Dependency Line:\n\
	Dep Internal Macro:\"\\$([<@*?%]|\\$@)\":::Preprocessor1:Dependency Line:\n\
	Dep Macro:\"\\$([A-Za-z0-9_]|\\([^)]*\\)|\\{[^}]*})\":::Preprocessor:Dependency Line:\n\
	Continuation:\"\\\\$\":::Keyword::\n\
	Macro:\"\\$([A-Za-z0-9_]|\\([^)]*\\)|\\{[^}]*})\":::Preprocessor::\n\
	Internal Macro:\"\\$([<@*?%]|\\$@)\":::Preprocessor1::\n\
	Escaped Dollar:\"\\$\\$\":::Comment::\n\
	Include:\"^( *| [ \\t]*)include[ \\t]\":::Keyword::\n\
	Exports:\"^( *| [ \\t]*)<export|unexport>[ \\t]\":\"$\"::Keyword::\n\
	Exports var:\".[A-Za-z0-9_+]*\":\"$\"::Keyword:Exports:\n\
	Conditionals:\"^( *| [ \\t]*)<ifeq|ifneq>[ \\t]\":::Keyword::D\n\
	Conditionals ifdefs:\"^( *| [ \\t]*)<ifdef|ifndef>[ \\t]\":\"$\"::Keyword::D\n\
	Conditionals ifdefs var:\".[A-Za-z0-9_+]*\":\"$\"::Preprocessor:Conditionals ifdefs:D\n\
	Conditional Ends:\"^( *| [ \\t]*)<else|endif>\":::Keyword::D\n\
	vpath:\"^( *| [ \\t]*)<vpath>[ \\t]\":::Keyword::D\n\
	define:\"^( *| [ \\t]*)<define>[ \\t]\":\"$\"::Keyword::D\n\
	define var:\".[A-Za-z0-9_+]*\":\"$\"::Preprocessor:define:D\n\
	define Ends:\"^( *| [ \\t]*)<endef>\":::Keyword::D}",
    "Matlab:1:0{\n\
	Comment:\"%\":\"$\"::Comment::\n\
	Comment in Octave:\"#\":\"$\"::Comment::\n\
	Keyword:\"<(break|clear|else|elseif|for|function|global|if|return|then|while|end(if|for|while|function))>\":::Keyword::\n\
	Transpose:\"[\\w.]('+)\":::Plain::\n\
	Paren transposed:\"\\)('+)\":::Keyword::\n\
	Paren transp close:\"\\1\":\"\"::Plain:Paren transposed:C\n\
	Parentheses:\"[\\(\\)]\":::Keyword::\n\
	Brackets transposed:\"\\]('+)\":::Text Key1::\n\
	Brack transp close:\"\\1\":\"\"::Plain:Brackets transposed:C\n\
	Brackets:\"[\\[\\]]\":::Text Key1::\n\
	Braces transposed:\"\\}('+)\":::Text Arg::\n\
	Braces transp close:\"\\1\":\"\"::Plain:Braces transposed:C\n\
	Braces:\"[\\{\\}]\":::Text Arg::\n\
	String:\"'\":\"'\"::String::\n\
	Numeric const:\"(?<!\\Y)(((\\d+\\.?\\d*)|(\\.\\d+))([eE][+\\-]?\\d+)?)(?!\\Y)\":::Numeric Const::\n\
	Three periods to end:\"(\\.\\.\\.)\":\"$\"::Comment::\n\
	Three periods:\"\\1\":\"\"::Keyword:Three periods to end:C\n\
	Shell command:\"!\":\"$\"::String1::\n\
	Comment in shell cmd:\"%\":\"$\"::Comment:Shell command:\n\
	Relational operators:\"==|~=|\\<=|\\>=|\\<|\\>\":::Text Arg1::\n\
	Wrong logical ops:\"&&|\\|\\|\":::Plain::\n\
	Logical operators:\"~|&|\\|\":::Text Arg2::}",
    "NEdit Macro:2:0{\n\
        README:\"NEdit Macro syntax highlighting patterns, version 2.6, maintainer Thorsten Haude, nedit at thorstenhau.de\":::Flag::D\n\
        Comment:\"#\":\"$\"::Comment::\n\
        Built-in Misc Vars:\"(?<!\\Y)\\$(?:active_pane|args|calltip_ID|column|cursor|display_width|empty_array|file_name|file_path|language_mode|line|locked|max_font_width|min_font_width|modified|n_display_lines|n_panes|rangeset_list|read_only|selection_(?:start|end|left|right)|server_name|text_length|top_line)>\":::Identifier::\n\
        Built-in Pref Vars:\"(?<!\\Y)\\$(?:auto_indent|em_tab_dist|file_format|font_name|font_name_bold|font_name_bold_italic|font_name_italic|highlight_syntax|incremental_backup|incremental_search_line|make_backup_copy|match_syntax_based|overtype_mode|show_line_numbers|show_matching|statistics_line|tab_dist|use_tabs|wrap_margin|wrap_text)>\":::Identifier2::\n\
        Built-in Special Vars:\"(?<!\\Y)\\$(?:[1-9]|list_dialog_button|n_args|read_status|search_end|shell_cmd_status|string_dialog_button|sub_sep)>\":::String1::\n\
        Built-in Subrs:\"<(?:append_file|beep|calltip|clipboard_to_string|dialog|focus_window|get_character|get_pattern_(by_name|at_pos)|get_range|get_selection|get_style_(by_name|at_pos)|getenv|kill_calltip|length|list_dialog|max|min|rangeset_(?:add|create|destroy|get_by_name|includes|info|invert|range|set_color|set_mode|set_name|subtract)|read_file|replace_in_string|replace_range|replace_selection|replace_substring|search|search_string|select|select_rectangle|set_cursor_pos|set_language_mode|set_locked|shell_command|split|string_compare|string_dialog|string_to_clipboard|substring|t_print|tolower|toupper|valid_number|write_file)>\":::Subroutine::\n\
        Menu Actions:\"<(?:new|open|open-dialog|open_dialog|open-selected|open_selected|close|save|save-as|save_as|save-as-dialog|save_as_dialog|revert-to-saved|revert_to_saved|revert_to_saved_dialog|include-file|include_file|include-file-dialog|include_file_dialog|load-macro-file|load_macro_file|load-macro-file-dialog|load_macro_file_dialog|load-tags-file|load_tags_file|load-tags-file-dialog|load_tags_file_dialog|unload_tags_file|load_tips_file|load_tips_file_dialog|unload_tips_file|print|print-selection|print_selection|exit|undo|redo|delete|select-all|select_all|shift-left|shift_left|shift-left-by-tab|shift_left_by_tab|shift-right|shift_right|shift-right-by-tab|shift_right_by_tab|find|find-dialog|find_dialog|find-again|find_again|find-selection|find_selection|find_incremental|start_incremental_find|replace|replace-dialog|replace_dialog|replace-all|replace_all|replace-in-selection|replace_in_selection|replace-again|replace_again|replace_find|replace_find_same|replace_find_again|goto-line-number|goto_line_number|goto-line-number-dialog|goto_line_number_dialog|goto-selected|goto_selected|mark|mark-dialog|mark_dialog|goto-mark|goto_mark|goto-mark-dialog|goto_mark_dialog|match|select_to_matching|goto_matching|find-definition|find_definition|show_tip|split-window|split_window|close-pane|close_pane|uppercase|lowercase|fill-paragraph|fill_paragraph|control-code-dialog|control_code_dialog|filter-selection-dialog|filter_selection_dialog|filter-selection|filter_selection|execute-command|execute_command|execute-command-dialog|execute_command_dialog|execute-command-line|execute_command_line|shell-menu-command|shell_menu_command|macro-menu-command|macro_menu_command|bg_menu_command|post_window_bg_menu|beginning-of-selection|beginning_of_selection|end-of-selection|end_of_selection|repeat_macro|repeat_dialog|raise_window|focus_pane|set_statistics_line|set_incremental_search_line|set_show_line_numbers|set_auto_indent|set_wrap_text|set_wrap_margin|set_highlight_syntax|set_make_backup_copy|set_incremental_backup|set_show_matching|set_match_syntax_based|set_overtype_mode|set_locked|set_tab_dist|set_em_tab_dist|set_use_tabs|set_fonts|set_language_mode)(?=\\s*\\()\":::Subroutine::\n\
        Text Actions:\"<(?:self-insert|self_insert|grab-focus|grab_focus|extend-adjust|extend_adjust|extend-start|extend_start|extend-end|extend_end|secondary-adjust|secondary_adjust|secondary-or-drag-adjust|secondary_or_drag_adjust|secondary-start|secondary_start|secondary-or-drag-start|secondary_or_drag_start|process-bdrag|process_bdrag|move-destination|move_destination|move-to|move_to|move-to-or-end-drag|move_to_or_end_drag|end_drag|copy-to|copy_to|copy-to-or-end-drag|copy_to_or_end_drag|exchange|process-cancel|process_cancel|paste-clipboard|paste_clipboard|copy-clipboard|copy_clipboard|cut-clipboard|cut_clipboard|copy-primary|copy_primary|cut-primary|cut_primary|newline|newline-and-indent|newline_and_indent|newline-no-indent|newline_no_indent|delete-selection|delete_selection|delete-previous-character|delete_previous_character|delete-next-character|delete_next_character|delete-previous-word|delete_previous_word|delete-next-word|delete_next_word|delete-to-start-of-line|delete_to_start_of_line|delete-to-end-of-line|delete_to_end_of_line|forward-character|forward_character|backward-character|backward_character|key-select|key_select|process-up|process_up|process-down|process_down|process-shift-up|process_shift_up|process-shift-down|process_shift_down|process-home|process_home|forward-word|forward_word|backward-word|backward_word|forward-paragraph|forward_paragraph|backward-paragraph|backward_paragraph|beginning-of-line|beginning_of_line|end-of-line|end_of_line|beginning-of-file|beginning_of_file|end-of-file|end_of_file|next-page|next_page|previous-page|previous_page|page-left|page_left|page-right|page_right|toggle-overstrike|toggle_overstrike|scroll-up|scroll_up|scroll-down|scroll_down|scroll_left|scroll_right|scroll-to-line|scroll_to_line|select-all|select_all|deselect-all|deselect_all|focusIn|focusOut|process-return|process_return|process-tab|process_tab|insert-string|insert_string|mouse_pan)>\":::Subroutine::\n\
        Keyword:\"<(?:break|continue|define|delete|else|for|if|in|return|while)>\":::Keyword::\n\
        Braces:\"[{}\\[\\]]\":::Keyword::\n\
        Global Variable:\"\\$[A-Za-z0-9_]+\":::Identifier1::\n\
        String:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
        String Escape Char:\"\\\\(?:.|\\n)\":::Text Escape:String:\n\
        Numeric Const:\"(?<!\\Y)-?[0-9]+>\":::Numeric Const::\n\
        Macro Definition:\"(?<=define)\\s+\\w+\":::Subroutine1::\n\
        Custom Macro:\"\\w+(?=\\s*(?:\\\\\\n)?\\s*[\\(])\":::Subroutine1::\n\
        Variables:\"\\w+\":::Identifier1::D}",
    "Pascal:1:0{\n\
	TP Directives:\"\\{\\$\":\"\\}\"::Comment::\n\
	Comment:\"\\(\\*|\\{\":\"\\*\\)|\\}\"::Comment::\n\
	String:\"'\":\"'\":\"\\n\":String::D\n\
	Array delimitors:\"\\(\\.|\\.\\)|\\[|\\]\":::Character Const::D\n\
	Parentheses:\"\\(|\\)\":::Keyword::D\n\
	X Numeric Values:\"<([2-9]|[12]\\d|3[0-6])#[\\d\\l]+>\":::Text Key::D\n\
	TP Numeric Values:\"(?<!\\Y)(#\\d+|\\$[\\da-fA-F]+)>\":::Text Key1::D\n\
	Numeric Values:\"<\\d+(\\.\\d+)?((e|E)(\\+|-)?\\d+)?>\":::Numeric Const::D\n\
	Reserved Words 1:\"<(?iBegin|Const|End|Program|Record|Type|Var)>\":::Keyword::D\n\
	Reserved Words 2:\"<(?iForward|Goto|Label|Of|Packed|With)>\":::Identifier::D\n\
	X Reserved Words:\"<(?iBindable|Export|Implementation|Import|Interface|Module|Only|Otherwise|Protected|Qualified|Restricted|Value)>\":::Identifier1::D\n\
	TP Reserved Words:\"<(?iAbsolute|Assembler|Exit|External|Far|Inline|Interrupt|Near|Private|Unit|Uses)>\":::Text Comment::D\n\
	Data Types:\"<(?iArray|Boolean|Char|File|Integer|Real|Set|Text)>\":::Storage Type::D\n\
	X Data Types:\"<(?iBindingType|Complex|String|TimeStamp)>\":::Text Arg1::D\n\
	TP Data Types:\"<(?iByte|Comp|Double|Extended|LongInt|ShortInt|Single|Word)>\":::Text Arg2::D\n\
	Predefined Consts:\"<(?iFalse|Input|MaxInt|Nil|Output|True)>\":::String1::D\n\
	X Predefined Consts:\"<(?iEpsReal|MaxChar|MaxReal|MinReal|StandardInput|StandardOutput)>\":::String2::D\n\
	Conditionals:\"<(?iCase|Do|DownTo|Else|For|If|Repeat|Then|To|Until|While)>\":::Ada Attributes::D\n\
	Proc declaration:\"<(?iProcedure)>\":::Character Const::D\n\
	Predefined Proc:\"<(?iDispose|Get|New|Pack|Page|Put|Read|ReadLn|Reset|Rewrite|Unpack|Write|WriteLn)>\":::Subroutine::D\n\
	X Predefined Proc:\"<(?iBind|Extend|GetTimeStamp|Halt|ReadStr|SeekRead|SeekUpdate|SeekWrite|Unbind|Update|WriteStr)>\":::Subroutine1::D\n\
	Func declaration:\"<(?iFunction)>\":::Identifier::D\n\
	Predefined Func:\"<(?iAbs|Arctan|Chr|Cos|Eof|Eoln|Exp|Ln|Odd|Ord|Pred|Round|Sin|Sqr|Sqrt|Succ|Trunc)>\":::Preprocessor::D\n\
	X Predefined Func:\"<(?iArg|Binding|Card|Cmplx|Date|Empty|Eq|Ge|Gt|Im|Index|LastPosition|Le|Length|Lt|Ne|Polar|Position|Re|SubStr|Time|Trim)>\":::Preprocessor1::D\n\
	X Operators:\"(\\>\\<|\\*\\*)|<(?iAnd_Then|Or_Else|Pow)>\":::Text Arg1::D\n\
	Assignment:\":=\":::Plain::D\n\
	Operators:\"(\\<|\\>|=|\\^|@)|<(?iAnd|Div|In|Mod|Not|Or)>\":::Text Arg::D\n\
	TP Operators:\"<(?iShl|Shr|Xor)>\":::Text Arg2::D}",
    "Perl:2:0{\n\
	dq here doc:\"(\\<\\<(\"\"?))EOF(\\2.*)$\":\"^EOF>\"::Label::\n\
	dq here doc delims:\"\\1\\3\":::Keyword:dq here doc:C\n\
	dq here doc esc chars:\"\\\\([nrtfbaeulULQE@%\\$\\\\]|0[0-7]+|x[0-9a-fA-F]+|c\\l)\":::Text Escape:dq here doc:\n\
	dq here doc variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:dq here doc:\n\
	dq here doc content:\".\":::String:dq here doc:\n\
	dq string:\"(?<!\\Y)\"\"\":\"\"\"\":\"\\n\\s*\\n\":String::\n\
	dq string delims:\"&\":\"&\"::Keyword:dq string:C\n\
	dq string esc chars:\"\\\\([nrtfbaeulULQE\"\"@%\\$\\\\]|0[0-7]+|x[0-9a-fA-F]+|c\\l)\":::Text Escape:dq string:\n\
	dq string variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:dq string:\n\
	gen dq string:\"<qq/\":\"(?!\\\\)/\":\"\\n\\s*\\n\":String::\n\
	gen dq string delims:\"&\":\"&\"::Keyword:gen dq string:C\n\
	gen dq string esc chars:\"\\\\([nrtfbaeulULQE@%\\$\\\\]|0[0-7]+|x[0-9a-fA-F]+|c\\l)\":::Text Escape:gen dq string:\n\
	gen dq string variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:gen dq string:\n\
	sq here doc:\"(\\<\\<')EOF('.*)$\":\"^EOF>\"::Label::\n\
	sq here doc delims:\"\\1\\2\":::Keyword:sq here doc:C\n\
	sq here doc esc chars:\"\\\\\\\\\":::Text Escape:sq here doc:\n\
	sq here doc content:\".\":::String1:sq here doc:\n\
	sq string:\"(?<!\\Y)'\":\"'\":\"\\n\\s*\\n\":String1::\n\
	sq string delims:\"&\":\"&\"::Keyword:sq string:C\n\
	sq string esc chars:\"\\\\(\\\\|')\":::Text Escape:sq string:\n\
	gen sq string:\"<q/\":\"(?!\\\\)/\":\"\\n\\s*\\n\":String1::\n\
	gen sq string delims:\"&\":\"&\"::Keyword:gen sq string:C\n\
	gen sq string esc chars:\"\\\\(\\\\|/)\":::Text Escape:gen sq string:\n\
	implicit sq:\"[-\\w]+(?=\\s*=\\>)|(\\{)[-\\w]+(\\})\":::String1::\n\
	implicit sq delims:\"\\1\\2\":::Keyword:implicit sq:C\n\
	word list:\"<qw\\(\":\"\\)\":\"\\n\\s*\\n\":Keyword::\n\
	word list content:\".\":::String1:word list:\n\
	bq here doc:\"(\\<\\<`)EOF(`.*)$\":\"^EOF>\"::Label::\n\
	bq here doc delims:\"\\1\\2\":::Keyword:bq here doc:C\n\
	bq here doc comment:\"#\":\"$\"::Comment:bq here doc:\n\
	bq here doc variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:bq here doc:\n\
	bq here doc content:\".\":::String1:bq here doc:\n\
	bq string:\"(?<!\\Y)`\":\"`(?!\\Y)\":\"\\n\\s*\\n\":String1::\n\
	bq string delims:\"&\":\"&\"::Keyword:bq string:C\n\
	bq string variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:bq string:\n\
	gen bq string:\"<qx/\":\"(?!\\\\)/\":\"\\n\\s*\\n\":String1::\n\
	gen bq string delims:\"&\":\"&\"::Keyword:gen bq string:C\n\
	gen bq string variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:gen bq string:\n\
	gen bq string esc chars:\"\\\\/\":::Text Escape:gen bq string:\n\
	transliteration:\"<((y|tr)/)(\\\\/|[^/])+(/)(\\\\/|[^/])*(/[cds]*)\":::String::D\n\
	transliteration delims:\"\\1\\4\\6\":::Keyword:transliteration:DC\n\
	last array index:\"\\$#([\\l_](\\w|::(?=\\w))*)?\":::Identifier1::\n\
	comment:\"#\":\"$\"::Comment::\n\
	label:\"((?:^|;)\\s*<([A-Z_]+)>\\s*:(?=(?:[^:]|\\n)))|(goto|last|next|redo)\\s+(<((if|unless)|[A-Z_]+)>|)\":::Plain::\n\
	label identifier:\"\\2\\5\":::Label:label:C\n\
	label keyword:\"\\3\\6\":::Keyword:label:C\n\
	handle:\"(\\<)[A-Z_]+(\\>)|(bind|binmode|close(?:dir)?|connect|eof|fcntl|fileno|flock|getc|getpeername|getsockname|getsockopt|ioctl|listen|open(?:dir)?|recv|read(?:dir)?|rewinddir|seek(?:dir)?|send|setsockopt|shutdown|socket|sysopen|sysread|sysseek|syswrite|tell(?:dir)?|write)>\\s*(\\(?)\\s*[A-Z_]+>|<(accept|pipe|socketpair)>\\s*(\\(?)\\s*[A-Z_]+\\s*(,)\\s*[A-Z_]+>|(print|printf|select)>\\s*(\\(?)\\s*[A-Z_]+>(?!\\s*,)\":::Storage Type::\n\
	handle delims:\"\\1\\2\\4\\6\\7\\9\":::Keyword:handle:C\n\
	handle functions:\"\\3\\5\\8\":::Subroutine:handle:C\n\
	statements:\"<(if|until|while|elsif|else|unless|for(each)?|continue|last|goto|next|redo|do(?=\\s*\\{)|BEGIN|END)>\":::Keyword::D\n\
	packages and modules:\"<(bless|caller|import|no|package|prototype|require|return|INIT|CHECK|BEGIN|END|use|new)>\":::Keyword::D\n\
	pragm modules:\"<(attrs|autouse|base|blib|constant|diagnostics|fields|integer|less|lib|locale|ops|overload|re|sigtrap|strict|subs|vars|vmsish)>\":::Keyword::D\n\
	standard methods:\"<(can|isa|VERSION)>\":::Keyword::D\n\
	file tests:\"-[rwxRWXoOezsfdlSpbcugktTBMAC]>\":::Subroutine::D\n\
	subr header:\"<sub\\s+<([\\l_]\\w*)>\":\"(?:\\{|;)\"::Keyword::D\n\
	subr header coloring:\"\\1\":::Plain:subr header:DC\n\
	subr prototype:\"\\(\":\"\\)\"::Flag:subr header:D\n\
	subr prototype delims:\"&\":\"&\"::Keyword:subr prototype:DC\n\
	subr prototype chars:\"\\\\?[@$%&*]|;\":::Identifier1:subr prototype:D\n\
	references:\"\\\\(\\$|@|%|&)(::)?[\\l_](\\w|::(?=\\w))*|\\\\(\\$?|@|%|&)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|\\\\(\\$|@|%|&)(?=\\{)\":::Identifier1::\n\
	variables:\"\\$([-_./,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1::\n\
	named operators:\"<(lt|gt|le|ge|eq|ne|cmp|not|and|or|xor|sub|x)>\":::Keyword::D\n\
	library functions:\"<((?# arithmetic functions)abs|atan2|cos|exp|int|log|rand|sin|sqrt|srand|time|(?# conversion functions)chr|gmtime|hex|localtime|oct|ord|vec|(?# structure conversion)pack|unpack|(?# string functions)chomp|chop|crypt|eval(?=\\s*[^{])|index|lc|lcfirst|length|quotemeta|rindex|substr|uc|ucfirst|(?# array and hash functions)delete|each|exists|grep|join|keys|map|pop|push|reverse|scalar|shift|sort|splice|split|unshift|values|(?# search and replace functions)pos|study|(?# file operations)chmod|chown|link|lstat|mkdir|readlink|rename|rmdir|stat|symlink|truncate|unlink|utime|(?# input/output)binmode|close|eof|fcntl|fileno|flock|getc|ioctl|open|pipe|print|printf|read|readline|readpipe|seek|select|sprintf|sysopen|sysread|sysseek|syswrite|tell|(?# formats)formline|write|(?# tying variables)tie|tied|untie|(?# directory reading routines)closedir|opendir|readdir|rewinddir|seekdir|telldir|(?# system interaction)alarm|chdir|chroot|die|exec|exit|fork|getlogin|getpgrp|getppid|getpriority|glob|kill|setpgrp|setpriority|sleep|syscall|system|times|umask|wait|waitpid|warn|(?# networking)accept|bind|connect|getpeername|getsockname|getsockopt|listen|recv|send|setsockopt|shutdown|socket|socketpair|(?# system V ipc)msgctl|msgget|msgrcv|msgsnd|semctl|semget|semop|shmctl|shmget|shmread|shmwrite|(?# miscellaneous)defined|do|dump|eval(?=\\s*\\{)|local|my|ref|reset|undef|(?# informations from system databases)endpwent|getpwent|getpwnam|getpwuid|setpwent|endgrent|getgrent|getgrgid|getgrnam|setgrent|endnetent|getnetbyaddr|getnetbyname|getnetent|setnetent|endhostend|gethostbyaddr|gethostbyname|gethostent|sethostent|endservent|getservbyname|getservbyport|getservent|setservent|endprotoent|getprotobyname|getprotobynumber|getprotoent|setprotoent)>\":::Subroutine::\n\
	subroutine call:\"(&|-\\>)\\w(\\w|::)*(?!\\Y)|<\\w(\\w|::)*(?=\\s*\\()\":::Subroutine1::D\n\
	symbolic operators:\">[-<>+.*/\\\\?!~=%^&:]<\":::Keyword::D\n\
	braces and parens:\"[\\[\\]{}\\(\\)\\<\\>]\":::Keyword::D\n\
	numerics:\"(?<!\\Y)((?i0x[\\da-f]+)|0[0-7]+|(\\d+\\.?\\d*|\\.\\d+)([eE][\\-+]?\\d+)?|[\\d_]+)(?!\\Y)\":::Numeric Const::D\n\
	tokens:\"__(FILE|PACKAGE|LINE|DIE|WARN)__\":::Preprocessor::D\n\
	end token:\"^__(END|DATA)__\":\"never_match_this_pattern\"::Plain::\n\
	end token delim:\"&\":::Preprocessor:end token:C\n\
	pod:\"(?=^=)\":\"^=cut\"::Text Comment::\n\
	re match:\"(?<!\\Y)((m|qr|~\\s*)/)\":\"(/(gc?|[imosx])*)\"::Plain::\n\
	re match delims:\"&\":\"&\"::Keyword:re match:C\n\
	re match esc chars:\"\\\\([/abdeflnrstuwzABDEGLQSUWZ+?.*$^(){}[\\]|\\\\]|0[0-7]{2}|x[0-9a-fA-F]{2})\":::Text Escape:re match:\n\
	re match class:\"\\[\\^?\":\"\\]\"::Plain:re match:\n\
	re match class delims:\"&\":\"&\"::Regex:re match class:C\n\
	re match class esc chars:\"\\\\([abdeflnrstuwzABDEGLQSUWZ^\\]\\\\-]|0[0-7]{2}|x[0-9a-fA-F]{2})\":::Text Escape:re match class:\n\
	re match variables:\"\\$([-_.,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:re match:\n\
	re match comment:\"\\(\\?#[^)]*\\)\":::Comment:re match:\n\
	re match syms:\"[.^$[\\])|)]|\\{\\d+(,\\d*)?\\}\\??|\\((\\?([:=!>imsx]|\\<[=!]))?|[?+*]\\??\":::Regex:re match:\n\
	re match refs:\"\\\\[1-9]\\d?\":::Identifier1:re match:\n\
	re sub:\"<(s/)\":\"(/)((?:\\\\/|\\\\[1-9]\\d?|[^/])*)(/[egimosx]*)\"::Plain::\n\
	re sub delims:\"\\1\":\"\\1\\3\"::Keyword:re sub:C\n\
	re sub subst:\"\\2\":\"\\2\"::String:re sub:C\n\
	re sub esc chars:\"\\\\([/abdeflnrstuwzABDEGLQSUWZ+?.*$^(){}[\\]|\\\\]|0[0-7]{2}|x[0-9a-fA-F]{2})\":::Text Escape:re sub:\n\
	re sub class:\"\\[\\^?\":\"\\]\"::Plain:re sub:\n\
	re sub class delims:\"&\":\"&\"::Regex:re sub class:C\n\
	re sub class esc chars:\"\\\\([abdeflnrstuwzABDEGLQSUWZ^\\]\\\\-]|0[0-7]{2}|x[0-9a-fA-F]{2})\":::Text Escape:re sub class:\n\
	re sub variables:\"\\$([-_.,\"\"\\\\*?#;!@$<>(%=~^|&`'+[\\]]|:(?!:)|\\^[ADEFHILMOPSTWX]|ARGV|\\d{1,2})|(@|\\$#)(ARGV|EXPORT|EXPORT_OK|F|INC|ISA|_)>|%(ENV|EXPORT_TAGS|INC|SIG)>|(\\$#?|@|%)(::)?[\\l_](\\w|::(?=\\w))*|(\\$#?|@|%)\\{(::)?[\\l_](\\w|::(?=\\w))*\\}|(\\$|@|%)(?=\\{)\":::Identifier1:re sub:\n\
	re sub comment:\"\\(\\?#[^)]*\\)\":::Comment:re sub:\n\
	re sub syms:\"[.^$[\\])|)]|\\{\\d+(,\\d*)?\\}\\??|\\((\\?([:=!>imsx]|\\<[=!]))?|[?+*]\\??\":::Regex:re sub:\n\
	re sub refs:\"\\\\[1-9]\\d?\":::Identifier1:re sub:\n\
	info:\"version: 2.02p1; author/maintainer: Joor Loohuis, joor@loohuis-consulting.nl\":::Plain::}",
    "PostScript:1:0{\n\
	DSCcomment:\"^%[%|!]\":\"$\"::Preprocessor::\n\
	Comment:\"%\":\"$\"::Comment::\n\
	string:\"\\(\":\"\\)\"::String::\n\
	string esc chars:\"\\\\(n|r|t|b|f|\\\\|\\(|\\)|[0-9][0-9]?[0-9]?)?\":::String2:string:\n\
	string2:\"\\(\":\"\\)\"::String:string:\n\
	string2 esc chars:\"\\\\(n|r|t|b|f|\\\\|\\(|\\)|[0-9][0-9]?[0-9]?)?\":::String2:string2:\n\
	string3:\"\\(\":\"\\)\"::String:string2:\n\
	string3 esc chars:\"\\\\(n|r|t|b|f|\\\\|\\(|\\)|[0-9][0-9]?[0-9]?)?\":::String2:string3:\n\
	ASCII 85 string:\"\\<~\":\"~\\>\":\"[^!-uz]\":String1::\n\
	Dictionary:\"(\\<\\<|\\>\\>)\":::Storage Type::\n\
	hex string:\"\\<\":\"\\>\":\"[^0-9a-fA-F> \\t]\":String1::\n\
	Literal:\"/[^/%{}\\(\\)\\<\\>\\[\\]\\f\\n\\r\\t ]*\":::Text Key::\n\
	Number:\"(?<!\\Y)((([2-9]|[1-2][0-9]|3[0-6])#[0-9a-zA-Z]*)|(((\\+|-)?[0-9]+\\.?[0-9]*)|((\\+|-)?\\.[0-9]+))((e|E)(\\+|-)?[0-9]+)?)(?!\\Y)\":::Numeric Const::D\n\
	Array:\"[\\[\\]]\":::Storage Type::D\n\
	Procedure:\"[{}]\":::Subroutine::D\n\
	Operator1:\"(?<!\\Y)(=|==|abs|add|aload|anchorsearch|and|arc|arcn|arcto|array|ashow|astore|atan|awidthshow|begin|bind|bitshift|bytesavailable|cachestatus|ceiling|charpath|clear|cleardictstack|cleartomark|clip|clippath|closefile|closepath|concat|concatmatrix|copy|copypage|cos|count|countdictstack|countexecstack|counttomark|currentdash|currentdict|currentfile|currentflat|currentfont|currentgray|currenthsbcolor|currentlinecap|currentlinejoin|currentlinewidth|currentmatrix|currentmiterlimit|currentpoint|currentrgbcolor|currentscreen|currenttransfer|curveto|cvi|cvlit|cvn|cvr|cvrs|cvs|cvx|def|defaultmatrix|definefont|dict|dictstack|div|dtransform|dup|echo|eexec|end|eoclip|eofill|eq|erasepage|errordict|exch|exec|execstack|executeonly|executive|exit|exitserver|exp|false|file|fill|findfont|flattenpath|floor|flush|flushfile|FontDirectory|for|forall|ge|get|getinterval|grestore|grestoreall|gsave|gt|handleerror|identmatrix|idiv|idtransform|if|ifelse|image|imagemask|index|initclip|initgraphics|initmatrix|internaldict|invertmatrix|itransform|known|kshow|le|length|lineto|ln|load|log|loop|lt|makefont|mark|matrix|maxlength|mod|moveto|mul|ne|neg|newpath|noaccess|not|null|nulldevice|or|pathbbox|pathforall|pop|print|prompt|pstack|put|putinterval|quit|rand|rcheck|rcurveto|read|readhexstring|readline|readonly|readstring|repeat|resetfile|restore|reversepath|rlineto|rmoveto|roll|rotate|round|rrand|run|save|scale|scalefont|search|serverdict|setcachedevice|setcachelimit|setcharwidth|setdash|setflat|setfont|setgray|sethsbcolor|setlinecap|setlinejoin|setlinewidth|setmatrix|setmiterlimit|setrgbcolor|setscreen|settransfer|show|showpage|sin|sqrt|srand|stack|StandardEncoding|start|status|statusdict|stop|stopped|store|string|stringwidth|stroke|strokepath|sub|systemdict|token|transform|translate|true|truncate|type|userdict|usertime|version|vmstatus|wcheck|where|widthshow|write|writehexstring|writestring|xcheck|xor)(?!\\Y)\":::Keyword::D\n\
	Operator2:\"<(arct|colorimage|cshow|currentblackgeneration|currentcacheparams|currentcmykcolor|currentcolor|currentcolorrendering|currentcolorscreen|currentcolorspace|currentcolortransfer|currentdevparams|currentglobal|currentgstate|currenthalftone|currentobjectformat|currentoverprint|currentpacking|currentpagedevice|currentshared|currentstrokeadjust|currentsystemparams|currentundercolorremoval|currentuserparams|defineresource|defineuserobject|deletefile|execform|execuserobject|filenameforall|fileposition|filter|findencoding|findresource|gcheck|globaldict|GlobalFontDirectory|glyphshow|gstate|ineofill|infill|instroke|inueofill|inufill|inustroke|ISOLatin1Encoding|languagelevel|makepattern|packedarray|printobject|product|realtime|rectclip|rectfill|rectstroke|renamefile|resourceforall|resourcestatus|revision|rootfont|scheck|selectfont|serialnumber|setbbox|setblackgeneration|setcachedevice2|setcacheparams|setcmykcolor|setcolor|setcolorrendering|setcolorscreen|setcolorspace|setcolortransfer|setdevparams|setfileposition|setglobal|setgstate|sethalftone|setobjectformat|setoverprint|setpacking|setpagedevice|setpattern|setshared|setstrokeadjust|setsystemparams|setucacheparams|setundercolorremoval|setuserparams|setvmthreshold|shareddict|SharedFontDirectory|startjob|uappend|ucache|ucachestatus|ueofill|ufill|undef|undefinefont|undefineresource|undefineuserobject|upath|UserObjects|ustroke|ustrokepath|vmreclaim|writeobject|xshow|xyshow|yshow)>\":::Keyword::D\n\
	Operator3:\"<(GetHalftoneName|GetPageDeviceName|GetSubstituteCRD|StartData|addglyph|beginbfchar|beginbfrange|begincidchar|begincidrange|begincmap|begincodespacerange|beginnotdefchar|beginnotdefrange|beginrearrangedfont|beginusematrix|cliprestore|clipsave|composefont|currentsmoothness|currenttrapparams|endbfchar|endbfrange|endcidchar|endcidrange|endcmap|endcodespacerange|endnotdefchar|endnotdefrange|endrearrangedfont|endusematrix|findcolorrendering|removeall|removeglyphs|setsmoothness|settrapparams|settrapzone|shfill|usecmap|usefont)>\":::Keyword::D\n\
	Old operator:\"<(condition|currentcontext|currenthalftonephase|defineusername|detach|deviceinfo|eoviewclip|fork|initviewclip|join|lock|monitor|notify|rectviewclip|sethalftonephase|viewclip|viewclippath|wait|wtranslation|yield)>\":::Keyword::D}",
    "Python:2:0{\n\
	Comment:\"#\":\"$\"::Comment::\n\
	String3s:\"[uU]?[rR]?'{3}\":\"'{3}\"::String::\n\
	String3d:\"[uU]?[rR]?\"\"{3}\":\"\"\"{3}\"::String::\n\
	String1s:\"[uU]?[rR]?'\":\"'\":\"$\":String::\n\
	String1d:\"[uU]?[rR]?\"\"\":\"\"\"\":\"$\":String::\n\
	String escape chars 3s:\"\\\\(?:\\n|\\\\|'|\"\"|a|b|f|n|r|t|v|[0-7]{1,3}|x[\\da-fA-F]{2}|u[\\da-fA-F]{4}|U[\\da-fA-F]{8})\":::String1:String3s:\n\
	String escape chars 3d:\"\\\\(?:\\n|\\\\|'|\"\"|a|b|f|n|r|t|v|[0-7]{1,3}|x[\\da-fA-F]{2}|u[\\da-fA-F]{4}|U[\\da-fA-F]{8})\":::String1:String3d:\n\
	String escape chars 1s:\"\\\\(?:\\n|\\\\|'|\"\"|a|b|f|n|r|t|v|[0-7]{1,3}|x[\\da-fA-F]{2}|u[\\da-fA-F]{4}|U[\\da-fA-F]{8})\":::String1:String1s:\n\
	String escape chars 1d:\"\\\\(?:\\n|\\\\|'|\"\"|a|b|f|n|r|t|v|[0-7]{1,3}|x[\\da-fA-F]{2}|u[\\da-fA-F]{4}|U[\\da-fA-F]{8})\":::String1:String1d:\n\
	Representation:\"`\":\"`\":\"$\":String2::\n\
	Representation cont:\"\\\\\\n\":::String2:Representation:\n\
	Number:\"(?<!\\Y)(?:(?:(?:[1-9]\\d*|(?:[1-9]\\d*|0)?\\.\\d+|(?:[1-9]\\d*|0)\\.)[eE][\\-+]?\\d+|(?:[1-9]\\d*|0)?\\.\\d+|(?:[1-9]\\d*|0)\\.)[jJ]?|(?:[1-9]\\d*|0)[jJ]|(?:0|[1-9]\\d*|0[oO]?[0-7]+|0[xX][\\da-fA-F]+|0[bB][0-1]+)[lL]?)(?!\\Y)\":::Numeric Const::\n\
	Multiline import:\"<from>.*?\\(\":\"\\)\"::Preprocessor::\n\
	Multiline import comment:\"#\":\"$\"::Comment:Multiline import:\n\
	Import:\"<(?:import|from)>\":\";|$\":\"#\":Preprocessor::\n\
	Import continuation:\"\\\\\\n\":::Preprocessor:Import:\n\
	Member definition:\"<(def)\\s+(?:(__(?:abs|add|and|call|cmp|coerce|complex|contains|del|delattr|delete|delitem|div|divmod|enter|eq|exit|float|floordiv|format|ge|get|getattr|getitem|gt|hash|hex|iadd|iand|idiv|ifloordiv|ilshift|imod|imul|index|init|int|invert|ior|ipow|irshift|isub|iter|itruediv|ixor|le|len|long|lshift|lt|mod|mul|ne|neg|nonzero|oct|or|pos|pow|radd|rand|rdiv|rdivmod|repr|reversed|rfloordiv|rlshift|rmod|rmul|ror|rpow|rrshift|rshift|rsub|rtruediv|rxor|set|setattr|setitem|str|sub|truediv|unicode|xor)__)|((__bases__|__class__|__dict__|__doc__|__func__|__metaclass__|__module__|__name__|__self__|__slots__|co_argcount|co_cellvars|co_code|co_filename|co_firstlineno|co_flags|co_lnotab|co_name|co_names|co_nlocals|co_stacksize|co_varnames|f_back|f_builtins|f_code|f_exc_traceback|f_exc_type|f_exc_value|f_globals|f_lasti|f_restricted|f_trace|func_closure|func_code|func_defaults|func_dict|func_doc|func_globals|func_name|im_class|im_func|im_self|tb_frame|tb_lasti|tb_next)|(__(?:delslice|getslice|setslice)__)|(__(?:members|methods)__))|(and|as|assert|break|continue|def|del|elif|else|except|exec|finally|for|from|if|import|in|is|not|or|pass|print|raise|return|try|while|with|yield|class|global|lambda)|([\\l_]\\w*))(?=(?:\\s*(?:\\\\\\n\\s*)?\\(\\s*|\\s*\\(\\s*(?:\\\\?\\n\\s*)?)self>)\":::Plain::\n\
	Member def color:\"\\1\":::Keyword:Member definition:C\n\
	Member def special:\"\\2\":::Subroutine:Member definition:C\n\
	Member def deprecated:\"\\3\":::Warning:Member definition:C\n\
	Member def error:\"\\7\":::Flag:Member definition:C\n\
	Static method definition:\"<(def)\\s+(__(?:new)__)\":::Plain::\n\
	Static def color:\"\\1\":::Keyword:Static method definition:C\n\
	Static def special:\"\\2\":::Subroutine:Static method definition:C\n\
	Function definition:\"<(def)\\s+(?:(ArithmeticError|AssertionError|AttributeError|BaseException|BufferError|BytesWarning|DeprecationWarning|EOFError|Ellipsis|EnvironmentError|Exception|False|FloatingPointError|FutureWarning|GeneratorExit|IOError|ImportError|ImportWarning|IndentationError|IndexError|KeyError|KeyboardInterrupt|LookupError|MemoryError|NameError|None|NotImplemented|NotImplementedError|OSError|OverflowError|PendingDeprecationWarning|ReferenceError|RuntimeError|RuntimeWarning|StandardError|StopIteration|SyntaxError|SyntaxWarning|SystemError|SystemExit|TabError|True|TypeError|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError|UnicodeTranslateError|UnicodeWarning|UserWarning|ValueError|Warning|WindowsError|ZeroDivisionError|__builtins__|__debug__|__doc__|__import__|__name__|abs|all|any|apply|basestring|bin|bool|buffer|bytearray|bytes|callable|chr|classmethod|cmp|coerce|compile|complex|copyright|credits|delattr|dict|dir|divmod|enumerate|eval|execfile|exit|file|filter|float|format|frozenset|getattr|globals|hasattr|hash|help|hex|id|input|int|intern|isinstance|issubclass|iter|len|license|list|locals|long|map|max|min|object|oct|open|ord|pow|property|quit|range|raw_input|reduce|reload|repr|reversed|round|self|set|setattr|slice|sorted|staticmethod|str|sum|super|tuple|type|unichr|unicode|vars|xrange|zip)|(and|as|assert|break|continue|def|del|elif|else|except|exec|finally|for|from|if|import|in|is|not|or|pass|print|raise|return|try|while|with|yield|class|global|lambda)|([\\l_]\\w*))>\":::Plain::\n\
	Function def color:\"\\1\":::Keyword:Function definition:C\n\
	Function def deprecated:\"\\2\":::Warning:Function definition:C\n\
	Function def error:\"\\3\":::Flag:Function definition:C\n\
	Class definition:\"<(class)\\s+(?:(ArithmeticError|AssertionError|AttributeError|BaseException|BufferError|BytesWarning|DeprecationWarning|EOFError|Ellipsis|EnvironmentError|Exception|False|FloatingPointError|FutureWarning|GeneratorExit|IOError|ImportError|ImportWarning|IndentationError|IndexError|KeyError|KeyboardInterrupt|LookupError|MemoryError|NameError|None|NotImplemented|NotImplementedError|OSError|OverflowError|PendingDeprecationWarning|ReferenceError|RuntimeError|RuntimeWarning|StandardError|StopIteration|SyntaxError|SyntaxWarning|SystemError|SystemExit|TabError|True|TypeError|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError|UnicodeTranslateError|UnicodeWarning|UserWarning|ValueError|Warning|WindowsError|ZeroDivisionError|__builtins__|__debug__|__doc__|__import__|__name__|abs|all|any|apply|basestring|bin|bool|buffer|bytearray|bytes|callable|chr|classmethod|cmp|coerce|compile|complex|copyright|credits|delattr|dict|dir|divmod|enumerate|eval|execfile|exit|file|filter|float|format|frozenset|getattr|globals|hasattr|hash|help|hex|id|input|int|intern|isinstance|issubclass|iter|len|license|list|locals|long|map|max|min|object|oct|open|ord|pow|property|quit|range|raw_input|reduce|reload|repr|reversed|round|self|set|setattr|slice|sorted|staticmethod|str|sum|super|tuple|type|unichr|unicode|vars|xrange|zip)|(and|as|assert|break|continue|def|del|elif|else|except|exec|finally|for|from|if|import|in|is|not|or|pass|print|raise|return|try|while|with|yield|class|global|lambda)|([\\l_]\\w*))>\":::Plain::\n\
	Class def color:\"\\1\":::Storage Type:Class definition:C\n\
	Class def deprecated:\"\\2\":::Warning:Class definition:C\n\
	Class def error:\"\\3\":::Flag:Class definition:C\n\
	Member reference:\"\\.\\s*(?:\\\\?\\n\\s*)?(?:((__(?:abs|add|and|call|cmp|coerce|complex|contains|del|delattr|delete|delitem|div|divmod|enter|eq|exit|float|floordiv|format|ge|get|getattr|getitem|gt|hash|hex|iadd|iand|idiv|ifloordiv|ilshift|imod|imul|index|init|int|invert|ior|ipow|irshift|isub|iter|itruediv|ixor|le|len|long|lshift|lt|mod|mul|ne|neg|nonzero|oct|or|pos|pow|radd|rand|rdiv|rdivmod|repr|reversed|rfloordiv|rlshift|rmod|rmul|ror|rpow|rrshift|rshift|rsub|rtruediv|rxor|set|setattr|setitem|str|sub|truediv|unicode|xor)__)|(__(?:new)__))|((__(?:delslice|getslice|setslice)__)|(__(?:members|methods)__))|(__bases__|__class__|__dict__|__doc__|__func__|__metaclass__|__module__|__name__|__self__|__slots__|co_argcount|co_cellvars|co_code|co_filename|co_firstlineno|co_flags|co_lnotab|co_name|co_names|co_nlocals|co_stacksize|co_varnames|f_back|f_builtins|f_code|f_exc_traceback|f_exc_type|f_exc_value|f_globals|f_lasti|f_restricted|f_trace|func_closure|func_code|func_defaults|func_dict|func_doc|func_globals|func_name|im_class|im_func|im_self|tb_frame|tb_lasti|tb_next)|(and|as|assert|break|continue|def|del|elif|else|except|exec|finally|for|from|if|import|in|is|not|or|pass|print|raise|return|try|while|with|yield|class|global|lambda)|([\\l_]\\w*))>\":::Plain::\n\
	Member special method:\"\\1\":::Subroutine:Member reference:C\n\
	Member deprecated:\"\\4\":::Warning:Member reference:C\n\
	Member special attrib:\"\\7\":::Identifier1:Member reference:C\n\
	Member ref error:\"\\8\":::Flag:Member reference:C\n\
	Storage keyword:\"<(?:class|global|lambda)>\":::Storage Type::\n\
	Keyword:\"<(?:and|as|assert|break|continue|def|del|elif|else|except|exec|finally|for|from|if|import|in|is|not|or|pass|print|raise|return|try|while|with|yield)>\":::Keyword::\n\
	Built-in function:\"<(?:__import__|abs|all|any|basestring|bin|bool|bytearray|bytes|callable|chr|classmethod|cmp|compile|complex|delattr|dict|dir|divmod|enumerate|eval|execfile|exit|file|filter|float|format|frozenset|getattr|globals|hasattr|hash|help|hex|id|input|int|isinstance|issubclass|iter|len|list|locals|long|map|max|min|object|oct|open|ord|pow|property|quit|range|raw_input|reduce|reload|repr|reversed|round|set|setattr|slice|sorted|staticmethod|str|sum|super|tuple|type|unichr|unicode|vars|xrange|zip)>\":::Subroutine::\n\
	Built-in name:\"<(?:Ellipsis|False|None|NotImplemented|True|__builtins__|__debug__|__doc__|__name__|copyright|credits|license|self)>\":::Identifier1::\n\
	Built-in exceptions:\"<(?:ArithmeticError|AssertionError|AttributeError|BaseException|BufferError|EOFError|EnvironmentError|Exception|FloatingPointError|GeneratorExit|IOError|ImportError|IndentationError|IndexError|KeyError|KeyboardInterrupt|LookupError|MemoryError|NameError|NotImplementedError|OSError|OverflowError|ReferenceError|RuntimeError|StandardError|StopIteration|SyntaxError|SystemError|SystemExit|TabError|TypeError|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError|UnicodeTranslateError|ValueError|WindowsError|ZeroDivisionError)>\":::Identifier1::\n\
	Built-in warnings:\"<(?:BytesWarning|DeprecationWarning|FutureWarning|ImportWarning|PendingDeprecationWarning|RuntimeWarning|SyntaxWarning|UnicodeWarning|UserWarning|Warning)>\":::Identifier1::\n\
	Deprecated function:\"<(?:apply|buffer|coerce|intern)>\":::Warning::\n\
	Braces and parens:\"[[{()}\\]]\":::Keyword::D\n\
	Decorator:\"(@)\":\"$\":\"#\":Preprocessor1::\n\
	Decorator continuation:\"\\\\\\n\":::Preprocessor1:Decorator:\n\
	Decorator marker:\"\\1\":::Storage Type:Decorator:C\n\
	Operators:\"\\+|-|\\*|\\*\\*|/|//|%|\\<\\<|\\>\\>|\\&|\\||\\^|~|\\<|\\>|\\<=|\\>=|==|!=\":::Keyword::\n\
	Delimiter:\"\\(|\\)|\\[|\\]|\\{|\\}|,|:|\\.|;|=|\\+=|-=|\\*=|/=|//=|%=|\\&=|\\|=|\\^=|\\>\\>=|\\<\\<=|\\*\\*=\":::Keyword::\n\
	Invalid:\"\\$|\\?|<(?:0[bB]\\w+|0[xX]\\w+|(?:0|[1-9]\\d*)\\w+)>\":::Flag::}",
    "Regex:1:0{\n\
	Comments:\"(?#This is a comment!)\\(\\?#[^)]*(?:\\)|$)\":::Comment::\n\
	Literal Escape:\"(?#Special chars that need escapes)\\\\[abefnrtv()\\[\\]<>{}.|^$*+?&\\\\]\":::Preprocessor::\n\
	Shortcut Escapes:\"(?#Shortcuts for common char classes)\\\\[dDlLsSwW]\":::Character Const::\n\
	Backreferences:\"(?#Internal regex backreferences)\\\\[1-9]\":::Storage Type::\n\
	Word Delimiter:\"(?#Special token to match NEdit [non]word-delimiters)\\\\[yY]\":::Subroutine::\n\
	Numeric Escape:\"(?#Negative lookahead is to exclude \\x0 and \\00)(?!\\\\[xX0]0*(?:[^\\da-fA-F]|$))\\\\(?:[xX]0*[1-9a-fA-F][\\da-fA-F]?|0*[1-3]?[0-7]?[0-7])\":::Numeric Const::\n\
	Quantifiers:\"(?#Matches greedy and lazy quantifiers)[*+?]\\??\":::Flag::\n\
	Counting Quantifiers:\"(?#Properly limits range numbers to 0-65535)\\{(?:[0-5]?\\d?\\d?\\d?\\d|6[0-4]\\d\\d\\d|65[0-4]\\d\\d|655[0-2]\\d|6553[0-5])?(?:,(?:[0-5]?\\d?\\d?\\d?\\d|6[0-4]\\d\\d\\d|65[0-4]\\d\\d|655[0-2]\\d|6553[0-5])?)?\\}\\??\":::Numeric Const::\n\
	Character Class:\"(?#Handles escapes, char ranges, ^-] at beginning and - at end)\\[\\^?[-\\]]?(?:(?:\\\\(?:[abdeflnrstvwDLSW\\-()\\[\\]<>{}.|^$*+?&\\\\]|[xX0][\\da-fA-F]+)|[^\\\\\\]])(?:-(?:\\\\(?:[abdeflnrstvwDLSW\\-()\\[\\]<>{}.|^$*+?&\\\\]|[xX0][\\da-fA-F]+)|[^\\\\\\]]))?)*\\-?]\":::Character Const::\n\
	Anchors:\"(?#\\B is the \"\"not a word boundary\"\" anchor)[$^<>]|\\\\B\":::Flag::\n\
	Parens and Alternation:\"\\(?:\\?(?:[:=!iInN])|[()|]\":::Keyword::\n\
	Match Themselves:\"(?#Highlight chars left over which just match themselves).\":::Text Comment::D}",
    "SGML HTML:6:0{\n\
	markup declaration:\"\\<!\":\"\\>\"::Plain::\n\
	mdo-mdc:\"&\":\"&\"::Storage Type:markup declaration:C\n\
	markup declaration dq string:\"\"\"\":\"\"\"\"::String1:markup declaration:\n\
	markup declaration sq string:\"'\":\"'\"::String1:markup declaration:\n\
	entity declaration:\"((?ientity))[ \\t\\n][ \\t]*\\n?[ \\t]*(%[ \\t\\n][ \\t]*\\n?[ \\t]*)?(\\l[\\l\\d\\-\\.]*|#((?idefault)))[ \\t\\n][ \\t]*\\n?[ \\t]*((?i[cs]data|pi|starttag|endtag|m[ds]))?\":::Preprocessor:markup declaration:\n\
	ed name:\"\\2\":\"\"::String2:element declaration:C\n\
	ed type:\"\\4\":\"\"::Storage Type:entity declaration:C\n\
	doctype declaration:\"((?idoctype))[ \\t\\n][ \\t]*\\n?[ \\t]*(\\l[\\l\\d\\-\\.]*)\":::Preprocessor:markup declaration:\n\
	dt name:\"\\2\":\"\"::String2:doctype declaration:C\n\
	element declaration:\"((?ielement))[ \\t\\n][ \\t]*\\n?[ \\t]*(\\l[\\l\\d\\-\\.]*)\":::Preprocessor:markup declaration:\n\
	attribute declaration:\"((?iattlist))[ \\t\\n][ \\t]*\\n?[ \\t]*(\\l[\\l\\d\\-\\.]*)\":::Preprocessor:markup declaration:\n\
	ad name:\"\\2\":\"\"::String2:attribute declaration:C\n\
	notation declaration:\"((?inotation))[ \\t\\n][ \\t]*\\n?[ \\t]*(\\l[\\l\\d\\-\\.]*)\":::Preprocessor:markup declaration:\n\
	nd name:\"\\2\":\"\"::String2:notation declaration:C\n\
	shortref declaration:\"((?ishortref))[ \\t\\n][ \\t]*\\n?[ \\t]*(\\l[\\l\\d\\-\\.]*)\":::Preprocessor:markup declaration:\n\
	sd name:\"\\2\":\"\"::String2:shortref declaration:C\n\
	comment:\"\\-\\-\":\"\\-\\-\"::Comment:markup declaration:\n\
	pi:\"\\<\\?[^\\>]*\\??\\>\":::Flag::\n\
	stag:\"(\\<)(\\(\\l[\\w\\-\\.:]*\\))?\\l[\\w\\-\\.:]*\":\"/?\\>\"::Text Key1::\n\
	stago-tagc:\"\\1\":\"&\"::Text Arg:stag:C\n\
	Attribute:\"([\\l\\-]+)[ \\t\\v]*\\n?[ \\t\\v]*=[ \\t\\v]*\\n?[ \\t\\v]*(\"\"([^\"\"]*\\n){,4}[^\"\"]*\"\"|'([^']*\\n){,4}[^']*'|\\&([^;]*\\n){,4}[^;]*;|[\\w\\-\\.:]+)\":::Plain:stag:\n\
	Attribute name:\"\\1\":\"\"::Text Arg2:Attribute:C\n\
	Attribute value:\"\\2\":\"\"::String:Attribute:C\n\
	Boolean Attribute:\"([\\l\\-]+)\":::Text Arg1:stag:\n\
	etag:\"(\\</)(\\(\\l[\\w\\-\\.:]*\\))?(\\l[\\w\\-\\.:]*[ \\t\\v]*\\n?[ \\t\\v]*)?(\\>)\":::Text Key1::\n\
	etago-tagc:\"\\1\\4\":\"\"::Text Arg:etag:C\n\
	Character reference:\"\\&((\\(\\l[\\l\\d\\-\\.]*\\))?\\l[\\l\\d]*|#\\d+|#[xX][a-fA-F\\d]+);?\":::Text Escape::\n\
	parameter entity:\"%(\\(\\l[\\l\\d\\-\\.]*\\))?\\l[\\l\\d\\-\\.]*;?\":::Text Escape::\n\
	md parameter entity:\"%(\\(\\l[\\l\\d\\-\\.]*\\))?\\l[\\l\\d\\-\\.]*;?\":::Text Escape:markup declaration:\n\
	system-public id:\"<(?isystem|public|cdata)>\":::Storage Type:markup declaration:}",
    "SQL:1:0{\n\
	keywords:\",|%|\\<|\\>|:=|=|<(SELECT|ON|FROM|ORDER BY|DESC|WHERE|AND|OR|NOT|NULL|TRUE|FALSE)>\":::Keyword::\n\
	comment:\"--\":\"$\"::Comment::\n\
	data types:\"<(CHAR|VARCHAR2\\([0-9]*\\)|INT[0-9]*|POINT|BOX|TEXT|BOOLEAN|VARCHAR2|VARCHAR|NUMBER\\([0-9]*\\)|NUMBER)(?!\\Y)\":::Storage Type::\n\
	string:\"'\":\"'\"::String::\n\
	keywords2:\"END IF;|(?<!\\Y)(CREATE|REPLACE|BEGIN|END|FUNCTION|RETURN|FETCH|OPEN|CLOSE| IS|NOTFOUND|CURSOR|IF|ELSE|THEN|INTO|IS|IN|WHEN|OTHERS|GRANT|ON|TO|EXCEPTION|SHOW|SET|OUT|PRAGMA|AS|PACKAGE)>\":::Preprocessor1::\n\
	comment2:\"/\\*\":\"\\*/\"::Comment::}",
    "Sh Ksh Bash:1:0{\n\
        README:\"Shell syntax highlighting patterns, version 2.2, maintainer Thorsten Haude, nedit at thorstenhau.de\":::Flag::D\n\
        escaped special characters:\"\\\\[\\\\\"\"$`']\":::Keyword::\n\
        single quoted string:\"'\":\"'\"::String1::\n\
        double quoted string:\"\"\"\":\"\"\"\"::String::\n\
        double quoted escape:\"\\\\[\\\\\"\"$`]\":::String2:double quoted string:\n\
        dq command sub:\"`\":\"`\":\"\"\"\":Subroutine:double quoted string:\n\
        dq arithmetic expansion:\"\\$\\(\\(\":\"\\)\\)\":\"\"\"\":String:double quoted string:\n\
        dq new command sub:\"\\$\\(\":\"\\)\":\"\"\"\":Subroutine:double quoted string:\n\
        dqncs single quoted string:\"'\":\"'\"::String1:dq new command sub:\n\
        dq variables:\"\\$([-*@#?$!0-9]|[a-zA-Z_][0-9a-zA-Z_]*)\":::Identifier1:double quoted string:\n\
        dq variables2:\"\\$\\{\":\"}\":\"\\n\":Identifier1:double quoted string:\n\
        arithmetic expansion:\"\\$\\(\\(\":\"\\)\\)\"::String::\n\
        ae escapes:\"\\\\[\\\\$`\"\"']\":::String2:arithmetic expansion:\n\
        ae single quoted string:\"'\":\"'\":\"\\)\\)\":String1:arithmetic expansion:\n\
        ae command sub:\"`\":\"`\":\"\\)\\)\":Subroutine:arithmetic expansion:\n\
        ae arithmetic expansion:\"\\$\\(\\(\":\"\\)\\)\"::String:arithmetic expansion:\n\
        ae new command sub:\"\\$\\(\":\"\\)\":\"\\)\\)\":Subroutine:arithmetic expansion:\n\
        ae variables:\"\\$([-*@#?$!0-9]|[a-zA-Z_][0-9a-zA-Z_]*)\":::Identifier1:arithmetic expansion:\n\
        ae variables2:\"\\$\\{\":\"}\":\"\\)\\)\":Identifier1:arithmetic expansion:\n\
        comments:\"^[ \\t]*#\":\"$\"::Comment::\n\
        command substitution:\"`\":\"`\"::Subroutine::\n\
        cs escapes:\"\\\\[\\\\$`\"\"']\":::Subroutine1:command substitution:\n\
        cs single quoted string:\"'\":\"'\":\"`\":String1:command substitution:\n\
        cs variables:\"\\$([-*@#?$!0-9]|[a-zA-Z_][0-9a-zA-Z_]*)\":::Identifier1:command substitution:\n\
        cs variables2:\"\\$\\{\":\"}\":\"`\":Identifier1:command substitution:\n\
        new command substitution:\"\\$\\(\":\"\\)\"::Subroutine::\n\
        ncs new command substitution:\"\\$\\(\":\"\\)\"::Subroutine:new command substitution:\n\
        ncs escapes:\"\\\\[\\\\$`\"\"']\":::Subroutine1:new command substitution:\n\
        ncs single quoted string:\"'\":\"'\"::String1:new command substitution:\n\
        ncs variables:\"\\$([-*@#?$!0-9]|[a-zA-Z_][0-9a-zA-Z_]*)\":::Identifier1:new command substitution:\n\
        ncs variables2:\"\\$\\{\":\"}\":\"\\)\":Identifier1:new command substitution:\n\
        assignment:\"[a-zA-Z_][0-9a-zA-Z_]*=\":::Identifier1::\n\
        variables:\"\\$([-*@#?$!0-9_]|[a-zA-Z_][0-9a-zA-Z_]*)\":::Identifier1::\n\
        variables2:\"\\$\\{\":\"}\"::Identifier1::\n\
        internal var:\"\\$\\{\":\"}\"::Identifier1:variables2:\n\
        comments in line:\"#\":\"$\"::Comment::\n\
        numbers:\"<(?i0x[\\da-f]+)|((\\d*\\.)?\\d+([eE][-+]?\\d+)?(?iul?|l|f)?)>\":::Numeric Const::D\n\
        keywords:\"(?<!\\Y)(if|fi|then|else|elif|case|esac|while|for|do|done|in|select|time|until|function|\\[\\[|\\]\\])(?!\\Y)[\\s\\n]\":::Keyword::D\n\
        command options:\"(?<=\\s)-[^ \\t{}[\\],()'\"\"~!@#$%^&*|\\\\<>?]+\":::Identifier::\n\
        delimiters:\"[{};<>&~=!|^%[\\]+*|]\":::Text Key::D\n\
        built ins:\"(?<!\\Y)(:|\\.|source|alias|bg|bind|break|builtin|cd|chdir|command|compgen|complete|continue|declare|dirs|disown|echo|enable|eval|exec|exit|export|fc|fg|getopts|hash|help|history|jobs|kill|let|local|logout|popd|print|printf|pushd|pwd|read|readonly|return|set|shift|shopt|stop|suspend|test|times|trap|type|typeset|ulimit|umask|unalias|unset|wait|whence)(?!\\Y)[\\s\\n;]\":::Subroutine1::D}",
    "Tcl:1:0{\n\
	Double Quote String:\"\"\"\":\"\"\"\"::String::\n\
	Single Quote String:\"'\":\"'\":\"[^\\\\][^']\":String::\n\
	Ignore Escaped Chars:\"\\\\(.|\\n)\":::Plain::\n\
	Variable Ref:\"\\$\\w+|\\$\\{[^}]*}|\\$|#auto\":::Identifier1::\n\
	Comment:\"#\":\"$\"::Comment::\n\
	Keywords:\"<(after\\s+(\\d+|cancel|idle|info)?|append|array\\s+(anymore|donesearch|exists|get|names|nextelement|set|size|startsearch|unset)|bell|bgerror|binary\\s+(format|scan)|bind(tags)?|body|break|case|catch|cd|class|clipboard\\s+(clear|append)|clock\\s+(clicks|format|scan|seconds)|close|code|common|concat|configbody|constructor|continue|delete\\s+(class|object|namespace)|destroy|destructor|else|elseif|encoding\\s+(convertfrom|convertto|names|system)|ensemble|eof|error|eval|event\\s+(add|delete|generate|info)|exec|exit|expr|fblocked|fconfigure|fcopy|file\\s+(atime|attributes|channels|copy|delete|dirname|executable|exists|extension|isdirectory|isfile|join|lstat|mkdir|mtime|nativename|owned|pathtype|readable|readlink|rename|rootname|size|split|stat|tail|type|volume|writable)|fileevent|find\\s+(classes|objects)|flush|focus|font\\s+(actual|configure|create|delete|families|measure|metrics|names)|foreach|format|gets|glob(al)?|grab\\s+(current|release|set|status|(-global\\s+)?\\w+)|grid(\\s+bbox|(column|row)?configure|forget|info|location|propagate|remove|size|slaves)?|history\\s+(add|change|clear|event|info|keep|nextid|redo)|if|image\\s+(create|delete|height|names|type|width)|incr|info\\s+(args|body|cmdcount|commands|complete|default|exists|globals|hostname|level|library|loaded|locals|nameofexecutable|patchlevel|procs|script|sharedlibextension|tclversion|vars)|inherit|interp\\s+(alias(es)?|create|delete|eval|exists|expose|hide|hidden|invokehidden|issafe|marktrusted|share|slaves|target|transfer)|join|lappend|lindex|linsert|list|llength|load|local|lrange|lreplace|lsearch|lsort|method|memory\\s+(info|(trace|validate)\\s+(on|off)|trace_on_at_malloc|break_on_malloc|display)|namespace\\s+(children|code|current|delete|eval|export|forget|import|inscope|origin|parent|qualifiers|tail|which)|open|option\\s+(add|clear|get|read(file))|pack\\s+(configure|forget|info|propagate|slaves)?|package\\s+(forget|ifneeded|names|present|provide|require|unknown|vcompare|versions|vsatisfies)|pid|place\\s+(configure|forget|info|slaves)?|proc|puts|pwd|raise|read|regexp|regsub|rename|resource\\s+(close|delete|files|list|open|read|types|write)|return|scan|scope(dobject)?|seek|selection\\s+(clear|get|handle|own)|send|set|socket|source|split|string\\s+(bytelength|compare|equal|first|index|is|last|length|map|match|range|repeat|replace|tolower|totitle|toupper|trim|trimleft|trimright|wordend|wordstart)|subst|switch|tell|time|tk\\s+(appname|scaling|useinputmethods)|tk_(bindForTraversal|bisque|chooseColor|chooseDirectory|dialog|focusFollowsMouse|focusNext|focusPrev|getOpenFile|getSaveFile|menuBar|messageBox|optionMenu|popup|setPalette)|tkerror|tkwait\\s+(variable|visibility|window)|trace\\s+(variable|vdelete|vinfo)|unknown|unset|update|uplevel|upvar|usual|variable|while|winfo\\s+(atom|atomname|cells|children|class|colormapfull|containing|depth|exists|fpixels|geometry|height|id|interp|ismapped|manager|name|parent|pathname|pixels|pointerx|pointerxy|pointery|reqheight|reqwidth|rgb|rootx|rooty|screen(cells|depth|height|mmheigth|mmidth|visual|width)?|server|toplevel|viewable|visual(id|savailable)?|vroot(height|width|x|y)|width|x|y)|wm\\s+(aspect|client|colormapwindows|command|deiconify|focusmodel|frame|geometry|grid|group|iconbitmap|icon(ify|mask|name|position|window)|(max|min)size|overrideredirect|positionfrom|protocol|resizable|sizefrom|state|title|transient|withdraw))(?!\\Y)\":::Keyword::D\n\
	Widgets:\"<(button(box){0,1}|calendar|canvas(printbox|printdialog){0,1}|check(box|button)|combobox|date(entry|field)|dialog(shell){0,1}|entry(field){0,1}|(ext){0,1}fileselection(box|dialog)|feedback|finddialog|frame|hierarchy|hyperhelp|label(edframe|edwidget){0,1}|listbox|mainwindow|menu(bar|button){0,1}|message(box|dialog){0,1}|notebook|optionmenu|panedwindow|promptdialog|pushbutton|radio(box|button)|scale|scrollbar|scrolled(canvas|frame|html|listbox|text)|selection(box|dialog)|shell|spin(date|int|ner|time)|tab(notebook|set)|text|time(entry|field)|toolbar|toplevel|watch)>\":::Identifier::\n\
	Braces and Brackets:\"[\\[\\]{}]\":::Keyword::D\n\
	DQ String Esc Chars:\"\\\\(.|\\n)\":::String1:Double Quote String:\n\
	SQ String Esc Chars:\"\\\\(.|\\n)\":::String1:Single Quote String:\n\
	Variable in String:\"\\$\\w+|\\$\\{[^}]*}|\\$\":::Identifier1:Double Quote String:\n\
	Storage:\"<(public|private|protected)>\":::Storage Type::\n\
	Namespace:\"\\w+::\":::Keyword::}",
    "VHDL:1:0{\n\
	Comments:\"--\":\"$\"::Comment::\n\
	String Literals:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	Vhdl Attributes:\"'[a-zA-Z][a-zA-Z_]+\":::Ada Attributes::\n\
	Character Literals:\"'\":\"'\":\"[^\\\\][^']\":Character Const::\n\
	Numeric Literals:\"(?<!\\Y)(((2#|8#|10#|16#)[_0-9a-fA-F]*#)|[0-9.]+)(?!\\Y)\":::Numeric Const::\n\
	Predefined Types:\"<(?ialias|constant|signal|variable|subtype|type|resolved|boolean|string|integer|natural|time)>\":::Storage Type::D\n\
	Predefined SubTypes:\"<(?istd_logic|std_logic_vector|std_ulogic|std_ulogic_vector|bit|bit_vector)>\":::Storage Type::D\n\
	Reserved Words:\"<(?iabs|access|after|all|and|architecture|array|assert|attribute|begin|block|body|buffer|bus|case|component|configuration|disconnect|downto|else|elsif|end|entity|error|exit|failure|file|for|function|generate|generic|guarded|if|in|inout|is|label|library|linkage|loop|map|mod|nand|new|next|nor|not|note|null|of|on|open|or|others|out|package|port|procedure|process|range|record|register|rem|report|return|select|severity|then|to|transport|units|until|use|wait|warning|when|while|with|xor|group|impure|inertial|literal|postponed|pure|reject|rol|ror|shared|sla|sll|sra|srl|unaffected|xnor)>\":::Keyword::D\n\
	Identifiers:\"<([a-zA-Z][a-zA-Z0-9_]*)>\":::Plain::D\n\
	Flag Special Comments:\"--\\<[^a-zA-Z0-9]+\\>\":::Flag:Comments:\n\
	Instantiation:\"([a-zA-Z][a-zA-Z0-9_]*)([ \\t]+):([ \\t]+)([a-zA-Z][a-zA-Z0-9_]*)([ \\t]+)(port|generic|map)\":::Keyword::\n\
	Instance Name:\"\\1\":\"\"::Identifier1:Instantiation:C\n\
	Component Name:\"\\4\":\"\"::Identifier:Instantiation:C\n\
	Syntax Character:\"(\\<=|=\\>|:|=|:=|;|,|\\(|\\))\":::Keyword::}",
    "Verilog:1:0{\n\
	Comment:\"/\\*\":\"\\*/\"::Comment::\n\
	cplus comment:\"//\":\"$\"::Comment::\n\
	String Literals:\"\"\"\":\"\"\"\":\"\\n\":String::\n\
	preprocessor line:\"^[ ]*`\":\"$\"::Preprocessor::\n\
	Reserved WordsA:\"(?<!\\Y)(module|endmodule|parameter|specify|endspecify|begin|end|initial|always|if|else|task|endtask|force|release|attribute|case|case[xz]|default|endattribute|endcase|endfunction|endprimitive|endtable|for|forever|function|primitive|table|while|;)(?!\\Y)\":::Keyword::\n\
	Predefined Types:\"<(and|assign|buf|bufif[01]|cmos|deassign|defparam|disable|edge|event|force|fork|highz[01]|initial|inout|input|integer|join|large|macromodule|medium|nand|negedge|nmos|nor|not|notif[01]|or|output|parameter|pmos|posedge|pullup|rcmos|real|realtime|reg|release|repeat|rnmos|rpmos|rtran|rtranif[01]|scalered|signed|small|specparam|strength|strong[01]|supply[01]|time|tran|tranif[01]|tri[01]?|triand|trior|trireg|unsigned|vectored|wait|wand|weak[01]|wire|wor|xnor|xor)>\":::Storage Type::D\n\
	System Functions:\"\\$[a-z_]+\":::Subroutine::D\n\
	Numeric Literals:\"(?<!\\Y)([0-9]*'[dD][0-9xz\\\\?_]+|[0-9]*'[hH][0-9a-fxz\\\\?_]+|[0-9]*'[oO][0-7xz\\\\?_]+|[0-9]*'[bB][01xz\\\\?_]+|[0-9.]+((e|E)(\\\\+|-)?)?[0-9]*|[0-9]+)(?!\\Y)\":::Numeric Const::\n\
	Delay Word:\"(?<!\\Y)((#\\(.*\\))|(#[0-9]*))(?!\\Y)\":::Ada Attributes::D\n\
	Simple Word:\"([a-zA-Z][a-zA-Z0-9]*)\":::Plain::D\n\
	Instance Declaration:\"([a-zA-Z][a-zA-Z0-9_]*)([ \\t]+)([a-zA-Z][a-zA-Z0-9_$]*)([ \\t]*)\\(\":::Plain::\n\
	Module name:\"\\1\":\"\"::Identifier:Instance Declaration:C\n\
	Instance Name:\"\\3\":\"\"::Identifier1:Instance Declaration:C\n\
	Pins Declaration:\"(?<!\\Y)(\\.([a-zA-Z0-9_]+))>\":::Storage Type1::\n\
	Special Chars:\"(\\{|\\}|,|;|=|\\.)\":::Keyword::}",
    "XML:1:0{\n\
	comment:\"\\<!--\":\"--\\>\"::Comment::\n\
	ignored section:\"\\<!\\[\\s*IGNORE\\s*\\[\":\"\\]\\]\\>\"::Text Comment::\n\
	declaration:\"\\<\\?(?ixml)\":\"\\?\\>\"::Warning::\n\
	declaration delims:\"&\":\"&\"::Keyword:declaration:C\n\
	declaration attributes:\"((?iversion|encoding|standalone))=\":::Keyword:declaration:\n\
	declaration attribute names:\"\\1\":::Preprocessor:declaration attributes:C\n\
	declaration sq string:\"'\":\"'\":\"\\n\\n\":String1:declaration:\n\
	declaration sq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:declaration sq string:\n\
	declaration dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:declaration:\n\
	declaration dq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:declaration dq string:\n\
	doctype:\"(\\<!(?idoctype))\\s+(\\<?(?!(?ixml))[\\l_][\\w:-]*\\>?)\":\"\\>\":\"\\[\":Warning::\n\
	doctype delims:\"\\1\":\"&\"::Keyword:doctype:C\n\
	doctype root element:\"\\2\":::Identifier:doctype:C\n\
	doctype keyword:\"(SYSTEM|PUBLIC)\":::Keyword:doctype:\n\
	doctype sq string:\"'\":\"'\":\"\\n\\n\":String1:doctype:\n\
	doctype dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:doctype:\n\
	processing instruction:\"\\<\\?\\S+\":\"\\?\\>\"::Preprocessor::\n\
	processing instruction attribute:\"[\\l_][\\w:-]*=((\"\"[^\"\"]*\"\")|('[^']*'))\":::Preprocessor:processing instruction:\n\
	processing instruction value:\"\\1\":::String:processing instruction attribute:C\n\
	cdata:\"\\<!\\[(?icdata)\\[\":\"\\]\\]\\>\"::Text Comment::\n\
	cdata delims:\"&\":\"&\"::Preprocessor:cdata:C\n\
	element declaration:\"\\<!ELEMENT\":\"\\>\"::Warning::\n\
	element declaration delims:\"&\":\"&\"::Keyword:element declaration:C\n\
	element declaration entity ref:\"%(?!(?ixml))[\\l_][\\w:-]*;\":::Identifier1:element declaration:\n\
	element declaration keyword:\"(?<!\\Y)(ANY|#PCDATA|EMPTY)>\":::Storage Type:element declaration:\n\
	element declaration name:\"<(?!(?ixml))[\\l_][\\w:-]*\":::Identifier:element declaration:\n\
	element declaration operator:\"[(),?*+|]\":::Keyword:element declaration:\n\
	entity declaration:\"\\<!ENTITY\":\"\\>\"::Warning::\n\
	entity declaration delims:\"&\":\"&\"::Keyword:entity declaration:C\n\
	entity declaration sq string:\"'\":\"'\":\"\\n\\n\":String1:entity declaration:\n\
	entity declaration sq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:entity declaration sq string:\n\
	entity declaration dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:entity declaration:\n\
	entity declaration dq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:entity declaration dq string:\n\
	entity declaration keyword:\"SYSTEM|NDATA\":::Keyword:entity declaration:\n\
	entity declaration name:\"<(?!(?ixml))[\\l_][\\w:-]*\":::Identifier:entity declaration:\n\
	parameter entity declaration:\"%\\s+((?!(?ixml))[\\l_][\\w:-]*)>\":::Keyword:entity declaration:\n\
	parameter entity name:\"\\1\":::Identifier:parameter entity declaration:C\n\
	notation:\"\\<!NOTATION\":\"\\>\"::Warning::\n\
	notation delims:\"&\":\"&\"::Keyword:notation:C\n\
	notation sq string:\"'\":\"'\":\"\\n\\n\":String1:notation:\n\
	notation sq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:notation sq string:\n\
	notation dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:notation:\n\
	notation dq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:notation dq string:\n\
	notation keyword:\"SYSTEM\":::Keyword:notation:\n\
	notation name:\"<(?!(?ixml))[\\l_][\\w:-]*\":::Identifier:notation:\n\
	attribute declaration:\"\\<!ATTLIST\":\"\\>\"::Warning::\n\
	attribute declaration delims:\"&\":\"&\"::Keyword:attribute declaration:C\n\
	attribute declaration sq string:\"'\":\"'\":\"\\n\\n\":String1:attribute declaration:\n\
	attribute declaration sq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:attribute declaration sq string:\n\
	attribute declaration dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:attribute declaration:\n\
	attribute declaration dq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:attribute declaration dq string:\n\
	attribute declaration namespace:\"(?ixmlns)(:[\\l_][\\w:]*)?\":::Preprocessor:attribute declaration:\n\
	attribute declaration default modifier:\"#(REQUIRED|IMPLIED|FIXED)>\":::Keyword:attribute declaration:\n\
	attribute declaration data type:\"<(CDATA|ENTIT(Y|IES)|ID(REFS?)?|NMTOKENS?|NOTATION)>\":::Storage Type:attribute declaration:\n\
	attribute declaration name:\"<(?!(?ixml))[\\l_][\\w:-]*\":::Identifier:attribute declaration:\n\
	attribute declaration operator:\"[(),?*+|]\":::Keyword:attribute declaration:\n\
	element:\"(\\</?)((?!(?ixml))[\\l_][\\w:-]*)\":\"/?\\>\"::Warning::\n\
	element delims:\"\\1\":\"&\"::Keyword:element:C\n\
	element name:\"\\2\":::Identifier:element:C\n\
	element assign:\"=\":::Keyword:element:\n\
	element reserved attribute:\"(?ixml:(lang|space|link|attribute))(?==)\":::Text Key:element:\n\
	element namespace:\"(?ixmlns:[\\l_]\\w*)(?==)\":::Preprocessor:element:\n\
	element attribute:\"[\\l_][\\w:-]*(?==)\":::Text Key1:element:\n\
	element sq string:\"'\":\"'\":\"\\n\\n\":String1:element:\n\
	element sq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:element sq string:\n\
	element dq string:\"\"\"\":\"\"\"\":\"\\n\\n\":String:element:\n\
	element dq string entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape:element dq string:\n\
	entity:\"&((amp|lt|gt|quot|apos)|#x[\\da-fA-F]*|[\\l_]\\w*);\":::Text Escape::\n\
	marked section:\"\\<!\\[\\s*(?:INCLUDE|(%(?!(?ixml))[\\l_][\\w:-]*;))\\s*\\[|\\]\\]\\>\":::Label::\n\
	marked section entity ref:\"\\1\":::Identifier:marked section:C\n\
	internal subset delims:\"[\\[\\]>]\":::Keyword::D\n\
	info:\"(?# version 0.1; author/maintainer: Joor Loohuis, joor@loohuis-consulting.nl)\":::Comment::D}",
    "X Resources:2:0{\n\
	Preprocessor:\"^\\s*#\":\"$\"::Preprocessor::\n\
	Preprocessor Wrap:\"\\\\\\n\":::Preprocessor1:Preprocessor:\n\
	Comment:\"^\\s*!\":\"$\"::Comment::\n\
	Comment Wrap:\"\\\\\\n\":::Comment:Comment:\n\
	Resource Continued:\"^(\\s*[^:\\s]+\\s*:)(?:(\\\\.)|.)*(\\\\)\\n\":\"$\"::Plain::\n\
	RC Space Warning:\"\\\\\\s+$\":::Flag:Resource Continued:\n\
	RC Esc Chars:\"\\\\.\":::Text Arg2:Resource Continued:\n\
	RC Esc Chars 2:\"\\2\":\"\"::Text Arg2:Resource Continued:C\n\
	RC Name:\"\\1\":\"\"::Identifier:Resource Continued:C\n\
	RC Wrap:\"\\\\\\n\":::Text Arg1:Resource Continued:\n\
	RC Wrap2:\"\\3\":\"\"::Text Arg1:Resource Continued:C\n\
	Resource:\"^\\s*[^:\\s]+\\s*:\":\"$\"::Plain::\n\
	Resource Space Warning:\"\\S+\\s+$\":::Flag:Resource:\n\
	Resource Esc Chars:\"\\\\.\":::Text Arg2:Resource:\n\
	Resource Name:\"&\":\"\"::Identifier:Resource:C\n\
	Free Text:\"^.*$\":::Flag::}",
    "Yacc:1:0{\n\
	comment:\"/\\*\":\"\\*/\"::Comment::\n\
	string:\"L?\"\"\":\"\"\"\":\"\\n\":String::\n\
	preprocessor line:\"^\\s*#\\s*(include|define|if|ifn?def|line|error|else|endif|elif|undef|pragma)>\":\"$\"::Preprocessor::\n\
	string escape chars:\"\\\\(.|\\n)\":::String1:string:\n\
	preprocessor esc chars:\"\\\\(.|\\n)\":::Preprocessor1:preprocessor line:\n\
	preprocessor comment:\"/\\*\":\"\\*/\"::Comment:preprocessor line:\n\
    	preprocessor string:\"L?\"\"\":\"\"\"\":\"\\n\":Preprocessor1:preprocessor line:\n\
    	prepr string esc chars:\"\\\\(?:.|\\n)\":::String1:preprocessor string:\n\
	character constant:\"'\":\"'\":\"[^\\\\][^']\":Character Const::\n\
	numeric constant:\"(?<!\\Y)((0(x|X)[0-9a-fA-F]*)|(([0-9]+\\.?[0-9]*)|(\\.[0-9]+))((e|E)(\\+|-)?[0-9]+)?)(L|l|UL|ul|u|U|F|f)?(?!\\Y)\":::Numeric Const::D\n\
	storage keyword:\"<(const|extern|auto|register|static|unsigned|signed|volatile|char|double|float|int|long|short|void|typedef|struct|union|enum)>\":::Storage Type::D\n\
	rule:\"^[ \\t]*[A-Za-z_][A-Za-z0-9_]*[ \\t]*:\":::Preprocessor1::D\n\
	keyword:\"<(return|goto|if|else|case|default|switch|break|continue|while|do|for|sizeof)>\":::Keyword::D\n\
	yacc keyword:\"<(error|YYABORT|YYACCEPT|YYBACKUP|YYERROR|YYINITDEPTH|YYLTYPE|YYMAXDEPTH|YYRECOVERING|YYSTYPE|yychar|yyclearin|yydebug|yyerrok|yyerror|yylex|yylval|yylloc|yynerrs|yyparse)>\":::Text Arg::D\n\
	percent keyword:\"(?<!\\Y)(%left|%nonassoc|%prec|%right|%start|%token|%type|%union)>([ \\t]*\\<.*\\>)?\":::Text Arg::D\n\
	braces:\"[{}]\":::Keyword::D\n\
	markers:\"(?<!\\Y)(%\\{|%\\}|%%)(?!\\Y)\":::Flag::D\n\
	percent sub-expr:\"\\2\":::Text Arg2:percent keyword:DC}"
};


/*
** Read a string (from the  value of the styles resource) containing highlight
** styles information, parse it, and load it into the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
int LoadStylesString(char *inString)
{    
    char *errMsg, *fontStr;
    char *inPtr = inString;
    highlightStyleRec *hs;
    int i;

    for (;;) {
   	
	/* skip over blank space */
	inPtr += strspn(inPtr, " \t");

	/* Allocate a language mode structure in which to store the info. */
	hs = (highlightStyleRec *)NEditMalloc(sizeof(highlightStyleRec));

	/* read style name */
	hs->name = ReadSymbolicField(&inPtr);
	if (hs->name == NULL)
    	    return styleError(inString,inPtr, "style name required");
	if (!SkipDelimiter(&inPtr, &errMsg)) {
	    NEditFree(hs->name);
	    NEditFree(hs);
    	    return styleError(inString,inPtr, errMsg);
    	}
    	
    	/* read color */
	hs->color = ReadSymbolicField(&inPtr);
	if (hs->color == NULL) {
	    NEditFree(hs->name);
	    NEditFree(hs);
    	    return styleError(inString,inPtr, "color name required");
	}
        hs->bgColor = NULL;
        if (SkipOptSeparator('/', &inPtr)) {
    	    /* read bgColor */
	    hs->bgColor = ReadSymbolicField(&inPtr); /* no error if fails */
    	}
	if (!SkipDelimiter(&inPtr, &errMsg)) {
	    freeHighlightStyleRec(hs);
    	    return styleError(inString,inPtr, errMsg);
    	}
    	
	/* read the font type */
	fontStr = ReadSymbolicField(&inPtr);
	for (i=0; i<N_FONT_TYPES; i++) {
	    if (!strcmp(FontTypeNames[i], fontStr)) {
	    	hs->font = i;
	    	break;
	    }
	}
	if (i == N_FONT_TYPES) {
	    NEditFree(fontStr);
	    freeHighlightStyleRec(hs);
	    return styleError(inString, inPtr, "unrecognized font type");
	}
	NEditFree(fontStr);

   	/* pattern set was read correctly, add/change it in the list */
   	for (i=0; i<NHighlightStyles; i++) {
	    if (!strcmp(HighlightStyles[i]->name, hs->name)) {
		freeHighlightStyleRec(HighlightStyles[i]);
		HighlightStyles[i] = hs;
		break;
	    }
	}
	if (i == NHighlightStyles) {
	    HighlightStyles[NHighlightStyles++] = hs;
   	    if (NHighlightStyles > MAX_HIGHLIGHT_STYLES)
   		return styleError(inString, inPtr,
   	    		"maximum allowable number of styles exceeded");
	}
	
    	/* if the string ends here, we're done */
   	inPtr += strspn(inPtr, " \t\n");
    	if (*inPtr == '\0')
    	    return True;
    }
}

/*
** Create a string in the correct format for the styles resource, containing
** all of the highlight styles information from the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
char *WriteStylesString(void)
{
    int i;
    char *outStr;
    textBuffer *outBuf;
    highlightStyleRec *style;
    
    outBuf = BufCreate();
    for (i=0; i<NHighlightStyles; i++) {
    	style = HighlightStyles[i];
    	BufInsert(outBuf, outBuf->length, "\t");
    	BufInsert(outBuf, outBuf->length, style->name);
    	BufInsert(outBuf, outBuf->length, ":");
    	BufInsert(outBuf, outBuf->length, style->color);
        if (style->bgColor) {
            BufInsert(outBuf, outBuf->length, "/");
            BufInsert(outBuf, outBuf->length, style->bgColor);
        }
    	BufInsert(outBuf, outBuf->length, ":");
    	BufInsert(outBuf, outBuf->length, FontTypeNames[style->font]);
    	BufInsert(outBuf, outBuf->length, "\\n\\\n");
    }
    
    /* Get the output, and lop off the trailing newlines */
    outStr = BufGetRange(outBuf, 0, outBuf->length - (i==1?0:4));
    BufFree(outBuf);
    return outStr;
}

/*
** Read a string representing highlight pattern sets and add them
** to the PatternSets list of loaded highlight patterns.  Note that the
** patterns themselves are not parsed until they are actually used.
**
** The argument convertOld, reads patterns in pre 5.1 format (which means
** that they may contain regular expressions are of the older syntax where
** braces were not quoted, and \0 was a legal substitution character).
*/
int LoadHighlightString(char *inString, int convertOld)
{
    char *inPtr = inString;
    patternSet *patSet;
    int i;
    
    for (;;) {
   	
   	/* Read each pattern set, abort on error */
   	patSet = readPatternSet(&inPtr, convertOld);
   	if (patSet == NULL)
   	    return False;
   	
	/* Add/change the pattern set in the list */
	for (i=0; i<NPatternSets; i++) {
	    if (!strcmp(PatternSets[i]->languageMode, patSet->languageMode)) {
		freePatternSet(PatternSets[i]);
		PatternSets[i] = patSet;
		break;
	    }
	}
	if (i == NPatternSets) {
	    PatternSets[NPatternSets++] = patSet;
   	    if (NPatternSets > MAX_LANGUAGE_MODES)
   		return False;
	}
	
    	/* if the string ends here, we're done */
   	inPtr += strspn(inPtr, " \t\n");
    	if (*inPtr == '\0')
    	    return True;
    }
}

/*
** Create a string in the correct format for the highlightPatterns resource,
** containing all of the highlight pattern information from the stored
** highlight pattern list (PatternSets) for this NEdit session.
*/
char *WriteHighlightString(void)
{
    char *outStr, *str, *escapedStr;
    textBuffer *outBuf;
    int psn, written = False;
    patternSet *patSet;
    
    outBuf = BufCreate();
    for (psn=0; psn<NPatternSets; psn++) {
    	patSet = PatternSets[psn];
    	if (patSet->nPatterns == 0)
    	    continue;
    	written = True;
    	BufInsert(outBuf, outBuf->length, patSet->languageMode);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (isDefaultPatternSet(patSet))
    	    BufInsert(outBuf, outBuf->length, "Default\n\t");
    	else {
    	    BufInsert(outBuf, outBuf->length, intToStr(patSet->lineContext));
    	    BufInsert(outBuf, outBuf->length, ":");
    	    BufInsert(outBuf, outBuf->length, intToStr(patSet->charContext));
    	    BufInsert(outBuf, outBuf->length, "{\n");
    	    BufInsert(outBuf, outBuf->length,
    	    	    str = createPatternsString(patSet, "\t\t"));
    	    NEditFree(str);
    	    BufInsert(outBuf, outBuf->length, "\t}\n\t");
    	}
    }
    
    /* Get the output string, and lop off the trailing newline and tab */
    outStr = BufGetRange(outBuf, 0, outBuf->length - (written?2:0));
    BufFree(outBuf);
    
    /* Protect newlines and backslashes from translation by the resource
       reader */
    escapedStr = EscapeSensitiveChars(outStr);
    NEditFree(outStr);
    return escapedStr;
}

/*
** Update regular expressions in stored pattern sets to version 5.1 regular
** expression syntax, in which braces and \0 have different meanings
*/
static void convertOldPatternSet(patternSet *patSet)
{
    int p;
    highlightPattern *pattern;
    
    for (p=0; p<patSet->nPatterns; p++) {
	pattern = &patSet->patterns[p];
	convertPatternExpr(&pattern->startRE, patSet->languageMode,
		pattern->name, pattern->flags & COLOR_ONLY);
	convertPatternExpr(&pattern->endRE, patSet->languageMode,
		pattern->name, pattern->flags & COLOR_ONLY);
	convertPatternExpr(&pattern->errorRE, patSet->languageMode,
		pattern->name, pattern->flags & COLOR_ONLY);
    }
}

/*
** Convert a single regular expression, patternRE, to version 5.1 regular
** expression syntax.  It will convert either a match expression or a
** substitution expression, which must be specified by the setting of
** isSubsExpr.  Error messages are directed to stderr, and include the
** pattern set name and pattern name as passed in patSetName and patName.
*/
static void convertPatternExpr(char **patternRE, char *patSetName,
	char *patName, int isSubsExpr)
{
    char *newRE, *errorText;
    
    if (*patternRE == NULL)
	return;
    if (isSubsExpr) {
	newRE = (char*)NEditMalloc(strlen(*patternRE) + 5000);
	ConvertSubstituteRE(*patternRE, newRE, strlen(*patternRE) + 5000);
	NEditFree(*patternRE);
	*patternRE = NEditStrdup(newRE);
	NEditFree(newRE);
    } else{
	newRE = ConvertRE(*patternRE, &errorText);
	if (newRE == NULL) {
	    fprintf(stderr, "NEdit error converting old format regular "
		    "expression in pattern set %s, pattern %s: %s\n",
		    patSetName, patName, errorText);
	} 
	NEditFree(*patternRE);
	*patternRE = newRE;
    }
}

/*
** Find the font (font struct) associated with a named style.
** This routine must only be called with a valid styleName (call
** NamedStyleExists to find out whether styleName is valid).
*/
XFontStruct *FontOfNamedStyle(WindowInfo *window, const char *styleName)
{
    int styleNo=lookupNamedStyle(styleName),fontNum;
    XFontStruct *font;
    
    if (styleNo<0)
        return GetDefaultFontStruct(TheDisplay, window->fontList);
    fontNum = HighlightStyles[styleNo]->font;
    if (fontNum == BOLD_FONT)
    	font = window->boldFontStruct;
    else if (fontNum == ITALIC_FONT)
    	font = window->italicFontStruct;
    else if (fontNum == BOLD_ITALIC_FONT)
    	font = window->boldItalicFontStruct;
    else /* fontNum == PLAIN_FONT */
    	font = GetDefaultFontStruct(TheDisplay, window->fontList);
    
    /* If font isn't loaded, silently substitute primary font */
    return font == NULL ? GetDefaultFontStruct(TheDisplay, window->fontList) : font;
}

int FontOfNamedStyleIsBold(char *styleName)
{
    int styleNo=lookupNamedStyle(styleName),fontNum;
    
    if (styleNo<0)
        return 0;
    fontNum = HighlightStyles[styleNo]->font;
    return (fontNum == BOLD_FONT || fontNum == BOLD_ITALIC_FONT);
}

int FontOfNamedStyleIsItalic(char *styleName)
{
    int styleNo=lookupNamedStyle(styleName),fontNum;
    
    if (styleNo<0)
        return 0;
    fontNum = HighlightStyles[styleNo]->font;
    return (fontNum == ITALIC_FONT || fontNum == BOLD_ITALIC_FONT);
}

/*
** Find the color associated with a named style.  This routine must only be
** called with a valid styleName (call NamedStyleExists to find out whether
** styleName is valid).
*/
char *ColorOfNamedStyle(const char *styleName)
{
    int styleNo=lookupNamedStyle(styleName);
    
    if (styleNo<0)
        return "black";
    return HighlightStyles[styleNo]->color;
}

/*
** Find the background color associated with a named style.
*/
char *BgColorOfNamedStyle(const char *styleName)
{
    int styleNo=lookupNamedStyle(styleName);

    if (styleNo<0)
        return "";
    return HighlightStyles[styleNo]->bgColor;
}

/*
** Determine whether a named style exists
*/
int NamedStyleExists(const char *styleName)
{
    return lookupNamedStyle(styleName) != -1;
}

/*
** Look through the list of pattern sets, and find the one for a particular
** language.  Returns NULL if not found.
*/
patternSet *FindPatternSet(const char *langModeName)
{
    int i;
    
    if (langModeName == NULL)
    	return NULL;
	
    for (i=0; i<NPatternSets; i++)
    	if (!strcmp(langModeName, PatternSets[i]->languageMode))
    	    return PatternSets[i];
    return NULL;
    
}

/*
** Returns True if there are highlight patterns, or potential patterns
** not yet committed in the syntax highlighting dialog for a language mode,
*/
int LMHasHighlightPatterns(const char *languageMode)
{
    if (FindPatternSet(languageMode) != NULL)
    	return True;
    return HighlightDialog.shell!=NULL && !strcmp(HighlightDialog.langModeName,
    	    languageMode) && HighlightDialog.nPatterns != 0;
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void RenameHighlightPattern(const char *oldName, const char *newName)
{
    int i;
    
    for (i=0; i<NPatternSets; i++) {
    	if (!strcmp(oldName, PatternSets[i]->languageMode)) {
    	    NEditFree(PatternSets[i]->languageMode);
    	    PatternSets[i]->languageMode = NEditStrdup(newName);
    	}
    }
    if (HighlightDialog.shell != NULL) {
    	if (!strcmp(HighlightDialog.langModeName, oldName)) {
    	    NEditFree(HighlightDialog.langModeName);
    	    HighlightDialog.langModeName = NEditStrdup(newName);
    	}
    }
}

/*
** Create a pulldown menu pane with the names of the current highlight styles.
** XmNuserData for each item contains a pointer to the name.
*/
static Widget createHighlightStylesMenu(Widget parent)
{
    Widget menu;
    int i;
    XmString s1;

    menu = CreatePulldownMenu(parent, "highlightStyles", NULL, 0);
    for (i=0; i<NHighlightStyles; i++) {
        XtVaCreateManagedWidget("highlightStyles", xmPushButtonWidgetClass,menu,
    	      XmNlabelString, s1=XmStringCreateSimple(HighlightStyles[i]->name),
    	      XmNuserData, (void *)HighlightStyles[i]->name, NULL);
        XmStringFree(s1);
    }
    return menu;
}

static char *createPatternsString(patternSet *patSet, char *indentStr)
{
    char *outStr, *str;
    textBuffer *outBuf;
    int pn;
    highlightPattern *pat;
    
    outBuf = BufCreate();
    for (pn=0; pn<patSet->nPatterns; pn++) {
    	pat = &patSet->patterns[pn];
    	BufInsert(outBuf, outBuf->length, indentStr);
    	BufInsert(outBuf, outBuf->length, pat->name);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (pat->startRE != NULL) {
    	    BufInsert(outBuf, outBuf->length,
    	    	    str=MakeQuotedString(pat->startRE));
    	    NEditFree(str);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (pat->endRE != NULL) {
    	    BufInsert(outBuf, outBuf->length, str=MakeQuotedString(pat->endRE));
    	    NEditFree(str);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	if (pat->errorRE != NULL) {
    	    BufInsert(outBuf, outBuf->length,
    	    	    str=MakeQuotedString(pat->errorRE));
    	    NEditFree(str);
    	}
    	BufInsert(outBuf, outBuf->length, ":");
    	BufInsert(outBuf, outBuf->length, pat->style);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (pat->subPatternOf != NULL)
    	    BufInsert(outBuf, outBuf->length, pat->subPatternOf);
    	BufInsert(outBuf, outBuf->length, ":");
    	if (pat->flags & DEFER_PARSING)
    	    BufInsert(outBuf, outBuf->length, "D");
    	if (pat->flags & PARSE_SUBPATS_FROM_START)
    	    BufInsert(outBuf, outBuf->length, "R");
    	if (pat->flags & COLOR_ONLY)
    	    BufInsert(outBuf, outBuf->length, "C");
    	BufInsert(outBuf, outBuf->length, "\n");
    }
    outStr = BufGetAll(outBuf);
    BufFree(outBuf);
    return outStr;
}

/*
** Read in a pattern set character string, and advance *inPtr beyond it.
** Returns NULL and outputs an error to stderr on failure.
*/
static patternSet *readPatternSet(char **inPtr, int convertOld)
{
    char *errMsg, *stringStart = *inPtr;
    patternSet patSet, *retPatSet;

    /* remove leading whitespace */
    *inPtr += strspn(*inPtr, " \t\n");

    /* read language mode field */
    patSet.languageMode = ReadSymbolicField(inPtr);
    if (patSet.languageMode == NULL)
    	return highlightError(stringStart, *inPtr,
    	    	"language mode must be specified");
    if (!SkipDelimiter(inPtr, &errMsg))
    	return highlightError(stringStart, *inPtr, errMsg);

    /* look for "Default" keyword, and if it's there, return the default
       pattern set */
    if (!strncmp(*inPtr, "Default", 7)) {
    	*inPtr += 7;
    	retPatSet = readDefaultPatternSet(patSet.languageMode);
    	NEditFree(patSet.languageMode);
    	if (retPatSet == NULL)
    	    return highlightError(stringStart, *inPtr,
    	    	    "No default pattern set");
    	return retPatSet;
    }
    	
    /* read line context field */
    if (!ReadNumericField(inPtr, &patSet.lineContext))
	return highlightError(stringStart, *inPtr,
	    	"unreadable line context field");
    if (!SkipDelimiter(inPtr, &errMsg))
    	return highlightError(stringStart, *inPtr, errMsg);

    /* read character context field */
    if (!ReadNumericField(inPtr, &patSet.charContext))
	return highlightError(stringStart, *inPtr,
	    	"unreadable character context field");

    /* read pattern list */
    patSet.patterns = readHighlightPatterns(inPtr,
   	    True, &errMsg, &patSet.nPatterns);
    if (patSet.patterns == NULL)
	return highlightError(stringStart, *inPtr, errMsg);

    /* pattern set was read correctly, make an allocated copy to return */
    retPatSet = (patternSet *)NEditMalloc(sizeof(patternSet));
    memcpy(retPatSet, &patSet, sizeof(patternSet));
    
    /* Convert pre-5.1 pattern sets which use old regular expression
       syntax to quote braces and use & rather than \0 */
    if (convertOld)
    	convertOldPatternSet(retPatSet);
    
    return retPatSet;
}

/*
** Parse a set of highlight patterns into an array of highlightPattern
** structures, and a language mode name.  If unsuccessful, returns NULL with
** (statically allocated) message in "errMsg".
*/
static highlightPattern *readHighlightPatterns(char **inPtr, int withBraces,
    	char **errMsg, int *nPatterns)
{    
    highlightPattern *pat, *returnedList, patternList[MAX_PATTERNS];
   
    /* skip over blank space */
    *inPtr += strspn(*inPtr, " \t\n");
    
    /* look for initial brace */
    if (withBraces) {
	if (**inPtr != '{') {
    	    *errMsg = "pattern list must begin with \"{\"";
    	    return False;
	}
	(*inPtr)++;
    }
    
    /*
    ** parse each pattern in the list
    */
    pat = patternList;
    while (True) {
    	*inPtr += strspn(*inPtr, " \t\n");
    	if (**inPtr == '\0') {
    	    if (withBraces) {
    		*errMsg = "end of pattern list not found";
    		return NULL;
    	    } else
    	    	break;
	} else if (**inPtr == '}') {
	    (*inPtr)++;
    	    break;
    	}
    	if (pat - patternList >= MAX_PATTERNS) {
    	    *errMsg = "max number of patterns exceeded\n";
    	    return NULL;
    	}
    	if (!readHighlightPattern(inPtr, errMsg, pat++))
    	    return NULL;
    }
    
    /* allocate a more appropriately sized list to return patterns */
    *nPatterns = pat - patternList;
    returnedList = (highlightPattern *)NEditMalloc(
    	    sizeof(highlightPattern) * *nPatterns);
    memcpy(returnedList, patternList, sizeof(highlightPattern) * *nPatterns);
    return returnedList;
}

static int readHighlightPattern(char **inPtr, char **errMsg,
    	highlightPattern *pattern)
{
    /* read the name field */
    pattern->name = ReadSymbolicField(inPtr);
    if (pattern->name == NULL) {
    	*errMsg = "pattern name is required";
    	return False;
    }
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    
    /* read the start pattern */
    if (!ReadQuotedString(inPtr, errMsg, &pattern->startRE))
    	return False;
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    
    /* read the end pattern */
    if (**inPtr == ':')
    	pattern->endRE = NULL;
    else if (!ReadQuotedString(inPtr, errMsg, &pattern->endRE))
    	return False;
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    
    /* read the error pattern */
    if (**inPtr == ':')
    	pattern->errorRE = NULL;
    else if (!ReadQuotedString(inPtr, errMsg, &pattern->errorRE))
    	return False;
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    
    /* read the style field */
    pattern->style = ReadSymbolicField(inPtr);
    if (pattern->style == NULL) {
    	*errMsg = "style field required in pattern";
    	return False;
    }
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    
    /* read the sub-pattern-of field */
    pattern->subPatternOf = ReadSymbolicField(inPtr);
    if (!SkipDelimiter(inPtr, errMsg))
    	return False;
    	
    /* read flags field */
    pattern->flags = 0;
    for (; **inPtr != '\n' && **inPtr != '}'; (*inPtr)++) {
	if (**inPtr == 'D')
	    pattern->flags |= DEFER_PARSING;
	else if (**inPtr == 'R')
	    pattern->flags |= PARSE_SUBPATS_FROM_START;
	else if (**inPtr == 'C')
	    pattern->flags |= COLOR_ONLY;
	else if (**inPtr != ' ' && **inPtr != '\t') {
	    *errMsg = "unreadable flag field";
	    return False;
	}
    }
    return True;
}

/*
** Given a language mode name, determine if there is a default (built-in)
** pattern set available for that language mode, and if so, read it and
** return a new allocated copy of it.  The returned pattern set should be
** freed by the caller with freePatternSet()
*/
static patternSet *readDefaultPatternSet(const char *langModeName)
{
    int i;
    size_t modeNameLen;
    char *strPtr;
    
    modeNameLen = strlen(langModeName);
    for (i=0; i<(int)XtNumber(DefaultPatternSets); i++) {
    	if (!strncmp(langModeName, DefaultPatternSets[i], modeNameLen) &&
    	    	DefaultPatternSets[i][modeNameLen] == ':') {
    	    strPtr = DefaultPatternSets[i];
    	    return readPatternSet(&strPtr, False);
    	}
    }
    return NULL;
}

/*
** Return True if patSet exactly matches one of the default pattern sets
*/
static int isDefaultPatternSet(patternSet *patSet)
{
    patternSet *defaultPatSet;
    int retVal;
    
    defaultPatSet = readDefaultPatternSet(patSet->languageMode);
    if (defaultPatSet == NULL)
    	return False;
    retVal = !patternSetsDiffer(patSet, defaultPatSet);
    freePatternSet(defaultPatSet);
    return retVal;
}

/*
** Short-hand functions for formating and outputing errors for
*/
static patternSet *highlightError(char *stringStart, char *stoppedAt,
    	const char *message)
{
    ParseError(NULL, stringStart, stoppedAt, "highlight pattern", message);
    return NULL;
}


static int styleError(const char *stringStart, const char *stoppedAt,
        const  char *message)
{
    ParseError(NULL, stringStart, stoppedAt, "style specification", message);
    return False;
}

/*
** Present a dialog for editing highlight style information
*/
void EditHighlightStyles(const char *initialStyle)
{
#define HS_LIST_RIGHT 60
#define HS_LEFT_MARGIN_POS 1
#define HS_RIGHT_MARGIN_POS 99
#define HS_H_MARGIN 10
    Widget form, nameLbl, topLbl, colorLbl, bgColorLbl, fontLbl;
    Widget fontBox, sep1, okBtn, applyBtn, closeBtn;
    XmString s1;
    int i, ac;
    Arg args[20];

    /* if the dialog is already displayed, just pop it to the top and return */
    if (HSDialog.shell != NULL) {
	if (initialStyle != NULL)
	    setStyleByName(initialStyle);
    	RaiseDialogWindow(HSDialog.shell);
    	return;
    }
    
    /* Copy the list of highlight style information to one that the user
       can freely edit (via the dialog and managed-list code) */
    HSDialog.highlightStyleList = (highlightStyleRec **)NEditMalloc(
    	    sizeof(highlightStyleRec *) * MAX_HIGHLIGHT_STYLES);
    for (i=0; i<NHighlightStyles; i++)
    	HSDialog.highlightStyleList[i] =
    	copyHighlightStyleRec(HighlightStyles[i]);
    HSDialog.nHighlightStyles = NHighlightStyles;
    
    /* Create a form widget in an application shell */
    ac = 0;
    XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
    XtSetArg(args[ac], XmNiconName, "NEdit Text Drawing Styles"); ac++;
    XtSetArg(args[ac], XmNtitle, "Text Drawing Styles"); ac++;
    HSDialog.shell = CreateWidget(TheAppShell, "textStyles",
	    topLevelShellWidgetClass, args, ac);
    AddSmallIcon(HSDialog.shell);
    form = XtVaCreateManagedWidget("editHighlightStyles", xmFormWidgetClass,
	    HSDialog.shell, XmNautoUnmanage, False,
	    XmNresizePolicy, XmRESIZE_NONE, NULL);
    XtAddCallback(form, XmNdestroyCallback, hsDestroyCB, NULL);
    AddMotifCloseCallback(HSDialog.shell, hsCloseCB, NULL);
        
    topLbl = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=MKSTRING(
"To modify the properties of an existing highlight style, select the name\n\
from the list on the left.  Select \"New\" to add a new style to the list."),
	    XmNmnemonic, 'N',
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 2,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, HS_LEFT_MARGIN_POS,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, HS_RIGHT_MARGIN_POS, NULL);
    XmStringFree(s1);
    
    nameLbl = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Name:"),
    	    XmNmnemonic, 'm',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, HS_H_MARGIN,
	    XmNtopWidget, topLbl, NULL);
    XmStringFree(s1);
 
    HSDialog.nameW = XtVaCreateManagedWidget("name", xmTextWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, nameLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, HS_RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(HSDialog.nameW);
    XtVaSetValues(nameLbl, XmNuserData, HSDialog.nameW, NULL);
    
    colorLbl = XtVaCreateManagedWidget("colorLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Foreground Color:"),
    	    XmNmnemonic, 'C',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, HS_H_MARGIN,
	    XmNtopWidget, HSDialog.nameW, NULL);
    XmStringFree(s1);
 
    HSDialog.colorW = XtVaCreateManagedWidget("color", xmTextWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, colorLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, HS_RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(HSDialog.colorW);
    XtVaSetValues(colorLbl, XmNuserData, HSDialog.colorW, NULL);
    
    bgColorLbl = XtVaCreateManagedWidget("bgColorLbl", xmLabelGadgetClass, form,
    	    XmNlabelString,
    	      s1=XmStringCreateSimple("Background Color (optional)"),
    	    XmNmnemonic, 'g',
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, HS_H_MARGIN,
	    XmNtopWidget, HSDialog.colorW, NULL);
    XmStringFree(s1);
 
    HSDialog.bgColorW = XtVaCreateManagedWidget("bgColor",
            xmTextWidgetClass, form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, bgColorLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, HS_RIGHT_MARGIN_POS, NULL);
    RemapDeleteKey(HSDialog.bgColorW);
    XtVaSetValues(bgColorLbl, XmNuserData, HSDialog.bgColorW, NULL);
    
    fontLbl = XtVaCreateManagedWidget("fontLbl", xmLabelGadgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Font:"),
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, HS_H_MARGIN,
	    XmNtopWidget, HSDialog.bgColorW, NULL);
    XmStringFree(s1);

    fontBox = XtVaCreateManagedWidget("fontBox", xmRowColumnWidgetClass, form,
    	    XmNpacking, XmPACK_COLUMN,
    	    XmNnumColumns, 2,
    	    XmNradioBehavior, True,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, HS_LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, fontLbl, NULL);
    HSDialog.plainW = XtVaCreateManagedWidget("plain", 
    	    xmToggleButtonWidgetClass, fontBox,
    	    XmNset, True,
    	    XmNlabelString, s1=XmStringCreateSimple("Plain"),
    	    XmNmnemonic, 'P', NULL);
    XmStringFree(s1);
    HSDialog.boldW = XtVaCreateManagedWidget("bold", 
    	    xmToggleButtonWidgetClass, fontBox,
    	    XmNlabelString, s1=XmStringCreateSimple("Bold"),
    	    XmNmnemonic, 'B', NULL);
    XmStringFree(s1);
    HSDialog.italicW = XtVaCreateManagedWidget("italic", 
    	    xmToggleButtonWidgetClass, fontBox,
    	    XmNlabelString, s1=XmStringCreateSimple("Italic"),
    	    XmNmnemonic, 'I', NULL);
    XmStringFree(s1);
    HSDialog.boldItalicW = XtVaCreateManagedWidget("boldItalic", 
    	    xmToggleButtonWidgetClass, fontBox,
    	    XmNlabelString, s1=XmStringCreateSimple("Bold Italic"),
    	    XmNmnemonic, 'o', NULL);
    XmStringFree(s1);
    	    
    okBtn = XtVaCreateManagedWidget("ok",xmPushButtonWidgetClass,form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 10,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 30,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, hsOkCB, NULL);
    XmStringFree(s1);

    applyBtn = XtVaCreateManagedWidget("apply",xmPushButtonWidgetClass,form,
    	    XmNlabelString, s1=XmStringCreateSimple("Apply"),
    	    XmNmnemonic, 'A',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 40,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 60,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, hsApplyCB, NULL);
    XmStringFree(s1);

    closeBtn = XtVaCreateManagedWidget("close",
            xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Close"),
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 70,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 90,
    	    XmNbottomAttachment, XmATTACH_POSITION,
    	    XmNbottomPosition, 99,
            NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, hsCloseCB, NULL);
    XmStringFree(s1);
    
    sep1 = XtVaCreateManagedWidget("sep1", xmSeparatorGadgetClass, form,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, fontBox,
	    XmNtopOffset, HS_H_MARGIN,
 	    XmNrightAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_WIDGET,
    	    XmNbottomWidget, closeBtn, 0,
	    XmNbottomOffset, HS_H_MARGIN, NULL);
    
    ac = 0;
    XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNtopOffset, HS_H_MARGIN); ac++;
    XtSetArg(args[ac], XmNtopWidget, topLbl); ac++;
    XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNleftPosition, HS_LEFT_MARGIN_POS); ac++;
    XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION); ac++;
    XtSetArg(args[ac], XmNrightPosition, HS_LIST_RIGHT-1); ac++;
    XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(args[ac], XmNbottomWidget, sep1); ac++;
    XtSetArg(args[ac], XmNbottomOffset, HS_H_MARGIN); ac++;
    HSDialog.managedListW = CreateManagedList(form, "list", args, ac,
    	    (void **)HSDialog.highlightStyleList, &HSDialog.nHighlightStyles,
    	    MAX_HIGHLIGHT_STYLES, 20, hsGetDisplayedCB, NULL, hsSetDisplayedCB,
    	    form, hsFreeItemCB);
    XtVaSetValues(topLbl, XmNuserData, HSDialog.managedListW, NULL);
 
    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    /* If there's a suggestion for an initial selection, make it */
    if (initialStyle != NULL)
	setStyleByName(initialStyle);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* Realize all of the widgets in the new dialog */
    RealizeWithoutForcingPosition(HSDialog.shell);
}

static void hsDestroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int i;
    
    for (i=0; i<HSDialog.nHighlightStyles; i++)
    	freeHighlightStyleRec(HSDialog.highlightStyleList[i]);
    NEditFree(HSDialog.highlightStyleList);
}

static void hsOkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (!updateHSList())
    	return;

    /* pop down and destroy the dialog */
    XtDestroyWidget(HSDialog.shell);
    HSDialog.shell = NULL;
}

static void hsApplyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    updateHSList();
}

static void hsCloseCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    XtDestroyWidget(HSDialog.shell);
    HSDialog.shell = NULL;
}

static void *hsGetDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg)
{
    highlightStyleRec *hs;
    
    /* If the dialog is currently displaying the "new" entry and the
       fields are empty, that's just fine */
    if (oldItem == NULL && hsDialogEmpty())
    	return NULL;
    
    /* If there are no problems reading the data, just return it */
    hs = readHSDialogFields(True);
    if (hs != NULL)
    	return (void *)hs;
    
    /* If there are problems, and the user didn't ask for the fields to be
       read, give more warning */
    if (!explicitRequest)
    {
        if (DialogF(DF_WARN, HSDialog.shell, 2, "Incomplete Style",
                "Discard incomplete entry\nfor current highlight style?",
                "Keep", "Discard") == 2)
        {
            return oldItem == NULL
                    ? NULL
                    : (void *)copyHighlightStyleRec((highlightStyleRec *)oldItem);
        }
    }

    /* Do readHSDialogFields again without "silent" mode to display warning */
    hs = readHSDialogFields(False);
    *abort = True;
    return NULL;
}

static void hsSetDisplayedCB(void *item, void *cbArg)
{
    highlightStyleRec *hs = (highlightStyleRec *)item;

    if (item == NULL) {
    	XmTextSetString(HSDialog.nameW, "");
    	XmTextSetString(HSDialog.colorW, "");
    	XmTextSetString(HSDialog.bgColorW, "");
    	RadioButtonChangeState(HSDialog.plainW, True, False);
    	RadioButtonChangeState(HSDialog.boldW, False, False);
    	RadioButtonChangeState(HSDialog.italicW, False, False);
    	RadioButtonChangeState(HSDialog.boldItalicW, False, False);
    } else {
        if (strcmp(hs->name, "Plain") == 0) {
            /* you should not be able to delete the reserved style "Plain" */
            int i, others = 0;
            int nList = HSDialog.nHighlightStyles;
            highlightStyleRec **list = HSDialog.highlightStyleList;
            /* do we have other styles called Plain? */
            for (i = 0; i < nList; i++) {
                if (list[i] != hs && strcmp(list[i]->name, "Plain") == 0) {
                      others++;
                }
            }
            if (others == 0) {
                /* this is the last style entry named "Plain" */
                Widget form = (Widget)cbArg;
                Widget deleteBtn = XtNameToWidget(form, "*delete");
                /* disable delete button */
                if (deleteBtn) {
                    XtSetSensitive(deleteBtn, False);
                }
            }
        }
    	XmTextSetString(HSDialog.nameW, hs->name);
    	XmTextSetString(HSDialog.colorW, hs->color);
    	XmTextSetString(HSDialog.bgColorW, hs->bgColor ? hs->bgColor : "");
    	RadioButtonChangeState(HSDialog.plainW, hs->font==PLAIN_FONT, False);
    	RadioButtonChangeState(HSDialog.boldW, hs->font==BOLD_FONT, False);
    	RadioButtonChangeState(HSDialog.italicW, hs->font==ITALIC_FONT, False);
    	RadioButtonChangeState(HSDialog.boldItalicW, hs->font==BOLD_ITALIC_FONT,
    	        False);
    }
}

static void hsFreeItemCB(void *item)
{
    freeHighlightStyleRec((highlightStyleRec *)item);
}

static highlightStyleRec *readHSDialogFields(int silent)
{
    highlightStyleRec *hs;
    Display *display = XtDisplay(HSDialog.shell);
    int screenNum = XScreenNumberOfScreen(XtScreen(HSDialog.shell));
    XColor rgb;

    /* Allocate a language mode structure to return */
    hs = (highlightStyleRec *)NEditMalloc(sizeof(highlightStyleRec));

    /* read the name field */
    hs->name = ReadSymbolicFieldTextWidget(HSDialog.nameW,
    	    "highlight style name", silent);
    if (hs->name == NULL) {
    	NEditFree(hs);
    	return NULL;
    }

    if (*hs->name == '\0')
    {
        if (!silent)
        {
            DialogF(DF_WARN, HSDialog.shell, 1, "Highlight Style",
                    "Please specify a name\nfor the highlight style", "OK");
            XmProcessTraversal(HSDialog.nameW, XmTRAVERSE_CURRENT);
        }
        NEditFree(hs->name);
        NEditFree(hs);
        return NULL;
    }

    /* read the color field */
    hs->color = ReadSymbolicFieldTextWidget(HSDialog.colorW, "color", silent);
    if (hs->color == NULL) {
    	NEditFree(hs->name);
    	NEditFree(hs);
    	return NULL;
    }

    if (*hs->color == '\0')
    {
        if (!silent)
        {
            DialogF(DF_WARN, HSDialog.shell, 1, "Style Color",
                    "Please specify a color\nfor the highlight style",
                    "OK");
            XmProcessTraversal(HSDialog.colorW, XmTRAVERSE_CURRENT);
        }
        NEditFree(hs->name);
        NEditFree(hs->color);
        NEditFree(hs);
        return NULL;
    }

    /* Verify that the color is a valid X color spec */
    if (!XParseColor(display, DefaultColormap(display, screenNum), hs->color,
            &rgb))
    {
        if (!silent)
        {
            DialogF(DF_WARN, HSDialog.shell, 1, "Invalid Color",
                    "Invalid X color specification: %s\n",  "OK",
                    hs->color);
            XmProcessTraversal(HSDialog.colorW, XmTRAVERSE_CURRENT);
        }
        NEditFree(hs->name);
        NEditFree(hs->color);
        NEditFree(hs);
        return NULL;;
    }
    
    /* read the background color field - this may be empty */
    hs->bgColor = ReadSymbolicFieldTextWidget(HSDialog.bgColorW,
                        "bgColor", silent);
    if (hs->bgColor && *hs->bgColor == '\0') {
        NEditFree(hs->bgColor);
        hs->bgColor = NULL;
    }

    /* Verify that the background color (if present) is a valid X color spec */
    if (hs->bgColor && !XParseColor(display, DefaultColormap(display, screenNum),
            hs->bgColor, &rgb))
    {
        if (!silent)
        {
            DialogF(DF_WARN, HSDialog.shell, 1, "Invalid Color",
                    "Invalid X background color specification: %s\n", "OK",
                    hs->bgColor);
            XmProcessTraversal(HSDialog.bgColorW, XmTRAVERSE_CURRENT);
        }
        NEditFree(hs->name);
        NEditFree(hs->color);
        NEditFree(hs->bgColor);
        NEditFree(hs);
        return NULL;;
    }
    
    /* read the font buttons */
    if (XmToggleButtonGetState(HSDialog.boldW))
    	hs->font = BOLD_FONT;
    else if (XmToggleButtonGetState(HSDialog.italicW))
    	hs->font = ITALIC_FONT;
    else if (XmToggleButtonGetState(HSDialog.boldItalicW))
    	hs->font = BOLD_ITALIC_FONT;
    else
    	hs->font = PLAIN_FONT;

    return hs;
}

/*
** Copy a highlightStyleRec data structure, and all of the allocated memory
** it contains.
*/
static highlightStyleRec *copyHighlightStyleRec(highlightStyleRec *hs)
{
    highlightStyleRec *newHS;
    
    newHS = (highlightStyleRec *)NEditMalloc(sizeof(highlightStyleRec));
    newHS->name = (char*)NEditMalloc(strlen(hs->name)+1);
    strcpy(newHS->name, hs->name);
    if (hs->color == NULL)
    	newHS->color = NULL;
    else {
	newHS->color = (char*)NEditMalloc(strlen(hs->color)+1);
	strcpy(newHS->color, hs->color);
    }
    if (hs->bgColor == NULL)
    	newHS->bgColor = NULL;
    else {
	newHS->bgColor = (char*)NEditMalloc(strlen(hs->bgColor)+1);
	strcpy(newHS->bgColor, hs->bgColor);
    }
    newHS->font = hs->font;
    return newHS;
}

/*
** Free all of the allocated data in a highlightStyleRec, including the
** structure itself.
*/
static void freeHighlightStyleRec(highlightStyleRec *hs)
{
    NEditFree(hs->name);
    NEditFree(hs->color);
    NEditFree(hs);
}

/*
** Select a particular style in the highlight styles dialog
*/
static void setStyleByName(const char *style)
{
    int i;
    
    for (i=0; i<HSDialog.nHighlightStyles; i++) {
    	if (!strcmp(HSDialog.highlightStyleList[i]->name, style)) {
    	    SelectManagedListItem(HSDialog.managedListW, i);
    	    break;
    	}
    }
}

/*
** Return True if the fields of the highlight styles dialog are consistent
** with a blank "New" style in the dialog.
*/
static int hsDialogEmpty(void)
{
    return TextWidgetIsBlank(HSDialog.nameW) &&
 	    TextWidgetIsBlank(HSDialog.colorW) &&
	    XmToggleButtonGetState(HSDialog.plainW);
}   	

/*
** Apply the changes made in the highlight styles dialog to the stored
** highlight style information in HighlightStyles
*/
static int updateHSList(void)
{
    WindowInfo *window;
    int i;
    
    /* Get the current contents of the dialog fields */
    if (!UpdateManagedList(HSDialog.managedListW, True))
    	return False;
    
    /* Replace the old highlight styles list with the new one from the dialog */
    for (i=0; i<NHighlightStyles; i++)
    	freeHighlightStyleRec(HighlightStyles[i]);
    for (i=0; i<HSDialog.nHighlightStyles; i++)
    	HighlightStyles[i] =
    	    	copyHighlightStyleRec(HSDialog.highlightStyleList[i]);
    NHighlightStyles = HSDialog.nHighlightStyles;
    
    /* If a syntax highlighting dialog is up, update its menu */
    updateHighlightStyleMenu();
    
    /* Redisplay highlighted windows which use changed style(s) */
    for (window=WindowList; window!=NULL; window=window->next)
    	UpdateHighlightStyles(window);
    
    /* Note that preferences have been changed */
    MarkPrefsChanged();

    return True;
}

/*
** Present a dialog for editing highlight pattern information
*/
void EditHighlightPatterns(WindowInfo *window)
{
#define BORDER 4
#define LIST_RIGHT 41
    Widget form, lmOptMenu, patternsForm, patternsFrame, patternsLbl;
    Widget lmForm, contextFrame, contextForm, styleLbl, styleBtn;
    Widget okBtn, applyBtn, checkBtn, deleteBtn, closeBtn, helpBtn;
    Widget restoreBtn, nameLbl, typeLbl, typeBox, lmBtn, matchBox;
    patternSet *patSet;
    XmString s1;
    int i, n, nPatterns;
    Arg args[20];

    /* if the dialog is already displayed, just pop it to the top and return */
    if (HighlightDialog.shell != NULL) {
    	RaiseDialogWindow(HighlightDialog.shell);
    	return;
    }
    
    if (LanguageModeName(0) == NULL)
    {
        DialogF(DF_WARN, window->shell, 1, "No Language Modes",
                "No Language Modes available for syntax highlighting\n"
                "Add language modes under Preferenses->Language Modes",
                "OK");
        return;
    }
    
    /* Decide on an initial language mode */
    HighlightDialog.langModeName = NEditStrdup(
    	    LanguageModeName(window->languageMode == PLAIN_LANGUAGE_MODE ? 0 :
    	    window->languageMode));

    /* Find the associated pattern set (patSet) to edit */
    patSet = FindPatternSet(HighlightDialog.langModeName);
    
    /* Copy the list of patterns to one that the user can freely edit */
    HighlightDialog.patterns = (highlightPattern **)NEditMalloc(
    	    sizeof(highlightPattern *) * MAX_PATTERNS);
    nPatterns = patSet == NULL ? 0 : patSet->nPatterns;
    for (i=0; i<nPatterns; i++)
    	HighlightDialog.patterns[i] = copyPatternSrc(&patSet->patterns[i],NULL);
    HighlightDialog.nPatterns = nPatterns;


    /* Create a form widget in an application shell */
    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
    XtSetArg(args[n], XmNiconName, "NEdit Highlight Patterns"); n++;
    XtSetArg(args[n], XmNtitle, "Syntax Highlighting Patterns"); n++;
    HighlightDialog.shell = CreateWidget(TheAppShell, "syntaxHighlight",
	    topLevelShellWidgetClass, args, n);
    AddSmallIcon(HighlightDialog.shell);
    form = XtVaCreateManagedWidget("editHighlightPatterns",
            xmFormWidgetClass, HighlightDialog.shell,
            XmNautoUnmanage, False,
            XmNresizePolicy, XmRESIZE_NONE,
            NULL);
    XtAddCallback(form, XmNdestroyCallback, destroyCB, NULL);
    AddMotifCloseCallback(HighlightDialog.shell, closeCB, NULL);

    lmForm = XtVaCreateManagedWidget("lmForm", xmFormWidgetClass,
    	    form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNtopAttachment, XmATTACH_POSITION,
	    XmNtopPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, 99,
            NULL);
 
    HighlightDialog.lmPulldown = CreateLanguageModeMenu(lmForm, langModeCB,
    	    NULL);
    n = 0;
    XtSetArg(args[n], XmNspacing, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 50); n++;
    XtSetArg(args[n], XmNsubMenuId, HighlightDialog.lmPulldown); n++;
    lmOptMenu = XmCreateOptionMenu(lmForm, "langModeOptMenu", args, n);
    XtManageChild(lmOptMenu);
    HighlightDialog.lmOptMenu = lmOptMenu;
    
    XtVaCreateManagedWidget("lmLbl", xmLabelGadgetClass, lmForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Language Mode:"),
    	    XmNmnemonic, 'M',
    	    XmNuserData, XtParent(HighlightDialog.lmOptMenu),
    	    XmNalignment, XmALIGNMENT_END,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 50,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, lmOptMenu, NULL);
    XmStringFree(s1);
    
    lmBtn = XtVaCreateManagedWidget("lmBtn", xmPushButtonWidgetClass, lmForm,
    	    XmNlabelString, s1=MKSTRING("Add / Modify\nLanguage Mode..."),
    	    XmNmnemonic, 'A',
    	    XmNrightAttachment, XmATTACH_FORM,
    	    XmNtopAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(lmBtn, XmNactivateCallback, lmDialogCB, NULL);
    XmStringFree(s1);
    
    okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form,
            XmNlabelString, s1=XmStringCreateSimple("OK"),
            XmNmarginWidth, BUTTON_WIDTH_MARGIN,
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 13,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(okBtn, XmNactivateCallback, okCB, NULL);
    XmStringFree(s1);
    
    applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Apply"),
    	    XmNmnemonic, 'y',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 13,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 26,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(applyBtn, XmNactivateCallback, applyCB, NULL);
    XmStringFree(s1);
    
    checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Check"),
    	    XmNmnemonic, 'k',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 26,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 39,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(checkBtn, XmNactivateCallback, checkCB, NULL);
    XmStringFree(s1);
    
    deleteBtn = XtVaCreateManagedWidget("delete", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Delete"),
    	    XmNmnemonic, 'D',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 39,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 52,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(deleteBtn, XmNactivateCallback, deleteCB, NULL);
    XmStringFree(s1);
    
    restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, form,
    	    XmNlabelString, s1=XmStringCreateSimple("Restore Defaults"),
    	    XmNmnemonic, 'f',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 52,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 73,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(restoreBtn, XmNactivateCallback, restoreCB, NULL);
    XmStringFree(s1);
    
    closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass,
    	    form,
    	    XmNlabelString, s1=XmStringCreateSimple("Close"),
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 73,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 86,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(closeBtn, XmNactivateCallback, closeCB, NULL);
    XmStringFree(s1);
    
    helpBtn = XtVaCreateManagedWidget("help", xmPushButtonWidgetClass,
    	    form,
    	    XmNlabelString, s1=XmStringCreateSimple("Help"),
    	    XmNmnemonic, 'H',
    	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 86,
    	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, 99,
    	    XmNbottomAttachment, XmATTACH_FORM,
    	    XmNbottomOffset, BORDER, NULL);
    XtAddCallback(helpBtn, XmNactivateCallback, helpCB, NULL);
    XmStringFree(s1);
    
    contextFrame = XtVaCreateManagedWidget("contextFrame", xmFrameWidgetClass,
    	    form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99,
	    XmNbottomAttachment, XmATTACH_WIDGET,
    	    XmNbottomWidget, okBtn,
    	    XmNbottomOffset, BORDER, NULL);
    contextForm = XtVaCreateManagedWidget("contextForm", xmFormWidgetClass,
	    contextFrame, NULL);
    XtVaCreateManagedWidget("contextLbl", xmLabelGadgetClass, contextFrame,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	      "Context requirements for incremental re-parsing after changes"),
	    XmNchildType, XmFRAME_TITLE_CHILD, NULL);
    XmStringFree(s1);
    
    HighlightDialog.lineContextW = XtVaCreateManagedWidget("lineContext",
    	    xmTextWidgetClass, contextForm,
	    XmNcolumns, 5,
	    XmNmaxLength, 12,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 15,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 25, NULL);
    RemapDeleteKey(HighlightDialog.lineContextW);
    
    XtVaCreateManagedWidget("lineContLbl",
    	    xmLabelGadgetClass, contextForm,
    	    XmNlabelString, s1=XmStringCreateSimple("lines"),
    	    XmNmnemonic, 'l',
    	    XmNuserData, HighlightDialog.lineContextW,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, HighlightDialog.lineContextW,
	    XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNtopWidget, HighlightDialog.lineContextW,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
    	    XmNbottomWidget, HighlightDialog.lineContextW, NULL);
    XmStringFree(s1);

    HighlightDialog.charContextW = XtVaCreateManagedWidget("charContext",
    	    xmTextWidgetClass, contextForm,
	    XmNcolumns, 5,
	    XmNmaxLength, 12,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 58,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 68, NULL);
    RemapDeleteKey(HighlightDialog.lineContextW);
    
    XtVaCreateManagedWidget("charContLbl",
    	    xmLabelGadgetClass, contextForm,
    	    XmNlabelString, s1=XmStringCreateSimple("characters"),
    	    XmNmnemonic, 'c',
    	    XmNuserData, HighlightDialog.charContextW,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, HighlightDialog.charContextW,
	    XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNtopWidget, HighlightDialog.charContextW,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
    	    XmNbottomWidget, HighlightDialog.charContextW, NULL);
    XmStringFree(s1);
    
    patternsFrame = XtVaCreateManagedWidget("patternsFrame", xmFrameWidgetClass,
    	    form,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lmForm,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99,
	    XmNbottomAttachment, XmATTACH_WIDGET,
    	    XmNbottomWidget, contextFrame,
    	    XmNbottomOffset, BORDER, NULL);
    patternsForm = XtVaCreateManagedWidget("patternsForm", xmFormWidgetClass,
	    patternsFrame, NULL);
    patternsLbl = XtVaCreateManagedWidget("patternsLbl", xmLabelGadgetClass,
    	    patternsFrame,
    	    XmNlabelString, s1=XmStringCreateSimple("Patterns"),
    	    XmNmnemonic, 'P',
    	    XmNmarginHeight, 0,
	    XmNchildType, XmFRAME_TITLE_CHILD, NULL);
    XmStringFree(s1);
    
    typeLbl = XtVaCreateManagedWidget("typeLbl", xmLabelGadgetClass,
    	    patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Pattern Type:"),
    	    XmNmarginHeight, 0,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_FORM, NULL);
    XmStringFree(s1);

    typeBox = XtVaCreateManagedWidget("typeBox", xmRowColumnWidgetClass,
    	    patternsForm,
    	    XmNpacking, XmPACK_COLUMN,
    	    XmNradioBehavior, True,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, typeLbl, NULL);
    HighlightDialog.topLevelW = XtVaCreateManagedWidget("top", 
    	    xmToggleButtonWidgetClass, typeBox,
    	    XmNset, True,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	        "Pass-1 (applied to all text when loaded or modified)"),
    	    XmNmnemonic, '1', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.topLevelW, XmNvalueChangedCallback,
    	    patTypeCB, NULL);
    HighlightDialog.deferredW = XtVaCreateManagedWidget("deferred", 
    	    xmToggleButtonWidgetClass, typeBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	        "Pass-2 (parsing is deferred until text is exposed)"),
    	    XmNmnemonic, '2', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.deferredW, XmNvalueChangedCallback,
    	    patTypeCB, NULL);
    HighlightDialog.subPatW = XtVaCreateManagedWidget("subPat", 
    	    xmToggleButtonWidgetClass, typeBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Sub-pattern (processed within start & end of parent)"),
    	    XmNmnemonic, 'u', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.subPatW, XmNvalueChangedCallback,
    	    patTypeCB, NULL);
    HighlightDialog.colorPatW = XtVaCreateManagedWidget("color", 
    	    xmToggleButtonWidgetClass, typeBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Coloring for sub-expressions of parent pattern"),
    	    XmNmnemonic, 'g', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.colorPatW, XmNvalueChangedCallback,
    	    patTypeCB, NULL);

    HighlightDialog.matchLbl = XtVaCreateManagedWidget("matchLbl",
    	    xmLabelGadgetClass, patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Matching:"),
    	    XmNmarginHeight, 0,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopOffset, BORDER,
	    XmNtopWidget, typeBox, NULL);
    XmStringFree(s1);

    matchBox = XtVaCreateManagedWidget("matchBox", xmRowColumnWidgetClass,
    	    patternsForm,
    	    XmNpacking, XmPACK_COLUMN,
    	    XmNradioBehavior, True,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, HighlightDialog.matchLbl, NULL);
    HighlightDialog.simpleW = XtVaCreateManagedWidget("simple", 
    	    xmToggleButtonWidgetClass, matchBox,
    	    XmNset, True,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Highlight text matching regular expression"),
    	    XmNmnemonic, 'x', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.simpleW, XmNvalueChangedCallback,
    	    matchTypeCB, NULL);
    HighlightDialog.rangeW = XtVaCreateManagedWidget("range", 
    	    xmToggleButtonWidgetClass, matchBox,
    	    XmNmarginHeight, 0,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Highlight text between starting and ending REs"),
    	    XmNmnemonic, 'b', NULL);
    XmStringFree(s1);
    XtAddCallback(HighlightDialog.rangeW, XmNvalueChangedCallback,
    	    matchTypeCB, NULL);

    nameLbl = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass,
    	    patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Pattern Name"),
    	    XmNmnemonic, 'N',
    	    XmNrows, 20,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, matchBox,
	    XmNtopOffset, BORDER, NULL);
    XmStringFree(s1);
 
    HighlightDialog.nameW = XtVaCreateManagedWidget("name", xmTextWidgetClass,
    	    patternsForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, LIST_RIGHT,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, nameLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, (99 + LIST_RIGHT)/2, NULL);
    RemapDeleteKey(HighlightDialog.nameW);
    XtVaSetValues(nameLbl, XmNuserData, HighlightDialog.nameW, NULL);

    HighlightDialog.parentLbl = XtVaCreateManagedWidget("parentLbl",
    	    xmLabelGadgetClass, patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Parent Pattern"),
    	    XmNmnemonic, 't',
    	    XmNrows, 20,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, (99 + LIST_RIGHT)/2 + 1,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, matchBox,
	    XmNtopOffset, BORDER, NULL);
    XmStringFree(s1);
 
    HighlightDialog.parentW = XtVaCreateManagedWidget("parent",
    	    xmTextWidgetClass, patternsForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, (99 + LIST_RIGHT)/2 + 1,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, HighlightDialog.parentLbl,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(HighlightDialog.parentW);
    XtVaSetValues(HighlightDialog.parentLbl, XmNuserData,
    	    HighlightDialog.parentW, NULL);

    HighlightDialog.startLbl = XtVaCreateManagedWidget("startLbl",
    	    xmLabelGadgetClass, patternsForm,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNmnemonic, 'R',
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, HighlightDialog.parentW,
	    XmNtopOffset, BORDER,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1, NULL);
 
    HighlightDialog.errorW = XtVaCreateManagedWidget("error",
    	    xmTextWidgetClass, patternsForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99,
	    XmNbottomAttachment, XmATTACH_POSITION,
	    XmNbottomPosition, 99, NULL);
    RemapDeleteKey(HighlightDialog.errorW);

    HighlightDialog.errorLbl = XtVaCreateManagedWidget("errorLbl",
    	    xmLabelGadgetClass, patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple(
    	    	"Regular Expression Indicating Error in Match (Optional)"),
    	    XmNmnemonic, 'o',
    	    XmNuserData, HighlightDialog.errorW,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, HighlightDialog.errorW, NULL);
    XmStringFree(s1);
 
    HighlightDialog.endW = XtVaCreateManagedWidget("end",
    	    xmTextWidgetClass, patternsForm,
	    XmNleftAttachment, XmATTACH_POSITION,
	    XmNleftPosition, 1,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, HighlightDialog.errorLbl,
	    XmNbottomOffset, BORDER,
	    XmNrightAttachment, XmATTACH_POSITION,
	    XmNrightPosition, 99, NULL);
    RemapDeleteKey(HighlightDialog.endW);

    HighlightDialog.endLbl = XtVaCreateManagedWidget("endLbl",
    	    xmLabelGadgetClass, patternsForm,
    	    XmNmnemonic, 'E',
    	    XmNuserData, HighlightDialog.endW,
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, HighlightDialog.endW, NULL);

    n = 0;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNscrollHorizontal, False); n++;
    XtSetArg(args[n], XmNwordWrap, True); n++;
    XtSetArg(args[n], XmNrows, 3); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, HighlightDialog.endLbl); n++;
    XtSetArg(args[n], XmNbottomOffset, BORDER); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, HighlightDialog.startLbl); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 99); n++;
    HighlightDialog.startW = XmCreateScrolledText(patternsForm, "start",args,n);
    AddMouseWheelSupport(HighlightDialog.startW);
    XtManageChild(HighlightDialog.startW);
    MakeSingleLineTextW(HighlightDialog.startW);
    RemapDeleteKey(HighlightDialog.startW);
    XtVaSetValues(HighlightDialog.startLbl,
    		XmNuserData,HighlightDialog.startW, NULL);

    styleBtn = XtVaCreateManagedWidget("styleLbl", xmPushButtonWidgetClass,
    	    patternsForm,
    	    XmNlabelString, s1=MKSTRING("Add / Modify\nStyle..."),
    	    XmNmnemonic, 'i',
	    XmNrightAttachment, XmATTACH_POSITION,
    	    XmNrightPosition, LIST_RIGHT-1,
	    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNbottomWidget, HighlightDialog.parentW, NULL);
    XmStringFree(s1);
    XtAddCallback(styleBtn, XmNactivateCallback, styleDialogCB, NULL);

    HighlightDialog.stylePulldown = createHighlightStylesMenu(patternsForm);
    n = 0;
    XtSetArg(args[n], XmNspacing, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, HighlightDialog.parentW); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, styleBtn); n++;
    XtSetArg(args[n], XmNsubMenuId, HighlightDialog.stylePulldown); n++;
    HighlightDialog.styleOptMenu = XmCreateOptionMenu(patternsForm,
    		"styleOptMenu", args, n);
    XtManageChild(HighlightDialog.styleOptMenu);

    styleLbl = XtVaCreateManagedWidget("styleLbl", xmLabelGadgetClass,
    	    patternsForm,
    	    XmNlabelString, s1=XmStringCreateSimple("Highlight Style"),
    	    XmNmnemonic, 'S',
    	    XmNuserData, XtParent(HighlightDialog.styleOptMenu),
    	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNleftAttachment, XmATTACH_POSITION,
    	    XmNleftPosition, 1,
	    XmNbottomAttachment, XmATTACH_WIDGET,
	    XmNbottomWidget, HighlightDialog.styleOptMenu, NULL);
    XmStringFree(s1);

    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, LIST_RIGHT-1); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, styleLbl); n++;
    XtSetArg(args[n], XmNbottomOffset, BORDER); n++;
    HighlightDialog.managedListW = CreateManagedList(patternsForm, "list", args,
    	    n, (void **)HighlightDialog.patterns, &HighlightDialog.nPatterns,
    	    MAX_PATTERNS, 18, getDisplayedCB, NULL, setDisplayedCB,
    	    NULL, freeItemCB);
    XtVaSetValues(patternsLbl, XmNuserData, HighlightDialog.managedListW, NULL);

    /* Set initial default button */
    XtVaSetValues(form, XmNdefaultButton, okBtn, NULL);
    XtVaSetValues(form, XmNcancelButton, closeBtn, NULL);
    
    /* Handle mnemonic selection of buttons and focus to dialog */
    AddDialogMnemonicHandler(form, FALSE);
    
    /* Fill in the dialog information for the selected language mode */
    SetIntText(HighlightDialog.lineContextW, patSet==NULL ? 1 :
    	    patSet->lineContext);
    SetIntText(HighlightDialog.charContextW, patSet==NULL ? 0 :
    	    patSet->charContext);
    SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);
    updateLabels();
    
    /* Realize all of the widgets in the new dialog */
    RealizeWithoutForcingPosition(HighlightDialog.shell);
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing highlight styles updated (via a call to createHighlightStylesMenu)
*/
static void updateHighlightStyleMenu(void)
{
    Widget oldMenu;
    int patIndex;
    
    if (HighlightDialog.shell == NULL)
    	return;
    
    oldMenu = HighlightDialog.stylePulldown;
    HighlightDialog.stylePulldown = createHighlightStylesMenu(
    	    XtParent(XtParent(oldMenu)));
    XtVaSetValues(XmOptionButtonGadget(HighlightDialog.styleOptMenu),
    	    XmNsubMenuId, HighlightDialog.stylePulldown, NULL);
    patIndex = ManagedListSelectedIndex(HighlightDialog.managedListW);
    if (patIndex == -1)
    	setStyleMenu("Plain");
    else
    	setStyleMenu(HighlightDialog.patterns[patIndex]->style);
    
    XtDestroyWidget(oldMenu);
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLanguageModeMenu(void)
{
    Widget oldMenu;

    if (HighlightDialog.shell == NULL)
    	return;

    oldMenu = HighlightDialog.lmPulldown;
    HighlightDialog.lmPulldown = CreateLanguageModeMenu(
    	    XtParent(XtParent(oldMenu)), langModeCB, NULL);
    XtVaSetValues(XmOptionButtonGadget(HighlightDialog.lmOptMenu),
    	    XmNsubMenuId, HighlightDialog.lmPulldown, NULL);
    SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);

    XtDestroyWidget(oldMenu);
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int i;
    
    NEditFree(HighlightDialog.langModeName);
    for (i=0; i<HighlightDialog.nPatterns; i++)
     	freePatternSrc(HighlightDialog.patterns[i], True);
    HighlightDialog.shell = NULL;
}

static void langModeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    char *modeName;
    patternSet *oldPatSet, *newPatSet;
    patternSet emptyPatSet = {NULL, 1, 0, 0, NULL};
    int i, resp;
    
    /* Get the newly selected mode name.  If it's the same, do nothing */
    XtVaGetValues(w, XmNuserData, &modeName, NULL);
    if (!strcmp(modeName, HighlightDialog.langModeName))
    	return;
    
    /* Look up the original version of the patterns being edited */
    oldPatSet = FindPatternSet(HighlightDialog.langModeName);
    if (oldPatSet == NULL)
    	oldPatSet = &emptyPatSet;
    
    /* Get the current information displayed by the dialog.  If it's bad,
       give the user the chance to throw it out or go back and fix it.  If
       it has changed, give the user the chance to apply discard or cancel. */
    newPatSet = getDialogPatternSet();

    if (newPatSet == NULL)
    {
        if (DialogF(DF_WARN, HighlightDialog.shell, 2,
                "Incomplete Language Mode", "Discard incomplete entry\n"
                "for current language mode?", "Keep", "Discard") == 1)
        {
            SetLangModeMenu(HighlightDialog.lmOptMenu,
                    HighlightDialog.langModeName);
            return;
        }
    } else if (patternSetsDiffer(oldPatSet, newPatSet))
    {
        resp = DialogF(DF_WARN, HighlightDialog.shell, 3, "Language Mode",
                "Apply changes for language mode %s?", "Apply Changes",
                "Discard Changes", "Cancel", HighlightDialog.langModeName);
        if (resp == 3)
        {
            SetLangModeMenu(HighlightDialog.lmOptMenu,
                    HighlightDialog.langModeName);
            return;
        }
        if (resp == 1)
        {
            updatePatternSet();
        }
    }

    if (newPatSet != NULL)
    	freePatternSet(newPatSet);

    /* Free the old dialog information */
    NEditFree(HighlightDialog.langModeName);
    for (i=0; i<HighlightDialog.nPatterns; i++)
     	freePatternSrc(HighlightDialog.patterns[i], True);
    
    /* Fill the dialog with the new language mode information */
    HighlightDialog.langModeName = NEditStrdup(modeName);
    newPatSet = FindPatternSet(modeName);
    if (newPatSet == NULL) {
    	HighlightDialog.nPatterns = 0;
    	SetIntText(HighlightDialog.lineContextW, 1);
    	SetIntText(HighlightDialog.charContextW, 0);
    } else {
	for (i=0; i<newPatSet->nPatterns; i++)
    	    HighlightDialog.patterns[i] =
    		    copyPatternSrc(&newPatSet->patterns[i], NULL);
	HighlightDialog.nPatterns = newPatSet->nPatterns;
    	SetIntText(HighlightDialog.lineContextW, newPatSet->lineContext);
    	SetIntText(HighlightDialog.charContextW, newPatSet->charContext);
    }
    ChangeManagedListData(HighlightDialog.managedListW);
}

static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData)
{
    EditLanguageModes();
}

static void styleDialogCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Widget selectedItem;
    char *style;
    
    XtVaGetValues(HighlightDialog.styleOptMenu,
            XmNmenuHistory, &selectedItem,
            NULL);
    XtVaGetValues(selectedItem,
            XmNuserData, &style,
            NULL);
    EditHighlightStyles(style);
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the patterns */
    if (!updatePatternSet())
    	return;
    
    /* pop down and destroy the dialog */
    CloseAllPopupsFor(HighlightDialog.shell);
    XtDestroyWidget(HighlightDialog.shell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* change the patterns */
    updatePatternSet();
}

static void checkCB(Widget w, XtPointer clientData, XtPointer callData)
{
    if (checkHighlightDialogData())
    {
        DialogF(DF_INF, HighlightDialog.shell, 1, "Pattern compiled",
                "Patterns compiled without error", "OK");
    }
}

static void restoreCB(Widget w, XtPointer clientData, XtPointer callData)
{
    patternSet *defaultPatSet;
    int i, psn;
    
    defaultPatSet = readDefaultPatternSet(HighlightDialog.langModeName);
    if (defaultPatSet == NULL)
    {
        DialogF(DF_WARN, HighlightDialog.shell, 1, "No Default Pattern",
                "There is no default pattern set\nfor language mode %s",
                "OK", HighlightDialog.langModeName);
        return;
    }
    
    if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Discard Changes",
            "Are you sure you want to discard\n"
            "all changes to syntax highlighting\n"
            "patterns for language mode %s?", "Discard", "Cancel",
            HighlightDialog.langModeName) == 2)
    {
        return;
    }
    
    /* if a stored version of the pattern set exists, replace it, if it
       doesn't, add a new one */
    for (psn=0; psn<NPatternSets; psn++)
    	if (!strcmp(HighlightDialog.langModeName,
    	    	PatternSets[psn]->languageMode))
    	    break;
    if (psn < NPatternSets) {
     	freePatternSet(PatternSets[psn]);
   	PatternSets[psn] = defaultPatSet;
    } else
    	PatternSets[NPatternSets++] = defaultPatSet;

    /* Free the old dialog information */
    for (i=0; i<HighlightDialog.nPatterns; i++)
     	freePatternSrc(HighlightDialog.patterns[i], True);
    
    /* Update the dialog */
    HighlightDialog.nPatterns = defaultPatSet->nPatterns;
    for (i=0; i<defaultPatSet->nPatterns; i++)
    	HighlightDialog.patterns[i] =
    		copyPatternSrc(&defaultPatSet->patterns[i], NULL);
    	SetIntText(HighlightDialog.lineContextW, defaultPatSet->lineContext);
    	SetIntText(HighlightDialog.charContextW, defaultPatSet->charContext);
    ChangeManagedListData(HighlightDialog.managedListW);
}
	
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int i, psn;
    
    if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Delete Pattern",
            "Are you sure you want to delete\n"
            "syntax highlighting patterns for\n"
            "language mode %s?", "Yes, Delete", "Cancel",
            HighlightDialog.langModeName) == 2)
    {
        return;
    }
    
    /* if a stored version of the pattern set exists, delete it from the list */
    for (psn=0; psn<NPatternSets; psn++)
    	if (!strcmp(HighlightDialog.langModeName,
    	    	PatternSets[psn]->languageMode))
    	    break;
    if (psn < NPatternSets) {
     	freePatternSet(PatternSets[psn]);
   	memmove(&PatternSets[psn], &PatternSets[psn+1],
   	    	(NPatternSets-1 - psn) * sizeof(patternSet *));
    	NPatternSets--;
    }

    /* Free the old dialog information */
    for (i=0; i<HighlightDialog.nPatterns; i++)
     	freePatternSrc(HighlightDialog.patterns[i], True);
    
    /* Clear out the dialog */
    HighlightDialog.nPatterns = 0;
    SetIntText(HighlightDialog.lineContextW, 1);
    SetIntText(HighlightDialog.charContextW, 0);
    ChangeManagedListData(HighlightDialog.managedListW);
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    /* pop down and destroy the dialog */
    CloseAllPopupsFor(HighlightDialog.shell);
    XtDestroyWidget(HighlightDialog.shell);
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData)
{
    Help(HELP_PATTERNS);
}

static void patTypeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    updateLabels();
}

static void matchTypeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    updateLabels();
}

static void *getDisplayedCB(void *oldItem, int explicitRequest, int *abort,
    	void *cbArg)
{
    highlightPattern *pat;
    
    /* If the dialog is currently displaying the "new" entry and the
       fields are empty, that's just fine */
    if (oldItem == NULL && dialogEmpty())
    	return NULL;
    
    /* If there are no problems reading the data, just return it */
    pat = readDialogFields(True);
    if (pat != NULL)
    	return (void *)pat;
    
    /* If there are problems, and the user didn't ask for the fields to be
       read, give more warning */
    if (!explicitRequest)
    {
        if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Discard Entry",
                "Discard incomplete entry\nfor current pattern?", "Keep",
                "Discard") == 2)
        {
            return oldItem == NULL
                    ? NULL
                    : (void *)copyPatternSrc((highlightPattern *)oldItem, NULL);
        }
    }

    /* Do readDialogFields again without "silent" mode to display warning */
    pat = readDialogFields(False);
    *abort = True;
    return NULL;
}

static void setDisplayedCB(void *item, void *cbArg)
{
    highlightPattern *pat = (highlightPattern *)item;
    int isSubpat, isDeferred, isColorOnly, isRange;

    if (item == NULL) {
    	XmTextSetString(HighlightDialog.nameW, "");
    	XmTextSetString(HighlightDialog.parentW, "");
    	XmTextSetString(HighlightDialog.startW, "");
    	XmTextSetString(HighlightDialog.endW, "");
    	XmTextSetString(HighlightDialog.errorW, "");
    	RadioButtonChangeState(HighlightDialog.topLevelW, True, False);
    	RadioButtonChangeState(HighlightDialog.deferredW, False, False);
    	RadioButtonChangeState(HighlightDialog.subPatW, False, False);
    	RadioButtonChangeState(HighlightDialog.colorPatW, False, False);
    	RadioButtonChangeState(HighlightDialog.simpleW, True, False);
    	RadioButtonChangeState(HighlightDialog.rangeW, False, False);
    	setStyleMenu("Plain");
    } else {
    	isSubpat = pat->subPatternOf != NULL;
    	isDeferred = pat->flags & DEFER_PARSING;
    	isColorOnly = pat->flags & COLOR_ONLY;
    	isRange = pat->endRE != NULL;
    	XmTextSetString(HighlightDialog.nameW, pat->name);
    	XmTextSetString(HighlightDialog.parentW, pat->subPatternOf);
    	XmTextSetString(HighlightDialog.startW, pat->startRE);
    	XmTextSetString(HighlightDialog.endW, pat->endRE);
    	XmTextSetString(HighlightDialog.errorW, pat->errorRE);
    	RadioButtonChangeState(HighlightDialog.topLevelW,
    	    	!isSubpat && !isDeferred, False);
    	RadioButtonChangeState(HighlightDialog.deferredW,
    	    	!isSubpat && isDeferred, False);
    	RadioButtonChangeState(HighlightDialog.subPatW,
    	    	isSubpat && !isColorOnly, False);
    	RadioButtonChangeState(HighlightDialog.colorPatW,
    	    	isSubpat && isColorOnly, False);
    	RadioButtonChangeState(HighlightDialog.simpleW, !isRange, False);
    	RadioButtonChangeState(HighlightDialog.rangeW, isRange, False);
    	setStyleMenu(pat->style);
    }
    updateLabels();
}

static void freeItemCB(void *item)
{
    freePatternSrc((highlightPattern *)item, True);
}

/*
** Do a test compile of the patterns currently displayed in the highlight
** patterns dialog, and display warning dialogs if there are problems
*/
static int checkHighlightDialogData(void)
{
    patternSet *patSet;
    int result;
    
    /* Get the pattern information from the dialog */
    patSet = getDialogPatternSet();
    if (patSet == NULL)
    	return False;
     
    /* Compile the patterns  */
    result = patSet->nPatterns == 0 ? True : TestHighlightPatterns(patSet);
    freePatternSet(patSet);
    return result;
}

/*
** Update the text field labels and sensitivity of various fields, based on
** the settings of the Pattern Type and Matching radio buttons in the highlight
** patterns dialog.
*/
static void updateLabels(void)
{
    char *startLbl, *endLbl;
    int endSense, errSense, matchSense, parentSense;
    XmString s1;
    
    if (XmToggleButtonGetState(HighlightDialog.colorPatW)) {
	startLbl =  "Sub-expressions to Highlight in Parent's Starting \
Regular Expression (\\1, &, etc.)";
	endLbl = "Sub-expressions to Highlight in Parent Pattern's Ending \
Regular Expression";
    	endSense = True;
    	errSense = False;
    	matchSense = False;
    	parentSense = True;
    } else {
    	endLbl = "Ending Regular Expression";
    	matchSense = True;
    	parentSense = XmToggleButtonGetState(HighlightDialog.subPatW);
    	if (XmToggleButtonGetState(HighlightDialog.simpleW)) {
    	    startLbl = "Regular Expression to Match";
    	    endSense = False;
    	    errSense = False;
    	} else {
    	    startLbl = "Starting Regular Expression";
    	    endSense = True;
    	    errSense = True;
	}
    }
    
    XtSetSensitive(HighlightDialog.parentLbl, parentSense);
    XtSetSensitive(HighlightDialog.parentW, parentSense);
    XtSetSensitive(HighlightDialog.endW, endSense);
    XtSetSensitive(HighlightDialog.endLbl, endSense);
    XtSetSensitive(HighlightDialog.errorW, errSense);
    XtSetSensitive(HighlightDialog.errorLbl, errSense);
    XtSetSensitive(HighlightDialog.errorLbl, errSense);
    XtSetSensitive(HighlightDialog.simpleW, matchSense);
    XtSetSensitive(HighlightDialog.rangeW, matchSense);
    XtSetSensitive(HighlightDialog.matchLbl, matchSense);
    XtVaSetValues(HighlightDialog.startLbl, XmNlabelString,
    	    s1=XmStringCreateSimple(startLbl), NULL);
    XmStringFree(s1);
    XtVaSetValues(HighlightDialog.endLbl, XmNlabelString,
    	    s1=XmStringCreateSimple(endLbl), NULL);
    XmStringFree(s1);
}

/*
** Set the styles menu in the currently displayed highlight dialog to show
** a particular style
*/
static void setStyleMenu(const char *styleName)
{
    int i;
    Cardinal nItems;
    WidgetList items;
    Widget selectedItem;
    char *itemStyle;

    XtVaGetValues(HighlightDialog.stylePulldown, XmNchildren, &items,
    	    XmNnumChildren, &nItems, NULL);
    if (nItems == 0)
    	return;
    selectedItem = items[0];
    for (i=0; i<(int)nItems; i++) {
    	XtVaGetValues(items[i], XmNuserData, &itemStyle, NULL);
    	if (!strcmp(itemStyle, styleName)) {
    	    selectedItem = items[i];
    	    break;
    	}
    }
    XtVaSetValues(HighlightDialog.styleOptMenu, XmNmenuHistory, selectedItem, NULL);
}

/*
** Read the pattern fields of the highlight dialog, and produce an allocated
** highlightPattern structure reflecting the contents, or pop up dialogs
** telling the user what's wrong (Passing "silent" as True, suppresses these
** dialogs).  Returns NULL on error.
*/ 
static highlightPattern *readDialogFields(int silent)
{
    highlightPattern *pat;
    char *inPtr, *outPtr, *style;
    Widget selectedItem;
    int colorOnly;

    /* Allocate a pattern source structure to return, zero out fields
       so that the whole pattern can be freed on error with freePatternSrc */
    pat = (highlightPattern *)NEditMalloc(sizeof(highlightPattern));
    pat->endRE = NULL;
    pat->errorRE = NULL;
    pat->style = NULL;
    pat->subPatternOf = NULL;
    
    /* read the type buttons */
    pat->flags = 0;
    colorOnly = XmToggleButtonGetState(HighlightDialog.colorPatW);
    if (XmToggleButtonGetState(HighlightDialog.deferredW))
    	pat->flags |= DEFER_PARSING;
    else if (colorOnly)
    	pat->flags = COLOR_ONLY;

    /* read the name field */
    pat->name = ReadSymbolicFieldTextWidget(HighlightDialog.nameW,
    	    "highlight pattern name", silent);
    if (pat->name == NULL) {
    	NEditFree(pat);
    	return NULL;
    }

    if (*pat->name == '\0')
    {
        if (!silent)
        {
            DialogF(DF_WARN, HighlightDialog.shell, 1, "Pattern Name",
                    "Please specify a name\nfor the pattern", "OK");
            XmProcessTraversal(HighlightDialog.nameW, XmTRAVERSE_CURRENT);
        }
        NEditFree(pat->name);
        NEditFree(pat);
        return NULL;
    }
    
    /* read the startRE field */
    pat->startRE = XmTextGetString(HighlightDialog.startW);
    if (*pat->startRE == '\0')
    {
        if (!silent)
        {
            DialogF(DF_WARN, HighlightDialog.shell, 1, "Matching Regex",
                    "Please specify a regular\nexpression to match", "OK");
            XmProcessTraversal(HighlightDialog.startW, XmTRAVERSE_CURRENT);
        }
        freePatternSrc(pat, True);
        return NULL;
    }
    
    /* Make sure coloring patterns contain only sub-expression references
       and put it in replacement regular-expression form */
    if (colorOnly)
    {
        for (inPtr=pat->startRE, outPtr=pat->startRE; *inPtr!='\0'; inPtr++)
        {
            if (*inPtr!=' ' && *inPtr!='\t')
            {
                *outPtr++ = *inPtr;
            }
        }

        *outPtr = '\0';
        if (strspn(pat->startRE, "&\\123456789 \t") != strlen(pat->startRE)
                || (*pat->startRE != '\\' && *pat->startRE != '&')
                || strstr(pat->startRE, "\\\\") != NULL)
        {
            if (!silent)
            {
                DialogF(DF_WARN, HighlightDialog.shell, 1, "Pattern Error",
                        "The expression field in patterns which specify highlighting for\n"
                        "a parent, must contain only sub-expression references in regular\n"
                        "expression replacement form (&\\1\\2 etc.).  See Help -> Regular\n"
                        "Expressions and Help -> Syntax Highlighting for more information",
                        "OK");
                XmProcessTraversal(HighlightDialog.startW, XmTRAVERSE_CURRENT);
            }
            freePatternSrc(pat, True);
            return NULL;
        }
    }

    /* read the parent field */
    if (XmToggleButtonGetState(HighlightDialog.subPatW) || colorOnly)
    {
        if (TextWidgetIsBlank(HighlightDialog.parentW))
        {
            if (!silent)
            {
                DialogF(DF_WARN, HighlightDialog.shell, 1,
                        "Specify Parent Pattern",
                        "Please specify a parent pattern", "OK");
                XmProcessTraversal(HighlightDialog.parentW, XmTRAVERSE_CURRENT);
            }
            freePatternSrc(pat, True);
            return NULL;
        }
        pat->subPatternOf = XmTextGetString(HighlightDialog.parentW);
    }
    
    /* read the styles option menu */
    XtVaGetValues(HighlightDialog.styleOptMenu, XmNmenuHistory, &selectedItem, NULL);
    XtVaGetValues(selectedItem, XmNuserData, &style, NULL);
    pat->style = (char*)NEditMalloc(strlen(style) + 1);
    strcpy(pat->style, style);
    

    /* read the endRE field */
    if (colorOnly || XmToggleButtonGetState(HighlightDialog.rangeW))
    {
        pat->endRE = XmTextGetString(HighlightDialog.endW);
        if (!colorOnly && *pat->endRE == '\0')
        {
            if (!silent)
            {
                DialogF(DF_WARN, HighlightDialog.shell, 1, "Specify Regex",
                        "Please specify an ending\nregular expression",
                        "OK");
                XmProcessTraversal(HighlightDialog.endW, XmTRAVERSE_CURRENT);
            }
            freePatternSrc(pat, True);
            return NULL;
        }
    }
    
    /* read the errorRE field */
    if (XmToggleButtonGetState(HighlightDialog.rangeW)) {
	pat->errorRE = XmTextGetString(HighlightDialog.errorW);
	if (*pat->errorRE == '\0') {
            NEditFree(pat->errorRE);
            pat->errorRE = NULL;
	}
    }
    return pat;
}

/*
** Returns true if the pattern fields of the highlight dialog are set to
** the default ("New" pattern) state.
*/
static int dialogEmpty(void)
{
    return TextWidgetIsBlank(HighlightDialog.nameW) &&
	    XmToggleButtonGetState(HighlightDialog.topLevelW) &&
	    XmToggleButtonGetState(HighlightDialog.simpleW) &&
	    TextWidgetIsBlank(HighlightDialog.parentW) &&
	    TextWidgetIsBlank(HighlightDialog.startW) &&
	    TextWidgetIsBlank(HighlightDialog.endW) &&
	    TextWidgetIsBlank(HighlightDialog.errorW);
}   	

/*
** Update the pattern set being edited in the Syntax Highlighting dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the patterns.
*/
static int updatePatternSet(void)
{
    patternSet *patSet;
    WindowInfo *window;
    int psn, oldNum = -1;
    

    /* Make sure the patterns are valid and compile */
    if (!checkHighlightDialogData())
    	return False;
    
    /* Get the current data */
    patSet = getDialogPatternSet();
    if (patSet == NULL)
    	return False;
    
    /* Find the pattern being modified */
    for (psn=0; psn<NPatternSets; psn++)
    	if (!strcmp(HighlightDialog.langModeName,
    	    	PatternSets[psn]->languageMode))
    	    break;
    
    /* If it's a new pattern, add it at the end, otherwise free the
       existing pattern set and replace it */
    if (psn == NPatternSets) {
    	PatternSets[NPatternSets++] = patSet;
        oldNum = 0;
    } else {
        oldNum = PatternSets[psn]->nPatterns;
	freePatternSet(PatternSets[psn]);
	PatternSets[psn] = patSet;
    }

    /* Find windows that are currently using this pattern set and
       re-do the highlighting */
    for (window = WindowList; window != NULL; window = window->next) {
        if (patSet->nPatterns > 0) {
            if (window->languageMode != PLAIN_LANGUAGE_MODE
                    && 0 == strcmp(LanguageModeName(window->languageMode),
                        patSet->languageMode)) {
                /*  The user worked on the current document's language mode, so
                    we have to make some changes immediately. For inactive
                    modes, the changes will be activated on activation.  */
                if (oldNum == 0) {
                    /*  Highlighting (including menu entry) was deactivated in
                        this function or in preferences.c::reapplyLanguageMode()
                        if the old set had no patterns, so reactivate menu entry. */
                    if (IsTopDocument(window)) {
                        XtSetSensitive(window->highlightItem, True);
                    }

                    /*  Reactivate highlighting if it's default  */
                    window->highlightSyntax = GetPrefHighlightSyntax();
                }

                if (window->highlightSyntax) {
                    StopHighlighting(window);
                    if (IsTopDocument(window)) {
                        XtSetSensitive(window->highlightItem, True);
                        SetToggleButtonState(window, window->highlightItem,
                                True, False);
                    }
                    StartHighlighting(window, True);
                }
            }
        } else {
            /*  No pattern in pattern set. This will probably not happen much,
                but you never know.  */
            StopHighlighting(window);
            window->highlightSyntax = False;

            if (IsTopDocument(window)) {
                XtSetSensitive(window->highlightItem, False);
                SetToggleButtonState(window, window->highlightItem, False, False);
            }
        }
    }

    /* Note that preferences have been changed */
    MarkPrefsChanged();

    return True;
}

/*
** Get the current information that the user has entered in the syntax
** highlighting dialog.  Return NULL if the data is currently invalid
*/
static patternSet *getDialogPatternSet(void)
{
    int i, lineContext, charContext;
    patternSet *patSet;
    
    /* Get the current contents of the "patterns" dialog fields */
    if (!UpdateManagedList(HighlightDialog.managedListW, True))
    	return NULL;
    
    /* Get the line and character context values */
    if (GetIntTextWarn(HighlightDialog.lineContextW, &lineContext,
    	    "context lines", True) != TEXT_READ_OK)
    	return NULL;
    if (GetIntTextWarn(HighlightDialog.charContextW, &charContext,
    	    "context lines", True) != TEXT_READ_OK)
    	return NULL;
    
    /* Allocate a new pattern set structure and copy the fields read from the
       dialog, including the modified pattern list into it */
    patSet = (patternSet *)NEditMalloc(sizeof(patternSet));
    patSet->languageMode = NEditStrdup(HighlightDialog.langModeName);
    patSet->lineContext = lineContext;
    patSet->charContext = charContext;
    patSet->nPatterns = HighlightDialog.nPatterns;
    patSet->patterns = (highlightPattern *)NEditMalloc(sizeof(highlightPattern) *
    	    HighlightDialog.nPatterns);
    for (i=0; i<HighlightDialog.nPatterns; i++)
    	copyPatternSrc(HighlightDialog.patterns[i], &patSet->patterns[i]);
    return patSet;
}

/*
** Return True if "patSet1" and "patSet2" differ
*/
static int patternSetsDiffer(patternSet *patSet1, patternSet *patSet2)
{
    int i;
    highlightPattern *pat1, *pat2;
    
    if (patSet1->lineContext != patSet2->lineContext)
    	return True;
    if (patSet1->charContext != patSet2->charContext)
    	return True;
    if (patSet1->nPatterns != patSet2->nPatterns)
    	return True;
    for (i=0; i<patSet2->nPatterns; i++) {
    	pat1 = &patSet1->patterns[i];
    	pat2 = &patSet2->patterns[i];
    	if (pat1->flags != pat2->flags)
    	    return True;
    	if (AllocatedStringsDiffer(pat1->name, pat2->name))
    	    return True;
    	if (AllocatedStringsDiffer(pat1->startRE, pat2->startRE))
    	    return True;
    	if (AllocatedStringsDiffer(pat1->endRE, pat2->endRE))
    	    return True;
    	if (AllocatedStringsDiffer(pat1->errorRE, pat2->errorRE))
    	    return True;
    	if (AllocatedStringsDiffer(pat1->style, pat2->style))
    	    return True;
    	if (AllocatedStringsDiffer(pat1->subPatternOf, pat2->subPatternOf))
    	    return True;
    }
    return False;
}

/*
** Copy a highlight pattern data structure and all of the allocated data
** it contains.  If "copyTo" is non-null, use that as the top-level structure,
** otherwise allocate a new highlightPattern structure and return it as the
** function value.
*/
static highlightPattern *copyPatternSrc(highlightPattern *pat,
    	highlightPattern *copyTo)
{
    highlightPattern *newPat;
    
    if (copyTo == NULL)
    	newPat = (highlightPattern *)NEditMalloc(sizeof(highlightPattern));
    else
    	newPat = copyTo;
    newPat->name = NEditStrdup(pat->name);
    newPat->startRE = NEditStrdup(pat->startRE);
    newPat->endRE = NEditStrdup(pat->endRE);
    newPat->errorRE = NEditStrdup(pat->errorRE);
    newPat->style = NEditStrdup(pat->style);
    newPat->subPatternOf = NEditStrdup(pat->subPatternOf);
    newPat->flags = pat->flags;    
    return newPat;
}

/*
** Free the allocated memory contained in a highlightPattern data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static void freePatternSrc(highlightPattern *pat, int freeStruct)
{
    NEditFree(pat->name);
    NEditFree(pat->startRE);
    NEditFree(pat->endRE);
    NEditFree(pat->errorRE);
    NEditFree(pat->style);
    NEditFree(pat->subPatternOf);
    if (freeStruct)
    	NEditFree(pat);
}

/*
** Free the allocated memory contained in a patternSet data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static void freePatternSet(patternSet *p)
{
    int i;
    
    for (i=0; i<p->nPatterns; i++)
    	freePatternSrc(&p->patterns[i], False);
    NEditFree(p->languageMode);
    NEditFree(p->patterns);
    NEditFree(p);
}

#if 0
/*
** Free the allocated memory contained in a patternSet data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static int lookupNamedPattern(patternSet *p, char *patternName)
{
    int i;
    
    for (i=0; i<p->nPatterns; i++)
      if (strcmp(p->patterns[i].name, patternName))
              return i;
    return -1;
}
#endif

/*
** Find the index into the HighlightStyles array corresponding to "styleName".
** If styleName is not found, return -1.
*/
static int lookupNamedStyle(const char *styleName)
{
    int i;
    
    for (i = 0; i < NHighlightStyles; i++)
    {
        if (!strcmp(styleName, HighlightStyles[i]->name))
        {
            return i;
        }
    }

    return -1;
}

/*
** Returns a unique number of a given style name
*/
int IndexOfNamedStyle(const char *styleName)
{
    return lookupNamedStyle(styleName);
}

/*
** Write the string representation of int "i" to a static area, and
** return a pointer to it.
*/
static char *intToStr(int i)
{
    static char outBuf[12];
    
    sprintf(outBuf, "%d", i);
    return outBuf;
}
