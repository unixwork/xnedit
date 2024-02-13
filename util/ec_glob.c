/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2024 Mike Becker - All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ec_glob.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <regex.h>

struct numpair_s {
    long min;
    long max;
};

struct ec_glob_re {
    char *str;
    unsigned len;
    unsigned capacity;
};

#ifndef EC_GLOB_STACK_CAPACITY
#define EC_GLOB_STACK_CAPACITY 64
#endif

static void ec_glob_pattern_increase_capacity(struct ec_glob_re *re) {
    unsigned newcap = re->capacity * 2;
    char *newmem;
    if (re->capacity == EC_GLOB_STACK_CAPACITY) {
        newmem = malloc(newcap);
        if (newmem == NULL) abort();
        memcpy(newmem, re->str, re->len);
    } else {
        newmem = realloc(re->str, newcap);
        if (newmem == NULL) abort();
    }
    re->capacity = newcap;
    re->str = newmem;
}

#define ec_glob_pattern_ensure_capacity(re, n) \
    if (re.len + n > re.capacity) ec_glob_pattern_increase_capacity(&re)

#define ec_glob_cats(re, s) \
    ec_glob_pattern_ensure_capacity(re, sizeof(s) - 1); \
    memcpy((re).str + (re).len, s, sizeof(s) - 1); (re).len += sizeof(s)-1

#define ec_glob_catc(re, c) \
    ec_glob_pattern_ensure_capacity(re, 1); (re).str[(re).len++] = (c)


