#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>

#include "isa.h"

/* memory */

MemRec::MemRec():len(0),contents(NULL),ca(NULL){}
MemRec::MemRec(int l):len(((l+BPL-1)/BPL)*BPL),
    contents(new byte_t[l]),ca(NULL){
}
MemRec::MemRec(const MemRec &m):len(m.len),contents(new byte_t[len]){
    memcpy(contents, m.contents, len);
}
MemRec::~MemRec(){
    delete []contents;
    if(ca){
        delete ca;
    }
}
int MemRec::getLen(){
    return len;
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

bool_t MemRec::getCacheByte(word_t pos, byte_t *dest){
    return ca? ca->getByte(pos,dest): getByte(pos,dest);
}

bool_t MemRec::getCacheWord(word_t pos, word_t *dest){
    return ca? ca->getWord(pos,dest):getWord(pos,dest);
}

bool_t MemRec::setCacheByte(word_t pos, byte_t val){
    return ca? ca->setByte(pos,val): setByte(pos,val);
}

bool_t MemRec::setCacheWord(word_t pos, word_t val){
    return ca? ca->setWord(pos,val): setWord(pos,val);
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
        setByte(bytepos++, byte);


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

bool_t MemRec::useCache(int b, int s, int E, int t){
    if(ca){
        delete ca;
    }
    ca = new CacheRec(this,b,s,E,t);
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
CacheRec::CacheRec(mem_t target, int b, int s, int E, int t):
    MemRec(),tm(target),
    cb(b),cB(1<<b),cs(s),cS(1<<s),cE(E),ct(t),nlines(E<<s),
    valid(new bool_t[nlines]),dirty(new bool_t[nlines]),tags(new byte_t[nlines]){
    len = target->getLen();
    contents = new byte_t[nlines<<b];
}

CacheRec::~CacheRec(){
    delete []contents;
    delete []valid;
    delete []dirty;
    delete []tags;
}

void CacheRec::clear(bool_t wb){
    if(wb){ /* write back */
        int i;
        for(i=0;i<nlines;++i){
            commit(i);
        }
    }
    memset(valid,0,nlines);
    memset(dirty,0,nlines);
    memset(tags,0,nlines);
    memset(contents,0,nlines<<cb);
}

bool_t CacheRec::commit(word_t i){
    if(valid[i] && dirty[i]){
        word_t pos = makePos(tags[i],i/cE);
        byte_t *cur = contents + i*cB;
        byte_t *end = cur + cB;
        for(;cur<end;cur+=4,pos+=4){/* not shared */
            if(! tm->setWord(pos,*(word_t *)cur))
                printf("write back fail.\nCache line: %d, Mem pos: %d\n",i,pos);
        }
        dirty[i] = FALSE;
        return TRUE;
    }else{
        return FALSE;
    }
}

word_t CacheRec::invalidate(word_t pos, bool_t wb){
    int i;
    if((i=findLine(pos))<0)
        return -1;
    if(wb && dirty[i]){/* write back */
        commit(i);
    }
    valid[i] = FALSE;
    return i;
}

word_t CacheRec::findLine(word_t pos, bool_t forced,int *offsetPtr){
    int setIndex; byte_t tag;
    if(parsePos(pos,&setIndex,&tag,offsetPtr)){
        int i = setIndex*cE;
        int end = i + cE;
        for(;i<end;++i){
            if(valid[i] && tags[i]==tag)
                return i;
        }
        if(forced){
            if((i=spareLine(setIndex))<0){// no spare line
                i = setIndex*cE + rand()%cE;// evict random one
                commit(i); // write back
            }
            retrieve(tag,i);
            return i;
        }
    }
    return -1;
}

word_t CacheRec::spareLine(int setIndex){
    int i = setIndex*cE;
    int end = i + cE;
    for(;i<end;++i){
        if(!valid[i])
            return i;
    }
    return -1;

}

bool_t CacheRec::retrieve(byte_t tag, word_t i){
    word_t pos = makePos(tag, i/cE);
    word_t end = pos+cB;
    if(end>len) return FALSE;

    byte_t *cur = contents + i*cB;
    for(;pos<end;cur+=4,pos+=4){/* not shared */
        if(! tm->getWord(pos,(word_t *)cur))
            printf("cache retrieve fail.\nCache line: %d, Mem pos: %d\n",i,pos);
    }
    tags[i] = tag;
    valid[i] = TRUE;
    dirty[i] = FALSE;
    return TRUE;
}

bool_t CacheRec::getByte(word_t pos, byte_t *dest){
    int offset;
    word_t i = findLine(pos, TRUE, &offset);
    if(i>=0){
        *dest = contents[(i<<cb)+offset];
        return TRUE;
    }else{
        return FALSE; // invalid pos
    }

}

bool_t CacheRec::getWord(word_t pos, word_t *dest){
    int offset;// may jump across lines
    if(pos+4>len) return FALSE;
    word_t i = findLine(pos, TRUE, &offset);
    int d = cB-offset;
    byte_t *start = contents+(i<<cb)+offset;
    if(d<4){// exceed
        memcpy(dest,start,d);
        i = findLine(pos+d, TRUE);
        memcpy(((byte_t*)dest)+d, contents+(i<<cb), 4-d);
        printf("cross-line word %d at pos %d\n", *dest, pos);
    }else{
        *dest = *(word_t *)start;
    }
    return TRUE;
}

bool_t CacheRec::setByte(word_t pos, byte_t val){
    int offset;
    word_t i = findLine(pos, TRUE, &offset);
    if(i>=0){
        contents[(i<<cb)+offset] = val;
        dirty[i] = TRUE;
    }else{
        return FALSE; // invalid pos
    }
}

bool_t CacheRec::setWord(word_t pos, word_t val){
    int offset;// may jump across lines
    if(pos+4>len) return FALSE;
    word_t i = findLine(pos, TRUE, &offset);
    int d = cB-offset;
    byte_t *start = contents+(i<<cb)+offset;
    if(d<4){// exceed
        memcpy(start,&val,d);
        i = findLine(pos+d, TRUE);
        memcpy(contents+(i<<cb), ((byte_t*)&val)+d, 4-d);
        printf("cross-line word %d at pos %d\n", val, pos);
    }else{
        *(word_t *)start = val;
    }
    return TRUE;
}

word_t CacheRec::makePos(byte_t tag, int setIndex, int offset){
#ifdef CHECK_CACHE_POS
        tag &= CACHE_MASK(ct);
        setIndex &= CACHE_MASK(cs);
        offset &= CACHE_MASK(offset);
#endif
    return (((tag<<cs)|setIndex)<<cb)|offset;
}

bool_t CacheRec::parsePos(word_t pos, int *indexPtr, byte_t *tagPtr, int *offsetPtr){
    if(pos<0 || pos>=len){
        return FALSE;
    }
    if(offsetPtr){
        *offsetPtr = CACHE_MASK(cb) & pos;
    }
    pos = ((unsigned int)pos)>>cb;
    *indexPtr = CACHE_MASK(cs) & pos;
    pos = pos>>cs;
    *tagPtr = CACHE_MASK(ct) & pos;
    return TRUE;

}





void CacheRec::dump(FILE *outfile, word_t s, int l){
    word_t end = s+l;
    if(end>nlines)
        end = nlines;
    int i,j;
    for(i=s;i<end;++i){
        word_t val = 0;
        byte_t *start = contents+(i<<cb);
        fprintf(outfile, "\n0x%.4x: ", i);
        fprintf(outfile, "valid %.2x, dirty %.2x\n", valid[i], dirty[i]);
        for (j = 0; j < cB; j+= 4) {
            memcpy(&val, start+j, 4);
            fprintf(outfile, " %.8x", val);
        }
        fprintf(outfile, "\n");
    }
}





