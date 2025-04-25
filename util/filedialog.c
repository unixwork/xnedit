/*
 * Copyright 2019 Olaf Wintermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include "filedialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>

#include <Xm/PrimitiveP.h>
#include <X11/CoreP.h>

#include"../Microline/XmL/Grid.h"

#include <X11/Xresource.h>
#include <X11/keysym.h>


/* nedit utils */
#include "nedit_malloc.h"
#include "utils.h"
#include "fileUtils.h"
#include "getfiles.h"
#include "misc.h"
#include "textfield.h"
#include "ec_glob.h"
#include "pathutils.h"
#include "unicode.h"

#include "../source/preferences.h"
#include "../source/filter.h"

#include "DialogF.h"

#define WIDGET_SPACING 5
#define WINDOW_SPACING 8

#define BUTTON_EXTRA_SPACE 4

#define DATE_FORMAT_SAME_YEAR  "%b %d %H:%M"
#define DATE_FORMAT_OTHER_YEAR "%b %d  %Y"

#define KB_SUFFIX "KiB"
#define MB_SUFFIX "MiB"
#define GB_SUFFIX "GiB"
#define TB_SUFFIX "TiB"

static int pixmaps_initialized = 0;
static int pixmaps_error = 0;

static Pixmap newFolderIcon16;
static Pixmap newFolderIcon24;
static Pixmap newFolderIcon32;

static XColor bgColor;

static int LastView = -1; // 0: icon   1: list   2: grid
static int ShowHidden = -1;
static char *LastFilter;

typedef int(*FileCmpFunc)(const char *s1, const char *s2);

static FileCmpFunc FileCmp;

#define FSB_ENABLE_DETAIL

const char *newFolder16Data = "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377>>>\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377ZZZ\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\064\064\064\377\177\177\177\377\177\177\177\377\177"
  "\177\177\377www\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\000"
  "\000\000\377\363\267/\377\363\267/\377\000\000\000\377\064\064\064\377\064\064\064\377\343"
  "\343\343\377\064\064\064\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377"
  "ooo\377\000\000\000\377\363\267/\377\363\267/\377\000\000\000\377ooo\377^^^\377;;;\377"
  "\064\064\064\377ooo\377ooo\377ooo\377ooo\377ooo\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\000\000\000\377\061\061\061\377///\377---\377---\377---\377---\377\000\000\000\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\000\000\000\377\061\061\061\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\064\064\064\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064"
  "\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377"
  "\363\267/\377\363\267/\377\000\000\000\377MMM\377MMM\377\064\064\064\377\064\064\064"
  "\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\363"
  "\267/\377\363\267/\377\000\000\000\377MMM\377MMM\377\064\064\064\377\064\064\064\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000"
  "\377\000\000\000\377\000\000\000\377MMM\377MMM\377\064\064\064\377\064\064\064\377MMM\377M"
  "MM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM"
  "\377MMM\377MMM\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\064"
  "\064\064\377\064\064\064\377MMM\377MMM\377MMM\377MMM\377KKK\377KKK\377KKK\377"
  "KKK\377KKK\377JJJ\377JJJ\377GGG\377GGG\377GGG\377\064\064\064\377ZZZ\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377ZZZ\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377";

const char *newFolder24Data = "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377EEE\377\064\064\064\377\064\064"
  "\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\065\065\065\377"
  "uuu\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\064"
  "\064\064\377eee\377\202\202\202\377\202\202\202\377\202\202\202\377\202\202"
  "\202\377xxx\377fff\377AAA\377uuu\377\354\354\354\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\000\000\000\377\363\267/\377\363\267"
  "/\377\363\267/\377\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\374\374\374\377\064\064\064\377[[[\377ttt\377ttt\377ttt\377ttt\377ttt\377"
  "rrr\377eee\377EEE\377\071\071\071\377\071\071\071\377\071\071\071\377\071\071\071\377"
  "\070\070\070\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377"
  "\070\070\070\377\064\064\064\377;;;\377\264\264\264\377\064\064\064\377XXX\377ooo"
  "\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377kkk\377hhh\377hhh\377hhh\377"
  "hhh\377hhh\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377"
  "hhh\377hhh\377SSS\377\070\070\070\377\064\064\064\377XXX\377ooo\377ooo\377ooo\377"
  "ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377HHH\377VVV\377UUU\377UU"
  "U\377UUU\377UUU\377UUU\377UUU\377UUU\377UUU\377\000\000\000\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377///\377///"
  "\377...\377...\377...\377...\377...\377...\377...\377...\377...\377\000\000\000"
  "\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\000\000\000\377///\377@@@\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\363\267/"
  "\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\000\000\000\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377BBB\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377MMM\377MMM\377"
  "BBB\377\064\064\064\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\363\267"
  "/\377\363\267/\377\363\267/\377\000\000\000\377MMM\377MMM\377BBB\377\064\064\064\377"
  "\064\064\064\377BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM"
  "\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\363\267/\377\363\267/\377"
  "\363\267/\377\000\000\000\377MMM\377MMM\377BBB\377\064\064\064\377\064\064\064\377BB"
  "B\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377M"
  "MM\377MMM\377BBB\377\064\064\064\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377BBB\377\064\064\064\377\064\064"
  "\064\377BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377BBB\377\064\064\064\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377BBB\377\064\064\064\377\064\064\064\377"
  "BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "BBB\377\064\064\064\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377BBB\377\064\064\064\377\064\064\064\377BBB\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377KKK\377KKK\377KKK\377KKK\377@@@\377"
  "\064\064\064\377\064\064\064\377BBB\377MMM\377MMM\377MMM\377MMM\377MMM\377KKK\377"
  "III\377III\377III\377III\377III\377III\377III\377GGG\377GGG\377GGG\377FF"
  "F\377FFF\377FFF\377FFF\377>>>\377\064\064\064\377EEE\377\064\064\064\377\064\064"
  "\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377EEE\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377";

const char *newFolder32Data = "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377UUU\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377>>>\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\000\000\000\377\000\000\000\377\000\000\000\377\000"
  "\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377>>>"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\064\064\064\377"
  "\064\064\064\377\204\204\204\377\204\204\204\377\204\204\204\377\204\204\204"
  "\377\204\204\204\377\204\204\204\377yyy\377ooo\377UUU\377\064\064\064\377>>"
  ">\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\000\000\000\377\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\064\064\064\377\064\064\064"
  "\377yyy\377yyy\377yyy\377yyy\377yyy\377yyy\377yyy\377yyy\377ooo\377UUU\377"
  "\064\064\064\377>>>\377>>>\377>>>\377>>>\377>>>\377\000\000\000\377\000\000\000\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377;;;\377"
  ";;;\377\064\064\064\377\064\064\064\377MMM\377\377\377\377\377\064\064\064\377\064"
  "\064\064\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377"
  "ooo\377UUU\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064"
  "\064\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\363\267"
  "/\377\000\000\000\377\000\000\000\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377MMM\377\064\064\064\377\064\064\064\377ooo\377ooo\377ooo\377o"
  "oo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo"
  "\377ooo\377ooo\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267"
  "/\377\363\267/\377\000\000\000\377\000\000\000\377ooo\377ooo\377ooo\377ooo\377\064\064"
  "\064\377\064\064\064\377\064\064\064\377\064\064\064\377ooo\377ooo\377ooo\377ooo\377"
  "ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/"
  "\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000"
  "\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377\064\064\064\377ooo\377o"
  "oo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377ooo\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377+++\377+++\377+++\377+++\377+++\377+++\377+++\377"
  "+++\377+++\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267"
  "/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\000\000\000\377\000\000\000\377...\377...\377...\377...\377.."
  ".\377...\377...\377...\377...\377...\377...\377...\377\000\000\000\377\000\000\000\377"
  "\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267"
  "/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000"
  "\000\000\377...\377...\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377M"
  "MM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267"
  "/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\064\064\064\377\064\064\064\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000"
  "\000\000\377\000\000\000\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267/\377\363\267"
  "/\377\000\000\000\377\000\000\000\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\363\267/\377\363"
  "\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377\064\064\064\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377"
  "\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\064\064\064\377\064"
  "\064\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\363"
  "\267/\377\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377MMM\377"
  "MMM\377MMM\377MMM\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\363\267/\377"
  "\363\267/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377MMM\377MMM\377"
  "MMM\377MMM\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\363\267/\377\363\267"
  "/\377\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377MMM\377MMM\377MMM\377"
  "MMM\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377\000\000\000\377\000\000\000\377\363\267/\377\363\267/\377"
  "\363\267/\377\363\267/\377\000\000\000\377\000\000\000\377MMM\377MMM\377MMM\377MMM\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377\000\000\000\377\000\000\000\377MMM\377MMM\377MMM\377MMM\377\064\064\064\377\064\064"
  "\064\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\000"
  "\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000\000\377\000\000"
  "\000\377MMM\377MMM\377MMM\377MMM\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM"
  "\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377MMM\377M"
  "MM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM"
  "\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377MMM\377MMM\377MMM\377"
  "MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MM"
  "M\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377HHH\377HHH\377"
  "HHH\377HHH\377HHH\377HHH\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064"
  "\064\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377MMM\377HHH\377HH"
  "H\377HHH\377HHH\377HHH\377HHH\377HHH\377HHH\377HHH\377HHH\377FFF\377FFF\377"
  "FFF\377FFF\377FFF\377FFF\377FFF\377FFF\377FFF\377FFF\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377UUU\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064"
  "\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064"
  "\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377\064\064\064\377"
  "\064\064\064\377UUU\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377";

typedef struct FileDialogData FileDialogData;

static void FileListDetailSelect(FileDialogData *fsb, const char *item);
static void FileListSelect(FileDialogData *fsb, const char *item);
static void FileIconViewSelect(FileDialogData *fsb, const char *item);

static void FileSelect(FileDialogData *fsb, const char *item);

static void select_view(FileDialogData *data);

static void filedialog_update_dir(FileDialogData *data, char *path);

static void filedialog_ok(Widget w, FileDialogData *data, XtPointer d);

/*
 * get number of shifts for specific color mask
 */
static int get_shift(unsigned long mask) {
    if(mask == 0) {
        return 0;
    }
    
    int shift = 0;
    while((mask & 1L) == 0) {
        shift++;
        mask >>= 1;
    }
    return shift;
}

/*
 * get number of bits from color mask
 */
static int get_mask_len(unsigned long mask) {
    if(mask == 0) {
        return 0;
    }
    
    while((mask & 1L) == 0) {
        mask >>= 1;
    }
    int len = 0;
    while((mask & 1L) == 1) {
        len++;
        mask >>= 1;
    }
    return len;
}

/*
 * create an image from data and copy image to the specific pixmap
 */
