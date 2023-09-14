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


#include "editorconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "../util/filedialog.h" /* ParentPath, ConcatPath */
#include "../util/ec_glob.h"

#define EC_BUFSIZE 4096

#define ISCOMMENT(c) (c == ';' || c == '#' ? 1 : 0)


EditorConfig EditorConfigGet(const char *path, const char *name) {
    EditorConfig ec;
    memset(&ec, 0, sizeof(EditorConfig));
    
    size_t pathlen = strlen(path);
    if(pathlen == 0 || path[0] != '/') {
        // absolute path required
        return ec;
    }
    
    char *filepath = ConcatPath(path, name);
    
    EditorConfig editorconfig;
    memset(&editorconfig, 0, sizeof(EditorConfig));
    
    char *parent = strdup(path);
    int root = 0;
    while(parent) {
        char *ecfilename = ConcatPath(parent, ".editorconfig");
        ECFile *ecfile = ECLoadContent(ecfilename);
        if(ecfile) {
            root = ECGetConfig(ecfile, filepath, &editorconfig);
            ECDestroy(ecfile);
            editorconfig.found = 1;
        } else if(errno != ENOENT) {
            fprintf(stderr, "Cannot open editorconfig file '%s': %s", ecfilename, strerror(errno));
        }
        
        free(ecfilename);
        if(root || strlen(parent) == 1) {
            free(parent);
            parent = NULL;
        } else {
            char *newparent = ParentPath(parent);
            free(parent);
            parent = newparent;
        }
    }
    
    free(filepath);
    
    return editorconfig;
}

ECFile* ECLoadContent(const char *path) {
    int fd = open(path, O_RDONLY);
    if(fd < 0) {
        return NULL;
    }
    
    struct stat s;
    if(fstat(fd, &s)) {
        close(fd);
        return NULL;
    }
    
    size_t alloc = s.st_size;
    char *content = malloc(alloc+1);
    if(!content) {
        close(fd);
        return NULL;
    }
    content[alloc] = 0;
    
    size_t len = 0;
    
    char buf[EC_BUFSIZE];
    ssize_t r;
    while((r = read(fd, buf, EC_BUFSIZE)) > 0) {
        if(alloc-len == 0) {
            alloc += 1024;
            char *newcontent = realloc(content, alloc);
            if(!newcontent) {
                close(fd);
                free(content);
                return NULL;
            }
            content = newcontent;
        }
        memcpy(content+len, buf, r);
        len += r;
    }
    
    close(fd);
    
    ECFile *ecf = malloc(sizeof(ECFile));
    if(!ecf) {
        free(content);
        return NULL;
    }
    memset(ecf, 0, sizeof(ECFile));
    
    ecf->parent = ParentPath((char*)path);
    ecf->content = content;
    ecf->length = len;
    
    return ecf;
}

static char* ec_strdup(char *str, int len) {
    char *newstr = malloc(len+1);
    newstr[len] = 0;
    memcpy(newstr, str, len);
    return newstr;
}

static char* ec_strnchr(char *str, int len, char c) {
    for(int i=0;i<len;i++) {
        if(str[i] == c) {
            return str+i;
        }
    }
    return NULL;
}

static ECSection* create_section(char *name, int len) {
    ECSection *sec = malloc(sizeof(ECSection));
    memset(sec, 0, sizeof(ECSection));
    
    if(name && len > 0) {
        char *s = ec_strnchr(name, len, '/');
        if(!s) {
            // add **/
            int newlen = len+3;
            sec->name = malloc(newlen+1);
            memcpy(sec->name, "**/", 3);
            memcpy(sec->name+3, name, len);
            sec->name[newlen] = 0;
        } else if(name[0] == '/') {
            sec->name = ec_strdup(name, len);
        } else {
            // add **/
            int newlen = len+1;
            sec->name = malloc(newlen+1);
            sec->name[0] = '/';
            memcpy(sec->name+1, name, len);
            sec->name[newlen] = 0;
        }
        
    }
    
    return sec;
} 


