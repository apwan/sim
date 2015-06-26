#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <unistd.h>

#include "isa.h"
#include "bus.h"


/* memory */

MemRec::MemRec():len(0),contents(NULL),ca(NULL),bus(NULL){}
MemRec::MemRec(int l){
    len = ((l+BPL-1)/BPL)*BPL;
    contents = new byte_t[len];
    ca = NULL;
    bus = NULL;
}
MemRec::MemRec(const MemRec &m){
    len = m.len;
    contents = new byte_t[len];
    memcpy(contents, m.contents, len);
    ca = NULL;
    bus = NULL;
}
MemRec::~MemRec(){
    printf("mem releasing\n");
    delete []contents;
    if(ca){
        delete ca;
    }

}
int MemRec::getLen(){
    return len;
}

MemRec &MemRec::operator=(const MemRec &m){
    if(bus) bus->remove(this);
    bus = NULL;
    if(ca) delete ca;
    ca = NULL;
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
    printf("clear mem\n");
    memset(contents, 0, len);
    if(ca){
        ca->clear();
    }
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
    if(bus && bus->isShared(pos)){
        int j;
        for(j=0;j<bus->shared_count;++j){
            if(bus->mem_map[j]==this)
                break;
        }
        if(bus->recs[j][pos/bus->cB] & REC_WRITE){
            return ca->setByte(pos,val);
        }else{
            bus->signal_update(pos/bus->cB,j);
            ca->setByte(pos,val);
            bus->recs[j][pos/bus->cB] = REC_WRITE|REC_READ;
            return TRUE;
        }

    }else
        return ca? ca->setByte(pos,val): setByte(pos,val);
}

bool_t MemRec::setCacheWord(word_t pos, word_t val){
    if(bus && bus->isShared(pos)){

        int j;
        for(j=0;j<bus->shared_count;++j){
            if(bus->mem_map[j]==this)
                break;
        }
        if(bus->recs[j][pos/bus->cB] & REC_WRITE){
            return ca->setByte(pos,val);
        }else{
            bus->signal_update(pos/bus->cB,j);
            ca->setWord(pos,val);
            bus->recs[j][pos/bus->cB] = REC_WRITE|REC_READ;
            return TRUE;
        }

    }else{
        printf("set cache word, 0x%x\n",ca);
        if(ca){
            return ca->setWord(pos,val);
        }else{
            return setWord(pos,val);
        }

    }
}