static void create_image(Display *dp, Visual *visual, int depth, Pixmap pix, const char *data, int wh) {
    // wh is width and height
    size_t imglen = wh*wh*4;
    char *imgdata = malloc(imglen); // will be freed with XDestroyImage
    
    //get number of shifts required for each color bit mask
    int red_shift = get_shift(visual->red_mask);
    int green_shift = get_shift(visual->green_mask);
    int blue_shift = get_shift(visual->blue_mask);
    
    uint32_t *src = (uint32_t*)data;
    uint32_t *dst = (uint32_t*)imgdata;
    // init all bits that are not in any mask
    uint32_t pixel_init = UINT32_MAX ^ (visual->red_mask ^ visual->green_mask ^ visual->blue_mask);
    
    // convert pixels to visual-specific format
    size_t len = wh*wh;
    for(int i=0;i<len;i++) {
        uint32_t pixel = src[i];
#if defined(__sparc) || defined(__sparc__)
        // big endian: sparc is probably the only relevant platform
        //uint8_t alpha = pixel & 0xFF;
        uint8_t blue = (pixel & 0xFF00) >> 8;
        uint8_t green = (pixel & 0xFF0000) >> 16;
        uint8_t red = (pixel & 0xFF000000) >> 24;
#else
        // little endian: basically all modern cpus
        uint8_t red = pixel & 0xFF;
        uint8_t green = (pixel & 0xFF00) >> 8;
        uint8_t blue = (pixel & 0xFF0000) >> 16;
#endif
        // TODO: work with alpha value
        if(pixel == UINT32_MAX) {
            red = bgColor.red;
            green = bgColor.green;
            blue = bgColor.blue;
        }
        
        uint32_t out = pixel_init;
        out ^= (red << red_shift);
        out ^= (green << green_shift);
        out ^= (blue << blue_shift);
        dst[i] = out;
    }
    
    // create rgb image (32 bit alignment)
    XImage *img = XCreateImage(dp, visual, depth, ZPixmap, 0, imgdata, wh, wh, 32, 0);
    
    XGCValues gcval;
    gcval.graphics_exposures = 0;
    unsigned long valuemask = GCGraphicsExposures;	
    GC gc = XCreateGC(dp, newFolderIcon16, valuemask, &gcval);
    
    XPutImage(dp, pix, gc, img, 0, 0, 0, 0, wh, wh);
    
    XDestroyImage(img);
    XFreeGC(dp, gc);
}

static void initPixmaps(Display *dp, Drawable d, Screen *screen, int depth)
{    
    // get the correct visual for current screen/depth
    Visual *visual = NULL;
    for(int i=0;i<screen->ndepths;i++) {
        Depth d = screen->depths[i];
        if(d.depth == depth) {
            for(int v=0;v<d.nvisuals;v++) {
                Visual *vs = &d.visuals[v];
                if(get_mask_len(vs->red_mask) == 8) {
                    visual = vs;
                    break;
                }
            }
        }
    }
    
    if(!visual) {
        // no visual found for using rgb images
        fprintf(stderr, "can't use images with this visual\n");
        pixmaps_initialized = 1;
        pixmaps_error = 1;
        return;
    }
    
    // create pixmaps and load images
    newFolderIcon16 = XCreatePixmap(dp, d, 16, 16, depth);
    if(newFolderIcon16 == BadValue) {
        fprintf(stderr, "failed to create newFolderIcon16 pixmap\n");
        pixmaps_error = 1;
    } else {
        create_image(dp, visual, depth, newFolderIcon16, newFolder16Data, 16);
    }
    
    newFolderIcon24 = XCreatePixmap(dp, d, 24, 24, depth);
    if(newFolderIcon24 == BadValue) {
        fprintf(stderr, "failed to create newFolderIcon24 pixmap\n");
        pixmaps_error = 1;
    } else {
        create_image(dp, visual, depth, newFolderIcon24, newFolder24Data, 24);
    }
    
    newFolderIcon32 = XCreatePixmap(dp, d, 32, 32, depth);
    if(newFolderIcon32 == BadValue) {
        fprintf(stderr, "failed to create newFolderIcon32 pixmap\n");
        pixmaps_error = 1;
    } else {
        create_image(dp, visual, depth, newFolderIcon32, newFolder32Data, 32);
    }
    
    pixmaps_initialized = 1;
}

#ifdef __APPLE__
static void NameSetString(Widget textfield, const char *str) {
    char *str_nfc = StringNFD2NFC(str);
    XNETextSetString(textfield, str_nfc);
    free(str_nfc);
}
#else
#define NameSetString(textfield, str) XNETextSetString(textfield, str)
#endif


/* -------------------- path bar -------------------- */

typedef void(*updatedir_callback)(void*,char*);

typedef struct PathBar {  
    Widget widget;
    Widget textfield;
    
    Widget focus_widget;
    
    Widget left;
    Widget right;
    Dimension lw;
    Dimension rw;
    
    int shift;
    
    Widget *pathSegments;
    size_t numSegments;
    size_t segmentAlloc;
    
    char *path;
    int selection;
    Boolean input;
    
    int focus;
    
    updatedir_callback updateDir;
    void *updateDirData;
} PathBar;

void PathBarSetPath(PathBar *bar, char *path);

