/***********************************************************************
 * psim.c - Pipelined Y86 simulator
 * 
 * Copyright (c) 2010, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 ***********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "sim.h"
#include "simulator.h"

/*  implementation without Simulator class */


/*
 * run_tty_sim - Run the simulator in TTY mode
 */
void run_tty_sim(sim_config conf)
{
    int icount = 0;
    byte_t run_status = STAT_AOK;
    cc_t result_cc = 0;
    int byte_cnt = 0;
    mem_t mem0, reg0;
    state_ptr isa_state = NULL;


    /* In TTY mode, the default object file comes from stdin */
    if (!conf.object_file) {
        conf.object_file = stdin;
    }

    if (conf.verbosity >= 2)
        sim_set_dumpfile(stdout);
    sim_init();
    // init_cache(6, 8, mem);


    /* Emit simulator name */
    if (conf.verbosity >= 2)
        printf("%s\n", simname);

    byte_cnt = mem->load(conf.object_file, 1);
    if (byte_cnt == 0) {
        fprintf(stderr, "No lines of code found\n");
        exit(1);
    } else if (conf.verbosity >= 2) {
        printf("%d bytes of code read\n", byte_cnt);
    }
    fclose(conf.object_file);
    if (conf.do_check) {
        isa_state = new StateRec(mem,reg,cc);

    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);

    if(Use_Cache && mem->useCache()){
        printf("USE CACHE\n");
    }
    icount = run_pipe(conf.instr_limit, 5*conf.instr_limit, &run_status, &result_cc);
    if (conf.verbosity > 0) {
        printf("%d instructions executed\n", icount);
        printf("Status = %s\n", stat_name((stat_t)run_status));
        printf("Condition Codes: %s\n", cc_name(result_cc));
        printf("Changed Register State:\n");
        diff_reg(reg0, reg, stdout);
        printf("Changed Memory State:\n");
        diff_mem(mem0, mem, stdout);
    }
    if (conf.do_check) {
        byte_t e = STAT_AOK;
        int step;
        bool_t match = TRUE;

        printf("start pipe check\n");
        for (step = 0; step < conf.instr_limit && e == STAT_AOK; step++) {
            e = step_state(isa_state, stdout);
        }

    if (diff_reg(isa_state->r, reg, NULL)) {
        match = FALSE;
        if (conf.verbosity > 0) {
            printf("ISA Register != Pipeline Register File\n");
            diff_reg(isa_state->r, reg, stdout);
        }
    }
    if (diff_mem(isa_state->m, mem, NULL)) {
        match = FALSE;
        if (conf.verbosity > 0) {
            printf("ISA Memory != Pipeline Memory\n");
            diff_mem(isa_state->m, mem, stdout);
        }
    }
    if (isa_state->cc != result_cc) {
        match = FALSE;
        if (conf.verbosity > 0) {
            printf("ISA Cond. Codes (%s) != Pipeline Cond. Codes (%s)\n",
               cc_name(isa_state->cc), cc_name(result_cc));
        }
    }
    if (match) {
        printf("ISA Check Succeeds\n");
    } else {
        printf("ISA Check Fails\n");
    }
    }

    /* Emit CPI statistics */
    {
    double cpi = instructions > 0 ? (double) cycles/instructions : 1.0;
    printf("CPI: %d cycles/%d instructions = %.2f\n",
           cycles, instructions, cpi);
    }

}




/*********************************************************
 * Part 2: This part contains the core simulator routines.
 *********************************************************/


/*****************
 *  Part 2 Globals
 *****************/

/* Performance monitoring */

/* Has simulator gotten past initial bubbles? */
static int starting_up = 1;

/* How many cycles have been simulated? */
int cycles = 0;
/* How many instructions have passed through the WB stage? */
int instructions = 0;

/* Both instruction and data memory */
mem_t mem;
int minAddr = 0;
int memCnt = 0;

/* Register file */
mem_t reg;
/* Condition code register */
cc_t cc;
/* Status code */
stat_t status;