bool_t MemRec::swapCacheWord(word_t pos, word_t *dest){
    if(bus && bus->isShared(pos)){
        int j;
        for(j=0;j<bus->shared_count;++j){
            if(bus->mem_map[j]==this)
                break;
        }
        if(bus->recs[j][pos/bus->cB] & REC_WRITE){
            word_t val = *dest;
            return ca->getWord(pos,dest) && ca->setByte(pos,val);
        }else{
            bus->signal_update(pos/bus->cB,j);
            int val;
            ca->getWord(pos,dest);
            ca->setWord(pos,val);
            bus->recs[j][pos/bus->cB] = REC_WRITE|REC_READ;
            return TRUE;
        }

    }else if(ca){
        word_t val = *dest;
        return ca->getWord(pos,dest) && ca->setWord(pos,val);
    }else{
        word_t val = *dest;
        return getWord(pos, dest) && setWord(pos, val);

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
    if (reporter) {

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
            (*(void_func_dint_dcharptr)reporter)(line_no++, addr, hexcode, line);

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
    return TRUE;
}

void MemRec::setReporter(void* func_ptr){
    reporter = func_ptr;
    printf("set MemRec Reporter to 0x%x\n", reporter);
}


bool_t MemRec::share(BusController *b){
    if(bus){
        return FALSE;
    }
    if(b->add(this)){
        bus = b;
        printf("Share mem success\n");
    }else{
        bus = NULL;
        printf("Share mem failed\n");
    }

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

        if(reporter){
            (*(void_func_dint)reporter)(id,val);
        }
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

void RegRec::setReporter(void *func_ptr){

    reporter = func_ptr;
    printf("set RegRec Reporter to 0x%x\n", reporter);


}



/*  CACHE  */
CacheRec::CacheRec(mem_t target, int b, int s, int E, int t):
    cb(b),cB(1<<b),cs(s),cS(1<<s),cE(E),ct(t),nlines(E<<s){
    len = target->getLen();
    tm = target;
    contents = new byte_t[nlines<<b];
    valid = new bool_t[nlines];
    dirty = new bool_t[nlines];
    tags = new byte_t[nlines];
    clear();
#ifdef CHECK_CACHE
    checker = copy_mem(target);
#else
    checker = NULL;
#endif
}

CacheRec::~CacheRec(){

    delete []contents;
    contents = NULL; // prevent err in MemRec::~MemRec()
    delete []valid;
    delete []dirty;
    delete []tags;
#ifdef CHECK_CACHE
    if(checker){
        delete checker;
    }
#endif
}

void CacheRec::clear(bool_t wb){
    if(wb){ /* write back */
        int i;
        if(tm->bus){
            int id;
            for(id=tm->bus->shared_count-1;id>=0;--id){
                if(tm->bus->mem_map[id] == tm)
                    break;
            }
            for(i=0;i<nlines;++i){
                if(valid[i] && dirty[i]){
                    int pos = makePos(tags[i], i/cE);
                    commit(i);
                    tm->bus->syncBlock(pos/cB,id);
                }

            }

        }else{
            for(i=0;i<nlines;++i){
                commit(i);
            }
        }
        printf("commit all cache\n");
    }
    memset(valid,0,nlines*sizeof(bool_t));
    memset(dirty,0,nlines*sizeof(bool_t));
    memset(tags,0,nlines*sizeof(byte_t));
    memset(contents,0,nlines<<cb);
}

bool_t CacheRec::commit(word_t i){ // check i?
    if(valid[i] && dirty[i]){
        word_t pos = makePos(tags[i],i/cE);
        byte_t *cur = contents + i*cB;
        //byte_t *end = cur + cB;
#ifdef CHECK_CACHE
        if(checker){
            checker->dump(stdout, pos, cB);
        }
#endif
        word_t val;
        memcpy(tm->contents+pos,cur,cB);
        /*
        for(;cur<end;cur+=4,pos+=4){
            val = *(word_t*)cur;
            if(! tm->setWord(pos,val))
                printf("write back fail.\nCache line: %d, Mem pos: %d\n",i,pos);
        }
    */
        dirty[i] = FALSE;
        //dump(stdout,i,1);

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
            if(valid[i]==TRUE && tags[i]==tag)
                return i;
        }
        if(forced){
            i = spareLine(setIndex);
            if(i<0){// no spare line
                i = setIndex*cE + (rand()%cE);// evict random one
                printf("forced to evict\n");
                commit(i); // write back
                valid[i] == FALSE;
            }
            retrieve(tag,i);
            return i;
        }
    }
    return -1; // invalid pos
}

word_t CacheRec::spareLine(int setIndex){
    int i = setIndex*cE;
    int end = i + cE;
    for(;i<end;++i){
        if(valid[i] == FALSE)
            return i;
    }
    return -1;

}

bool_t CacheRec::retrieve(byte_t tag, word_t i){
    word_t pos = makePos(tag, i/cE);
    word_t end = pos+cB;
    if(end>len) return FALSE;

    void *cur = (void*)(contents + (i<<cb));
    word_t val;
    printf("retrieve cache from pos %.4x\n", pos);
    if(tm->bus && tm->bus->isShared(pos)){
        int j;
        for(j=0;j<tm->bus->shared_count;++j){
            if(tm->bus->mem_map[j]==tm)
                break;
        }
        tm->bus->signal_allocate(pos/cB,j);
        memcpy(cur, (void*)(tm->contents+pos), cB);
        tm->bus->recs[j][pos/cB] ^= REC_EXEC;

    }else{
        printf("new retrieve\n");
        memcpy(cur, (void*)(tm->contents+pos),cB);
        /*
        for(;pos<end;cur+=4,pos+=4){
            if(! tm->getWord(pos,&val))
                printf("cache retrieve fail.\nCache line: %.4x, Mem pos: %.8x\n",i,pos);
            else
                *(word_t *)cur = val;
        }
        */
    }

    tags[i] = tag;
    valid[i] = TRUE;
    dirty[i] = FALSE;
    //dump(stdout,i,1);
    return TRUE;
}

bool_t CacheRec::getByte(word_t pos, byte_t *dest){
    int offset;
    word_t i = findLine(pos, TRUE, &offset);
    if(i>=0){
        *dest = contents[(i<<cb)+offset];
#ifdef CHECK_CACHE
        if(checker){
            byte_t val;
            checker->getByte(pos, &val);
            if( val != *dest){
                printf("CHECK failed at pos 0x%.4x: %.2x not %.2x\n", pos, val, *dest);
                dump(stdout,0,nlines);
                //*dest = val;
                //return FALSE;
            }
        }
#endif
        return TRUE;
    }else{
        return FALSE; // invalid pos
    }


}

bool_t CacheRec::getWord(word_t pos, word_t *dest){
    int offset = 0;// may jump across lines
    if(pos+4>len) return FALSE;
    word_t i = findLine(pos, TRUE, &offset);
    int d = cB-offset;
    byte_t *start = contents+(i<<cb)+offset;
    if(d<4){// exceed
        memcpy((byte_t*)dest,start,d);
        //dump(stdout, i, 1);
        i = findLine(pos+d, TRUE);
        if(i<0){
            printf("invalid pos\n");
            return FALSE;
        }
        memcpy(((byte_t*)dest)+d, contents+(i<<cb), 4-d);
        //dump(stdout, i, 1);
        //printf("get cross-line word %.8x at pos %.4x\n", *dest, pos);
    }else{
        *dest = *(word_t *)start;
    }
#ifdef CHECK_CACHE
    if(checker){
        int val;
        checker->getWord(pos, &val);
        if(*dest != val){
            printf("CHECK failed at pos 0x%.4x: %.8x not %.8x\n", pos, val, *dest);
            dump(stdout,0,nlines);
            //*dest = val;
            //return FALSE;

        }
    }
#endif
    return TRUE;
}

bool_t CacheRec::setByte(word_t pos, byte_t val){
    int offset;
    word_t i = findLine(pos, TRUE, &offset);
    if(i>=0){
        contents[(i<<cb)+offset] = val;
        dirty[i] = TRUE;
#ifdef CHECK_CACHE
        if(checker){
            checker->setByte(pos, val);
        }
#endif
    }else{
        return FALSE; // invalid pos
    }
}

bool_t CacheRec::setWord(word_t pos, word_t val){
    int offset;// may jump across lines
    if(pos+4>len) return FALSE;
    word_t i = findLine(pos, TRUE, &offset);
    int d = cB-offset;
    byte_t *start = contents+((i<<cb)+offset);
    if(d<4){// exceed
        memcpy(start,(byte_t*)&val,d);
        dirty[i] = TRUE;
        //dump(stdout, i, 1);
        i = findLine(pos+d, TRUE);
        if(i<0){
            printf("invalid pos\n");
            return FALSE;
        }
        memcpy(contents+(i<<cb), ((byte_t*)&val)+d, 4-d);
        dirty[i] = TRUE;
        //dump(stdout, i, 1);
        //printf("set cross-line word %d at pos %d\n", val, pos);

    }else{
        *(word_t *)start = val;
        dirty[i] = TRUE;
    }
#ifdef CHECK_CACHE
    if(checker){
        checker->setWord(pos, val);
    }
#endif


    return TRUE;
}



word_t CacheRec::makePos(byte_t tag, int setIndex, int offset){
#ifdef CHECK_CACHE
        tag &= CACHE_MASK(ct);
        setIndex &= CACHE_MASK(cs);
        offset &= CACHE_MASK(cb);
#endif
    return (((tag<<cs)|setIndex)<<cb)|offset;
}

bool_t CacheRec::parsePos(word_t pos, int *indexPtr, byte_t *tagPtr, int *offsetPtr){
    if(pos<0 || pos>=len){
#ifdef CHECK_CACHE
        *indexPtr = 0;
        *tagPtr = 0;
#endif
        return FALSE;
    }
    if(offsetPtr){
        *offsetPtr = CACHE_MASK(cb) & pos;
    }
    pos = ((unsigned int)pos)>>cb;
    *indexPtr = CACHE_MASK(cs) & pos;
    pos = ((unsigned int)pos)>>cs;
    *tagPtr = CACHE_MASK(ct) & pos;
    return TRUE;

}


void CacheRec::dump(FILE *outfile, word_t s, int l){
    if(s<0 || s>=nlines){
        printf("INVALID DUMP!\n");
        return;
    }
    word_t end = s+l;
    if(end>nlines)
        end = nlines;
    int i,j;
    byte_t *cur = contents;
    for(i=s;i<end;++i){
        fprintf(outfile, "\n0x%.4x: ", i);
        fprintf(outfile, "valid %.2x, dirty %.2x\n", valid[i], dirty[i]);
        for (j = 0; j < cB; ++j) {
            if(! (j%4)) fprintf(outfile, " ");
            fprintf(outfile, "%.2x", *cur);
            cur += 1;
        }
        fprintf(outfile, "\n");
    }
}

bool_t diff_mem(mem_t oldm, mem_t newm, FILE *outfile)
{
    word_t pos;
    int len = oldm->len;
    bool_t diff = FALSE;
    if (newm->len < len)
    len = newm->len;

    // write back
    if(oldm->ca) oldm->ca->clear(TRUE);
    if(newm->ca) newm->ca->clear(TRUE);

    //remove bus
    /*
    if(oldm->bus) oldm->bus->remove(oldm);
    if(newm->bus) newm->bus->remove(newm);
    */


    for (pos = 0; (!diff || outfile) && pos+4 <= len; pos += 4) {
        word_t ov = 0;  word_t nv = 0;
        oldm->getWord(pos, &ov);
        newm->getWord(pos, &nv);
        if (nv != ov) {
            diff = TRUE;
            if (outfile)
                fprintf(outfile, "0x%.4x:\t0x%.8x\t0x%.8x\n", pos, ov, nv);
        }
    }
    //return *oldm->m != *newm->m;
    return diff;
}


bool_t diff_reg(mem_t oldr, mem_t newr, FILE *outfile)
{
    word_t pos;
    int len = oldr->len;
    bool_t diff = FALSE;
    if (newr->len < len)
    len = newr->len;
    //write back
    if(oldr->ca) oldr->ca->clear(TRUE);
    if(newr->ca) newr->ca->clear(TRUE);

    for (pos = 0; (!diff || outfile) && pos < len; pos += 4) {
        word_t ov = 0;
        word_t nv = 0;
    oldr->getWord(pos, &ov);
    newr->getWord(pos, &nv);
    if (nv != ov) {
        diff = TRUE;
        if (outfile)
        fprintf(outfile, "%s:\t0x%.8x\t0x%.8x\n",
            reg_table[pos/4].name, ov, nv);
    }
    }
    return diff;
}


BusController::BusController(int max_share_addr, int blockSize){
    cB = blockSize;
    bn = max_share_addr/cB;
    locked = new bool_t[bn];
    memset(locked, 0, bn);
    max_addr = bn * blockSize;
    shared_count = 0;
    pthread_mutex_init(&mu,NULL);
    printf("bus shared max_addr: 0x%.4x\n", max_addr);
}

BusController::~BusController(){
    int i;
    for(i=0;i<shared_count;++i){
        delete [](recs[i]);
        recs[i] = NULL;
    }
    delete []locked;
    pthread_mutex_destroy(&mu);
    printf("destroy bus: %d\n", shared_count);

}

bool_t BusController::lockRecs(int blockNum){// do not check
    while(TRUE){
        while(locked[blockNum]){
            usleep(1);
        }
        pthread_mutex_lock(&mu);
        if(!locked[blockNum]){
            locked[blockNum] = TRUE;
            pthread_mutex_unlock(&mu);
            break;
        }else{
            // another thread has locked the entry
            pthread_mutex_unlock(&mu);
        }
    }
    return TRUE;

}

bool_t BusController::unlockRecs(int blockNum){// do not check
    if(!locked[blockNum])
        return FALSE;

    pthread_mutex_lock(&mu);
    locked[blockNum] = FALSE;
    pthread_mutex_unlock(&mu);
    return TRUE;
}

bool_t BusController::getExec(int blockNum, int j){
    if(blockNum>=bn || recs[j][blockNum]==REC_EXEC)
        return FALSE;
    int id;
    while(TRUE){
        for(id=shared_count-1;id>=0;--id){
            if(recs[id][blockNum] & REC_EXEC){
                if(recs[id][blockNum == REC_EXEC] && recs[j][blockNum] == REC_NONE){
                    continue; // here multi REC_EXEC are allowed
                }else{
                    break;
                }
            }
        }
        if(id>=0){
            usleep(10);
            continue;
        }
        lockRecs(blockNum);
        for(id=shared_count-1;id>=0;--id){
            if(recs[id][blockNum] & REC_EXEC){
                if(recs[id][blockNum == REC_EXEC] && recs[j][blockNum] == REC_NONE){
                    continue; // here multi REC_EXEC are allowed
                }else{
                    break;
                }
            }
        }
        if(id>=0){ // aother thread get REC_EXEC first
            unlockRecs(blockNum);
            continue;
        }else{
            recs[j][blockNum] |= REC_EXEC;
            unlockRecs(blockNum);
            return TRUE;
        }
    }
}



bool_t BusController::isShared(word_t pos){
    return pos<max_addr;
}

bool_t BusController::isReadOnly(word_t pos){
    return pos<ro_addr;
}




bool_t BusController::add(MemRec *m){
    if(m->ca == NULL){
        return FALSE;
    }
    printf("Add to Bus!\n");
    recs[shared_count] = new byte_t[max_addr/cB];
    pthread_mutex_lock(&mu);
    mem_map[shared_count] = m;
    shared_count++;
    pthread_mutex_unlock(&mu);
    if(shared_count>1){
        // copy whole memory
        memcpy(m->contents,mem_map[0]->contents,max_addr);
        printf("copy shared mem!\n");
    }
    printf("successful added to bus!\n");
    return TRUE;
}

bool_t BusController::remove(MemRec *m){
    int i;
    pthread_mutex_lock(&mu);
    for(i=0;i<shared_count;++i){
        if(m == mem_map[i]){
            break;
        }
    }
    if(i>=shared_count){
        pthread_mutex_unlock(&mu);
        return FALSE;
    }
    byte_t *tmp = recs[i];

    int j;
    for(j=i;j<shared_count-1;++j){
        mem_map[j] = mem_map[j+1];
        recs[j] = recs[j+1];
    }
    mem_map[shared_count] = NULL;
    recs[shared_count] = NULL;
    shared_count--;
    pthread_mutex_unlock(&mu);
    m->bus = NULL;
    printf("removed from bus!\n");
    delete [] tmp;
    return TRUE;
}

// called after signal_allocate before release the write permission
bool_t BusController::syncBlock(word_t blockNum, int id){
    if(id<0 || blockNum >= bn)
        return FALSE;

    int i; word_t pos = blockNum * cB;
    void *start = (void*)(mem_map[id]->contents + pos);
    printf("start sync block %d\n", blockNum);
    for(i=0;i<shared_count;++i){
        if(i==id) continue;
        memcpy(mem_map[i]->contents+pos,start,cB);
    }

}

bool_t BusController::signal_update(int blockNum, int j){
    getExec(blockNum, j);
    printf("signal_update!\n");
    int i;
    for(i=0;i<shared_count;++i){
        if(i==j) continue;
        mem_map[i]->ca->invalidate(blockNum*cB);
    }
    lockRecs(blockNum);
    for(i=0;i<shared_count;++i){
        if(i==j) recs[i][blockNum] |= REC_WRITE;
        else recs[i][blockNum] = REC_NONE;
    }
    unlockRecs(blockNum);
    return TRUE;
}

bool_t BusController::signal_allocate(int blockNum, int j){
    getExec(blockNum, j);
    printf("signal allocate\n");
    int id;
    for(id=shared_count-1;id>=0;--id){
        if(recs[id][blockNum] & REC_WRITE)
            break;
    }
    if(id>=0){ // dirty cache
        int i = mem_map[id]->ca->findLine(blockNum*cB);
        mem_map[id]->ca->commit(i);
        syncBlock(blockNum, id);
        recs[id][blockNum] = REC_READ;
    }
    recs[j][blockNum] |= REC_READ;
    return TRUE;
}