void pathbar_resize(Widget w, PathBar *p, XtPointer d)
{
    Dimension width, height;
    XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
    
    Dimension *segW = NEditCalloc(p->numSegments, sizeof(Dimension));
    
    Dimension maxHeight = 0;
    
    /* get width/height from all widgets */
    Dimension pathWidth = 0;
    for(int i=0;i<p->numSegments;i++) {
        Dimension segWidth;
        Dimension segHeight;
        XtVaGetValues(p->pathSegments[i], XmNwidth, &segWidth, XmNheight, &segHeight, NULL);
        segW[i] = segWidth;
        pathWidth += segWidth;
        if(segHeight > maxHeight) {
            maxHeight = segHeight;
        }
    }
    Dimension tfHeight;
    XtVaGetValues(p->textfield, XmNheight, &tfHeight, NULL);
    if(tfHeight > maxHeight) {
        maxHeight = tfHeight;
    }
    
    Boolean arrows = False;
    if(pathWidth + 10 > width) {
        arrows = True;
        pathWidth += p->lw + p->rw;
    }
    
    /* calc max visible widgets */
    int start = 0;
    if(arrows) {
        Dimension vis = p->lw+p->rw;
        for(int i=p->numSegments;i>0;i--) {
            Dimension segWidth = segW[i-1];
            if(vis + segWidth + 10 > width) {
                start = i;
                arrows = True;
                break;
            }
            vis += segWidth;
        }
    } else {
        p->shift = 0;
    }
    
    int leftShift = 0;
    if(p->shift < 0) {
        if(start + p->shift < 0) {
            leftShift = start;
            start = 0;
            p->shift = -leftShift;
        } else {
            leftShift = -p->shift; /* negative shift */
            start += p->shift;
        }
    }
    
    int x = 0;
    if(arrows) {
        XtManageChild(p->left);
        XtManageChild(p->right);
        x = p->lw;
    } else {
        XtUnmanageChild(p->left);
        XtUnmanageChild(p->right);
    }
    
    for(int i=0;i<p->numSegments;i++) {
        if(i >= start && i < p->numSegments - leftShift && !p->input) {
            XtVaSetValues(p->pathSegments[i], XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
            x += segW[i];
            XtManageChild(p->pathSegments[i]);
        } else {
            XtUnmanageChild(p->pathSegments[i]);
        }
    }
    
    if(arrows) {
        XtVaSetValues(p->left, XmNx, 0, XmNy, 0, XmNheight, maxHeight, NULL);
        XtVaSetValues(p->right, XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
    }
    
    NEditFree(segW);
    
    Dimension rw, rh;
    XtMakeResizeRequest(w, width, maxHeight, &rw, &rh);
    
    XtVaSetValues(p->textfield, XmNwidth, rw, XmNheight, rh, NULL);
}

static void pathbarActivateTF(PathBar *p)
{
    XtUnmanageChild(p->left);
    XtUnmanageChild(p->right);
    XNETextSetSelection(p->textfield, 0, XNETextGetLastPosition(p->textfield), 0);
    XtManageChild(p->textfield);
    p->input = 1;

    XmProcessTraversal(p->textfield, XmTRAVERSE_CURRENT);

    pathbar_resize(p->widget, p, NULL);
}

void PathBarActivateTextfield(PathBar *p)
{
    p->focus = 1;
    pathbarActivateTF(p);
}

void pathbar_input(Widget w, PathBar *p, XtPointer c)
{
    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct*)c;
    XEvent *xevent = cbs->event;
    
    if (cbs->reason == XmCR_INPUT) {
        if (xevent->xany.type == ButtonPress) {
            p->focus = 0;
            pathbarActivateTF(p);
        }
    }
}

void pathbar_losingfocus(Widget w, PathBar *p, XtPointer c)
{
    if(--p->focus < 0) {
        p->input = False;
        XtUnmanageChild(p->textfield);
    }
}

void pathbar_pathinput(Widget w, PathBar *p, XtPointer d)
{
    char *newpath = XNETextGetString(p->textfield);
    if(newpath) {
        if(newpath[0] == '~') {
            char *p = newpath+1;
            char *cp = ConcatPath(GetHomeDir(), p);
            XtFree(newpath);
            newpath = cp;
        } else if(newpath[0] != '/') {
            char *cp = ConcatPath(GetCurrentDir(), newpath);
            XtFree(newpath);
            newpath = cp;
        }
        
        /* update path */
        PathBarSetPath(p, newpath);
        if(p->updateDir) {
            p->updateDir(p->updateDirData, newpath);
        }
        XtFree(newpath);
        
        /* hide textfield and show path as buttons */
        XtUnmanageChild(p->textfield);
        pathbar_resize(p->widget, p, NULL);
        
        if(p->focus_widget) {
            XmProcessTraversal(p->focus_widget, XmTRAVERSE_CURRENT);
        }
    }
}

void pathbar_shift_left(Widget w, PathBar *p, XtPointer d)
{
    p->shift--;
    pathbar_resize(p->widget, p, NULL);
}

void pathbar_shift_right(Widget w, PathBar *p, XtPointer d)
{
    if(p->shift < 0) {
        p->shift++;
    }
    pathbar_resize(p->widget, p, NULL);
}

static void pathTextEH(Widget widget, XtPointer data, XEvent *event, Boolean *dispatch) {
    PathBar *pb = data;
    if(event->type == KeyReleaseMask) {
        if(event->xkey.keycode == 9) {
            XtUnmanageChild(pb->textfield);
            pathbar_resize(pb->widget, pb, NULL);
            *dispatch = False;
        } else if(event->xkey.keycode == 36) {
            pathbar_pathinput(pb->textfield, pb, NULL);
            *dispatch = False;
        }
    }
}

PathBar* CreatePathBar(Widget parent, ArgList args, int n)
{
    PathBar *bar = NEditMalloc(sizeof(PathBar));
    bar->path = NULL;
    bar->updateDir = NULL;
    bar->updateDirData = NULL;
    
    bar->focus_widget = NULL;
    
    bar->shift = 0;
    
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    bar->widget = XmCreateDrawingArea(parent, "pathbar", args, n);
    XtAddCallback(
            bar->widget,
            XmNresizeCallback,
            (XtCallbackProc)pathbar_resize,
            bar);
    XtAddCallback(
            bar->widget,
            XmNinputCallback,
            (XtCallbackProc)pathbar_input,
            bar);
    
    Arg a[4];
    XtSetArg(a[0], XmNshadowThickness, 0);
    XtSetArg(a[1], XmNx, 0);
    XtSetArg(a[2], XmNy, 0);
    bar->textfield = XNECreateText(bar->widget, "pbtext", a, 3);
    bar->input = 0;
    XtAddCallback(
            bar->textfield,
            XmNlosingFocusCallback,
            (XtCallbackProc)pathbar_losingfocus,
            bar);
    XtAddCallback(bar->textfield, XmNactivateCallback,
                 (XtCallbackProc)pathbar_pathinput, bar);
    XtAddEventHandler(bar->textfield, KeyPressMask | KeyReleaseMask, FALSE, pathTextEH, bar);
    
    XtSetArg(a[0], XmNarrowDirection, XmARROW_LEFT);
    bar->left = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtSetArg(a[0], XmNarrowDirection, XmARROW_RIGHT);
    bar->right = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtAddCallback(
                bar->left,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_left,
                bar);
    XtAddCallback(
                bar->right,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_right,
                bar);
    
    Pixel bg;
    XtVaGetValues(bar->textfield, XmNbackground, &bg, NULL);
    XtVaSetValues(bar->widget, XmNbackground, bg, NULL);
    
    XtManageChild(bar->left);
    XtManageChild(bar->right);
    
    XtVaGetValues(bar->left, XmNwidth, &bar->lw, NULL);
    XtVaGetValues(bar->right, XmNwidth, &bar->rw, NULL);
    
    bar->segmentAlloc = 16;
    bar->numSegments = 0;
    bar->pathSegments = NEditCalloc(16, sizeof(Widget));
    
    bar->selection = 0;
    
    return bar;
}

void PathBarChangeDir(Widget w, PathBar *bar, XtPointer c)
{
    XmToggleButtonSetState(bar->pathSegments[bar->selection], False, False);
    
    for(int i=0;i<bar->numSegments;i++) {  
        if(bar->pathSegments[i] == w) {
            bar->selection = i;
            XmToggleButtonSetState(w, True, False);
            break;
        }
    }
    
    int plen = strlen(bar->path);
    int countSeg = 0;
    for(int i=0;i<=plen;i++) {
        char c = bar->path[i];
        if(c == '/' || c == '\0') {
            if(countSeg == bar->selection) {
                char *dir = NEditMalloc(i+2);
                memcpy(dir, bar->path, i+1);
                dir[i+1] = '\0';
                if(bar->updateDir) {
                    bar->updateDir(bar->updateDirData, dir);
                }
                XNETextSetString(bar->textfield, dir);
                NEditFree(dir);
            }
            countSeg++;
        }
    }
}

void PathBarSetPath(PathBar *bar, char *path)
{
    NEditFree(bar->path);
    bar->path = NEditStrdup(path);
    
    for(int i=0;i<bar->numSegments;i++) {
        XtDestroyWidget(bar->pathSegments[i]);
    }
    XtUnmanageChild(bar->textfield);
    XtManageChild(bar->left);
    XtManageChild(bar->right);
    bar->input = False;
    
    Arg args[4];
    XmString str;
    
    bar->numSegments = 0;
    
    int i=0;
    if(path[0] == '/') {
        str = XmStringCreateLocalized("/");
        XtSetArg(args[0], XmNlabelString, str);
        XtSetArg(args[1], XmNfillOnSelect, True);
        XtSetArg(args[2], XmNindicatorOn, False);
        bar->pathSegments[0] = XmCreateToggleButton(
                bar->widget, "pbbutton", args, 3);
        XtAddCallback(
                bar->pathSegments[0],
                XmNvalueChangedCallback,
                (XtCallbackProc)PathBarChangeDir,
                bar);
        XmStringFree(str);
        bar->numSegments++;
        i++;
    }
    
    int len = strlen(path);
    int begin = i;
    for(;i<=len;i++) {
        char c = path[i];
        if((c == '/' || c == '\0') && i > begin) {
            char *segStr = NEditMalloc(i - begin + 1);
            memcpy(segStr, path+begin, i-begin);
            segStr[i-begin] = '\0';
            begin = i+1;
            
            str = FSNameCreateLocalized(segStr);
            NEditFree(segStr);
            XtSetArg(args[0], XmNlabelString, str);
            XtSetArg(args[1], XmNfillOnSelect, True);
            XtSetArg(args[2], XmNindicatorOn, False);
            Widget button = XmCreateToggleButton(bar->widget, "pbbutton", args, 3);
            XtAddCallback(
                    button,
                    XmNvalueChangedCallback,
                    (XtCallbackProc)PathBarChangeDir,
                    bar);
            XmStringFree(str);
            
            if(bar->numSegments >= bar->segmentAlloc) {
                bar->segmentAlloc += 8;
                bar->pathSegments = realloc(bar->pathSegments, bar->segmentAlloc * sizeof(Widget));
            }
            
            bar->pathSegments[bar->numSegments++] = button;
        }
    }
    
    bar->selection = bar->numSegments-1;
    XmToggleButtonSetState(bar->pathSegments[bar->selection], True, False);
    
    XNETextSetString(bar->textfield, path);
    XNETextSetInsertionPosition(bar->textfield, XNETextGetLastPosition(bar->textfield));
    
    pathbar_resize(bar->widget, bar, NULL);
}

void PathBarDestroy(PathBar *pathbar) {
    NEditFree(pathbar->path);
    NEditFree(pathbar->pathSegments);
    NEditFree(pathbar);
}



/* -------------------- file dialog -------------------- */

typedef struct FileElm FileElm;
struct FileElm {
    char *path;
    uint64_t size;
    time_t lastModified;
    Boolean isDirectory;
    Boolean isHidden;
};

#define SEARCHBUF_SIZE 128

#ifdef DISABLE_FSB_EXTSEARCH
#define SEARCH_TIMEOUT 0
#endif

// SEARCH_TIMEOUT: time in ms until the search buffer is reset
#ifndef SEARCH_TIMEOUT
#define SEARCH_TIMEOUT 1500
#endif

struct FileDialogData {
    Widget shell;
    
    char *file_path;
    
    Widget path;
    PathBar *pathBar;
    Widget filter;
    
    int selectedview;
    
    // dir/file list view
    Widget listform;
    Widget dirlist;
    Widget filelist;
    Widget filelistcontainer;
    
    // detail view
    Widget grid;
    Widget gridcontainer;
    
    Widget okBtn;
    Widget name;
    Widget wrap;
    Widget unixFormat;
    Widget dosFormat;
    Widget macFormat;
    Widget encoding;
    Widget iofilter;
    Widget bom;
    Widget xattr;
    
    Widget keyEHWidget;
    Time lastKeyEvent;
    char searchBuf[SEARCHBUF_SIZE];
    int searchBufPos;
    
    FileElm *dirs;
    FileElm *files;
    int dircount;
    int filecount;
    int maxnamelen;
    
    int gridRealized;
    
    char *currentPath;
    char *selectedPath;
    int selIsDir;
    int showHidden;
    
    IOFilter **filters;
    size_t nfilters;
    IOFilter *selected_filter;
      
    int type;
    
    int end;
    int status;
};

static void filedialog_cancel(Widget w, FileDialogData *data, XtPointer d)
{
    data->end = 1;
    data->status = FILEDIALOG_CANCEL;
}

static void cleanupLists(FileDialogData *data)
{
    XmListDeleteAllItems(data->dirlist);
    XmListDeleteAllItems(data->filelist);
}

static void filedialog_check_iofilters(FileDialogData *data, const char *path)
{
    if(!path || !data->iofilter) {
        return;
    }
    
    for(int i=0;i<data->nfilters;i++) {
        if(data->filters[i]->ec_pattern) {
            if(!ec_glob(data->filters[i]->ec_pattern, path)) {
                XtVaSetValues(data->iofilter, XmNselectedPosition, i+1, NULL);
                return;
            }
        }
    }
    
    XtVaSetValues(data->iofilter, XmNselectedPosition, 0, NULL);
}

/*
 * file_cmp_field
 * 0: compare path
 * 1: compare size
 * 2: compare mtime
 */
static int file_cmp_field = 0;

/*
 * 1 or -1
 */
static int file_cmp_order = 1;

static int filecmp(const void *f1, const void *f2)
{
    const FileElm *file1 = f1;
    const FileElm *file2 = f2;
    if(file1->isDirectory != file2->isDirectory) {
        return file1->isDirectory < file2->isDirectory;
    }
    
    int cmp_field = file_cmp_field;
    int cmp_order = file_cmp_order;
    if(file1->isDirectory) {
        cmp_field = 0;
        cmp_order = 1;
    }
    
    int ret = 0;
    switch(cmp_field) {
        case 0: {
            ret = FileCmp(FileName(file1->path), FileName(file2->path));
            break;
        }
        case 1: {
            if(file1->size < file2->size) {
                ret = -1;
            } else if(file1->size == file2->size) {
                ret = 0;
            } else {
                ret = 1;
            }
            break;
        }
        case 2: {
            if(file1->lastModified < file2->lastModified) {
                ret = -1;
            } else if(file1->lastModified == file2->lastModified) {
                ret = 0;
            } else {
                ret = 1;
            }
            break;
        }
    }
    
    return ret * cmp_order;
}

typedef void(*ViewUpdateFunc)(FileDialogData*,FileElm*,FileElm*,int,int,int);


static void filelistwidget_add(Widget w, int showHidden, char *filter, FileElm *ls, int count)
{   
    if(count > 0) {
        XmStringTable items = NEditCalloc(count, sizeof(XmString));
        int i = 0;
        
        for(int j=0;j<count;j++) {
            FileElm *e = &ls[j];
            
            char *name = FileName(e->path);
            if((!showHidden && name[0] == '.') || fnmatch(filter, name, 0)) {
                e->isHidden = True;
                continue;
            }
            e->isHidden = False;
            
            items[i] = FSNameCreateLocalized(name);
            i++;
        }
        XmListAddItems(w, items, i, 0);
        for(int i=0;i<count;i++) {
            XmStringFree(items[i]);
        }
        NEditFree(items);
    }
}


static void filedialog_update_lists(
        FileDialogData *data,
        FileElm *dirs,
        FileElm *files,
        int dircount,
        int filecount,
        int maxnamelen)
{
    char *filter = XNETextGetString(data->filter);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    filelistwidget_add(data->dirlist, data->showHidden, "*", dirs, dircount);
    filelistwidget_add(data->filelist, data->showHidden, filterStr, files, filecount);
    
    XtFree(filter);
}

/*
 * create file size string with kb/mb/gb/tb suffix
 */
static char* size_str(FileElm *f) {
    char *str = malloc(16);
    uint64_t size = f->size;
    
    if(f->isDirectory) {
        str[0] = '\0';
    } else if(size < 0x400) {
        snprintf(str, 16, "%d bytes", (int)size);
    } else if(size < 0x100000) {
        float s = (float)size/0x400;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x2800 && diff != 0) {
            // size < 10 KiB
            snprintf(str, 16, "%.1f " KB_SUFFIX, s);
        } else {
            snprintf(str, 16, "%.0f " KB_SUFFIX, s);
        }
    } else if(size < 0x40000000) {
        float s = (float)size/0x100000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0xa00000 && diff != 0) {
            // size < 10 MiB
            snprintf(str, 16, "%.1f " MB_SUFFIX, s);
        } else {
            size /= 0x100000;
            snprintf(str, 16, "%.0f " MB_SUFFIX, s);
        }
    } else if(size < 0x1000000000ULL) {
        float s = (float)size/0x40000000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x280000000 && diff != 0) {
            // size < 10 GiB
            snprintf(str, 16, "%.1f " GB_SUFFIX, s);
        } else {
            size /= 0x40000000;
            snprintf(str, 16, "%.0f " GB_SUFFIX, s);
        }
    } else {
        size /= 1024;
        float s = (float)size/0x40000000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x280000000 && diff != 0) {
            // size < 10 TiB
            snprintf(str, 16, "%.1f " TB_SUFFIX, s);
        } else {
            size /= 0x40000000;
            snprintf(str, 16, "%.0f " TB_SUFFIX, s);
        }
    }
    return str;
}