/* Pending updates to state */
word_t cc_in = DEFAULT_CC;
word_t wb_destE = REG_NONE;
word_t wb_valE = 0;
word_t wb_destM = REG_NONE;
word_t wb_valM = 0;
word_t mem_addr = 0;
word_t mem_data = 0;
bool_t mem_write = FALSE;

/* EX Operand sources */
mux_source_t amux = MUX_NONE;
mux_source_t bmux = MUX_NONE;

/* Current and next states of all pipeline registers */
pc_ptr pc_curr;
if_id_ptr if_id_curr;
id_ex_ptr id_ex_curr;
ex_mem_ptr ex_mem_curr;
mem_wb_ptr mem_wb_curr;

pc_ptr pc_next;
if_id_ptr if_id_next;
id_ex_ptr id_ex_next;
ex_mem_ptr ex_mem_next;
mem_wb_ptr mem_wb_next;

/* Intermediate values */
word_t f_pc;
byte_t imem_icode;
byte_t imem_ifun;
bool_t imem_error;
bool_t instr_valid;
word_t d_regvala;
word_t d_regvalb;
word_t e_vala;
word_t e_valb;
bool_t e_bcond;
bool_t dmem_error;

/* The pipeline state */
/* C++ compatible */
pipe_ptr pc_state, if_id_state, id_ex_state, ex_mem_state, mem_wb_state;

/* Simulator operating mode */
sim_mode_t sim_mode = S_FORWARD;
/* Log file */
FILE *dumpfile = NULL;




/*****************************************************************************
 * reporting code
 *****************************************************************************/



/* Report system state */
static void sim_report() 
{

#ifdef HAS_GUI
    if (gui_mode) {
	report_pc(f_pc, pc_curr->status != STAT_BUB,
		  if_id_curr->stage_pc, if_id_curr->status != STAT_BUB,
		  id_ex_curr->stage_pc, id_ex_curr->status != STAT_BUB,
		  ex_mem_curr->stage_pc, ex_mem_curr->status != STAT_BUB,
		  mem_wb_curr->stage_pc, mem_wb_curr->status != STAT_BUB);
	report_state("F", 0, format_pc(pc_next));
	report_state("F", 1, format_pc(pc_curr));
	report_state("D", 0, format_if_id(if_id_next));
	report_state("D", 1, format_if_id(if_id_curr));
	report_state("E", 0, format_id_ex(id_ex_next));
	report_state("E", 1, format_id_ex(id_ex_curr));
	report_state("M", 0, format_ex_mem(ex_mem_next));
	report_state("M", 1, format_ex_mem(ex_mem_curr));
	report_state("W", 0, format_mem_wb(mem_wb_next));
	report_state("W", 1, format_mem_wb(mem_wb_curr));
    /* signal_sources(amux, bmux); */
	show_cc(cc);
	show_stat(status);
    show_cpi(instructions, cycles);
    }
#endif

}

/*****************************************************************************
 * pipeline control
 * These functions can be used to handle hazards
 *****************************************************************************/

Pipes pip;




/* bubble stage (has effect at next update) */
void sim_bubble_stage(stage_id_t stage) 
{
    switch (stage)
	{
	case IF_STAGE : pc_state->op     = P_BUBBLE; break;
	case ID_STAGE : if_id_state->op  = P_BUBBLE; break;
	case EX_STAGE : id_ex_state->op  = P_BUBBLE; break;
	case MEM_STAGE: ex_mem_state->op = P_BUBBLE; break;
	case WB_STAGE : mem_wb_state->op = P_BUBBLE; break;
	}
}

/* stall stage (has effect at next update) */
void sim_stall_stage(stage_id_t stage) {
    switch (stage)
	{
	case IF_STAGE : pc_state->op     = P_STALL; break;
	case ID_STAGE : if_id_state->op  = P_STALL; break;
	case EX_STAGE : id_ex_state->op  = P_STALL; break;
	case MEM_STAGE: ex_mem_state->op = P_STALL; break;
	case WB_STAGE : mem_wb_state->op = P_STALL; break;
	}
}




static int initialized = 0;

