#ifndef ISA_H
#define ISA_H
/* Instruction Set definition for Y86 Architecture */
/* Revisions:
   2009-03-11:
       Changed RNONE to be 0xF
       Changed J_XX and jump_t to C_XX and cond_t; take_branch to cond_holds
       Expanded RRMOVL to include conditional moves
*/




#ifdef __cplusplus
#define CPPBEGIN extern "C" {
#define CPPEND }
#else
#define CPPBEGIN
#define CPPEND
#endif


extern int gui_mode;

/**************** Registers *************************/

/* REG_NONE is a special one to indicate no register */
typedef enum { REG_EAX, REG_ECX, REG_EDX, REG_EBX,
	       REG_ESP, REG_EBP, REG_ESI, REG_EDI, REG_NONE=0xF, REG_ERR } reg_id_t;

/* Find register ID given its name */
reg_id_t find_register(char *name);
/* Return name of register given its ID */
char *reg_name(int id); // change from reg_id_t -> int

/**************** Instruction Encoding **************/

/* Different argument types */
typedef enum { R_ARG, M_ARG, I_ARG, NO_ARG } arg_t;

/* Different instruction types */
typedef enum { I_HALT, I_NOP, I_RRMOVL, I_IRMOVL, I_RMMOVL, I_MRMOVL,
	       I_ALU, I_JMP, I_CALL, I_RET, I_PUSHL, I_POPL,
           I_IADDL, I_LEAVE, I_POP2, I_RMSWAP } itype_t;

/* Different ALU operations */
typedef enum { A_ADD, A_SUB, A_AND, A_XOR, A_NONE } alu_t;

/* Default function code */
typedef enum { F_NONE } fun_t;

/* Return name of operation given its ID */
char op_name(alu_t op);

/* Different Jump conditions */
typedef enum { C_YES, C_LE, C_L, C_E, C_NE, C_GE, C_G } cond_t;

/* Pack itype and function into single byte */
#define HPACK(hi,lo) ((((hi)&0xF)<<4)|((lo)&0xF))

/* Unpack byte */
#define HI4(byte) (((byte)>>4)&0xF)
#define LO4(byte) ((byte)&0xF)

/* Get the opcode out of one byte instruction field */
#define GET_ICODE(instr) HI4(instr)

/* Get the ALU/JMP function out of one byte instruction field */
#define GET_FUN(instr) LO4(instr)

/* Return name of instruction given it's byte encoding */
char *iname(int instr);

/**************** Truth Values **************/
//typedef enum { FALSE, TRUE } bool_t;
typedef bool bool_t;
#define FALSE false
#define TRUE true
#define BPL 32

/* Table used to encode information about instructions */
typedef struct {
  char *name;
  unsigned char code; /* Byte code for instruction+op */
  int bytes;
  arg_t arg1;
  int arg1pos;
  int arg1hi;  /* 0/1 for register argument, # bytes for allocation */
  arg_t arg2;
  int arg2pos;
  int arg2hi;  /* 0/1 */
} instr_t, *instr_ptr;

instr_ptr find_instr(char *name);

/* Return invalid instruction for error handling purposes */
instr_ptr bad_instr();

/***********  Implementation of Memory *****************/
typedef unsigned char byte_t;
typedef int word_t;

/* number of address bits */

CPPBEGIN

/* Cache */

#define CACHE_m 16 // virtual address bits

#define CACHE_b 4 // block bits
#define CACHE_B (1<<CACHE_b) // block size
#define CACHE_s 6 // set index bits
#define CACHE_S (1<<CACHE_s) // number of sets
#define CACHE_E 16 // lines per set
#define CACHE_t 6 // tag bits
#define CACHE_MASK(s) (~((-1)<<(s)))
#define MASK(s,t) ((-1<<(t))^(-1<<(s)))

//forward declaration
class CacheRec;

class MemRec {
public:
    MemRec();
    MemRec(int l);
    MemRec(const MemRec&);
    ~MemRec();

    int getLen();
    virtual bool_t getByte(word_t pos, byte_t *dest);
    virtual bool_t getWord(word_t pos, word_t *dest);
    virtual bool_t setByte(word_t pos, byte_t val);
    virtual bool_t setWord(word_t pos, word_t val);
    virtual void dump(FILE *outfile, word_t pos, int l);
    virtual bool_t clear();
    int load(FILE *infile, int report_error);

    bool_t getCacheByte(word_t pos, byte_t *dest);
    bool_t getCacheWord(word_t pos, word_t *dest);
    bool_t setCacheByte(word_t pos, byte_t val);
    bool_t setCacheWord(word_t pos, word_t val);

    MemRec &operator=(const MemRec &);
    bool_t operator!=(const MemRec &);

    /* Print the differences between two memories */
    friend bool_t diff_mem(MemRec* oldm, MemRec* newm, FILE *outfile);
    /* Print the differences between two regs */
    friend bool_t diff_reg(MemRec* oldr, MemRec* newr, FILE *outfile);
protected:
    int len;
    //word_t maxaddr;
    byte_t *contents;
    CacheRec *ca; //cache
    bool_t useCache(int b=CACHE_b, int s=CACHE_s, int E=CACHE_E, int t=CACHE_t);
};


typedef struct{
    int len;
    byte_t *priv;
    int max_priv;
    byte_t *shrd;
    int fd;
} *mix_mem_rec, *mix_mem_t;




/* How big should the memory be? */
#ifdef BIG_MEM
#define MEM_SIZE (1<<16)
#else
#define MEM_SIZE (1<<13)
#endif

