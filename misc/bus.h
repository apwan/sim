#ifndef BUS_H
#define BUS_H

#include "isa.h"
#include "pthread.h"

#define MAX_SHARED 8
#define REC_
#define REC_READ (0x4)
#define REC_WRITE (0x2)
#define REC_EXEC (0x1)
#define REC_NONE (0x0)

class BusController{
public:
    BusController(int max_share_addr, int blockSize);
    ~BusController();

    bool_t isShared(word_t pos); // 0 ~ max_addr-1
    bool_t isReadOnly(word_t pos); // 0 ~ ro_addr-1

    bool_t add(MemRec *m);
    bool_t remove(MemRec *m);

    // lazy implementation of sharing
    bool_t syncBlock(word_t blockNum, int id);



    bool_t signal_update(int blockNum, int j);
    bool_t signal_allocate(int blockNum, int j); // for REC_READ

    friend class MemRec;
    friend class CacheRec;
private:
    int max_addr; // largest shared address + 1
    int ro_addr;  // largest read_only address + 1, larger than the code section
    int bn; // total number of blocks
    int cB; // block size
    int shared_count = 0; // <= MAX_SHARED;
    MemRec *mem_map[MAX_SHARED];
    byte_t *recs[MAX_SHARED];// records
    bool_t *locked = NULL;
    pthread_mutex_t mu;

    bool_t lockRecs(int blockNum);
    bool_t unlockRecs(int blockNum);
    bool_t getExec(int blockNum, int j);



};






#endif // BUS_H