void sim_init()
{
    /* Create memory and register files */
    initialized = 1;
    mem = init_mem(MEM_SIZE);
    reg = init_reg();

    /* create 5 pipe registers */
    pc_state     = pip[0];
    if_id_state  = pip[1];
    id_ex_state  = pip[2];
    ex_mem_state = pip[3];
    mem_wb_state = pip[4];
  
    /* connect them to the pipeline stages */
    pc_next   = (pc_ptr)pc_state->next;
    pc_curr   = (pc_ptr)pc_state->current;
  
    if_id_next = (if_id_ptr)if_id_state->next;
    if_id_curr = (if_id_ptr)if_id_state->current;

    id_ex_next = (id_ex_ptr)id_ex_state->next;
    id_ex_curr = (id_ex_ptr)id_ex_state->current;

    ex_mem_next = (ex_mem_ptr)ex_mem_state->next;
    ex_mem_curr = (ex_mem_ptr)ex_mem_state->current;

    mem_wb_next = (mem_wb_ptr)mem_wb_state->next;
    mem_wb_curr = (mem_wb_ptr)mem_wb_state->current;

    sim_reset();

    clear_mem(mem);
}

void sim_reset()
{
    if (!initialized)
        sim_init();
    pip.clear();
    clear_mem(reg);
    minAddr = 0;
    memCnt = 0;
    starting_up = 1;
    cycles = instructions = 0;
    cc = DEFAULT_CC;
    status = STAT_AOK;

#ifdef HAS_GUI
    if (gui_mode) {
        signal_register_clear();
        create_memory_display(minAddr, memCnt);
    }
#endif

    amux = bmux = MUX_NONE;
    cc = cc_in = DEFAULT_CC;
    wb_destE = REG_NONE;
    wb_valE = 0;
    wb_destM = REG_NONE;
    wb_valM = 0;
    mem_addr = 0;
    mem_data = 0;
    mem_write = FALSE;
    sim_report();
}

/* Update state elements */
/* May need to disable updating of memory & condition codes */
void update_state(bool_t update_mem, bool_t update_cc)
{
    /* Writeback(s):
       If either register is REG_NONE, write will have no effect .
       Order of two writes determines semantics of
       popl %esp.  According to ISA, %esp will get popped value
    */

    if (wb_destE != REG_NONE) {
	sim_log("\tWriteback: Wrote 0x%x to register %s\n",
        wb_valE, reg_name(wb_destE));
    set_reg_val(reg, wb_destE, wb_valE);
    }
    if (wb_destM != REG_NONE) {
	sim_log("\tWriteback: Wrote 0x%x to register %s\n",
        wb_valM, reg_name(wb_destM));
    set_reg_val(reg, wb_destM, wb_valM);
    }

    /* Memory write */
    if (mem_write && !update_mem) {
	sim_log("\tDisabled write of 0x%x to address 0x%x\n", mem_data, mem_addr);
    }
    if (update_mem && mem_write) {
	if (!set_word_val(mem, mem_addr, mem_data)) {
	    sim_log("\tCouldn't write to address 0x%x\n", mem_addr);
	} else {
	    sim_log("\tWrote 0x%x to address 0x%x\n", mem_data, mem_addr);


#ifdef HAS_GUI
	    if (gui_mode) {
		if (mem_addr % 4 != 0) {
		    /* Just did a misaligned write.
		       Need to display both words */
		    word_t align_addr = mem_addr & ~0x3;
		    word_t val;
            if(mem->ca){
                mem->ca->getWord(align_addr, &val);
                set_memory(align_addr, val);
                align_addr+=4;
                mem->ca->getWord(align_addr, &val);
                set_memory(align_addr, val);
            }else{
                mem->getWord(align_addr, &val);
                set_memory(align_addr, val);
                align_addr+=4;
                mem->getWord(align_addr, &val);
                set_memory(align_addr, val);
            }

		} else {
		    set_memory(mem_addr, mem_data);
		}
	    }
#endif

	}
    }
    if (update_cc)
	cc = cc_in;
}