static ECSection* parse_section_name(ECFile *ec, ECSection *last, char *line, int len) {
    int name_begin = 0;
    int name_end = 0;
    int comment = 0;
    for(int i=0;i<len;i++) {
        if(!comment) {
            char c = line[i];
            if(c == '[') {
                if(name_begin > 0) {
                    return NULL; // name_begin already set => error
                }
                name_begin = i+1;
            } else if(c == ']') {
                name_end = i;
            } else if(ISCOMMENT(c)) {
                comment = 1;
            }
        }
    }
    
    if(name_begin == 0 || name_end <= name_begin) {
        return NULL;
    }
    
    int name_len = name_end - name_begin;
    char *name = line+name_begin;
    
    ECSection *new_sec = create_section(name, name_len);
    if(ec->sections) {
        last->next = new_sec;
    } else {
        ec->sections = new_sec;
    }
    
    return new_sec;
}

static void string_trim(char **str, int *len) {
    char *s = *str;
    int l = *len;
    while(l > 0 && isspace(*s)) {
        s++;
        l--;
    }
    
    if(l > 0) {
        int k = l-1;
        while(isspace(s[k])) {
            k--;
            l--;
        }
    }
    
    *str = s;
    *len = l;
}

static int parse_key_value(ECFile *ec, ECSection *sec, char *line, int len) {
    int end = len;
    int comment = 0;
    int separator = 0;
    for(int i=0;i<len;i++) {
        if(!comment) {
            char c = line[i];
            if(ISCOMMENT(c)) {
                end = len;
                comment = 1;
            } else if(c == '=') {
                if(separator > 0) {
                    return 1; // separator already exists => error
                }
                separator = i;
            }
        }
    }
    
    if(separator == 0) {
        return 0;
    }
    
    char *name = line;
    char *value = line + separator + 1;
    int name_len = separator;
    int value_len = end - separator - 1;
    
    string_trim(&name, &name_len);
    string_trim(&value, &value_len);
    
    ECKeyValue *kv = malloc(sizeof(ECKeyValue));
    kv->name = ec_strdup(name, name_len);
    kv->value = ec_strdup(value, value_len);
    
    kv->next = sec->values;
    sec->values = kv;
    
    return 0;
}

int ECParse(ECFile *ec) {
    ECSection *current_section = create_section(NULL, 0);
    ec->preamble = current_section;
    
    int line_begin = 0;
    
    // line types:
    // 0: blank
    // 1: comment
    // 2: section name
    // 3: key/value
    int line_type = 0;
    int comment = 0;
    
    for(int i=0;i<ec->length;i++) {
        char c = ec->content[i];
        if(c == '\n') {
            int line_len = i - line_begin;
            char *line = ec->content + line_begin;
            //printf("ln %d {%.*s}\n", line_type, line_len, line);
            switch(line_type) {
                case 0: {
                    // blank
                    break;
                }
                case 1: {
                    // comment
                    break;
                }
                case 2: {
                    current_section = parse_section_name(ec, current_section, line, line_len);
                    if(!current_section) {
                        return 1;
                    }
                    break;
                }
                case 3: {
                    if(parse_key_value(ec, current_section, line, line_len)) {
                        return 1;
                    }
                    break;
                }
            }
            
            
            line_begin = i+1;
            line_type = 0;
            comment = 0;
        } else if(!comment) {
            if(!isspace(c)) {
                if(line_type == 0) {
                    // first non-whitespace char in this line
                    if(c == '#') {
                        line_type = 1;
                        comment = 1;
                    } else if(c == '[') {
                        line_type = 2;
                    } else {
                        line_type = 3;
                    }
                }
            }
        }
    }
    
    return 0;
}

static int ec_getbool(char *v) {
    if(v && (v[0] == 't' || v[0] == 'T')) {
        return 1;
    }
    return 0;
}

static int ec_getint(char *str, int *value) {
    char *end;
    errno = 0;
    long val = strtol(str, &end, 0);
     if(errno == 0) {
        *value = val;
        return 1;
    } else {
        return 0;
    }
}

static int ec_isroot(ECFile *ecf) {
    if(ecf->preamble) {
        ECKeyValue *v = ecf->preamble->values;
        while(v) {
            if(!strcmp(v->name, "root")) {
                return ec_getbool(v->value);
            }
            v = v->next;
        }
    }
    return 0;
}