int ec_glob(const char *pattern, const char *string) {
    char stack[EC_GLOB_STACK_CAPACITY];
    struct ec_glob_re re_pattern = {
            stack, 1, EC_GLOB_STACK_CAPACITY
    };
    re_pattern.str[0] = '^';

    unsigned scanidx = 0;
    unsigned inputlen = strlen(pattern);
    char c;

    // maintain information about braces
    const unsigned brace_stack_size = 32;
    char brace_stack[brace_stack_size];
    int depth_brace = 0;
    _Bool braces_valid = 1;

    // first, check if braces are syntactically valid
    for (unsigned i = 0 ; i < inputlen ; i++) {
        // skip potentially escaped braces
        if (pattern[i] == '\\') {
            i++;
        } else if (pattern[i] == '{') {
            depth_brace++;
        } else if (pattern[i] == '}') {
            if (depth_brace > 0) {
                depth_brace--;
            } else {
                braces_valid = 0;
                break;
            }
        }
    }
    if (depth_brace > 0) {
        braces_valid = 0;
        depth_brace = 0;
    }

    // prepare what we think is more than enough memory for matches
    const unsigned numrange_max = 32;
    unsigned numrange_grp_count = 0;
    unsigned numrange_grp_idx[numrange_max];
    struct numpair_s numrange_pairs[numrange_max];
    regmatch_t numrange_matches[numrange_max];

    // initialize first group number with zero
    // and increment whenever we create a new group
    numrange_grp_idx[0] = 0;

    // now translate the editorconfig pattern to a POSIX regular expression
    while (scanidx < inputlen) {
        c = pattern[scanidx++];

        // escape
        if (c == '\\') {
            if (strchr("?{}[]*\\-,", pattern[scanidx]) != NULL) {
                // also escape in regex when required
                if (strchr("?{}[]*\\", pattern[scanidx]) != NULL) {
                    ec_glob_catc(re_pattern, '\\');
                }
                c = pattern[scanidx++];
                ec_glob_catc(re_pattern, c);
            }
            // otherwise, it's just a backslash
            else {
                ec_glob_cats(re_pattern, "\\\\");
            }
        }
        // wildcard
        else if (c == '*') {
            // is double-wildcard?
            if (pattern[scanidx] == '*') {
                scanidx++;
                // check for collapsible slashes
                if (pattern[scanidx] == '/' && scanidx >= 3
                    && pattern[scanidx - 3] == '/') {
                    // the collapsible slash is simply discarded
                    scanidx++;
                }
                ec_glob_cats(re_pattern, ".*");
            } else {
                ec_glob_cats(re_pattern, "[^/]*");
            }
        }
        // arbitrary character
        else if (c == '?') {
            ec_glob_catc(re_pattern, '.');
        }
        // start of alternatives
        else if (c == '{') {
            // if braces are not syntactically valid, treat them as literals
            if (!braces_valid) {
                ec_glob_cats(re_pattern, "\\{");
                continue;
            }

            // check brace stack
            depth_brace++;
            if (depth_brace > brace_stack_size) {
                // treat brace literally when stacked too many of them
                ec_glob_cats(re_pattern, "\\{");
                continue;
            }

            // check if {single} or {num1..num2}
            _Bool single = 1;
            _Bool dotdot = strchr("+-0123456789", pattern[scanidx]) != NULL;
            _Bool dotdot_seen = 0;
            for (unsigned fw = scanidx; fw < inputlen; fw++) {
                if (pattern[fw] == ',') {
                    single = 0;
                    dotdot = 0;
                    break;
                }
                else if (pattern[fw] == '}') {
                    // check if this is a {num1..num2} pattern
                    if (dotdot) {
                        _Bool ok = 1;
                        unsigned ngc = numrange_grp_count;
                        char *chk;
                        errno = 0;
                        numrange_pairs[ngc].min = strtol(
                                &pattern[scanidx], &chk, 10);
                        ok &= *chk == '.' && 0 == errno;
                        numrange_pairs[ngc].max = strtol(
                                strrchr(&pattern[scanidx], '.')+1, &chk, 10);
                        ok &= *chk == '}' && 0 == errno;
                        if (ok) {
                            // a dotdot is not a single
                            single = 0;
                            // skip this subpattern later on
                            scanidx = fw+1;
                        } else {
                            // not ok, we could not parse the numbers
                            dotdot = 0;
                        }
                    }
                    break;
                } else if (dotdot) {
                    // check for dotdot separator
                    if (pattern[fw] == '.') {
                        if (!dotdot_seen &&
                            fw + 2 < inputlen && pattern[fw + 1] == '.' &&
                            strchr("+-0123456789", pattern[fw + 2]) != NULL) {
                            fw += 2;
                            dotdot_seen = 1;
                        } else {
                            dotdot = 0;
                        }
                    }
                    // everything must be a digit, otherwise
                    else if (!strchr("0123456789", pattern[fw])) {
                        dotdot = 0;
                    }
                }
            }

            if (single) {
                // push literal brace
                ec_glob_cats(re_pattern, "\\{");
                brace_stack[depth_brace-1] = '}';
            } else {
                // open choice and push parenthesis
                ec_glob_catc(re_pattern, '(');

                // increase the current group number
                numrange_grp_idx[numrange_grp_count]++;

                if (dotdot) {
                    // add the number matching pattern
                    ec_glob_cats(re_pattern, "[-+]?[0-9]+)");
                    // increase group counter and initialize
                    // next index with current group number
                    numrange_grp_count++;
                    if (numrange_grp_count < numrange_max) {
                        numrange_grp_idx[numrange_grp_count] =
                                numrange_grp_idx[numrange_grp_count - 1];
                    }
                    // we already took care of the closing brace
                    depth_brace--;
                } else {
                    // remember that we need to close the choice eventually
                    brace_stack[depth_brace - 1] = ')';
                }
            }
        }
        // end of alternatives
        else if (depth_brace > 0 && c == '}') {
            depth_brace--;
            if (depth_brace < brace_stack_size
                && brace_stack[depth_brace] == ')') {
                ec_glob_catc(re_pattern, ')');
            } else {
                ec_glob_cats(re_pattern, "\\}");
            }
        }
        // separator of alternatives
        else if (depth_brace > 0 && c == ',') {
            ec_glob_catc(re_pattern, '|');
        }
        // brackets
        else if (c == '[') {
            // check if we have a corresponding closing bracket
            _Bool valid = 0;
            _Bool closing_bracket_literal = 0;
            unsigned newidx;
            for (unsigned fw = scanidx ; fw < inputlen ; fw++) {
                if (pattern[fw] == ']') {
                    // only terminating if it's not the first char
                    if (fw == scanidx) {
                        // otherwise, auto-escaped
                        closing_bracket_literal = 1;
                    } else {
                        valid = 1;
                        newidx = fw+1;
                        break;
                    }
                } else if (pattern[fw] == '/') {
                    // special (undocumented) case: slash breaks
                    // https://github.com/editorconfig/editorconfig/issues/499
                    break;
                } else if (pattern[fw] == '\\') {
                    // skip escaped characters as usual
                    fw++;
                    closing_bracket_literal |= pattern[fw] == ']';
                }
            }


            if (valid) {
                // first of all, check, if the sequence is negated
                if (pattern[scanidx] == '!') {
                    scanidx++;
                    ec_glob_cats(re_pattern, "[^");
                } else {
                    ec_glob_catc(re_pattern, '[');
                }

                // if we have a closing bracket as literal, it must appear first
                if (closing_bracket_literal) {
                    ec_glob_catc(re_pattern, ']');
                    // but if the minus operator wanted to be there
                    // we need to move it to the end
                    if (pattern[scanidx] == '-') {
                        scanidx++;
                    }
                }

                // everything within brackets is treated as a literal character
                // we have to parse them one by one, though, because we might
                // need to escape regex-relevant stuff
                for (unsigned fw = scanidx ;  ; fw++) {
                    if (pattern[fw] == '\\') {
                        // skip to next char
                        continue;
                    }
                    // check for terminating bracket
                    else if (pattern[fw] == ']') {
                        if (fw > scanidx && pattern[fw-1] != '\\') {
                            break;
                        }
                    }
                    // include literal character
                    else {
                        if (strchr(".(){}[]", pattern[fw]) != NULL) {
                            ec_glob_catc(re_pattern, '\\');
                        }
                        ec_glob_catc(re_pattern, pattern[fw]);
                    }
                }

                // did we promise the minus a seat in the last row?
                if (pattern[scanidx-1] == '-') {
                    ec_glob_cats(re_pattern, "-]");
                } else {
                    ec_glob_catc(re_pattern, ']');
                }
                scanidx = newidx;
            } else {
                // literal bracket
                ec_glob_cats(re_pattern, "\\[");
            }
        }
        // escape special chars
        else if (strchr(".(){}[]", c) != NULL) {
            ec_glob_catc(re_pattern, '\\');
            ec_glob_catc(re_pattern, c);
        }
        // literal (includes path separators)
        else {
            ec_glob_catc(re_pattern, c);
        }
    }

    // terminate the regular expression
    ec_glob_catc(re_pattern, '$');
    ec_glob_catc(re_pattern, '\0');


    // compile pattern and execute matching
    regex_t re;
    int status;
    int flags = REG_EXTENDED;

    // when we don't have a num-pattern, don't capture anything
    if (numrange_grp_count == 0) {
        flags |= REG_NOSUB;
    }

    if ((status = regcomp(&re, re_pattern.str, flags)) == 0) {
        status = regexec(&re, string, numrange_max, numrange_matches, 0);

        // check num ranges
        for (unsigned i = 0 ; status == 0 && i < numrange_grp_count ; i++) {
            regmatch_t nm = numrange_matches[numrange_grp_idx[i]];
            int nmlen = nm.rm_eo-nm.rm_so;
            char *nmatch = malloc(nmlen+1);
            memcpy(nmatch, string+nm.rm_so, nmlen);
            nmatch[nmlen] = '\0';
            errno = 0;
            char *chk;
            long num = strtol(nmatch, &chk, 10);
            if (*chk == '\0' && 0 == errno) {
                // check if the matched number is within the range
                status |= !(numrange_pairs[i].min <= num
                        && num <= numrange_pairs[i].max);
            } else {
                // number not processable, return error
                status = 1;
            }
            free(nmatch);
        }
        regfree(&re);
    }

    if (re_pattern.capacity > EC_GLOB_STACK_CAPACITY) {
        free(re_pattern.str);
    }

    return status;
}