/* Text representation of status */
void tty_report(int cyc) {
  sim_log("\nCycle %d. CC=%s, Stat=%s\n", cyc, cc_name(cc), stat_name(status));

  sim_log("F: predPC = 0x%x\n", pc_curr->pc);

  sim_log("D: instr = %s, rA = %s, rB = %s, valC = 0x%x, valP = 0x%x, Stat = %s\n",
	  iname(HPACK(if_id_curr->icode, if_id_curr->ifun)),
      reg_name(if_id_curr->ra), reg_name(if_id_curr->rb),
	  if_id_curr->valc, if_id_curr->valp,
	  stat_name(if_id_curr->status));

  sim_log("E: instr = %s, valC = 0x%x, valA = 0x%x, valB = 0x%x\n   srcA = %s, srcB = %s, dstE = %s, dstM = %s, Stat = %s\n",
	  iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
	  id_ex_curr->valc, id_ex_curr->vala, id_ex_curr->valb,
      reg_name(id_ex_curr->srca), reg_name(id_ex_curr->srcb),
      reg_name(id_ex_curr->deste), reg_name(id_ex_curr->destm),
	  stat_name(id_ex_curr->status));

  sim_log("M: instr = %s, Cnd = %d, valE = 0x%x, valA = 0x%x\n   dstE = %s, dstM = %s, Stat = %s\n",
	  iname(HPACK(ex_mem_curr->icode, ex_mem_curr->ifun)),
	  ex_mem_curr->takebranch,
	  ex_mem_curr->vale, ex_mem_curr->vala,
	  reg_name(ex_mem_curr->deste), reg_name(ex_mem_curr->destm),
	  stat_name(ex_mem_curr->status));

  sim_log("W: instr = %s, valE = 0x%x, valM = 0x%x, dstE = %s, dstM = %s, Stat = %s\n",
	  iname(HPACK(mem_wb_curr->icode, mem_wb_curr->ifun)),
	  mem_wb_curr->vale, mem_wb_curr->valm,
	  reg_name(mem_wb_curr->deste), reg_name(mem_wb_curr->destm),
	  stat_name(mem_wb_curr->status));
}

/* Run pipeline for one cycle */
/* Return status of processor */
/* Max_instr indicates maximum number of instructions that
   want to complete during this simulation run.  */
byte_t sim_step_pipe(int max_instr, int ccount)
{
    byte_t wb_status = mem_wb_curr->status;
    byte_t mem_status = mem_wb_next->status;
    /* How many instructions are ahead of one in wb / ex? */
    int ahead_mem = (wb_status != STAT_BUB);
    int ahead_ex = ahead_mem + (mem_status != STAT_BUB);
    bool_t update_mem = ahead_mem < max_instr;
    bool_t update_cc = ahead_ex < max_instr;

    /* Update program-visible state */
    update_state(update_mem, update_cc);
    /* Update pipe registers */
    pip.update();
    tty_report(ccount);
    if (pc_state->op == P_ERROR)
	pc_curr->status = STAT_PIP;
    if (if_id_state->op == P_ERROR)
	if_id_curr->status = STAT_PIP;
    if (id_ex_state->op == P_ERROR)
	id_ex_curr->status = STAT_PIP;
    if (ex_mem_state->op == P_ERROR)
	ex_mem_curr->status = STAT_PIP;
    if (mem_wb_state->op == P_ERROR)
	mem_wb_curr->status = STAT_PIP;
    
    /* Need to do decode after execute & memory stages,
       and memory stage before execute, in order to propagate
       forwarding values properly */
    do_if_stage();
    do_mem_stage();
    do_ex_stage();
    do_id_wb_stages();

    do_stall_check();
#if 0
    /* This doesn't seem necessary */
    if (id_ex_curr->status != STAT_AOK
	&& id_ex_curr->status != STAT_BUB) {
	if_id_state->op = P_BUBBLE;
	id_ex_state->op = P_BUBBLE;
    }
#endif

    /* Performance monitoring */
    if (mem_wb_curr->status != STAT_BUB && mem_wb_curr->icode != I_POP2) {
	starting_up = 0;
	instructions++;
	cycles++;
    } else {
	if (!starting_up)
	    cycles++;
    }
    
    sim_report();
    return status;
}