static int sec_loadvalues(ECSection *sec, EditorConfig *config) {
    ECKeyValue *v = sec->values;
    while(v) {
        int unset_value = 0;
        if(!strcmp(v->value, "unset")) {
            unset_value = 1;
        }
        
        if(!strcmp(v->name, "indent_style")) {
            if(unset_value) {
                config->indent_style = EC_INDENT_STYLE_UNSET;
            } else if(!strcmp(v->value, "space")) {
                config->indent_style = EC_SPACE;
            } else if(!strcmp(v->value, "tab")) {
                config->indent_style = EC_TAB;
            }
        } else if(!strcmp(v->name, "indent_size")) {
            int val = 0;
            if(ec_getint(v->value, &val)) {
                config->indent_size = val;
            }
        } else if(!strcmp(v->name, "tab_width")) {
            int val = 0;
            if(ec_getint(v->value, &val)) {
                config->tab_width = val;
            }
        } else if(EC_EOL_UNSET && !strcmp(v->name, "end_of_line")) {
            if(unset_value) {
                config->end_of_line = EC_EOL_UNSET;
            } else if(!strcmp(v->value, "lf")) {
                config->end_of_line = EC_LF;
            } else if(!strcmp(v->value, "cr")) {
                config->end_of_line = EC_CR;
            } else if(!strcmp(v->value, "crlf")) {
                config->end_of_line = EC_CRLF;
            }
        } else if(!strcmp(v->name, "charset")) {
            if(config->charset) {
                free(config->charset);
                config->charset = NULL;
            }
            
            if(!strcmp(v->value, "utf-8-bom")) {
                config->charset = strdup("utf-8");
                config->bom = EC_BOM;
            } else if(!unset_value) {
                config->charset = strdup(v->value);
                size_t vlen = strlen(v->value);
                if(vlen >= 6 && (!memcmp(v->value, "utf-16", 6) || !memcmp(v->value, "utf-32", 6))) {
                    config->bom = EC_BOM;
                }
            }
        }
        
        v = v->next;
    }
    
    // check if every field is set, if yes we could stop loading editorconfig files
    if(     config->indent_style != EC_INDENT_STYLE_UNSET &&
            config->indent_size != 0 &&
            config->tab_width != 0 &&
            config->end_of_line != EC_EOL_UNSET &&
            config->charset)
    {
        return 1;
    }
    
    return 0;
}

int ECGetConfig(ECFile *ecf, const char *filepath, EditorConfig *config) {
    size_t parentlen = strlen(ecf->parent);
    const char *relpath = filepath + parentlen - 1;
    
    EditorConfig local_ec;
    memset(&local_ec, 0, sizeof(EditorConfig));
    
    if(ECParse(ecf)) {
        char *f = ConcatPath(ecf->parent, ".editorconfig");
        fprintf(stderr, "Cannot parse editorconfig file %s\n", f);
        free(f);
        return 0;
    }
    
    int root = ec_isroot(ecf);
    
    ECSection *sec = ecf->sections;
    while(sec) {
        if(sec->name && !ec_glob(sec->name, relpath)) {
            if(sec_loadvalues(sec, &local_ec)) {
                // every possible setting already set
                // we can skip the parent config
                root = 1;
            }
        }
        sec = sec->next;
    }
    
    // merge settings
    if(config->indent_style == EC_INDENT_STYLE_UNSET) {
        config->indent_style = local_ec.indent_style;
    }
    if(config->indent_size == 0) {
        config->indent_size = local_ec.indent_size;
    }
    if(config->tab_width == 0) {
        config->tab_width = local_ec.tab_width;
    }
    if(config->end_of_line == EC_EOL_UNSET) {
        config->end_of_line = local_ec.end_of_line;
    }
    if(!config->charset) {
        config->charset = local_ec.charset;
    }
    if(config->bom == EC_BOM_UNSET) {
        config->bom = local_ec.bom;
    }
    
    return root;
}

static void destroy_section(ECSection *sec) {
    if(!sec) return;
    if(sec->name) free(sec->name);
    
    ECKeyValue *v = sec->values;
    while(v) {
        if(v->name) free(v->name);
        if(v->value) free(v->value);
        v = v->next;
    }
}

void ECDestroy(ECFile *ecf) {
    destroy_section(ecf->preamble);
    ECSection *sec = ecf->sections;
    while(sec) {
        ECSection *next = sec->next;
        destroy_section(sec);
        sec = next;
    }
    free(ecf->parent);
    free(ecf->content);
    free(ecf);
}

