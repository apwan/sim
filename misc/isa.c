#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "isa.h"


reg_id_t find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
	if (!strcmp(name, reg_table[i].name))
        return (reg_id_t)reg_table[i].id;
    return REG_ERR;
}

char *reg_name(int id)
{
    if (id >= 0 && id < REG_NONE)
        return reg_table[id].name;
    else
        return reg_table[REG_NONE].name;
}

/* Is the given register ID a valid program register? */
int reg_valid(int id)
{
  return id >= 0 && id < REG_NONE && reg_table[id].id == id;
}

instr_t instruction_set[] = 
{
    {"nop",    HPACK(I_NOP, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"halt",   HPACK(I_HALT, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"rrmovl", HPACK(I_RRMOVL, F_NONE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* Conditional move instructions are variants of RRMOVL */
    {"cmovle", HPACK(I_RRMOVL, C_LE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovl", HPACK(I_RRMOVL, C_L), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmove", HPACK(I_RRMOVL, C_E), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovne", HPACK(I_RRMOVL, C_NE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovge", HPACK(I_RRMOVL, C_GE), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"cmovg", HPACK(I_RRMOVL, C_G), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* arg1hi indicates number of bytes */
    {"irmovl", HPACK(I_IRMOVL, F_NONE), 6, I_ARG, 2, 4, R_ARG, 1, 0 },
    {"rmmovl", HPACK(I_RMMOVL, F_NONE), 6, R_ARG, 1, 1, M_ARG, 1, 0 },
    /* mem addr stored in reg indicates by lo bits, so arg1hi=0 */
    {"mrmovl", HPACK(I_MRMOVL, F_NONE), 6, M_ARG, 1, /* here */ 0, R_ARG, 1, 1 },
    /* add swap */
    {"rmswap",  HPACK(I_RMSWAP, F_NONE), 6, R_ARG, 1, 1, M_ARG, 1, 0},
    {"addl",   HPACK(I_ALU, A_ADD), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"subl",   HPACK(I_ALU, A_SUB), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"andl",   HPACK(I_ALU, A_AND), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    {"xorl",   HPACK(I_ALU, A_XOR), 2, R_ARG, 1, 1, R_ARG, 1, 0 },
    /* arg1hi indicates number of bytes */
    {"jmp",    HPACK(I_JMP, C_YES), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"jle",    HPACK(I_JMP, C_LE), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"jl",     HPACK(I_JMP, C_L), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"je",     HPACK(I_JMP, C_E), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"jne",    HPACK(I_JMP, C_NE), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"jge",    HPACK(I_JMP, C_GE), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"jg",     HPACK(I_JMP, C_G), 5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"call",   HPACK(I_CALL, F_NONE),    5, I_ARG, 1, 4, NO_ARG, 0, 0 },
    {"ret",    HPACK(I_RET, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    {"pushl",  HPACK(I_PUSHL, F_NONE) , 2, R_ARG, 1, 1, NO_ARG, 0, 0 },
    {"popl",   HPACK(I_POPL, F_NONE) ,  2, R_ARG, 1, 1, NO_ARG, 0, 0 },
    {"iaddl",  HPACK(I_IADDL, F_NONE), 6, I_ARG, 2, 4, R_ARG, 1, 0 },
    {"leave",  HPACK(I_LEAVE, F_NONE), 1, NO_ARG, 0, 0, NO_ARG, 0, 0 },
    /* this is just a hack to make the I_POP2 code have an associated name */
    {"pop2",   HPACK(I_POP2, F_NONE) , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 },

    /* For allocation instructions, arg1hi indicates number of bytes */
    {".byte",  0x00, 1, I_ARG, 0, 1, NO_ARG, 0, 0 },
    {".word",  0x00, 2, I_ARG, 0, 2, NO_ARG, 0, 0 },
    {".long",  0x00, 4, I_ARG, 0, 4, NO_ARG, 0, 0 },
    {NULL,     0   , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 }
};

instr_t invalid_instr =
    {"XXX",     0   , 0, NO_ARG, 0, 0, NO_ARG, 0, 0 };

instr_ptr bad_instr()
{
    return &invalid_instr;
}


instr_ptr find_instr(char *name)
{
    int i;
    for (i = 0; instruction_set[i].name; i++)
	if (strcmp(instruction_set[i].name,name) == 0)
	    return &instruction_set[i];
    return NULL;
}

/* Return name of instruction given its encoding */
char *iname(int instr) {
    int i;
    for (i = 0; instruction_set[i].name; i++) {
	if (instr == instruction_set[i].code)
	    return instruction_set[i].name;
    }
    return "<bad>";
}





char op_name(alu_t op)
{
    if (op < A_NONE)
        return alu_table[op].symbol;
    else
        return alu_table[A_NONE].symbol;
}

word_t compute_alu(alu_t op, word_t argA, word_t argB)
{
    word_t val;
    switch(op) {
    case A_ADD:
        val = argA+argB;
        break;
    case A_SUB:
        val = argB-argA;
        break;
    case A_AND:
        val = argA&argB;
        break;
    case A_XOR:
        val = argA^argB;
        break;
    default:
        val = 0;
    }
    return val;
}

cc_t compute_cc(alu_t op, word_t argA, word_t argB)
{
    word_t val = compute_alu(op, argA, argB);
    bool_t zero = (val == 0);
    bool_t sign = ((int)val < 0);
    bool_t ovf;
    switch(op) {
    case A_ADD:
        ovf = (((int) argA < 0) == ((int) argB < 0)) &&
  	       (((int) val < 0) != ((int) argA < 0));
	break;
    case A_SUB:
        ovf = (((int) argA > 0) == ((int) argB < 0)) &&
	       (((int) val < 0) != ((int) argB < 0));
	break;
    case A_AND:
    case A_XOR:
	ovf = FALSE;
	break;
    default:
	ovf = FALSE;
    }
    return PACK_CC(zero,sign,ovf);
    
}

char *cc_names[8] = {
    "Z=0 S=0 O=0",
    "Z=0 S=0 O=1",
    "Z=0 S=1 O=0",
    "Z=0 S=1 O=1",
    "Z=1 S=0 O=0",
    "Z=1 S=0 O=1",
    "Z=1 S=1 O=0",
    "Z=1 S=1 O=1"};

char *cc_name(cc_t c)
{
    int ci = c;
    if (ci < 0 || ci > 7)
        return "???????????";
    else
        return cc_names[c];
}

/* Status types */

char *stat_names[] = { "BUB", "AOK", "HLT", "ADR", "INS", "PIP" };

char *stat_name(stat_t e)
{
    if (e < 0 || e > STAT_PIP)
	return "Invalid Status";
    return stat_names[e];
}





/**************** Implementation of ISA model ************************/



bool_t diff_state(state_ptr olds, state_ptr news, FILE *outfile) {
    bool_t diff = FALSE;

    if (olds->pc != news->pc) {
	diff = TRUE;
        if (outfile) {
            fprintf(outfile, "pc:\t0x%.8x\t0x%.8x\n", olds->pc, news->pc);
        }
    }
    if (olds->cc != news->cc) {
        diff = TRUE;
        if (outfile) {
            fprintf(outfile, "cc:\t%s\t%s\n", cc_name(olds->cc), cc_name(news->cc));
        }
    }
    if (diff_reg(olds->r, news->r, outfile))
        diff = TRUE;
    if (diff_mem(olds->m, news->m, outfile))
        diff = TRUE;
    return diff;
}


/* Branch logic */
// C++ compatible
bool_t cond_holds(cc_t cc, cond_t bcond) {
    bool_t zf = (bool_t)GET_ZF(cc);
    bool_t sf = (bool_t)GET_SF(cc);
    bool_t of = (bool_t)GET_OF(cc);
    bool_t jump = FALSE;
    
    switch(bcond) {
    case C_YES:
        jump = TRUE;
        break;
    case C_LE:
        jump = (sf^of)|zf;
        break;
    case C_L:
        jump = sf^of;
        break;
    case C_E:
        jump = zf;
        break;
    case C_NE:
        jump = zf^1;
        break;
    case C_GE:
        jump = sf^of^1;
        break;
    case C_G:
        jump = (sf^of^1)&(zf^1);
        break;
    default:
        jump = FALSE;
        break;
    }
    return jump;
}

StateRec::StateRec(mem_t mm, mem_t rr, cc_t c){
    pc = 0;
    m = new MemRec(*mm);
    r = new RegRec(*(RegRec*)rr);
    cc = c;
}

StateRec::StateRec(int memlen){
    pc = 0;
    r = new RegRec();
    m = new MemRec(memlen);
    cc = DEFAULT_CC;
}

StateRec::StateRec(const StateRec& s){
    pc = s.pc;
    r = new RegRec(*(RegRec*)s.r);
    m = new MemRec(*s.m);
    cc = s.cc;
}

StateRec::~StateRec(){

    delete r;
    printf("reg released\n");
    delete m;
    printf("mem released\n");
}

StateRec &StateRec::operator=(const StateRec& s){
    pc = s.pc;
    r = new RegRec(*(RegRec*)s.r);
    m = new MemRec(*s.m);
    cc = s.cc;
    return *this;
}

stat_t StateRec::step(FILE *errFile){
    return step_state(this, errFile);
}

/* Execute single instruction.  Return status. */

stat_t step_state(state_ptr s, FILE *error_file, int gui_id)
{
    word_t argA, argB;
    byte_t byte0 = 0;
    byte_t byte1 = 0;
    itype_t hi0;
    alu_t  lo0;
    reg_id_t hi1 = REG_NONE;
    reg_id_t lo1 = REG_NONE;
    bool_t ok1 = TRUE;
    word_t cval = 0;
    word_t okc = TRUE;
    word_t val, dval;
    bool_t need_regids;
    bool_t need_imm;
    word_t ftpc = s->pc;  /* Fall-through PC */


    if (!get_byte_val(s->m, ftpc, &byte0)) {
        if (error_file)
            fprintf(error_file,
                    "PC = 0x%x, Invalid instruction address\n", s->pc);
        return STAT_ADR;
    }
    ftpc++;

    hi0 = (itype_t)HI4(byte0);
    lo0 = (alu_t)LO4(byte0);


    /* add reg dependency of I_RMSWAP */
    need_regids =
	(hi0 == I_RRMOVL || hi0 == I_ALU || hi0 == I_PUSHL ||
	 hi0 == I_POPL || hi0 == I_IRMOVL || hi0 == I_RMMOVL ||
     hi0 == I_MRMOVL || hi0 == I_IADDL || hi0 == I_RMSWAP);


    if (need_regids) {
        ok1 = get_byte_val(s->m, ftpc, &byte1);
        ftpc++;
        hi1 = (reg_id_t)HI4(byte1);
        lo1 = (reg_id_t)LO4(byte1);
    }

    /* add mem dependency of I_RMSWAP */
    need_imm =
	(hi0 == I_IRMOVL || hi0 == I_RMMOVL || hi0 == I_MRMOVL ||
     hi0 == I_JMP || hi0 == I_CALL || hi0 == I_IADDL || hi0 == I_RMSWAP);

    if (need_imm) {
        okc = get_word_val(s->m, ftpc, &cval);
        ftpc += 4;
    }

    switch (hi0) {
    case I_NOP:
        s->pc = ftpc;
        break;
    case I_HALT:
        return STAT_HLT;
        break;
    case I_RRMOVL:  /* Both unconditional and conditional moves */
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, hi1);
            return STAT_INS;
        }
        if (!reg_valid(lo1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, lo1);
            return STAT_INS;
        }
        val = get_reg_val(s->r, hi1);
        if (cond_holds(s->cc, (cond_t)lo0))
            set_reg_val(s->r, lo1, val);
        s->pc = ftpc;
        break;
    case I_IRMOVL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address",
                        s->pc);
            return STAT_INS;
        }
        if (!reg_valid(lo1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, lo1);
            return STAT_INS;
        }
        set_reg_val(s->r, lo1, cval);
        s->pc = ftpc;
        break;
    case I_RMMOVL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_INS;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, hi1);
            return STAT_INS;
        }
        if (reg_valid(lo1)) /* offset + base */
            cval += get_reg_val(s->r, lo1);
        val = get_reg_val(s->r, hi1);
        if (!set_word_val(s->m, cval, val)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid data address 0x%x\n",
                        s->pc, cval);
            return STAT_ADR;
        }
        s->pc = ftpc;
        break;
    case I_MRMOVL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_INS;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, hi1);
            return STAT_INS;
        }
        if (reg_valid(lo1))
            cval += get_reg_val(s->r, lo1);
        if (!get_word_val(s->m, cval, &val))
            return STAT_ADR;
        set_reg_val(s->r, hi1, val);
        s->pc = ftpc;
        break;

    case I_RMSWAP: {
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_INS;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, hi1);
            return STAT_INS;
        }
        /* combine I_RMMOVL and I_MRMOVL */
        /* special part */
        if (reg_valid(lo1))
            cval += get_reg_val(s->r, lo1);

        val = get_reg_val(s->r, hi1);
        if(!s->m->swapCacheWord(cval, &val)){
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid data address 0x%x\n",
                        s->pc, cval);
            return STAT_ADR;
        }
        set_reg_val(s->r, hi1, val);
        s->pc = ftpc;
    }

        break;

    case I_ALU:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        argA = get_reg_val(s->r, hi1);
        argB = get_reg_val(s->r, lo1);
        val = compute_alu(lo0, argA, argB);
        set_reg_val(s->r, lo1, val);
        s->cc = compute_cc(lo0, argA, argB);
        s->pc = ftpc;
        break;
    case I_JMP:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (cond_holds(s->cc, (cond_t)lo0))
            s->pc = cval;
        else
            s->pc = ftpc;
        break;
    case I_CALL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        val = get_reg_val(s->r, REG_ESP) - 4;
        set_reg_val(s->r, REG_ESP, val);
        if (!set_word_val(s->m, val, ftpc)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid stack address 0x%x\n", s->pc, val);
            return STAT_ADR;
        }
        s->pc = cval;
        break;
    case I_RET:
        /* Return Instruction.  Pop address from stack */
        dval = get_reg_val(s->r, REG_ESP);
        if (!get_word_val(s->m, dval, &val)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid stack address 0x%x\n",
                        s->pc, dval);
            return STAT_ADR;
        }
        set_reg_val(s->r, REG_ESP, dval + 4);
        s->pc = val;
        break;
    case I_PUSHL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n", s->pc, hi1);
            return STAT_INS;
        }
        val = get_reg_val(s->r, hi1);
        dval = get_reg_val(s->r, REG_ESP) - 4;
        set_reg_val(s->r, REG_ESP, dval);
        if  (!set_word_val(s->m, dval, val)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid stack address 0x%x\n", s->pc, dval);
            return STAT_ADR;
        }
        s->pc = ftpc;
        break;
    case I_POPL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!reg_valid(hi1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n", s->pc, hi1);
            return STAT_INS;
        }
        dval = get_reg_val(s->r, REG_ESP);
        set_reg_val(s->r, REG_ESP, dval+4);
        if (!get_word_val(s->m, dval, &val)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid stack address 0x%x\n",
                        s->pc, dval);
            return STAT_ADR;
        }
        set_reg_val(s->r, hi1, val);
        s->pc = ftpc;
        break;
    case I_LEAVE:
        dval = get_reg_val(s->r, REG_EBP);
        set_reg_val(s->r, REG_ESP, dval+4);
        if (!get_word_val(s->m, dval, &val)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid stack address 0x%x\n",
                        s->pc, dval);
            return STAT_ADR;
        }
        set_reg_val(s->r, REG_EBP, val);
        s->pc = ftpc;
        break;
    case I_IADDL:
        if (!ok1) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address\n", s->pc);
            return STAT_ADR;
        }
        if (!okc) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid instruction address",
                        s->pc);
            return STAT_INS;
        }
        if (!reg_valid(lo1)) {
            if (error_file)
                fprintf(error_file,
                        "PC = 0x%x, Invalid register ID 0x%.1x\n",
                        s->pc, lo1);
            return STAT_INS;
        }
        argB = get_reg_val(s->r, lo1);
        val = argB + cval;
        set_reg_val(s->r, lo1, val);
        s->cc = compute_cc(A_ADD, cval, argB);
        s->pc = ftpc;
        break;
    default:
        if (error_file)
            fprintf(error_file,
                    "PC = 0x%x, Invalid instruction %.2x\n", s->pc, byte0);
        return STAT_INS;
    }
    return STAT_AOK;
}