/*
  Run pipeline until one of following occurs:
  - An error status is encountered in WB.
  - max_instr instructions have completed through WB
  - max_cycle cycles have been simulated

  Return number of instructions executed.
  if statusp nonnull, then will be set to status of final instruction
  if ccp nonnull, then will be set to condition codes of final instruction
*/

int run_pipe(int max_instr, int max_cycle, byte_t *statusp, cc_t *ccp, Simulator *s){

    int icount = 0;
    int ccount = 0;
    byte_t run_status = STAT_AOK;



    while (icount < max_instr && ccount < max_cycle) {
        run_status = s? s->step_pipe(max_instr-icount, ccount): sim_step_pipe(max_instr-icount, ccount);
        if (run_status != STAT_BUB)
            icount++;
        if (run_status != STAT_AOK && run_status != STAT_BUB)
            break;
        ccount++;
    }
    if (statusp)
        *statusp = run_status;
    if (ccp)
        *ccp = s? s->cc: cc;
    return icount;

}


/* If dumpfile set nonNULL, lots of status info printed out */
void sim_set_dumpfile(FILE *df)
{
    dumpfile = df;
}

/*
 * sim_log dumps a formatted string to the dumpfile, if it exists
 * accepts variable argument list
 */
void sim_log( const char *format, ... ) {
    if (dumpfile) {
	va_list arg;
	va_start( arg, format );
	vfprintf( dumpfile, format, arg );
	va_end( arg );
    }
}



/**************************************************************
 * Part 4: Code for implementing pipelined processor simulators
 *************************************************************/

/******************************************************************************
 *	function definitions
 ******************************************************************************/

/* Create new pipe with count bytes of state */
/* bubble_val indicates state corresponding to pipeline bubble */


PipeRec::PipeRec(int c, void *bv){
    bubble_val = bv;/* Contents of register when bubble occurs */
    count = c;/* Number of state bytes */
    op = P_LOAD;
    current = malloc(count);
    next = malloc(count);
    memcpy(current, bubble_val, count);
    memcpy(next, bubble_val, count);
}
PipeRec::~PipeRec(){
    free(current);
    free(next);
}
void PipeRec::clear(){
    memcpy(current, bubble_val, count);
    memcpy(next, bubble_val, count);
    op = P_LOAD;
}

void PipeRec::update(){
    switch (op)
      {
      case P_BUBBLE:
        /* insert a bubble into the next stage */
        memcpy(current, bubble_val, count);
        break;

      case P_LOAD:
        /* copy calculated state from previous stage */
        memcpy(current, next, count);
        break;
      case P_ERROR:
      /* Like a bubble, but insert error condition */
        memcpy(current, bubble_val, count);
        break;
      case P_STALL:
      default:
        /* do nothing: next stage gets same instr again */
        ;
      }
    if (op != P_ERROR)
    op = P_LOAD;

}

Pipes::Pipes(){
    count = 0;
    add(sizeof(pc_ele), (void *) new pc_ele(bubble_pc));
    add(sizeof(if_id_ele), (void *) new if_id_ele(bubble_if_id));
    add(sizeof(id_ex_ele), (void *) new id_ex_ele(bubble_id_ex));
    add(sizeof(ex_mem_ele), (void *) new ex_mem_ele(bubble_ex_mem));
    add(sizeof(mem_wb_ele), (void *) new mem_wb_ele(bubble_mem_wb));
}

Pipes::~Pipes(){
    int i;
    for(i=0;i<count;++i){
        delete pipes[i]->bubble_val;
        delete pipes[i];
    }
}

pipe_ptr Pipes::add(int c, void *bubble_val){
    return pipes[count++] = new PipeRec(c, bubble_val);
}

void Pipes::update(){
    int s;
    for (s = 0; s < count; s++) {
      pipes[s]->update();
    }

}

void Pipes::clear(){
    int s;
    for (s = 0; s < count; s++) {
      pipes[s]->clear();
    }

}

