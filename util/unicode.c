/*
 * Copyright 2025 Olaf Wintermann
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

#include "unicode.h"

#include <stdlib.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>

char* StringNFD2NFC(const char *str) {
    CFStringRef nfd = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
    CFMutableStringRef nfc = CFStringCreateMutableCopy(NULL, 0, nfd);
    CFStringNormalize(nfc, kCFStringNormalizationFormC);
    CFIndex length16 = CFStringGetLength(nfc); // number of utf16 code pairs
    size_t buflen = (length16+1) * 4; // allocate some extra space, this should be enough
    char *cstr = malloc(buflen);
    if(!cstr) {
        exit(-1);
    }
    CFStringGetCString(nfc, cstr, buflen, kCFStringEncodingUTF8);
    CFRelease(nfc);
    CFRelease(nfd);
    return cstr;
}

#endif
