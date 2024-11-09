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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include "libxattr.h"

#include <errno.h>
#include <sys/types.h>

#include <string.h>

#define LIST_BUF_LEN 1024
#define LIST_ARRAY_LEN 8
#define ATTR_BUF_LEN 1024


static int strarray_add(char ***array_ptr, size_t *pos, size_t *len, char *str) {
    char **array = *array_ptr;
    size_t array_pos = *pos;
    size_t array_len = *len;
    if(array_pos >= array_len) {
        size_t newlen = array_len * 2;
        char **new_array = realloc(array, newlen * sizeof(char*));
        if(!new_array) {
            return 1;
        }
        *len = array_len;
        array = new_array;
        *array_ptr = new_array;
    }
    
    array[array_pos] = str;
    *pos = array_pos + 1;
    
    return 0;
}

static void strarray_free(char **array, size_t nelm) {
    for(size_t i=0;i<nelm;i++) {
        free(array[i]);
    }
    free(array);
}

#ifdef __linux__
#define XATTR_SUPPORTED
#include <sys/xattr.h>

static char ** parse_xattrlist(char *buf, ssize_t length, ssize_t *nelm) {
    size_t arraylen = LIST_ARRAY_LEN;
    size_t arraypos = 0;
    char **array = malloc(LIST_ARRAY_LEN * sizeof(char*));
    if(!array) {
        *nelm = -1;
        return NULL;
    }
    
    char *name = NULL;
    for(int i=0;i<length;i++) {
        if(!name && buf[i] == '.') {
            name = buf + i + 1;
        }
        if(buf[i] == '\0') {
            char *attrname = strdup(name);
            if(!attrname) {
                strarray_free(array, arraypos);
                *nelm = -1;
                return NULL;
            }
            if(strarray_add(&array, &arraypos, &arraylen, attrname)) {
                strarray_free(array, arraypos);
                *nelm = -1;
                return NULL;
            }
            name = NULL;
        }
    }
    
    if(arraypos == 0) {
        free(array);
        array = NULL;
    }
    
    *nelm = arraypos;
    return array;
}

char ** xattr_list(const char *path, ssize_t *nelm) {
    char *list = malloc(LIST_BUF_LEN);
    if(!list) {
        *nelm = -1;
        return NULL;
    }
    ssize_t len = listxattr(path, list, LIST_BUF_LEN);
    if(len == -1) {
        switch(errno) {
            case ERANGE: {
                // buffer too, get size of attribute list
                ssize_t newlen = listxattr(path, NULL, 0);
                if(newlen > 0) {
                    // second try
                    char *new_list = realloc(list, newlen);
                    if(!new_list) {
                        free(list);
                        *nelm = -1;
                        return NULL;
                    }
                    list = new_list;
                    len = listxattr(path, list, newlen);
                    if(len != -1) {
                        // this time it worked
                        break;
                    }
                }
            }
            default: {
                free(list);
                *nelm = -1;
                return NULL;
            }
        }
    }
    
    char **ret = parse_xattrlist(list, len, nelm);
    free(list);
    return ret;
}

static char* name2nsname(const char *name) {
    // add the 'user' namespace to the name
    size_t namelen = strlen(name);
    char *attrname = malloc(8 + namelen);
    if(!attrname) {
        return NULL;
    }
    memcpy(attrname, "user.", 5);
    memcpy(attrname+5, name, namelen + 1);
    return attrname;
}

char * xattr_get(const char *path, const char *attr, ssize_t *len) {
    char *attrname = name2nsname(attr);
    if(!attrname) {
        *len = -1;
        return NULL;
    }
    
    char *buf = malloc(ATTR_BUF_LEN);
    if(!buf) {
        *len = -1;
        free(attrname);
        return NULL;
    }
    ssize_t vlen = getxattr(path, attrname, buf, ATTR_BUF_LEN - 1);
    if(vlen < 0) {
        switch(errno) {
            case ERANGE: {
                ssize_t attrlen = getxattr(path, attrname, NULL, 0);
                if(attrlen > 0) {
                    free(buf);
                    buf = malloc(attrlen + 1);
                    if(!buf) {
                        *len = -1;
                        free(attrname);
                        return NULL;
                    }
                    vlen = getxattr(path, attrname, buf, attrlen);
                    if(vlen > 0) {
                        break;
                    }
                }
            }
            default: {
                *len = -1;
                free(buf);
                free(attrname);
                return NULL;
            }
        }
    }
    buf[vlen] = 0;
    
    free(attrname);
    *len = vlen;
    return buf;
}

int xattr_set(const char *path, const char *name, const void *value, size_t len) {
    char *attrname = name2nsname(name);
    if(!attrname) {
        return 1;
    }
    int ret = setxattr(path, attrname, value, len, 0);
    free(attrname);
    return ret;
}