pipe_ptr &Pipes::operator[](int i){
    return pipes[i];
}



/********************************
 * Part 5: Stage implementations
 *********************************/


/*************** Stage Implementations *****************/

int gen_f_pc();
int gen_need_regids();
int gen_need_valC();
int gen_instr_valid();
int gen_f_predPC();
int gen_f_icode();
int gen_f_ifun();
int gen_f_stat();
int gen_instr_valid();

void do_if_stage()
{
    byte_t instr = HPACK(I_NOP, F_NONE);
    byte_t regids = HPACK(REG_NONE, REG_NONE);
    word_t valc = 0;
    word_t valp = f_pc = gen_f_pc();

    /* Ready to fetch instruction.  Speculatively fetch register byte
       and immediate word
    */
    imem_error = !get_byte_val(mem, valp, &instr);
    imem_icode = HI4(instr);
    imem_ifun = LO4(instr);
    if (!imem_error) {
      byte_t junk;
      /* Make sure can read maximum length instruction */
      imem_error = !get_byte_val(mem, valp+5, &junk);
    }
    if_id_next->icode = gen_f_icode();
    if_id_next->ifun  = gen_f_ifun();
    if (!imem_error) {
	sim_log("\tFetch: f_pc = 0x%x, imem_instr = %s, f_instr = %s\n",
		f_pc, iname(instr),
		iname(HPACK(if_id_next->icode, if_id_next->ifun)));
    }

    instr_valid = gen_instr_valid();
    if (!instr_valid) 
      sim_log("\tFetch: Instruction code 0x%x invalid\n", instr);
    if_id_next->status = (stat_t)gen_f_stat();

    valp++;
    if (gen_need_regids()) {
	get_byte_val(mem, valp, &regids);
	valp ++;
    }
    if_id_next->ra = HI4(regids);
    if_id_next->rb = LO4(regids);
    if (gen_need_valC()) {
	get_word_val(mem, valp, &valc);
	valp+= 4;
    }
    if_id_next->valp = valp;
    if_id_next->valc = valc;

    pc_next->pc = gen_f_predPC();

    pc_next->status = (if_id_next->status == STAT_AOK) ? STAT_AOK : STAT_BUB;

    if_id_next->stage_pc = f_pc;
}

int gen_d_srcA();
int gen_d_srcB();
int gen_d_dstE();
int gen_d_dstM();
int gen_d_valA();
int gen_d_valB();
int gen_w_dstE();
int gen_w_valE();
int gen_w_dstM();
int gen_w_valM();
int gen_Stat();

/* Implements both ID and WB */
void do_id_wb_stages()
{
    /* Set up write backs.  Don't occur until end of cycle */
    wb_destE = gen_w_dstE();
    wb_valE = gen_w_valE();
    wb_destM = gen_w_dstM();
    wb_valM = gen_w_valM();

    /* Update processor status */
    status = (stat_t)gen_Stat();

    id_ex_next->srca = gen_d_srcA();
    id_ex_next->srcb = gen_d_srcB();
    id_ex_next->deste = gen_d_dstE();
    id_ex_next->destm = gen_d_dstM();

    /* Read the registers */
    d_regvala = get_reg_val(reg, id_ex_next->srca);
    d_regvalb = get_reg_val(reg, id_ex_next->srcb);

    /* Do forwarding and valA selection */
    id_ex_next->vala = gen_d_valA();
    id_ex_next->valb = gen_d_valB();

    id_ex_next->icode = if_id_curr->icode;
    id_ex_next->ifun = if_id_curr->ifun;
    id_ex_next->valc = if_id_curr->valc;
    id_ex_next->stage_pc = if_id_curr->stage_pc;
    id_ex_next->status = if_id_curr->status;
}

int gen_alufun();
int gen_set_cc();
int gen_Bch();
int gen_aluA();
int gen_aluB();
int gen_e_valA();
int gen_e_dstE();