static char* date_str(time_t tm) {
    struct tm t;
    struct tm n;
    time_t now = time(NULL);
    
    localtime_r(&tm, &t);
    localtime_r(&now, &n);
    
    char *str = malloc(16);
    if(t.tm_year == n.tm_year) {
        strftime(str, 16, DATE_FORMAT_SAME_YEAR, &t);
    } else {
        strftime(str, 16, DATE_FORMAT_OTHER_YEAR, &t);
    }
    return str;
}

static void filegridwidget_add(Widget grid, int showHidden, char *filter, FileElm *ls, int count, int maxWidth)
{
    XmLGridAddRows(grid, XmCONTENT, 1, count);
    
    int row = 0;
    for(int i=0;i<count;i++) {
        FileElm *e = &ls[i];
        
        char *name = FileName(e->path);
        if((!showHidden && name[0] == '.') || (!e->isDirectory && fnmatch(filter, name, 0))) {
            e->isHidden = True;
            continue;
        }
        e->isHidden = False;
        
        // name
        XmString str = FSNameCreateLocalized(name);
        XtVaSetValues(grid,
                XmNcolumn, 0, 
                XmNrow, row,
                XmNcellString, str, NULL);
        XmStringFree(str);
        // size
        char *szbuf = size_str(e);
        str = XmStringCreateLocalized(szbuf);
        XtVaSetValues(grid,
                XmNcolumn, 1, 
                XmNrow, row,
                XmNcellString, str, NULL);
        free(szbuf);
        XmStringFree(str);
        // date
        char *datebuf = date_str(e->lastModified);
        str = XmStringCreateLocalized(datebuf);
        XtVaSetValues(grid,
                XmNcolumn, 2, 
                XmNrow, row,
                XmNcellString, str, NULL);
        free(datebuf);
        XmStringFree(str);
        
        XtVaSetValues(grid, XmNrow, row, XmNrowUserData, e, NULL);
        row++;
    }
    
    // remove unused rows
    if(count > row) {
        XmLGridDeleteRows(grid, XmCONTENT, row, count-row);
    }
    
    if(maxWidth < 16) {
        maxWidth = 16;
    }
    
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 0,
        XmNcolumnRangeEnd, 0,
        XmNcolumnWidth, maxWidth,
        XmNcellAlignment, XmALIGNMENT_LEFT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 1,
        XmNcolumnRangeEnd, 1,
        XmNcolumnWidth, 9,
        XmNcellAlignment, XmALIGNMENT_LEFT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 2,
        XmNcolumnRangeEnd, 2,
        XmNcolumnWidth, 16,
        XmNcellAlignment, XmALIGNMENT_RIGHT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    
    XmLGridColumn column0 = XmLGridGetColumn(grid, XmCONTENT, 1);
    XmLGridColumn column1 = XmLGridGetColumn(grid, XmCONTENT, 1);
    XmLGridColumn column2 = XmLGridGetColumn(grid, XmCONTENT, 2);
    
    Dimension col0Width = XmLGridColumnWidthInPixels(column0);
    Dimension col1Width = XmLGridColumnWidthInPixels(column1);
    Dimension col2Width = XmLGridColumnWidthInPixels(column2);
    
    Dimension totalWidth = col0Width + col1Width + col2Width;
    
    Dimension gridWidth = 0;
    Dimension gridShadow = 0;
    XtVaGetValues(grid, XmNwidth, &gridWidth, XmNshadowThickness, &gridShadow, NULL);
    
    Dimension widthDiff = gridWidth - totalWidth - gridShadow - gridShadow;
    
    if(gridWidth > totalWidth) {
            XtVaSetValues(grid,
            XmNcolumnRangeStart, 0,
            XmNcolumnRangeEnd, 0,
            XmNcolumnWidth, col0Width + widthDiff - XmLGridVSBWidth(grid) - 2,
            XmNcolumnSizePolicy, XmCONSTANT,
            NULL);
    }
}

static void filedialog_update_grid(
        FileDialogData *data,
        FileElm *dirs,
        FileElm *files,
        int dircount,
        int filecount,
        int maxnamelen)
{
    char *filter = XNETextGetString(data->filter);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    // update dir list
    filelistwidget_add(data->dirlist, data->showHidden, "*", dirs, dircount);
    // update file detail grid
    filegridwidget_add(data->grid, data->showHidden, filterStr, files, filecount, maxnamelen);
    
    XtFree(filter);
}

static void cleanupGrid(FileDialogData *data)
{
    // cleanup dir list widget
    XmListDeleteAllItems(data->dirlist);
    
    // cleanup grid
    Cardinal rows = 0;
    XtVaGetValues(data->grid, XmNrows, &rows, NULL);
    XmLGridDeleteRows(data->grid, XmCONTENT, 0, rows);
}


static void free_files(FileElm *ls, int count)
{
    for(int i=0;i<count;i++) {
        free(ls[i].path);
    }
    free(ls);
}

static void filedialog_cleanup_filedata(FileDialogData *data)
{
    if(data->dirs) {
        free_files(data->dirs, data->dircount);
    }
    if(data->files) {
        free_files(data->files, data->filecount);
    }
    data->dirs = NULL;
    data->files = NULL;
    data->dircount = 0;
    data->filecount = 0;
    data->maxnamelen = 0;
}


// ported from motifextfsb
static void FileListDetailSelect(FileDialogData *fsb, const char *item) {
    int numRows = 0;
    XtVaGetValues(fsb->grid, XmNrows, &numRows, NULL);
    
    XmLGridColumn col = XmLGridGetColumn(fsb->grid, XmCONTENT, 0);
    for(int i=0;i<numRows;i++) {
        XmLGridRow row = XmLGridGetRow(fsb->grid, XmCONTENT, i);
        FileElm *elm = NULL;
        XtVaGetValues(fsb->grid, XmNrowPtr, row, XmNcolumnPtr, col, XmNrowUserData, &elm, NULL);
        if(elm) {
            if(!strcmp(item, FileName(elm->path))) {
                XmLGridSelectRow(fsb->grid, i, False);
                XmLGridFocusAndShowRow(fsb->grid, i+1);
                break;
            }
        }
    }
}

static void FileListSelect(FileDialogData *fsb, const char *item) {
    int numItems = 0;
    XmStringTable items = NULL;
    XtVaGetValues(fsb->filelist, XmNitemCount, &numItems, XmNitems, &items, NULL);
    
    for(int i=0;i<numItems;i++) {
        char *str = NULL;
        XmStringGetLtoR(items[i], XmFONTLIST_DEFAULT_TAG, &str);
        if(!strcmp(str, item)) {
            XmListSelectPos(fsb->filelist, i+1, False);
            break;
        }
        XtFree(str);
    }
}

static void FileIconViewSelect(FileDialogData *fsb, const char *item) {
    // TODO
}

static void FileSelect(FileDialogData *fsb, const char *item) {
    FileListSelect(fsb, item);
    FileListDetailSelect(fsb, item),
    FileIconViewSelect(fsb, item);
}



#define FILE_ARRAY_SIZE 1024

void file_array_add(FileElm **files, int *alloc, int *count, FileElm elm) {
    int c = *count;
    int a = *alloc;
    if(c >= a) {
        a *= 2;
        FileElm *newarray = realloc(*files, sizeof(FileElm) * a);
        
        *files = newarray;
        *alloc = a;
    }
    
    (*files)[c] = elm;
    c++;
    *count = c;
}

static void filedialog_update_dir(FileDialogData *data, char *path)
{  
    ViewUpdateFunc update_view = NULL;
    switch(data->selectedview) {
        case 1: {
            cleanupLists(data);
            update_view = filedialog_update_lists;
            break;
        }
        case 2: {
            cleanupGrid(data);
            update_view = filedialog_update_grid;
            break;
        }
    }
    
    char *openFile = NULL;
    
    /* read dir and insert items */
    if(path) {
        struct stat s;
        int r = stat(path, &s);
        if((!r == !S_ISDIR(s.st_mode)) || (r && errno == ENOENT && data->type == FILEDIALOG_SAVE)) {
            // open file
            NEditFree(data->selectedPath);
            data->selectedPath = NEditStrdup(path);

            openFile = FileName(path);
            path = ParentPath(path);

            PathBarSetPath(data->pathBar, path);
        }
        
        FileElm *dirs = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        FileElm *files = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        int dirs_alloc = FILE_ARRAY_SIZE;
        int files_alloc = FILE_ARRAY_SIZE;
        
        filedialog_cleanup_filedata(data);
        
        int dircount = 0; 
        int filecount = 0;
        size_t maxNameLen = 0;
        DIR *dir = opendir(path);
        if(!dir) {
            DialogF(
                    DF_ERR,
                    data->shell,
                    1,
                    "Error",
                    "Directory %s cannot be opened: %s",
                    "OK",
                    path,
                    strerror(errno));
            return;
        }
    
        /* dir reading complete - set the path textfield */  
        char *oldPath = data->currentPath;
        data->currentPath = NEditStrdup(path);
        NEditFree(oldPath);
        if(openFile) {
            NEditFree(path);
        }
        path = data->currentPath;

        struct dirent *ent;
        while((ent = readdir(dir)) != NULL) {
            if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }

            char *entpath = ConcatPath(path, ent->d_name);

            struct stat s;
            if(stat(entpath, &s)) {
                NEditFree(entpath);
                continue;
            }

            FileElm new_entry;
            new_entry.path = entpath;
            new_entry.isDirectory = S_ISDIR(s.st_mode);
            new_entry.size = (uint64_t)s.st_size;
            new_entry.lastModified = s.st_mtime;

            size_t nameLen = strlen(ent->d_name);
            if(nameLen > maxNameLen) {
                maxNameLen = nameLen;
            }

            if(new_entry.isDirectory) {
                file_array_add(&dirs, &dirs_alloc, &dircount, new_entry);
            } else {
                file_array_add(&files, &files_alloc, &filecount, new_entry);
            }
        }
        closedir(dir);
        
        data->dirs = dirs;
        data->files = files;
        data->dircount = dircount;
        data->filecount = filecount;
        data->maxnamelen = maxNameLen;
        
        // sort file arrays
        qsort(dirs, dircount, sizeof(FileElm), filecmp);
        qsort(files, filecount, sizeof(FileElm), filecmp);
    }
    
    update_view(data, data->dirs, data->files,
            data->dircount, data->filecount, data->maxnamelen);
    
    if(openFile) {
        FileSelect(data, openFile);
        data->status = FILEDIALOG_OK;
        if(data->name) {
            NameSetString(data->name, openFile);
            XmProcessTraversal(data->name, XmTRAVERSE_CURRENT);
        } else {
            select_view(data);
        }
    }
}

