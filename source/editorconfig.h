/*
 * Copyright 2021 Olaf Wintermann
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


#ifndef EDITORCONFIG_H
#define EDITORCONFIG_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
  
typedef struct EditorConfig EditorConfig;
    
typedef struct ECSection  ECSection;
typedef struct ECKeyValue ECKeyValue;
    
enum ECIndentStyle { EC_INDENT_STYLE_UNSET = 0, EC_TAB, EC_SPACE };
enum ECEndOfLine { EC_EOL_UNSET = 0, EC_LF, EC_CR, EC_CRLF };
enum ECBOM { EC_BOM_UNSET = 0, EC_BOM };

struct EditorConfig {
    int found; /* 1: config found for the requested path; 0: no config found */
    enum ECIndentStyle indent_style;
    int indent_size;
    int tab_width;
    enum ECEndOfLine end_of_line;
    char *charset;
    enum ECBOM bom;
    /* trim_trailing_whitespace currently unsupported */
    /* insert_final_newline currently unsupported */
};

struct ECSection {
    char *name;
    
    ECKeyValue *values;
    
    ECSection *next;
};

struct ECKeyValue {
    char *name;
    char *value;
    ECKeyValue *next;
};

typedef struct {
    char *parent;
    
    char *content;
    size_t length;
    
    ECSection *preamble;
    ECSection *sections;
} ECFile;

void EditorConfigInit(void);

EditorConfig EditorConfigGet(const char *path);

ECFile* ECLoadContent(const char *path);
int ECParse(ECFile *ec);

int ECGetConfig(ECFile *ecf, EditorConfig *config);


#ifdef __cplusplus
}
#endif

#endif /* EDITORCONFIG_H */

