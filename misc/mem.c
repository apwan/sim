#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>

#include "isa.h"

/* memory */

int MemRec::getLen(){
    return len;
}

MemRec::MemRec(int l):len(((l+BPL-1)/BPL)*BPL),
    contents(new byte_t[l]){
}
MemRec::MemRec(const MemRec &m):len(m.len),contents(new byte_t[len]){
    memcpy(contents, m.contents, len);
}
MemRec::~MemRec(){
    delete []contents;
}

MemRec &MemRec::operator=(const MemRec &m){
    if(m.len != len){
        delete []contents;
        len = m.len;
        contents = new byte_t[len];
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

#define LINELEN 4096
int MemRec::load(FILE *infile, int report_error){
    /* Read contents of .yo file */
    char buf[LINELEN];
    char c, ch, cl;
    int byte_cnt = 0;
    int lineno = 0;
    word_t bytepos = 0;
    int empty_line = 1;
    int addr = 0;
    char hexcode[15];

#ifdef HAS_GUI
    /* For display */
    int line_no = 0;
    char line[LINELEN];
#endif /* HAS_GUI */

    int index = 0;
    while (fgets(buf, LINELEN, infile)) {
        int cpos = 0;
        empty_line = 1;
        lineno++;
        /* Skip white space */
        while (isspace((int)buf[cpos]))
            cpos++;

        if (buf[cpos] != '0' ||
                (buf[cpos+1] != 'x' && buf[cpos+1] != 'X'))
            continue; /* Skip this line */
        cpos+=2;

        /* Get address */
        bytepos = 0;
        while (isxdigit((int)(c=buf[cpos]))) {
            cpos++;
            bytepos = (bytepos<<4) + hex2dig(c);
        }

        while (isspace((int)buf[cpos]))
            cpos++;

        if (buf[cpos++] != ':') {
            if (report_error) {
                fprintf(stderr, "Error reading file. Expected colon\n");
                fprintf(stderr, "Line %d:%s\n", lineno, buf);
                fprintf(stderr,
                        "Reading '%c' at position %d\n", buf[cpos], cpos);
            }
            return 0;
    }

    addr = bytepos;

    while (isspace((int)buf[cpos]))
        cpos++;

    index = 0;

    /* Get code */
    while (isxdigit((int)(ch=buf[cpos++])) &&
           isxdigit((int)(cl=buf[cpos++]))) {
        byte_t byte = 0;
        if (bytepos >= len) {
            if (report_error) {
                fprintf(stderr,
                    "Error reading file. Invalid address. 0x%x\n",
                    bytepos);
                fprintf(stderr, "Line %d:%s\n", lineno, buf);
            }
            return 0;
        }
        byte = (hex2dig(ch)<<4)+hex2dig(cl);
        contents[bytepos++] = byte;

        byte_cnt++;
        empty_line = 0;
        hexcode[index++] = ch;
        hexcode[index++] = cl;
    }
    /* Fill rest of hexcode with blanks */
    for (; index < 12; index++)
        hexcode[index] = ' ';
    hexcode[index] = '\0';

#ifdef HAS_GUI
    if (gui_mode) {
        /* Now get the rest of the line */
        while (isspace((int)buf[cpos]))
        cpos++;
        cpos++; /* Skip over '|' */

        index = 0;
        while ((c = buf[cpos++]) != '\0' && c != '\n') {
        line[index++] = c;
        }
        line[index] = '\0';
        if (!empty_line){
            report_line(line_no++, addr, hexcode, line);
        }
    }
#endif /* HAS_GUI */
    }
    return byte_cnt;
}

/* REG */
word_t RegRec::getVal(int id){
    if (id >= REG_NONE)
        return 0;
    word_t val = 0;
    getWord(id*4, &val);
    return val;
}

void RegRec::setVal(int id, word_t val){
    if (id < REG_NONE) {
        setWord(id*4,val);

#ifdef HAS_GUI
    if (gui_mode) {
        printf("about to sent signal");
        signal_register_update(id, val);
    }
#endif /* HAS_GUI */
    }

}

void RegRec::dump(FILE *outfile){
    int id;
    for (id = 0; reg_valid(id); id++) {
        fprintf(outfile, "   %s  ", reg_table[id].name);
    }
    fprintf(outfile, "\n");
    word_t val;
    for (id = 0; reg_valid(id); id++) {
        val = 0;
        getWord(id*4, &val);
        fprintf(outfile, " %x", val);
    }
    fprintf(outfile, "\n");
}


/*  CACHE  */




cache_t init_cache(int s, int E){
    cache_t res = (cache_t) malloc(sizeof(cache_rec));
    res->cac_s = s;
    res->cac_E = E;
    res->len = E<<s;
    res->contents = (cache_line*) calloc(res->len, sizeof(cache_line));
    return res;
}

void clear_cache(cache_t c, mem_t m){
    int i;
    if(m){
        for(i=0;i<c->len;++i){
            evict_cache(c, i, m);
        }
    }
    memset(c->contents, 0, c->len*sizeof(cache_line));
}

void free_cache(cache_t c){
    free((void*)c->contents);
    free((void*)c);
}




bool_t evict_cache(cache_t c, word_t i, mem_t m){
    /* write back */
    if(i<0 || i>c->len) return FALSE;
    if(!c->contents[i].dirty || !c->contents[i].valid) return TRUE;

    word_t pos = (((c->contents[i].tag<<c->cac_s) | (i/c->cac_E))) << CACHE_b;
    int j; word_t val;
    for(j=0;j<CACHE_B;j+=4){
        memcpy(&val, c->contents[i].block+j, 4);
        m->setWord(pos+j, val);
    }
    c->contents[i].dirty = FALSE;
    return TRUE;
}

bool_t retr_cache(cache_t c, byte_t tag, word_t i, mem_t m){
    if(i<0 || i>c->len) return FALSE;
    c->contents->tag = tag;
    word_t pos = ((tag<<c->cac_s) | (i/c->cac_E))<<CACHE_b;
    int j; word_t val;
    for(j=0;j<CACHE_B;j+=4){
        m->getWord(pos+j,&val);
        memcpy(c->contents[i].block+j, &val, 4);
    }
    c->contents[i].valid = TRUE;
    c->contents[i].dirty = FALSE;
    return TRUE;
}


word_t search_cache(cache_t c, word_t pos, mem_t m){

    word_t s = MASK(c->cac_s,0) & (pos >> CACHE_b);
    byte_t t = CACHE_TAG_MASK & (pos>>(CACHE_b+c->cac_s));
    word_t i = s * c->cac_E;
    word_t e = i + c->cac_E;
    for(; i<e;++i){
        if(c->contents[i].valid && t == c->contents[i].tag)
            return i;
    }
    if(m == NULL)
        return -1;
    printf("\n%d, Cache Miss\n", pos);

    for (i=s*c->cac_E;i<e;++i){
        if(c->contents[i].valid)
            continue;
        else
            break;
    }
    if(i>=e){
        i = s+rand()%c->cac_E;
        evict_cache(c, i, m);
    }
    retr_cache(c, t, i, m);
    dump_cache(stdout, c, i, 1);
    return i;
}



bool_t get_cache_byte(cache_t c, word_t pos, byte_t *dest, mem_t m){
    if (pos < 0 || pos >= m->getLen())
        return FALSE;
    int i = search_cache(c, pos, m);
    word_t p = pos & CACHE_MASK(CACHE_b);
    *dest = c->contents[i].block[p];

    return TRUE;
}

bool_t get_cache_word(cache_t c, word_t pos, word_t *dest, mem_t m){
    if (pos < 0 || pos+4 > m->getLen())
        return FALSE;
    word_t i = search_cache(c, pos, m);
    word_t p = pos & MASK(CACHE_b,0);
    word_t d = CACHE_B - p;

    if(d>=4){
        memcpy(dest, c->contents[i].block+p, 4);
    }else{
        printf("word exceeds\n");
        memcpy(dest, c->contents[i].block+p, d);
        word_t i = search_cache(c, pos+d, m);
        memcpy((byte_t*)dest+d, c->contents[i].block, 4-d);
    }
    return TRUE;
}

word_t set_cache_byte(cache_t c, word_t pos, byte_t val, mem_t m){
    if (pos < 0 || pos >= m->getLen())
        return -1;
    word_t i = search_cache(c, pos, m);
    c->contents[i].block[pos&MASK(CACHE_b,0)] = val;
    c->contents[i].dirty = TRUE;
    return i;
}

word_t set_cache_word(cache_t c, word_t pos, word_t val, mem_t m){
    if (pos < 0 || pos >= m->getLen())
        return -1;
    word_t i = search_cache(c, pos, m);
    word_t p = pos&CACHE_MASK(CACHE_b);
    word_t d = CACHE_B - p;

    if(d>=4){
        memcpy(c->contents[i].block+p, &val, 4);
        c->contents[i].dirty = TRUE;
    }else{
        printf("word exceeds\n");
        memcpy(c->contents[i].block+p, &val, d);
        c->contents[i].dirty = TRUE;
        word_t i = search_cache(c, pos+d, m);
        memcpy(c->contents[i].block, (byte_t*)&val + d, 4-d);
        c->contents[i].dirty = TRUE;
    }
    return i;
}

void dump_cache(FILE *outfile, cache_t c, word_t s, int len)
{
    int i, j;
    word_t e = s+len;
    if (e > c->len)
        e = c->len;

    for (i = s; i < e; ++i) {
        word_t val = 0;
        fprintf(outfile, "\n0x%.4x:", i);
        fprintf(outfile, "valid:%.2x,dirty:%.2x\n", c->contents[i].valid, c->contents[i].dirty);
        for (j = 0; j < CACHE_B; j+= 4) {
            memcpy(&val, c->contents[i].block+j, 4);
            fprintf(outfile, " %.8x", val);
        }
        fprintf(outfile, "\n");
    }
}