static void filedialog_goup(Widget w, FileDialogData *data, XtPointer d)
{
    char *newPath = ParentPath(data->currentPath);
    filedialog_update_dir(data, newPath);
    PathBarSetPath(data->pathBar, newPath);
    NEditFree(newPath);
}

char* set_selected_path(FileDialogData *data, const char *path) {
    if(!path) {
        return NULL;
    }
    
    NEditFree(data->selectedPath);
    data->selectedPath = NEditStrdup(path);
    return data->selectedPath;
}

void set_path_from_row(FileDialogData *data, int row) {
    FileElm *elm = NULL;
    XmLGridRow rowPtr = XmLGridGetRow(data->grid, XmCONTENT, row);
    XtVaGetValues(data->grid, XmNrowPtr, rowPtr, XmNrowUserData, &elm, NULL);
    if(!elm) {
        fprintf(stderr, "error: no row data\n");
        return;
    }
    
    char *path = NEditStrdup(elm->path);
    filedialog_check_iofilters(data, path);
    
    if(data->type == FILEDIALOG_SAVE) {
        NameSetString(data->name, FileName(path));
        NEditFree(path);
    } else {
        NEditFree(data->selectedPath);
        data->selectedPath = path;
        data->selIsDir = False;
    }
}

static const char* get_path_from_list(FileElm *ls, int numelm, int index) {
    if(index >= numelm || index < 0) {
        return NULL;
    }
    int i = 0;
    for(int e=0;e<numelm;e++) {
        if(ls[e].isHidden) {
            continue;
        }
        if(i == index) {
            return ls[e].path;
        }
        i++;
    }
    return NULL;
}

void grid_select(Widget w, FileDialogData *data, XmLGridCallbackStruct *cb) {
    set_path_from_row(data, cb->row);
}

void grid_activate(Widget w, FileDialogData *data, XmLGridCallbackStruct *cb) {
    set_path_from_row(data, cb->row);
    filedialog_ok(w, data, NULL);
}
 
static int list_key_pressed(Widget widget, FileDialogData *data, XKeyEvent *event) {
    char chars[16];
    KeySym keysym;
    int nchars;
    
    nchars = XLookupString(event, chars, 15, &keysym, NULL);
    
    if(nchars == 0 || chars[0] < 0x020) {
        return 1;
    }
    
    Time t = event->time;
    Time diffMS = t - data->lastKeyEvent;
    data->lastKeyEvent = t;
    if(diffMS > SEARCH_TIMEOUT || (data->keyEHWidget && data->keyEHWidget != widget)) {
        data->searchBufPos = 0;
    }
    data->keyEHWidget = widget;
    
    if(keysym == XK_BackSpace || keysym == XK_Delete) {
        if(data->searchBufPos > 0) {
            data->searchBufPos--;
            data->searchBuf[data->searchBufPos] = '\0';
        }
    } else if(data->searchBufPos + nchars + 1 < SEARCHBUF_SIZE) {
        memcpy(data->searchBuf + data->searchBufPos, chars, nchars);
        data->searchBufPos += nchars;
        data->searchBuf[data->searchBufPos] = '\0';
    }
    
    return 0;
}

static void listKeyPressEH(Widget w, XtPointer callData, XEvent *event, Boolean *continueDispatch) {
    FileDialogData *data = callData;
    if(list_key_pressed(w, data, &event->xkey)) {
        return;
    }
    
    ListFindAndSelect(w, data->searchBuf, data->searchBufPos);    
    
    *continueDispatch = False;
}

void grid_key_pressed(Widget w, FileDialogData *data, XmLGridCallbackStruct *cb) {
    if(list_key_pressed(w, data, &cb->event->xkey)) {
        return;
    }

    // if data->showHidden is 0, data->files contains more items than the grid
    // this means SelectedRow might not be the correct index for data->files
    // we have to count files manually and increase 'row', if the file
    // is actually displayed in the grid
    int row = 0;
    int selectedRow = XmLGridGetSelectedRow(w);
    
    int match = -1;
    
    for(int i=0;i<data->filecount;i++) {
        const char *name = FileName(data->files[i].path);
        if(!data->showHidden && name[0] == '.') continue;
        
        size_t namelen = strlen(name);
        
        size_t cmplen = namelen < data->searchBufPos ? namelen : data->searchBufPos;
        if(!memcmp(name, data->searchBuf, cmplen)) {
            if(row <= selectedRow) {
                if(match == -1) {
                    match = row;
                }
            } else {
                match = row;
                break;
            }
        }
        
        row++;
    }
    
    if(match > -1) {
        XmLGridSelectRow(w, match, True);
        XmLGridFocusAndShowRow(w, match+1);
    } else {
        XBell(XtDisplay(w), 0);
    }
}

void grid_header_clicked(Widget w, FileDialogData *data, XmLGridCallbackStruct *cb) { 
    int new_cmp_field = 0;
    switch(cb->column) {
        case 0: {
            new_cmp_field = 0;            
            break;
        }
        case 1: {
            new_cmp_field = 1;
            break;
        }
        case 2: {
            new_cmp_field = 2;
            break;
        }
    }
    
    if(new_cmp_field == file_cmp_field) {
        file_cmp_order = -file_cmp_order; // revert sort order
    } else {
        file_cmp_field = new_cmp_field; // change file cmp order to new field
        file_cmp_order = 1;
    }
    
    int sort_type = file_cmp_order == 1 ? XmSORT_ASCENDING : XmSORT_DESCENDING;
    XmLGridSetSort(data->grid, file_cmp_field, sort_type);
    
    qsort(data->files, data->filecount, sizeof(FileElm), filecmp);
    
    // refresh widget
    filedialog_update_dir(data, NULL);
} 

void dirlist_activate(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    const char *p = get_path_from_list(data->dirs, data->dircount, cb->item_position-1);
    char *path = set_selected_path(data, p);
    if(path) {
        filedialog_update_dir(data, path);
        PathBarSetPath(data->pathBar, path);
        data->selIsDir = TRUE;
    }    
}

void dirlist_select(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    const char *p = get_path_from_list(data->dirs, data->dircount, cb->item_position-1);
    if(set_selected_path(data, p)) {
        data->selIsDir = TRUE;
    }
}

void filelist_activate(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    const char *p = get_path_from_list(data->files, data->filecount, cb->item_position-1);
    char *path = set_selected_path(data, p);
    if(path) {
        data->selIsDir = False;
        filedialog_ok(w, data, NULL);
    }
}

void filelist_select(Widget w, FileDialogData *data, XmListCallbackStruct *cb)
{
    if(data->type == FILEDIALOG_SAVE) {
        char *name = NULL;
        XmStringGetLtoR(cb->item, XmFONTLIST_DEFAULT_TAG, &name);
        NameSetString(data->name, name);
        char *path = name ? ConcatPath(data->currentPath, name) : NULL;
        XtFree(name);
        filedialog_check_iofilters(data, path);
        XtFree(path);
    } else {
        const char *p = get_path_from_list(data->files, data->filecount, cb->item_position-1);
        char *path = set_selected_path(data, p);
        if(path) {
            filedialog_check_iofilters(data, path);
            data->selIsDir = False;
        }
    }
}
 
static void filedialog_setshowhidden(
        Widget w,
        FileDialogData *data,
        XmToggleButtonCallbackStruct *tb)
{
    data->showHidden = tb->set;
    filedialog_update_dir(data, NULL);
}

static void filedilalog_ok_end(FileDialogData *data)
{
    struct stat s;
    
    if(data->type == FILEDIALOG_OPEN) {
        // check if the file can be opened
        int fd = open(data->selectedPath, O_RDONLY);
        if(fd == -1) {
            FileOpenErrorDialog(data->shell, data->selectedPath);
            return;
        } else {
            close(fd);
        }
        
    } else if(data->type == FILEDIALOG_SAVE && !stat(data->selectedPath, &s) &&
             (!data->file_path || strcmp(data->file_path, data->selectedPath)))
    {
        if(OverrideFileDialog(data->shell, FileName(data->selectedPath)) != 1) {
            return;
        }
    }
    
    data->status = FILEDIALOG_OK;
    data->end = True;
}

static void filedialog_ok(Widget w, FileDialogData *data, XtPointer d)
{
    XmPushButtonCallbackStruct *cb = d;
    if(cb && w != data->name) {
        if(cb->event->type == KeyPress && cb->event->xkey.keycode == 36) {
            return;
        }
    }
     
    if(data->type == FILEDIALOG_SAVE) {
        char *newName = XNETextGetString(data->name);
        if(newName) {
            if(strlen(newName) > 0) {
                data->selectedPath = newName[0] == '/' ? NEditStrdup(newName) : ConcatPath(data->currentPath, newName);
                data->selIsDir = 0;
                filedilalog_ok_end(data);
            }
            XtFree(newName);
        }
    } else if(data->selectedPath) {
        if(!data->selIsDir) {
            filedilalog_ok_end(data);
        }
    }
}

#define DETECT_ENCODING "detect"
static char *default_encodings[] = {
    DETECT_ENCODING,
    "UTF-8",
    "UTF-16",
    "UTF-16BE",
    "UTF-16LE",
    "UTF-32",
    "UTF-32BE",
    "UTF-32LE",
    "ISO8859-1",
    "ISO8859-2",
    "ISO8859-3",
    "ISO8859-4",
    "ISO8859-5",
    "ISO8859-6",
    "ISO8859-7",
    "ISO8859-8",
    "ISO8859-9",
    "ISO8859-10",
    "ISO8859-13",
    "ISO8859-14",
    "ISO8859-15",
    "ISO8859-16",
    NULL
};

