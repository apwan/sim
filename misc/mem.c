#include <cstring>
#include <cstdio>
#include "isa.h"


int MemRec::getLen(){
    return len;
}

MemRec::MemRec(int l){
    len = ((l+BPL-1)/BPL)*BPL;
    contents = new byte_t[l];

}

MemRec::~MemRec(){
    delete []contents;
}

MemRec &MemRec::operator=(const MemRec &m){
    if(m.len != len){
        delete []contents;
        len = m.len;
    }
    memcpy(contents, m.contents, len);
    return *this;
}

bool_t MemRec::operator!=(const MemRec &m){
    word_t pos;
    int l = m.len;
    if (l < len)
        l = len;
    word_t ov = 0;  word_t nv = 0;
    for (pos = 0; pos+4 <= l; pos += 4) {
        memcpy(&ov, contents+pos, 4);
        memcpy(&nv, m.contents+pos, 4);
        if (nv != ov) {
            printf("diff at pos: 0x%x, %d, %d\n", pos, ov, nv);
            return TRUE;
        }
    }
    return FALSE;
}

void MemRec::dump(FILE *outfile, word_t pos, int l){
    int i, j;
    while (pos % BPL) {
        pos --;
        l ++;
    }

    l = ((l+BPL-1)/BPL)*BPL;

    if (pos+l > len)
        l = len-pos;
    byte_t *p = contents + pos;
    word_t val = 0;
    for (i = 0; i < l; i+=BPL) {
        fprintf(outfile, "0x%.4x:", pos+i);
        for (j = 0; j < BPL; j+= 4) {
            memcpy(&val, p+i+j, 4);
            fprintf(outfile, " %.8x", val);
        }
    }
}

bool_t MemRec::clear(){
    memset(contents, 0, len);
}

bool_t MemRec::getByte(word_t pos, byte_t *dest){
    if (pos < 0 || pos >= len)
        return FALSE;
    else{
        *dest = contents[pos];
        return TRUE;
    }
}

bool_t MemRec::getWord(word_t pos, word_t *dest){
    if (pos < 0 || pos + 4 > len)
        return FALSE;
    else{
        memcpy(dest, contents+pos, 4);
        return TRUE;
    }
}

bool_t MemRec::setByte(word_t pos, byte_t val){
    if (pos < 0 || pos >= len)
        return FALSE;
    else{
        contents[pos] = val;
        return TRUE;
    }
}

bool_t MemRec::setWord(word_t pos, word_t val){
    if (pos < 0 || pos + 4 > len)
        return FALSE;
    else{
        memcpy(contents+pos, &val, 4);
        return TRUE;
    }
}