int xattr_remove(const char *path, const char *name) {
    char *attrname = name2nsname(name);
    if(!attrname) {
        return 1;
    }
    int ret = removexattr(path, attrname);
    free(attrname);
    return ret;
}

#endif /* Linux */

#ifdef __APPLE__
#define XATTR_SUPPORTED
#include <sys/xattr.h>

static char ** parse_xattrlist(char *buf, ssize_t length, ssize_t *nelm) {
    size_t arraylen = LIST_ARRAY_LEN;
    size_t arraypos = 0;
    char **array = malloc(LIST_ARRAY_LEN * sizeof(char*));
    
    char *name = buf;
    for(int i=0;i<length;i++) {
        if(buf[i] == '\0') {
            char *attrname = strdup(name);
            if(strarray_add(&array, &arraypos, &arraylen, attrname)) {
                strarray_free(array, arraypos);
                *nelm = -1;
                return NULL;
            }
            name = buf + i + 1;
        }
    }
    
    if(arraypos == 0) {
        free(array);
        array = NULL;
    }
    
    *nelm = arraypos;
    return array;
}

char ** xattr_list(const char *path, ssize_t *nelm) {
    char *list = malloc(LIST_BUF_LEN);
    if(!list) {
        *nelm = -1;
        return NULL;
    }
    ssize_t len = listxattr(path, list, LIST_BUF_LEN, 0);
    if(len == -1) {
        switch(errno) {
            case ERANGE: {
                // buffer too, get size of attribute list
                ssize_t newlen = listxattr(path, NULL, 0, 0);
                if(newlen > 0) {
                    // second try
                    char *new_list = realloc(list, newlen);
                    if(!new_list) {
                        free(list);
                        *nelm = -1;
                        return NULL;
                    }
                    list = new_list;
                    len = listxattr(path, list, newlen, 0);
                    if(len != -1) {
                        // this time it worked
                        break;
                    }
                }
            }
            default: {
                free(list);
                *nelm = -1;
                return NULL;
            }
        }
    }
    
    char **ret = parse_xattrlist(list, len, nelm);
    free(list);
    return ret;
}

char * xattr_get(const char *path, const char *attr, ssize_t *len) { 
    // get attribute length
    ssize_t attrlen = getxattr(path, attr, NULL, 0, 0, 0);
    if(attrlen < 0) {
        *len = -1;
        return NULL;
    }
    
    char *buf = malloc(attrlen + 1);
    if(!buf) {
        *len = -1;
        return NULL;
    }
    ssize_t vlen = getxattr(path, attr, buf, attrlen, 0, 0);
    if(vlen < 0) {
        *len = -1;
        free(buf);
        return NULL;
    }
    buf[vlen] = 0;
    
    *len = vlen;
    return buf;
}

int xattr_set(const char *path, const char *name, const void *value, size_t len) {
    int ret = setxattr(path, name, value, len, 0, 0);
    return ret;
}

int xattr_remove(const char *path, const char *name) {
    return removexattr(path, name, 0);
}

#endif /* Apple */

#ifdef __sun
#define XATTR_SUPPORTED
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

static int open_attrfile(const char *path, const char *attr, int oflag) {
    int file = open(path, O_RDONLY);
    if(file == -1) {
        return -1;
    }
    
    int attrfile = openat(file, attr, oflag, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    close(file);
    return attrfile;
}

char ** xattr_list(const char *path, ssize_t *nelm) {
    *nelm = -1;
    
    int attrdir = open_attrfile(path, ".", O_RDONLY|O_XATTR);
    if(attrdir == -1) {
        return NULL;
    }
    
    DIR *dir = fdopendir(attrdir);
    if(!dir) {
        close(attrdir);
        return NULL;
    }
    
    size_t arraylen = LIST_ARRAY_LEN;
    size_t arraypos = 0;
    char **array = malloc(LIST_ARRAY_LEN * sizeof(char*));
    if(!array) {
        closedir(dir);
        *nelm = -1;
        return NULL;
    }
    
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
        if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || !strcmp(ent->d_name, "SUNWattr_ro") || !strcmp(ent->d_name, "SUNWattr_rw")) {
            continue;
        }
        char *name = strdup(ent->d_name);
        if(strarray_add(&array, &arraypos, &arraylen, name)) {
            strarray_free(array, arraypos);
            *nelm = -1;
            closedir(dir);
            return NULL;
        }
    }
    closedir(dir);
    
    *nelm = arraypos;
    return array;
}