static void adjust_enc_settings(FileDialogData *data) {
    /* this should never happen, but make sure it will not do anything */
    if(data->type != FILEDIALOG_SAVE) return;
    
    int encPos;
    XtVaGetValues(data->encoding, XmNselectedPosition, &encPos, NULL);
    
    /*
     * the save file dialog doesn't has the "detect" item
     * the default_encodings index is encPos + 1
     */
    if(encPos > 6) {
        /* no unicode no bom */
        XtSetSensitive(data->bom, False);
        if(GetAutoEnableXattr()) {
            XtVaSetValues(data->xattr, XmNset, 1, NULL);
        }
    } else {
        // UTF-16 or UTF-32 without LE/BE setting will always write a BOM
        // disable data->bom in that case
        // pos1: UTF-16   pos4: UTF-32
        XtSetSensitive(data->bom, !(encPos == 1 || encPos == 4));
        if(encPos > 0) {
            /* enable bom for all non-UTF-8 unicode encodings */
            XtVaSetValues(data->bom, XmNset, 1, NULL);
        }
    }
}

static void filedialog_select_encoding(
        Widget w,
        FileDialogData *data,
        XmComboBoxCallbackStruct *cb)
{
    if(cb->reason == XmCR_SELECT) {
        adjust_enc_settings(data);
    }
}

static int str_has_suffix(const char *str, const char *suffix) {
    size_t str_len = str ? strlen(str) : 0;
    size_t suffix_len = suffix ? strlen(suffix) : 0;
    if(str_len >= suffix_len) {
        const char *str_end = str + str_len - suffix_len;
        int result = memcmp(str_end, suffix, suffix_len) == 0;
        return result;
    }
    return 0;
}

static void filedialog_select_iofilter(
        Widget w,
        FileDialogData *data,
        XmComboBoxCallbackStruct *cb)
{
    if(cb->reason == XmCR_SELECT) {
        if(data->type == FILEDIALOG_SAVE) {
            // remove previous extension
            if(data->selected_filter) {
                // compare current file name extension with selected_filter extension
                char *name = XNETextGetString(data->name);
                size_t name_len = strlen(name);
                if(str_has_suffix(name, data->selected_filter->ext)) {
                    name[name_len - strlen(data->selected_filter->ext)] = 0; // remove ext
                    XNETextSetString(data->name, name);
                }
                XtFree(name);
            }
        }
        
        // get the current filter from the combobox
        int selectedFilterIndex;
        XtVaGetValues(data->iofilter, XmNselectedPosition, &selectedFilterIndex, NULL);
        // index 0 is always '-' no filter
        if(selectedFilterIndex == 0) {
            data->selected_filter = NULL;
        } else {
            // combobox indices are always +1 compared to data->filters indices
            data->selected_filter = data->filters[selectedFilterIndex-1];

            // set extension if the name doesn't already contain the extension
            if(data->name && data->selected_filter->ext) {
                char *name = XNETextGetString(data->name);
                if(!str_has_suffix(name, data->selected_filter->ext)) {
                    size_t name_len = strlen(name);
                    size_t ext_len = strlen(data->selected_filter->ext);
                    size_t newname_len = name_len + ext_len;
                    char *newname = NEditMalloc(newname_len + 1);
                    memcpy(newname, name, name_len);
                    memcpy(newname+name_len, data->selected_filter->ext, ext_len);
                    newname[newname_len] = '\0';
                    XNETextSetString(data->name, newname);
                    NEditFree(newname);
                }
                XtFree(name);
            }
        }
    }
}

static void filedialog_filter(Widget w, FileDialogData *data, XtPointer c)
{
    filedialog_update_dir(data, NULL);
}

static void unselect_view(FileDialogData *data)
{
    switch(data->selectedview) {
        case 1: {
            XtUnmanageChild(data->listform);
            XtUnmanageChild(data->filelistcontainer);
            cleanupLists(data);
            break;
        }
        case 2: {
            XtUnmanageChild(data->listform);
            XtUnmanageChild(data->gridcontainer);
            cleanupGrid(data);
            
            // reset sort options and resort files
            file_cmp_field = 0;
            file_cmp_order = 1;
            qsort(data->files, data->filecount, sizeof(FileElm), filecmp);
            
            break;
        }
    }
}

static void select_listview(Widget w, FileDialogData *data, XtPointer u)
{
    unselect_view(data);
    data->selectedview = 1;
    XtManageChild(data->listform);
    XtManageChild(data->filelistcontainer);
    filedialog_update_dir(data, NULL);
    data->pathBar->focus_widget = data->filelist;
    XmProcessTraversal(data->filelist, XmTRAVERSE_CURRENT);
}

static void select_detailview(Widget w, FileDialogData *data, XtPointer u)
{
    //XmLGridSetSort(data->grid, 0, XmSORT_ASCENDING);
    
    unselect_view(data);
    data->selectedview = 2;
    XtManageChild(data->listform);
    XtManageChild(data->gridcontainer);
    filedialog_update_dir(data, NULL);
    data->pathBar->focus_widget = data->grid;
    XmProcessTraversal(data->grid, XmTRAVERSE_CURRENT);
}

static void select_view(FileDialogData *data)
{
    Widget w;
    switch(data->selectedview) {
        default: return;
        case 0: w = data->grid; break;
        case 1: w = data->filelist; break;
    }
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
}

