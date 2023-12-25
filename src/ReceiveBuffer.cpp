#include "ReceiveBuffer.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_CCNT_CHKSUM 100
#define NON_HEXCHAR_VAL 255
#define CHAR_CR '\r'
#define CHAR_LF '\n'

ReceiveBuffer::ReceiveBuffer() {
    // constructor: initialize member variables
    cPage = allocatePage(&rPage);
    clear();
}

ReceiveBuffer::~ReceiveBuffer() {
    // destuctor: release dynamically allocated buffer
    freePages(rPage);
}

void ReceiveBuffer::freePages(bufferpage_t *pg) {
    // release all pages from the given to the last
    // if NULL is given, do nothing
    bufferpage_t *cp = pg;
    while (cp != NULL) {
        bufferpage_t *np = cp->next;
        free(cp);
        cp = np;
    }
}

bufferpage_t *ReceiveBuffer::allocatePage(bufferpage_t **pg) {
    // create a new page and fill it with zero
    bufferpage_t *np = (bufferpage_t *)malloc(sizeof(bufferpage_t));
    memset(np, 0, sizeof(bufferpage_t));

    // assign the page to the given argment (if it is NOT NULL)
    if (pg != NULL) {
        *pg = np;
    }

    // return the page
    return np;
}

void ReceiveBuffer::clear() {
    // release second and subsequent pages, zero-fill the first page
    freePages(rPage->next);
    memset(rPage, 0, sizeof(bufferpage_t));

    // clear member variables
    cPage = rPage;
    ptr = 0;
    dLen = 0;
    cCnt = 0;
    cChk = 0;
    rChk = 0;
}

bool ReceiveBuffer::isChecksumCorrect() { return (cChk == rChk); }

bool ReceiveBuffer::isTextChar(const char ch) {
    // return true if the given chars is usable in PMTK sentence
    switch (ch) {
        case '$':
        case '0' ... '9':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case ',':
        case '*':
            return true;
    }

    // returns false for the others ([\r\n] and others)
    return false;
}

uint8_t ReceiveBuffer::hexCharToByte(const char ch) {
    switch (ch) {
        case '0' ... '9':  // return 0-9 for '0'-'9'
            return (ch - '0');

        case 'a' ... 'f':  // return 10-16 for 'a'-'f'
            return (ch - 'a' + 10);

        case 'A' ... 'F':  // return 10-16 for 'A'-'F'
            return (ch - 'A' + 10);
    }

    // return zero for others ([$*,\r\n] and others)
    return NON_HEXCHAR_VAL;
}

void ReceiveBuffer::appendToBuffer(const char ch) {
    if (ptr < BP_BUFFER_SIZE) {
        cPage->buf[ptr] = ch;
        ptr += 1;
    }

    if (ptr == BP_BUFFER_SIZE) {
        cPage = allocatePage(&(cPage->next));
        ptr = 0;
    }
}

void ReceiveBuffer::updateChecksum(const char ch) {
    if (cCnt < RB_CCNT_CHKSUM) {  // calculate checksum of sentence text
        switch (ch) {
            case '0' ... '9':
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case ',':
                cChk ^= (byte)ch;
                break;
        }
    } else {  // store received checksum value
        rChk = (rChk << 4) + hexCharToByte(ch);
    }
}

bool ReceiveBuffer::put(const char ch) {
    switch (ch) {
        case '$':
            clear();
            break;
        case ',':
            cCnt += 1;
            break;
        case '*':
            cCnt = RB_CCNT_CHKSUM;
            break;
    }

    //
    if (isTextChar(ch) == true) {
        // append given char (when buffer available), then
        // calculate checksum of a receiving sentence or extract checksum value
        appendToBuffer(ch);
        updateChecksum(ch);

        dLen += 1;
    }

    return (ch == CHAR_LF);
}

char ReceiveBuffer::get() {
    char ch = cPage->buf[ptr];

    if ((ch != 0) && (ptr < BP_BUFFER_SIZE)) {
        ptr += 1;

        if ((ptr == BP_BUFFER_SIZE) && (cPage->next != NULL)) {
            cPage = cPage->next;
            ptr = 0;
        }
    }

    return ch;
}

bool ReceiveBuffer::readHexByteFull(byte *by) {
    byte upper = hexCharToByte(get());
    byte lower = hexCharToByte(get());

    if ((upper == NON_HEXCHAR_VAL) || (lower == NON_HEXCHAR_VAL)) {
        return false;
    }

    *by = (upper << 4) + lower;

    return true;
}

bool ReceiveBuffer::readHexByteHalf(byte *by) {
    byte lower = hexCharToByte(get());

    if (lower == NON_HEXCHAR_VAL) {
        return false;
    }

    *by = lower;

    return true;
}

void ReceiveBuffer::print_d() {
    // put NMEA sentense stored on this buffer to stdout
    uint8_t pn = 0;
    bufferpage_t *cp = rPage;
    while (cp != NULL) {
        printf("**debug RB.print_d: page[%02i]@%p=", pn++, cp);
        for (byte i = 0; i < (BP_BUFFER_SIZE); i++) {
            if (cp->buf[i] == 0) break;

            printf("%c", cp->buf[i]);
        }
        printf("\n");

        cp = cp->next;

        // put only 1 line with break
        break;
    }

    //printf("**debug** RB.print_d: dLen=%d, cChk=%02X, rChk=%02X\n", dLen, cChk, rChk);
}

bool ReceiveBuffer::match(const char *str) {
    return (strstr(rPage->buf, str) != NULL);
}

bool ReceiveBuffer::seek(const uint8_t clm) {
    bufferpage_t *pg = rPage;
    uint8_t pt = 0;
    uint8_t cc = 0;

    while ((cc < clm) && (pg != NULL)) {
        for (pt = 0; pt < BP_BUFFER_SIZE; pt++) {
            if ((cc == clm) || (pg->buf[pt] == 0)) {
                break;
            }

            switch (pg->buf[pt]) {
                case ',':
                    cc += 1;
                    break;
                case '*':
                    cc = RB_CCNT_CHKSUM;
                    break;
            }
        }

        if (cc < clm) {
            pg = pg->next;
        }
    }

    if (cc == clm) {
        cPage = pg;
        ptr = pt;
        cCnt = cc;
    }

    // printf("**debug** RB.seek: seekTo=%d, result=%d, cPage=%p, ptr=%d\n", clm, (cc == clm), cPage, pt);

    return (cc == clm);
}
