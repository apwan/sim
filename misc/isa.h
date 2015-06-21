#ifndef ISA_H
#define ISA_H
/* Instruction Set definition for Y86 Architecture */
/* Revisions:
   2009-03-11:
       Changed RNONE to be 0xF
       Changed J_XX and jump_t to C_XX and cond_t; take_branch to cond_holds
       Expanded RRMOVL to include conditional moves
*/



#define DEBUG(m) //printf(m)
#define DEBUGP(m,n) //printf(m,n)

#ifdef __cplusplus
#define CPPBEGIN extern "C" {
#define CPPEND }
#else
#define CPPBEGIN
#define CPPEND
#endif


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
#define CACHE_m 16

#define CACHE_b 4
#define CACHE_B (1<<CACHE_b)
#define CACHE_s 6
#define CACHE_S (1<<CACHE_s)
#define CACHE_E 16

#define CACHE_TAG_MASK 0x3F
#define CACHE_MASK(s) (~((-1)<<(s)))
#define MASK(s,t) ((-1<<(t))^(-1<<(s)))






typedef struct {
    byte_t valid:1;
    byte_t dirty:1;
    byte_t tag:6;
    byte_t block[CACHE_B];
} cache_line;


typedef struct{
    int cac_s;
    int cac_E;
    int len; /* number of cache_line */
    cache_line *contents;
} cache_rec, *cache_t;


CPPBEGIN

class MemRec {
public:
    MemRec(int l);
    ~MemRec();

    int getLen();
    bool_t getByte(word_t pos, byte_t *dest);
    bool_t getWord(word_t pos, word_t *dest);
    bool_t setByte(word_t pos, byte_t val);
    bool_t setWord(word_t pos, word_t val);
    void dump(FILE *outfile, word_t pos, int l);
    bool_t clear();

    MemRec &operator=(const MemRec &);
    bool_t operator!=(const MemRec &);
private:
    int len;
    word_t maxaddr;
    byte_t *contents;
};

/* Represent a memory as an array of bytes */
typedef struct {
  int len;
  word_t maxaddr;
  MemRec *m;
  cache_t c;
} mem_rec, *mem_t;

CPPEND

typedef struct{
    int len;
    byte_t *priv;
    int max_priv;
    byte_t *shrd;
    int fd;
} *mix_mem_rec, *mix_mem_t;




CPPBEGIN

/* Create a memory with len bytes */
mem_t init_mem(int len);
void free_mem(mem_t m);

/* Set contents of memory to 0 */
void clear_mem(mem_t m);

/* Make a copy of a memory */
mem_t copy_mem(mem_t oldm);
/* Print the differences between two memories */
bool_t diff_mem(mem_t oldm, mem_t newm, FILE *outfile);

/* How big should the memory be? */
#ifdef BIG_MEM
#define MEM_SIZE (1<<16)
#else
#define MEM_SIZE (1<<13)
#endif

/*** In the following functions, a return value of 1 means success ***/

/* Load memory from .yo file.  Return number of bytes read */
int load_mem(mem_t m, FILE *infile, int report_error);

/* Get byte from memory */
bool_t get_byte_val(mem_t m, word_t pos, byte_t *dest);

/* Get 4 bytes from memory */
bool_t get_word_val(mem_t m, word_t pos, word_t *dest);

/* Set byte in memory */
bool_t set_byte_val(mem_t m, word_t pos, byte_t val);

/* Set 4 bytes in memory */
bool_t set_word_val(mem_t m, word_t pos, word_t val);

/* Print contents of memory */
void dump_memory(FILE *outfile, mem_t m, word_t pos, int cnt);





cache_t init_cache(int s, int E);

void free_cache(cache_t c);
void clear_cache(cache_t c, mem_t m);


word_t search_cache(cache_t c, word_t pos, mem_t m);
bool_t evict_cache(cache_t c, word_t i, mem_t m);
bool_t retr_cache(cache_t c, byte_t tag, word_t i, mem_t m);

bool_t get_cache_byte(cache_t c, word_t pos, byte_t *dest, mem_t m);
bool_t get_cache_word(cache_t c, word_t pos, word_t *dest, mem_t m);

word_t set_cache_byte(cache_t c, word_t pos, byte_t val, mem_t m);
word_t set_cache_word(cache_t c, word_t pos, word_t val, mem_t m);

void dump_cache(FILE *outfile, cache_t c, word_t s, int len);


/********** Implementation of Register File *************/

mem_t init_reg();
void free_reg(mem_t r);

/* Make a copy of a register file */
mem_t copy_reg(mem_t oldr);
/* Print the differences between two register files */
bool_t diff_reg(mem_t oldr, mem_t newr, FILE *outfile);


word_t get_reg_val(mem_t r, int id);
void set_reg_val(mem_t r, int id, word_t val);
void dump_reg(FILE *outfile, mem_t r);


CPPEND

/* ****************  ALU Function **********************/

/* Compute ALU operation */
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

typedef struct {
  word_t pc;
  mem_t r;
  mem_t m;
  cc_t cc;
} state_rec, *state_ptr;

state_ptr new_state(int memlen);
void free_state(state_ptr s);

state_ptr copy_state(state_ptr s);
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