void do_ex_stage()
{
    alu_t alufun = (alu_t)gen_alufun();
    bool_t setcc = gen_set_cc();
    word_t alua, alub;

    alua = gen_aluA();
    alub = gen_aluB();

    e_bcond = 	cond_holds(cc, (cond_t)id_ex_curr->ifun);
    
    ex_mem_next->takebranch = e_bcond;

    if (id_ex_curr->icode == I_JMP)
        sim_log("\tExecute: instr = %s, cc = %s, branch %staken\n",
                iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
                cc_name(cc),
                ex_mem_next->takebranch ? "" : "not ");
    
    /* Perform the ALU operation */
    word_t aluout = compute_alu(alufun, alua, alub);
    ex_mem_next->vale = aluout;
    sim_log("\tExecute: ALU: %c 0x%x 0x%x --> 0x%x\n",
            op_name(alufun), alua, alub, aluout);

    if (setcc) {
	cc_in = compute_cc(alufun, alua, alub);
	sim_log("\tExecute: New cc = %s\n", cc_name(cc_in));
    }

    ex_mem_next->icode = id_ex_curr->icode;
    ex_mem_next->ifun = id_ex_curr->ifun;
    ex_mem_next->vala = gen_e_valA();
    ex_mem_next->deste = gen_e_dstE();
    ex_mem_next->destm = id_ex_curr->destm;
    ex_mem_next->srca = id_ex_curr->srca;
    ex_mem_next->status = id_ex_curr->status;
    ex_mem_next->stage_pc = id_ex_curr->stage_pc;
}

/* Functions defined using HCL */
int gen_mem_addr();
int gen_mem_read();
int gen_mem_write();
int gen_m_stat();

void do_mem_stage()
{
    bool_t read = gen_mem_read();

    word_t valm = 0;

    mem_addr = gen_mem_addr();
    mem_data = ex_mem_curr->vala;
    mem_write = gen_mem_write();
    dmem_error = FALSE;

    if (read) {
	dmem_error = dmem_error || !get_word_val(mem, mem_addr, &valm);
	if (!dmem_error)
	  sim_log("\tMemory: Read 0x%x from 0x%x\n",
		  valm, mem_addr);
    }
    if (mem_write) {
	word_t sink;
	/* Do a read of address just to check validity */
	dmem_error = dmem_error || !get_word_val(mem, mem_addr, &sink);
	if (dmem_error)
	  sim_log("\tMemory: Invalid address 0x%x\n",
		  mem_addr);
    }
    mem_wb_next->icode = ex_mem_curr->icode;
    mem_wb_next->ifun = ex_mem_curr->ifun;
    mem_wb_next->vale = ex_mem_curr->vale;
    mem_wb_next->valm = valm;
    mem_wb_next->deste = ex_mem_curr->deste;
    mem_wb_next->destm = ex_mem_curr->destm;
    mem_wb_next->status = (stat_t)gen_m_stat();
    mem_wb_next->stage_pc = ex_mem_curr->stage_pc;
}

/* Set stalling conditions for different stages */

int gen_F_stall(), gen_F_bubble();
int gen_D_stall(), gen_D_bubble();
int gen_E_stall(), gen_E_bubble();
int gen_M_stall(), gen_M_bubble();
int gen_W_stall(), gen_W_bubble();



void do_stall_check()
{
    pc_state->op = pipe_cntl("PC", gen_F_stall(), gen_F_bubble());
    if_id_state->op = pipe_cntl("ID", gen_D_stall(), gen_D_bubble());
    id_ex_state->op = pipe_cntl("EX", gen_E_stall(), gen_E_bubble());
    ex_mem_state->op = pipe_cntl("MEM", gen_M_stall(), gen_M_bubble());
    mem_wb_state->op = pipe_cntl("WB", gen_W_stall(), gen_W_bubble());
}


p_stat_t pipe_cntl(char *name, int stall, int bubble)
{
    if (stall) {
    if (bubble) {
        sim_log("%s: Conflicting control signals for pipe register\n",
            name);
        return P_ERROR;
    } else
        return P_STALL;
    } else {
    return bubble ? P_BUBBLE : P_LOAD;
    }
}