static void new_folder(Widget w, FileDialogData *data, XtPointer u)
{
    char fileName[DF_MAX_PROMPT_LENGTH];
    fileName[0] = 0;
    
    int response = DialogF(
            DF_PROMPT,
            data->shell,
            2,
            "Create new directory", "Directory name:",
            fileName,
            "OK",
            "Cancel");
    
    if(response == 2 || strlen(fileName) == 0) {
        return;
    }
    
    char *newFolder = ConcatPath(data->currentPath ? data->currentPath : "", fileName);
    if(mkdir(newFolder, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
        DialogF(
                DF_ERR,
                data->shell,
                1,
                "Error creating Directory",
                "Can't create %s:\n%s", "OK",
                newFolder,
                strerror(errno));
    } else {
        char *p = strdup(data->currentPath);
        filedialog_update_dir(data, p);
        free(p);
    }
    free(newFolder);
}

static unsigned int keycodeL;

static void shortcutEH(Widget widget, XtPointer data, XEvent *event, Boolean *dispatch)
{
    FileDialogData *fsb = data;
    if(event->xkey.keycode == keycodeL) {
        PathBarActivateTextfield(fsb->pathBar);
        *dispatch = False;
    }
}

static void createFilterWidgets(FileDialogData *data, const char *current_filter, Widget parent, Arg *args, int n) {
    size_t nfilters;
    IOFilter **filter = GetFilterList(&nfilters);
    data->filters = filter;
    data->nfilters = nfilters;
       
    XmStringTable filterStrTable = NEditCalloc(nfilters+1, sizeof(XmString));
    filterStrTable[0] = XmStringCreateSimple("-");
    for(int i=0;i<nfilters;i++) {
        filterStrTable[i+1] = XmStringCreateLocalized(filter[i]->name);
    }

    XtSetArg(args[n], XmNcolumns, 11); n++;
    XtSetArg(args[n], XmNitemCount, nfilters+1); n++;
    XtSetArg(args[n], XmNitems, filterStrTable); n++;
    data->iofilter = XmCreateDropDownList(parent, "filtercombobox", args, n);
    XtManageChild(data->iofilter);
    for(int i=0;i<nfilters+1;i++) {
        XmStringFree(filterStrTable[i]);
    }
    NEditFree(filterStrTable);
    
    XtAddCallback(
            data->iofilter,
            XmNselectionCallback,
            (XtCallbackProc)filedialog_select_iofilter,
            data);
    
    if(current_filter) {
        XmString xCurrentFilter = XmStringCreateLocalized((char*)current_filter);
        XmComboBoxSelectItem(data->iofilter, xCurrentFilter);
        XmStringFree(xCurrentFilter);
        
        XmComboBoxCallbackStruct cb;
        cb.reason = XmCR_SELECT;
        filedialog_select_iofilter(data->iofilter, data, &cb);
    }
}

int FileDialog(Widget parent, char *promptString, FileSelection *file, int type, const char *defaultName)
{
    Arg args[32];
    int n = 0;
    XmString str;
    
    int currentEncItem = 0;
    
    if(LastView == -1) {
        LastView = GetPrefFsbView();
        if(LastView < 0 || LastView > 2) {
            LastView = 1;
        }
    }
#ifndef FSB_ENABLE_DETAIL
    if(LastView == 2) {
        LastView = 1;
    }
#endif
    Boolean showHiddenValue = ShowHidden >= 0 ? ShowHidden : GetPrefFsbShowHidden();
    
    switch(GetPrefFsbFileCmp()) {
        default: {
            FileCmp = (FileCmpFunc)strcmp;
            break;
        }
        case 1: {
            FileCmp = (FileCmpFunc)strcasecmp;
            break;
        }
    }
    
    FileDialogData data;
    memset(&data, 0, sizeof(FileDialogData));
    data.type = type;
    data.file_path = file->path;
    
    file->addwrap = FALSE;
    file->setxattr = FALSE;
    
    Widget dialog = CreateDialogShell(parent, promptString, args, 0);
    AddMotifCloseCallback(dialog, (XtCallbackProc)filedialog_cancel, &data);
    data.shell = dialog;
    
    /* shortcut handler */
    XtAddEventHandler(dialog, KeyPressMask , False,
    	    (XtEventHandler)shortcutEH, &data); 
    
    n = 0;
    XtSetArg(args[n],  XmNautoUnmanage, False); n++;
    Widget form = XmCreateForm(dialog, "form", args, n);
    
    /* upper part of the gui */
     
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNresizable, True); n++;
    XtSetArg(args[n], XmNarrowDirection, XmARROW_UP); n++;
    Widget goUp = XmCreateArrowButton(form, "button", args, n);
    //XtManageChild(goUp);
    XtAddCallback(goUp, XmNactivateCallback,
                 (XtCallbackProc)filedialog_goup, &data);
    
    // View Option Menu
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    Widget viewframe = XmCreateForm(form, "vframe", args, n);
    XtManageChild(viewframe);
    
    XmString v1 = XmStringCreateLocalized("List");
    XmString v2 = XmStringCreateLocalized("Detail");
    
    Widget menu = XmCreatePulldownMenu(viewframe, "menu", NULL, 0);
    
    XtSetArg(args[0], XmNlabelString, v1);
    XtSetArg(args[1], XmNpositionIndex, LastView == 1 ? 0 : 1);
    Widget mitem1 = XmCreatePushButton(menu, "menuitem", args, 2);
    XtSetArg(args[0], XmNlabelString, v2);
    XtSetArg(args[1], XmNpositionIndex, LastView == 2 ? 0 : 2);
    Widget mitem2 = XmCreatePushButton(menu, "menuitem", args, 2);
    XtManageChild(mitem1);
#ifdef FSB_ENABLE_DETAIL
    XtManageChild(mitem2);
#endif
    XmStringFree(v1);
    XmStringFree(v2);;
    XtAddCallback(
            mitem1,
            XmNactivateCallback,
            (XtCallbackProc)select_listview,
            &data);
    XtAddCallback(
            mitem2,
            XmNactivateCallback,
            (XtCallbackProc)select_detailview,
            &data);
     
    n = 0;
    XtSetArg(args[n], XmNsubMenuId, menu); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    Widget view = XmCreateOptionMenu(viewframe, "option_menu", args, n);
    XtManageChild(view);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightWidget, view); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    Widget newFolder = XmCreatePushButton(viewframe, "newFolder", args, n);
    XtManageChild(newFolder);
    XtAddCallback(
            newFolder,
            XmNactivateCallback,
            (XtCallbackProc)new_folder,
            &data);
    
    Dimension xh;
    Pixel buttonFg;
    Pixel buttonBg;
    XtVaGetValues(newFolder, XmNheight, &xh, XmNforeground, &buttonFg, XmNbackground, &buttonBg, NULL);
    
    // get rgb value of buttonBg
    memset(&bgColor, 0, sizeof(XColor));
    bgColor.pixel = buttonBg;
    XQueryColor(XtDisplay(newFolder), newFolder->core.colormap, &bgColor);
    
    // init pixmaps after we got the background color
    if(!pixmaps_initialized) {
        initPixmaps(XtDisplay(parent), XtWindow(parent), newFolder->core.screen, newFolder->core.depth);
    }
    
    if(pixmaps_error) {
        XtVaSetValues(newFolder, XmNlabelType, XmSTRING, NULL);
    } else if(xh > 32+BUTTON_EXTRA_SPACE) {
        XtVaSetValues(newFolder, XmNlabelPixmap, newFolderIcon32, NULL);
    } else if(xh > 24+BUTTON_EXTRA_SPACE) {
        XtVaSetValues(newFolder, XmNlabelPixmap, newFolderIcon24, NULL);
    } else {
        XtVaSetValues(newFolder, XmNlabelPixmap, newFolderIcon16, NULL);
    }
    
    // pathbar
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, goUp); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, viewframe); n++;
    XtSetArg(args[n], XmNrightOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
    Widget pathBarFrame = XmCreateFrame(form, "pathbar_frame", args, n);
    XtManageChild(pathBarFrame);
    data.pathBar = CreatePathBar(pathBarFrame, args, 0);
    data.pathBar->updateDir = (updatedir_callback)filedialog_update_dir;
    data.pathBar->updateDirData = &data;
    XtManageChild(data.pathBar->widget);
    
    n = 0;
    str = XmStringCreateLocalized("Filter");
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, pathBarFrame); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget filterLabel = XmCreateLabel(form, "label", args, n);
    XtManageChild(filterLabel);
    XmStringFree(str);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, filterLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    Widget filterform = XmCreateForm(form, "filterform", args, n);
    XtManageChild(filterform);
    
    n = 0;
    str = XmStringCreateSimple("Show hidden files");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNset, showHiddenValue); n++;
    Widget showHidden = XmCreateToggleButton(filterform, "showHidden", args, n);
    XtManageChild(showHidden);
    XmStringFree(str);
    XtAddCallback(showHidden, XmNvalueChangedCallback,
                 (XtCallbackProc)filedialog_setshowhidden, &data);
    data.showHidden = showHiddenValue;
    
    n = 0;
    str = XmStringCreateLocalized("Filter");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, showHidden); n++;
    Widget filterButton = XmCreatePushButton(filterform, "filedialog_filter", args, n);
    XtManageChild(filterButton);
    XmStringFree(str);
    XtAddCallback(filterButton, XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, &data);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, filterButton); n++;
    XtSetArg(args[n], XmNrightOffset, WIDGET_SPACING); n++;
    data.filter = XNECreateText(filterform, "filedialog_filter_textfield", args, n);
    XtManageChild(data.filter);
    XtAddCallback(data.filter, XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, &data);
    if(LastFilter) {
        XNETextSetString(data.filter, LastFilter);
        XtFree(LastFilter);
        LastFilter = NULL;
    } else {
        XNETextSetString(data.filter, "*");
    }
    
    /* lower part */
    n = 0;
    str = XmStringCreateLocalized(type == FILEDIALOG_OPEN ? "Open" : "Save");
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING * 2); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    data.okBtn = XmCreatePushButton(form, "filedialog_open", args, n);
    XtManageChild(data.okBtn);
    XmStringFree(str);
    XtAddCallback(data.okBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_ok, &data);
    
    n = 0;
    str = XmStringCreateLocalized("Cancel");
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    Widget cancelBtn = XmCreatePushButton(form, "filedialog_cancel", args, n);
    XtManageChild(cancelBtn);
    XmStringFree(str);
    XtAddCallback(cancelBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_cancel, &data);
    
    XtVaSetValues(form, XmNdefaultButton, data.okBtn, XmNcancelButton, cancelBtn, NULL);
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, data.okBtn); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 1); n++;
    Widget separator = XmCreateSeparator(form, "ofd_separator", args, n);
    XtManageChild(separator);
    
    Widget bottomWidget = separator;
    
    if(type == FILEDIALOG_SAVE) { 
        if(file->extraoptions) {
            n = 0;
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, separator); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
            createFilterWidgets(&data, file->filter, form, args, n);

            n = 0;
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, separator); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
            XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNrightWidget, data.iofilter); n++;
            XtSetArg(args[n], XmNrightOffset, WIDGET_SPACING); n++;
            //XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
            //XtSetArg(args[n], XmNtopWidget, data.iofilter); n++;
            data.name = XNECreateText(form, "textfield", args, n);
            XtManageChild(data.name);
            XtAddCallback(data.name, XmNactivateCallback,
                     (XtCallbackProc)filedialog_ok, &data);
            if(defaultName) {
                XNETextSetString(data.name, (char*)defaultName);
            }

            n = 0;
            str = XmStringCreateSimple("Filter");
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, data.name); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNleftWidget, data.name); n++;
            XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNlabelString, str); n++;
            Widget filterLabel = XmCreateLabel(form, "label", args, n);
            XtManageChild(filterLabel);
            XmStringFree(str);

            n = 0;
            str = XmStringCreateSimple("New File Name");
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, data.name); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
            XtSetArg(args[n], XmNlabelString, str); n++;
            Widget nameLabel = XmCreateLabel(form, "label", args, n);
            XtManageChild(nameLabel);
            XmStringFree(str);

            n = 0;
            str = XmStringCreateSimple("Add line breaks where wrapped");
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, nameLabel); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
            XtSetArg(args[n], XmNmnemonic, 'A'); n++;
            XtSetArg(args[n], XmNlabelString, str); n++;
            data.wrap = XmCreateToggleButton(form, "addWrap", args, n);
            XtManageChild(data.wrap);
            XmStringFree(str);

            Widget formatBtns = CreateFormatButtons(
                    form,
                    data.wrap,
                    file->format,
                    &data.unixFormat,
                    &data.dosFormat,
                    &data.macFormat);

            bottomWidget = formatBtns;
        } else {
            n = 0;
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, separator); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
            XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
            //XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
            //XtSetArg(args[n], XmNtopWidget, data.iofilter); n++;
            data.name = XNECreateText(form, "textfield", args, n);
            XtManageChild(data.name);
            XtAddCallback(data.name, XmNactivateCallback,
                     (XtCallbackProc)filedialog_ok, &data);
            if(defaultName) {
                XNETextSetString(data.name, (char*)defaultName);
            }
            
            n = 0;
            str = XmStringCreateSimple("New File Name");
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNbottomWidget, data.name); n++;
            XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
            XtSetArg(args[n], XmNlabelString, str); n++;
            Widget nameLabel = XmCreateLabel(form, "label", args, n);
            XtManageChild(nameLabel);
            XmStringFree(str);
            
            bottomWidget = nameLabel;
        } 
    }
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, bottomWidget); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    Widget enc = XmCreateRowColumn(form, "enc", args, n);
    XtManageChild(enc);
    
    if(file->extraoptions) {
        n = 0;
        str = XmStringCreateSimple("Encoding:");
        XtSetArg(args[n], XmNlabelString, str); n++;
        Widget encLabel = XmCreateLabel(enc, "label", args, n);
        XtManageChild(encLabel);
        XmStringFree(str);

        n = 0;
        int arraylen = 22;
        
        // TODO: code dup of encoding list generation (window.c)
        
        const char *encStr;
        XmStringTable encodings = NEditCalloc(arraylen, sizeof(XmString));
        /* skip the "detect" item on type == save */
        int skip = type == FILEDIALOG_OPEN ? 0 : 1;
        char *defEncoding = type == FILEDIALOG_OPEN ? NULL : file->encoding;
        int hasDef = 0;
        int i;
        for(i=skip;(encStr=default_encodings[i]);i++) {
            if(i >= arraylen) {
                arraylen *= 2;
                encodings = NEditRealloc(encodings, arraylen * sizeof(XmString));
            }
            encodings[i] = XmStringCreateSimple((char*)encStr);
            if(defEncoding) {
                if(!strcasecmp(defEncoding, encStr)) {
                    hasDef = 1;
                    defEncoding = NULL;
                }
            }
        }
        if(skip == 1 && !hasDef && file->encoding) {
            /* Current encoding is not in the list of
             * default encodings
             * Add an extra item at pos 0 for the current encoding
             */
            encodings[0] = XmStringCreateSimple(file->encoding);
            currentEncItem = 1;
            skip = 0;
        }
        XtSetArg(args[n], XmNcolumns, 11); n++;
        XtSetArg(args[n], XmNitemCount, i-skip); n++;
        XtSetArg(args[n], XmNitems, encodings+skip); n++;
        data.encoding = XmCreateDropDownList(enc, "combobox", args, n);
        XtManageChild(data.encoding);
        for(int j=0;j<i;j++) {
            XmStringFree(encodings[j]);
        }
        NEditFree(encodings);
        
        if(file->encoding) {
            char *encStr = NEditStrdup(file->encoding);
            size_t encLen = strlen(encStr);
            for(int i=0;i<encLen;i++) {
                encStr[i] = toupper(encStr[i]);
            }
            str = XmStringCreateSimple(encStr);
            XmComboBoxSelectItem(data.encoding, str);
            XmStringFree(str);
            NEditFree(encStr);
        }
        
        /* bom and xattr option */
        if(type == FILEDIALOG_SAVE) {
            /* only the save file dialog needs an encoding select callback */
            XtAddCallback(
                    data.encoding,
                    XmNselectionCallback,
                    (XtCallbackProc)filedialog_select_encoding,
                    &data);
            
            n = 0;
            str = XmStringCreateSimple("Write BOM");
            XtSetArg(args[n], XmNlabelString, str); n++;
            XtSetArg(args[n], XmNset, file->writebom); n++;
            data.bom = XmCreateToggleButton(enc, "togglebutton", args, n);
            XtManageChild(data.bom);
            XmStringFree(str);
            
            n = 0;
            str = XmStringCreateSimple("Store encoding in extended attribute");
            XtSetArg(args[n], XmNlabelString, str); n++;
            data.xattr = XmCreateToggleButton(enc, "togglebutton", args, n);
            XtManageChild(data.xattr);
            XmStringFree(str);
        } else {
            // FILEDIALOG_OPEN
            n = 0;
            XmString str = XmStringCreateSimple("Filter");
            XtSetArg(args[n], XmNlabelString, str); n++;
            Widget filterLabel = XmCreateLabel(enc, "filter_label", args, n);
            XtManageChild(filterLabel);
            XmStringFree(str);
            
            createFilterWidgets(&data, file->filter, enc, args, 0);
        }
        
    }
    
    /* middle */
    data.selectedview = LastView;
    
    // form for dir/file lists
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, filterform); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, enc); n++;
    XtSetArg(args[n], XmNleftOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNrightOffset, WINDOW_SPACING); n++;
    XtSetArg(args[n], XmNbottomOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNwidth, 580); n++;
    XtSetArg(args[n], XmNheight, 400); n++;
    data.listform = XmCreateForm(form, "fds_listform", args, n); 
    
    // dir/file lists
    n = 0;
    str = XmStringCreateLocalized("Directories");
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget lsDirLabel = XmCreateLabel(data.listform, "label", args, n);
    XtManageChild(lsDirLabel);
    XmStringFree(str);
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 35); n++;
    data.dirlist = XmCreateScrolledList(data.listform, "dirlist", args, n);
    Dimension w, h;
    XtMakeResizeRequest(data.dirlist, 150, 200, &w, &h);
    XtManageChild(data.dirlist);
    XtAddCallback(
            data.dirlist,
            XmNdefaultActionCallback,
            (XtCallbackProc)dirlist_activate,
            &data); 
    XtAddCallback(
            data.dirlist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)dirlist_select,
            &data);
    XtAddEventHandler(data.dirlist, KeyPressMask, False, listKeyPressEH, &data);
    
    n = 0;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, data.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    data.filelistcontainer = XmCreateFrame(data.listform, "filelistframe", args, n);
    //XtManageChild(data.filelistcontainer);
    
    data.filelist = XmCreateScrolledList(data.filelistcontainer, "filelist", NULL, 0);
    XtManageChild(data.filelist);
    XtAddCallback(
            data.filelist,
            XmNdefaultActionCallback,
            (XtCallbackProc)filelist_activate,
            &data); 
    XtAddCallback(
            data.filelist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)filelist_select,
            &data);
    XtAddEventHandler(data.filelist, KeyPressMask, False, listKeyPressEH, &data);
     
    // Detail FileList
    // the detail view shares widgets with the list view
    // switching between list and detail view only changes
    // filelistcontainer <-> gridcontainer
    n = 0;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, data.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    data.gridcontainer = XmCreateFrame(data.listform, "gridcontainer", args, n);
    //XtManageChild(data.gridcontainer);
    
    n = 0;
    XtSetArg(args[n], XmNcolumns, 3); n++;
    XtSetArg(args[n], XmNheadingColumns, 0); n++;
    XtSetArg(args[n], XmNheadingRows, 1); n++;
    XtSetArg(args[n], XmNallowColumnResize, 1); n++;
    XtSetArg(args[n], XmNsimpleHeadings, "Name|Size|Last Modified"); n++;
    XtSetArg(args[n], XmNhorizontalSizePolicy, XmCONSTANT); n++;
    
    data.grid = XmLCreateGrid(data.gridcontainer, "grid", args, n);
    XmLGridSetIgnoreModifyVerify(data.grid, True);
    int sort_type = file_cmp_order == 1 ? XmSORT_ASCENDING : XmSORT_DESCENDING;
    XmLGridSetSort(data.grid, file_cmp_field, sort_type);  
    XtManageChild(data.grid);
    
    // get the XmList background color
    const char *cellBg = "white";
    XrmValue value;
    char *resourceType = NULL;
    
    // Get the resource value from the resource database
    if(XrmGetResource(XtDatabase(XtDisplay(data.grid)), "XmList.background", NULL, &resourceType, &value)) {
        if(!strcmp(resourceType, "String")) {
            cellBg = value.addr;
        }
    }
    
    XtVaSetValues(
            data.grid,
            XmNcellDefaults, True,
            XtVaTypedArg, XmNblankBackground, XmRString, cellBg, strlen(cellBg),
            XtVaTypedArg, XmNcellBackground, XmRString, cellBg, strlen(cellBg),
            NULL);
    
    //XmLGridSetStrings(data.grid, "Name|Size|Last Modified");
    XtAddCallback(data.grid, XmNselectCallback, (XtCallbackProc)grid_select, &data);
    XtAddCallback(data.grid, XmNactivateCallback, (XtCallbackProc)grid_activate, &data);
    XtAddCallback(data.grid, XmNheaderClickCallback, (XtCallbackProc)grid_header_clicked, &data);
    XtAddCallback(data.grid, XmNgridKeyPressedCallback, (XtCallbackProc)grid_key_pressed, &data);
    
       
    n = 0;
    str = XmStringCreateLocalized("Files");
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, data.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, WIDGET_SPACING); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, lsDirLabel); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    Widget lsFileLabel = XmCreateLabel(data.listform, "label", args, n);
    XtManageChild(lsFileLabel);
    XmStringFree(str);
    
    Widget focus = NULL;
    switch(data.selectedview) {
        case 1: XtManageChild(data.listform); XtManageChild(data.filelistcontainer); focus = data.filelist; break;
        case 2: XtManageChild(data.listform); XtManageChild(data.gridcontainer); focus = data.grid; break;
    }
    if(data.type == FILEDIALOG_SAVE) {
        focus = data.name;
    }
    data.pathBar->focus_widget = focus;
     
    if(file->path) {
        char *defDir = ParentPath(file->path);
        filedialog_update_dir(&data, defDir);
        PathBarSetPath(data.pathBar, defDir);
        NEditFree(defDir);
        
        XNETextSetString(data.name, FileName(file->path));
    } else {
        char *defDirStr = GetDefaultDirectoryStr();
        char *defDir = defDirStr ? defDirStr : getenv("HOME");
        
        filedialog_update_dir(&data, defDir);
        PathBarSetPath(data.pathBar, defDir);
    }
    
    //init_container_size(&data);
     
    /* event loop */
    keycodeL = XKeysymToKeycode(XtDisplay(dialog), XStringToKeysym("L"));
    XtGrabKey(
            dialog,
            keycodeL,
            ControlMask,
            True,
            GrabModeAsync,
            GrabModeAsync);
    ManageDialogCenteredOnPointer(form);
    
    XmProcessTraversal(focus, XmTRAVERSE_CURRENT);
    
    XtAppContext app = XtWidgetToApplicationContext(dialog);
    while(!data.end && !XtAppGetExitFlag(app)) {
        XEvent event;
        XtAppNextEvent(app, &event);
        XtDispatchEvent(&event);
    }
    
    // remember filter string
    LastFilter = XNETextGetString(data.filter);
    if(LastFilter) {
        if(strlen(LastFilter) == 0) {
            XtFree(LastFilter);
            LastFilter = NULL;
        }
    }
    
    LastView = data.selectedview;
    ShowHidden = data.showHidden;
    
    if(data.selectedPath && !data.selIsDir && data.status == FILEDIALOG_OK) {
        file->path = data.selectedPath;
        data.selectedPath = NULL;
        
        file->filter = data.selected_filter ? NEditStrdup(data.selected_filter->name) : NULL;
        
        if(file->extraoptions) {
            int encPos;
            XtVaGetValues(data.encoding, XmNselectedPosition, &encPos, NULL);
            if(type == FILEDIALOG_OPEN) {
                if(encPos > 0) {
                    /* index 0 is the "detect" item which is not a valid 
                       encoding string that can be used later */
                    file->encoding = default_encodings[encPos];
                }
            } else {
                if(currentEncItem) {
                    /* first item is the current encoding that is not 
                     * in default_encodings */
                    if(encPos > 0) {
                        file->encoding = default_encodings[encPos];
                    }
                } else {
                    /* first item is "UTF-8" */
                    file->encoding = default_encodings[encPos+1];
                }
            }
        }
        
        if(type == FILEDIALOG_SAVE) {
            int bomVal = 0;
            int xattrVal = 0;
            int wrapVal = 0;
            if(data.bom) {
                XtVaGetValues(data.bom, XmNset, &bomVal, NULL);
            }
            if(data.xattr) {
                XtVaGetValues(data.xattr, XmNset, &xattrVal, NULL);
            }
            if(data.wrap) {
                XtVaGetValues(data.wrap, XmNset, &wrapVal, NULL);
            }
            
            file->writebom = bomVal;
            file->setxattr = xattrVal;
            file->addwrap = wrapVal;
            
            
            if(data.unixFormat) {
                int formatVal = 0;
                XtVaGetValues(data.unixFormat, XmNset, &formatVal, NULL);
                if(formatVal) {
                    file->format = UNIX_FILE_FORMAT;
                } else {
                    XtVaGetValues(data.dosFormat, XmNset, &formatVal, NULL);
                    if(formatVal) {
                        file->format = DOS_FILE_FORMAT;
                    } else {
                        XtVaGetValues(data.macFormat, XmNset, &formatVal, NULL);
                        if(formatVal) {
                            file->format = MAC_FILE_FORMAT;
                        }
                    }
                }
            } else {
                file->format = 0;
            }
        }
    } else {
        data.status = FILEDIALOG_CANCEL;
        NEditFree(data.selectedPath);
    }
   
    filedialog_cleanup_filedata(&data);
    PathBarDestroy(data.pathBar);
    NEditFree(data.currentPath);
    XtUnmapWidget(dialog);
    XtDestroyWidget(dialog);
    return data.status;
}

const char ** FileDialogDefaultEncodings(void) {
    return (const char **)default_encodings;
}

char* FileDialogGetFilter(void) {
    return NEditStrdup(LastFilter ? LastFilter : "*");
}

void FileDialogSetFilter(const char *filterStr) {
    NEditFree(LastFilter);
    LastFilter = filterStr ? NEditStrdup(filterStr) : NULL;
}

void FileDialogResetSettings(void) {
    LastView = -1;
    ShowHidden = -1;
}
