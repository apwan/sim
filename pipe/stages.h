#ifndef STAGES_H
#define STAGES_H
/* 
 * stages.h - Defines the layout of the pipe registers
 * Declares the functions that implement the pipeline stages
*/


/********** Pipeline register contents **************/

/* Program Counter */
struct pc_ele{
    word_t pc;
    stat_t status;
};
typedef pc_ele* pc_ptr;

/* IF/ID Pipe Register */
struct if_id_ele{
    byte_t icode;  /* Single byte instruction code */
    byte_t ifun;    /* ALU/JMP qualifier */
    byte_t ra; /* Register ra ID */
    byte_t rb; /* Register rb ID */
    word_t valc;  /* Instruction word encoding immediate data */
    word_t valp; /* Incremented program counter */
    stat_t status;
    /* The following is included for debugging */
    word_t stage_pc;
};
typedef if_id_ele* if_id_ptr;

/* ID/EX Pipe Register */
struct id_ex_ele{
    byte_t icode;        /* Instruction code */
    byte_t ifun;        /* ALU/JMP qualifier */
    word_t valc;        /* Immediate data */
    word_t vala;        /* valA */
    word_t valb;        /* valB */
    byte_t srca;  /* Source Reg ID for valA */
    byte_t srcb;  /* Source Reg ID for valB */
    byte_t deste; /* Destination register for valE */
    byte_t destm; /* Destination register for valM */
    stat_t status;
    /* The following is included for debugging */
    word_t stage_pc;
};
typedef id_ex_ele* id_ex_ptr;

/* EX/MEM Pipe Register */
struct ex_mem_ele{
    byte_t icode;        /* Instruction code */
    byte_t ifun;          /* ALU/JMP qualifier */
    bool_t takebranch;  /* Taken branch signal */
    word_t vale;        /* valE */
    word_t vala;        /* valA */
    byte_t deste; /* Destination register for valE */
    byte_t destm; /* Destination register for valM */
    byte_t srca;  /* Source register for valA */
    stat_t status;
    /* The following is included for debugging */
    word_t stage_pc;
};
typedef ex_mem_ele* ex_mem_ptr;

/* Mem/WB Pipe Register */
struct mem_wb_ele{
    byte_t icode;        /* Instruction code */
    byte_t ifun;         /* ALU/JMP qualifier */
    word_t vale;         /* valE */
    word_t valm;         /* valM */
    byte_t deste; /* Destination register for valE */
    byte_t destm; /* Destination register for valM */
    stat_t status;
    /* The following is included for debugging */
    word_t stage_pc;
};
typedef mem_wb_ele* mem_wb_ptr;

/************ Global Declarations ********************/

const pc_ele bubble_pc = {0,STAT_AOK};
const if_id_ele bubble_if_id = { I_NOP, 0, REG_NONE,REG_NONE,
               0, 0, STAT_BUB, 0};
const id_ex_ele bubble_id_ex = { I_NOP, 0, 0, 0, 0,
               REG_NONE, REG_NONE, REG_NONE, REG_NONE,
               STAT_BUB, 0};

const ex_mem_ele bubble_ex_mem = { I_NOP, 0, FALSE, 0, 0,
                 REG_NONE, REG_NONE, STAT_BUB, (stat_t)0};

const mem_wb_ele bubble_mem_wb = { I_NOP, 0, 0, 0, REG_NONE, REG_NONE,
                 STAT_BUB, 0};


/************ Function declarations *******************/

/* Stage functions */
void do_if_stage();
void do_id_wb_stages();  /* Both ID and WB */
void do_ex_stage();
void do_mem_stage();

/* Set stalling conditions for different stages */
void do_stall_check();

#endif