#define get_byte_val(MEM,pos,dest) (MEM)->getCacheByte(pos,dest)
#define get_word_val(MEM,pos,dest) (MEM)->getCacheWord(pos,dest)
#define set_byte_val(MEM,pos,val) (MEM)->setCacheByte(pos,val)
#define set_word_val(MEM,pos,val) (MEM)->setCacheWord(pos,val)
#define dump_memory(outfile, MEM, pos, len) (MEM)->dump(outfile,pos,len)
#define clear_mem(MEM) (MEM)->clear()
#define copy_mem(MEM) new MemRec(*MEM)
#define init_mem(len) new MemRec(len)
#define free_mem(MEM) delete (MEM)
#define load_mem(MEM,infile,report_err) (MEM)->load(infile,report_err)

#define init_reg() new RegRec()
#define free_reg(REG) delete (REG)
#define copy_reg(REG) new RegRec(*((RegRec*)REG))
#define get_reg_val(REG,id) ((RegRec*)REG)->getVal(id)
#define set_reg_val(REG,id,val) ((RegRec*)REG)->setVal(id,val)
#define dump_reg(outfile, REG) (REG)->dump(outfile)

typedef MemRec* mem_t;



typedef struct {
    byte_t valid:1;
    byte_t dirty:1;
    byte_t tag:6;
    byte_t block[CACHE_B];
} cache_line;


class CacheRec: public MemRec {
public:
    CacheRec(mem_t target, int b=CACHE_b, int s=CACHE_s, int E=CACHE_E, int t=CACHE_t);
    ~CacheRec();

    word_t findLine(word_t pos, bool_t forced=FALSE, int *offsetPtr=NULL);
    word_t spareLine(int setIndex);
    bool_t commit(word_t i);
    word_t invalidate(word_t pos, bool_t wb = FALSE);
    bool_t retrieve(byte_t tag, word_t i);
    // override
    bool_t getByte(word_t pos, byte_t *dest);
    bool_t getWord(word_t pos, word_t *dest);
    bool_t setByte(word_t pos, byte_t val);
    bool_t setWord(word_t pos, word_t val);
    void clear(bool_t wb=FALSE);/*wb: writeBack? */
    void dump(FILE *outfile, word_t s, int l);

private:
    mem_t tm; // target mem
    bool_t *valid, *dirty;
    byte_t *tags;
    int cb,cB,cs,cS,cE,ct;// cache params
    int nlines;
    word_t makePos(byte_t tag, int setIndex, int offset = 0);
    bool_t parsePos(word_t pos, int *indexPtr, byte_t *tagPtr, int *offsetPtr=NULL);

};




/********** Implementation of Register File *************/


/* Print the differences between two register files */


class RegRec: public MemRec{
public:
    RegRec(): MemRec(32){}
    RegRec(const RegRec& r):MemRec(r){}

    word_t getVal(int id);
    void setVal(int id, word_t val);
    void dump(FILE *outfile);

};

int reg_valid(int id);
const struct {
    char *name;
    int id;// int -> reg_id_t
} reg_table[REG_ERR+1] =
{
    {"%eax",   REG_EAX},
    {"%ecx",   REG_ECX},
    {"%edx",   REG_EDX},
    {"%ebx",   REG_EBX},
    {"%esp",   REG_ESP},
    {"%ebp",   REG_EBP},
    {"%esi",   REG_ESI},
    {"%edi",   REG_EDI},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_ERR},
    {"----",  REG_NONE},
    {"----",  REG_ERR}
};


CPPEND

/* ****************  ALU Function **********************/

/* Compute ALU operation */

const struct {
    char symbol;
    int id;
} alu_table[A_NONE+1] =
{
    {'+',   A_ADD},
    {'-',   A_SUB},
    {'&',   A_AND},
    {'^',   A_XOR},
    {'?',   A_NONE}
};

#define hex2dig(c) (int)(isdigit((int)c)? (c - '0'): isupper((int)c)? (c - 'A' + 10): (c - 'a' + 10))


word_t compute_alu(alu_t op, word_t arg1, word_t arg2);

typedef unsigned char cc_t;

#define GET_ZF(cc) (((cc) >> 2)&0x1)
#define GET_SF(cc) (((cc) >> 1)&0x1)
#define GET_OF(cc) (((cc) >> 0)&0x1)

#define PACK_CC(z,s,o) (((z)<<2)|((s)<<1)|((o)<<0))

#define DEFAULT_CC PACK_CC(1,0,0)

/* Compute condition code.  */
cc_t compute_cc(alu_t op, word_t arg1, word_t arg2);

/* Generated printed form of condition code */
char *cc_name(cc_t c);

/* **************** Status types *******************/

typedef enum 
 {STAT_BUB, STAT_AOK, STAT_HLT, STAT_ADR, STAT_INS, STAT_PIP } stat_t;

/* Describe Status */
char *stat_name(stat_t e);

/* **************** ISA level implementation *********/

#define new_state(len) new StateRec(len)
#define copy_state(ST) new StateRec(*ST)
#define free_state(ST) delete (ST)

struct StateRec{

  StateRec(int memlen);
  StateRec(const StateRec&);
  StateRec(mem_t mm, mem_t rr, cc_t c);
  ~StateRec();
  StateRec &operator=(const StateRec&);
  stat_t step(FILE *);

  word_t pc;
  mem_t r;
  mem_t m;
  cc_t cc;
};

typedef StateRec state_rec;
typedef StateRec* state_ptr;

bool_t diff_state(state_ptr olds, state_ptr news, FILE *outfile);

/* Determine if condition satisified */
bool_t cond_holds(cc_t cc, cond_t bcond);

/* Execute single instruction.  Return status. */
stat_t step_state(state_ptr s, FILE *error_file);


/************************ Interface Functions *************/

#ifdef HAS_GUI

CPPBEGIN

void report_line(int line_no, int addr, char *hexcode, char *line);
void signal_register_update(int r, int val);

CPPEND

#endif

#endif