char * xattr_get(const char *path, const char *attr, ssize_t *len) {
    *len = -1;
    
    int attrfile = open_attrfile(path, attr, O_RDONLY|O_XATTR);
    if(attrfile == -1) {
        return NULL;
    }
    
    struct stat s;
    if(fstat(attrfile, &s)) {
        close(attrfile);
        return NULL;
    }
    
    size_t bufsize = (size_t)s.st_size;
    char *buf = malloc(bufsize + 1);
    if(!buf) {
        close(attrfile);
        return NULL;
    }
    
    char *b = buf;
    size_t cur = 0;
    while(cur < bufsize) {
        ssize_t r = read(attrfile, buf + cur, bufsize - cur);
        if(r <= 0) {
            break;
        }
        cur += r;
    }
    
    close(attrfile);
    if(cur != bufsize) {
        free(buf);
        return NULL;
    }
    
    buf[cur] = 0;
    *len = (ssize_t)bufsize;
    return buf;    
}

int xattr_set(const char *path, const char *name, const void *value, size_t len) {
    int attrfile = open_attrfile(path, name, O_CREAT|O_WRONLY|O_XATTR|O_TRUNC);
    if(attrfile == -1) {
        return -1;
    }
    
    const char *p = value;
    size_t remaining = len;
    while(remaining > 0) {
        ssize_t w = write(attrfile, p, remaining);
        if(w <= 0) {
            break;
        }
        p += w;
        remaining -= w;
    }
    
    close(attrfile);
    
    return remaining > 0 ? -1 : 0;
}

int xattr_remove(const char *path, const char *name) {
    int attrdir = open_attrfile(path, ".", O_RDONLY|O_XATTR);
    if(attrdir == -1) {
        return -1;
    }
    
    int ret = unlinkat(attrdir, name, 0);
    close(attrdir);
    return ret;
}

#endif /* Sun */


#ifdef __FreeBSD__
#define XATTR_SUPPORTED

#include <sys/types.h>
#include <sys/extattr.h>

static char ** parse_xattrlist(char *buf, ssize_t length, ssize_t *nelm) {
    size_t arraylen = LIST_ARRAY_LEN;
    size_t arraypos = 0;
    char **array = malloc(LIST_ARRAY_LEN * sizeof(char*));
    if(!array) {
        *nelm = -1;
        return NULL;
    }
    
    for(int i=0;i<length;i++) {
        int namelen = buf[i];
        char *name = buf + i + 1;
        char *attrname = malloc(namelen + 1);
        if(!attrname) {
            strarray_free(array, arraypos);
            *nelm = -1;
            return NULL;
        }
        memcpy(attrname, name, namelen);
        attrname[namelen] = 0;
        if(strarray_add(&array, &arraypos, &arraylen, attrname)) {
            strarray_free(array, arraypos);
            *nelm = -1;
            return NULL;
        }
        i += namelen;
    }
    
    if(arraypos == 0) {
        free(array);
        array = NULL;
    }
    
    *nelm = arraypos;
    return array;
}

char ** xattr_list(const char *path, ssize_t *nelm) {
    *nelm = -1;
    ssize_t lslen = extattr_list_file(path, EXTATTR_NAMESPACE_USER, NULL, 0);
    if(lslen <= 0) {
        if(lslen == 0) {
            *nelm = 0;
        }
        return NULL;
    }
    
    char *list = malloc(lslen);
    if(!list) {
        return NULL;
    }
    ssize_t len = extattr_list_file(path, EXTATTR_NAMESPACE_USER, list, lslen);
    if(len == -1) {
        free(list);
        return NULL;
    }
    
    char **ret = parse_xattrlist(list, len, nelm);
    free(list);
    return ret;
}

char * xattr_get(const char *path, const char *attr, ssize_t *len) { 
    // get attribute length
    ssize_t attrlen = extattr_get_file(path, EXTATTR_NAMESPACE_USER, attr, NULL, 0);
    if(attrlen < 0) {
        *len = -1;
        return NULL;
    }
    
    char *buf = malloc(attrlen + 1);
    ssize_t vlen = extattr_get_file(path, EXTATTR_NAMESPACE_USER, attr, buf, attrlen);
    if(vlen < 0) {
        *len = -1;
        free(buf);
        return NULL;
    }
    buf[attrlen] = 0;
    *len = vlen;
    return buf;
}

int xattr_set(const char *path, const char *name, const void *value, size_t len) {
    int ret = extattr_set_file(path, EXTATTR_NAMESPACE_USER, name, value, len);
    return ret >= 0 ? 0 : ret;
}

int xattr_remove(const char *path, const char *name) {
    return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, name);
}

#endif /* FreeBSD */


#ifndef XATTR_SUPPORTED

char ** xattr_list(const char *path, ssize_t *nelm) {
    *nelm = -1;
    return NULL;
}

char * xattr_get(const char *path, const char *attr, ssize_t *len) {
    *len = -1;
    return NULL;
}

int xattr_set(const char *path, const char *name, const void *value, size_t len) {
    return -1;
}

int xattr_remove(const char *path, const char *name) {
    return -1;
}

#endif /* unsupported platform */

void xattr_free_list(char **attrnames, ssize_t nelm) {
    if(attrnames) {
        for(int i=0;i<nelm;i++) {
            free(attrnames[i]);
        }
        free(attrnames);
    }
}
