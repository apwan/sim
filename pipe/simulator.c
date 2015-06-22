#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include "simulator.h"



Simulator Simulator::sim0, Simulator::sim1;
Simulator *Simulator::simref[2] = {&Simulator::sim0, &Simulator::sim1};
int Simulator::ctrlId = 0;

void Simulator::tty_sim(){
    int icount = 0;
    byte_t run_status = STAT_AOK;
    cc_t result_cc = 0;
    int byte_cnt = 0;
    mem_t mem0, reg0;
    state_ptr isa_state = NULL;


    /* In TTY mode, the default object file comes from stdin */
    if (!object_file) {
         object_file = stdin;
    }

    if (verbosity >= 2)
        setDumpFile(stdout);

    init();
    // init_cache(6, 8, mem);


    /* Emit simulator name */
    if (verbosity >= 2)
        printf("%s\n", simname);



    printf("file: %s\n", this->object_filename);
    byte_cnt = mem->load(object_file, 1);
    CMARK("finish load mem\n")


    if (byte_cnt == 0) {
        fprintf(stderr, "No lines of code found\n");
        exit(1);
    } else if (verbosity >= 2) {
        printf("%d bytes of code read\n", byte_cnt);
    }
    fclose(object_file);


    if (do_check) {
        isa_state = new StateRec(mem,reg,cc);
    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);


    icount = run_pipe(instr_limit, 5*instr_limit, &run_status, &result_cc);
    if (verbosity > 0) {
        printf("%d instructions executed\n", icount);
        printf("Status = %s\n", stat_name((stat_t)run_status));
        printf("Condition Codes: %s\n", cc_name(result_cc));
        printf("Changed Register State:\n");
        diff_reg(reg0, reg, stdout);
        printf("Changed Memory State:\n");
        diff_mem(mem0, mem, stdout);
    }
    if (do_check) {
        byte_t e = STAT_AOK;
        int step;
        bool_t match = TRUE;

        printf("start stepping\n");
        for (step = 0; step < instr_limit && e == STAT_AOK; step++) {
            e = step_state(isa_state, stdout);
    }

    if (diff_reg(isa_state->r, reg, NULL)) {
        match = FALSE;
        if (verbosity > 0) {
        printf("ISA Register != Pipeline Register File\n");
        diff_reg(isa_state->r, reg, stdout);
        }
    }
    if (diff_mem(isa_state->m, mem, NULL)) {
        match = FALSE;
        if (verbosity > 0) {
        printf("ISA Memory != Pipeline Memory\n");
        diff_mem(isa_state->m, mem, stdout);
        }
    }
    if (isa_state->cc != result_cc) {
        match = FALSE;
        if (verbosity > 0) {
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



Simulator::Simulator(){
    //init();
}

void Simulator::init(){
    initialized = 1;
    mem = init_mem(MEM_SIZE);
    reg = init_reg();


    /* create 5 pipe registers */
    pc_state     = new_pipe(sizeof(pc_ele), (void *) &bubble_pc);
    if_id_state  = new_pipe(sizeof(if_id_ele), (void *) &bubble_if_id);
    id_ex_state  = new_pipe(sizeof(id_ex_ele), (void *) &bubble_id_ex);
    ex_mem_state = new_pipe(sizeof(ex_mem_ele), (void *) &bubble_ex_mem);
    mem_wb_state = new_pipe(sizeof(mem_wb_ele), (void *) &bubble_mem_wb);

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

    reset();
    clear_mem(mem);

}

void Simulator::reset(){
    if (!initialized)
        init();
    clear_pipes();
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

    create_memory_display();//(minAddr, memCnt);
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
    if(gui_mode){
        report(0); //GUI
    }

}

void Simulator::setName(char *name){

}

void Simulator::report(int cyc){

    if(gui_mode){
#ifdef HAS_GUI

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
        /* signal_sources(); */
        show_cc(cc);
        show_stat(status);
        show_cpi();

#endif


    }else{
        log("\nCycle %d. CC=%s, Stat=%s\n", cyc, cc_name(cc), stat_name(status));

        log("F: predPC = 0x%x\n", pc_curr->pc);

        log("D: instr = %s, rA = %s, rB = %s, valC = 0x%x, valP = 0x%x, Stat = %s\n",
            iname(HPACK(if_id_curr->icode, if_id_curr->ifun)),
            reg_name(if_id_curr->ra), reg_name(if_id_curr->rb),
            if_id_curr->valc, if_id_curr->valp,
            stat_name(if_id_curr->status));

        log("E: instr = %s, valC = 0x%x, valA = 0x%x, valB = 0x%x\n   srcA = %s, srcB = %s, dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
            id_ex_curr->valc, id_ex_curr->vala, id_ex_curr->valb,
            reg_name(id_ex_curr->srca), reg_name(id_ex_curr->srcb),
            reg_name(id_ex_curr->deste), reg_name(id_ex_curr->destm),
            stat_name(id_ex_curr->status));

        log("M: instr = %s, Cnd = %d, valE = 0x%x, valA = 0x%x\n   dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(ex_mem_curr->icode, ex_mem_curr->ifun)),
            ex_mem_curr->takebranch,
            ex_mem_curr->vale, ex_mem_curr->vala,
            reg_name(ex_mem_curr->deste), reg_name(ex_mem_curr->destm),
            stat_name(ex_mem_curr->status));

        log("W: instr = %s, valE = 0x%x, valM = 0x%x, dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(mem_wb_curr->icode, mem_wb_curr->ifun)),
            mem_wb_curr->vale, mem_wb_curr->valm,
            reg_name(mem_wb_curr->deste), reg_name(mem_wb_curr->destm),
            stat_name(mem_wb_curr->status));
    }
}

byte_t Simulator::step_pipe(int max_instr, int ccount){
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
    update_pipes();

    report(ccount);

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


    /* Performance monitoring */
    if (mem_wb_curr->status != STAT_BUB && mem_wb_curr->icode != I_POP2) {
    starting_up = 0;
    instructions++;
    cycles++;
    } else {
    if (!starting_up)
        cycles++;
    }

    if(gui_mode){
        report(0); // GUI
    }

    return status;

}

void Simulator::setDumpFile(FILE *file){
    dumpfile = file;

}

void Simulator::log(const char *format, ...){
    if (dumpfile) {
        va_list arg;
        va_start( arg, format );
        vfprintf( dumpfile, format, arg );
        va_end( arg );
    }
}


void Simulator::update_state(bool_t update_mem, bool_t update_cc)
{
    /* Writeback(s):
       If either register is REG_NONE, write will have no effect .
       Order of two writes determines semantics of
       popl %esp.  According to ISA, %esp will get popped value
    */



    if (wb_destE != REG_NONE) {
        log("\tWriteback: Wrote 0x%x to register %s\n",
        wb_valE, reg_name(wb_destE));
        set_reg_val(reg, wb_destE, wb_valE);

    }
    if (wb_destM != REG_NONE) {
    log("\tWriteback: Wrote 0x%x to register %s\n",
        wb_valM, reg_name(wb_destM));
    set_reg_val(reg, wb_destM, wb_valM);
    }


    /* Memory write */
    if (mem_write && !update_mem) {
    log("\tDisabled write of 0x%x to address 0x%x\n", mem_data, mem_addr);
    }

    if (update_mem && mem_write) {
        if (!set_word_val(mem, mem_addr, mem_data)) {
            log("\tCouldn't write to address 0x%x\n", mem_addr);
        } else {
            log("\tWrote 0x%x to address 0x%x\n", mem_data, mem_addr);



            if (gui_mode) {
#ifdef HAS_GUI

        if (mem_addr % 4 != 0) {
            /* Just did a misaligned write.
               Need to display both words */
            word_t align_addr = mem_addr & ~0x3;
            word_t val;
            get_word_val(mem, align_addr, &val);
            set_memory(align_addr, val);
            align_addr+=4;
            get_word_val(mem, align_addr, &val);
            set_memory(align_addr, val);
        } else {
            set_memory(mem_addr, mem_data);
        }

#endif
            }

        }
    }
    if (update_cc)
    cc = cc_in;
}


int Simulator::run_pipe(int max_instr, int max_cycle, byte_t *statusp, cc_t *ccp){

    int icount = 0;
    int ccount = 0;
    byte_t run_status = STAT_AOK;
    while (icount < max_instr && ccount < max_cycle) {
        run_status = step_pipe(max_instr-icount, ccount);
    if (run_status != STAT_BUB)
        icount++;
    if (run_status != STAT_AOK && run_status != STAT_BUB)
        break;
    ccount++;
    }
    if (statusp)
        *statusp = run_status;
    if (ccp)
        *ccp = cc;
    return icount;

}

void Simulator::clear_pipes(){
    int s;
    for (s = 0; s < pipe_count; s++) {
      pipe_ptr p = pipes[s];
      memcpy(p->current, p->bubble_val, p->count);
      memcpy(p->next, p->bubble_val, p->count);
      p->op = P_LOAD;
    }

}
pipe_ptr Simulator::new_pipe(int count, void *bubble_val)
{
  pipe_ptr result = (pipe_ptr) malloc(sizeof(pipe_ele));
  result->current = malloc(count);
  result->next = malloc(count);
  memcpy(result->current, bubble_val, count);
  memcpy(result->next, bubble_val, count);
  result->count = count;
  result->op = P_LOAD;
  result->bubble_val = bubble_val;
  pipes[pipe_count++] = result;
  return result;
}

void Simulator::update_pipes(){

    int s;
    for (s = 0; s < pipe_count; s++) {
      pipe_ptr p = pipes[s];
      switch (p->op)
        {
        case P_BUBBLE:
          /* insert a bubble into the next stage */
          memcpy(p->current, p->bubble_val, p->count);
          break;

        case P_LOAD:
          /* copy calculated state from previous stage */
          memcpy(p->current, p->next, p->count);
          break;
        case P_ERROR:
        /* Like a bubble, but insert error condition */
          memcpy(p->current, p->bubble_val, p->count);
          break;
        case P_STALL:
        default:
          /* do nothing: next stage gets same instr again */
          ;
        }
      if (p->op != P_ERROR)
      p->op = P_LOAD;
    }

}


/* bubble stage (has effect at next update) */
void Simulator::bubble_stage(stage_id_t stage) {
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
void Simulator::stall_stage(stage_id_t stage) {
    switch (stage)
    {
    case IF_STAGE : pc_state->op     = P_STALL; break;
    case ID_STAGE : if_id_state->op  = P_STALL; break;
    case EX_STAGE : id_ex_state->op  = P_STALL; break;
    case MEM_STAGE: ex_mem_state->op = P_STALL; break;
    case WB_STAGE : mem_wb_state->op = P_STALL; break;
    }
}



/*************** Stage Implementations *****************/









void Simulator::do_if_stage()
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
        log("\tFetch: f_pc = 0x%x, imem_instr = %s, f_instr = %s\n",
        f_pc, iname(instr),
        iname(HPACK(if_id_next->icode, if_id_next->ifun)));
    }

    instr_valid = gen_instr_valid();
    if (!instr_valid)
        log("\tFetch: Instruction code 0x%x invalid\n", instr);
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



/* Implements both ID and WB */
void Simulator::do_id_wb_stages()
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



void Simulator::do_ex_stage()
{
    alu_t alufun = (alu_t)gen_alufun();
    bool_t setcc = gen_set_cc();
    word_t alua, alub;

    alua = gen_aluA();
    alub = gen_aluB();

    e_bcond = 	cond_holds(cc, (cond_t)id_ex_curr->ifun);

    ex_mem_next->takebranch = e_bcond;

    if (id_ex_curr->icode == I_JMP)
      log("\tExecute: instr = %s, cc = %s, branch %staken\n",
          iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
          cc_name(cc),
          ex_mem_next->takebranch ? "" : "not ");

    /* Perform the ALU operation */
    word_t aluout = compute_alu(alufun, alua, alub);
    ex_mem_next->vale = aluout;
        log("\tExecute: ALU: %c 0x%x 0x%x --> 0x%x\n",
        op_name(alufun), alua, alub, aluout);

    if (setcc) {
    cc_in = compute_cc(alufun, alua, alub);
        log("\tExecute: New cc = %s\n", cc_name(cc_in));
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




void Simulator::do_mem_stage()
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
          log("\tMemory: Read 0x%x from 0x%x\n",
          valm, mem_addr);
    }
    if (mem_write) {
    word_t sink;
    /* Do a read of address just to check validity */
    dmem_error = dmem_error || !get_word_val(mem, mem_addr, &sink);
    if (dmem_error)
          log("\tMemory: Invalid address 0x%x\n",
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



p_stat_t Simulator::pipe_cntl(char *name, int stall, int bubble)
{
    if (stall) {
    if (bubble) {
            log("%s: Conflicting control signals for pipe register\n",
            name);
        return P_ERROR;
    } else
        return P_STALL;
    } else {
    return bubble ? P_BUBBLE : P_LOAD;
    }
}


void Simulator::do_stall_check()
{
    pc_state->op = pipe_cntl("PC", gen_F_stall(), gen_F_bubble());
    if_id_state->op = pipe_cntl("ID", gen_D_stall(), gen_D_bubble());
    id_ex_state->op = pipe_cntl("EX", gen_E_stall(), gen_E_bubble());
    ex_mem_state->op = pipe_cntl("MEM", gen_M_stall(), gen_M_bubble());
    mem_wb_state->op = pipe_cntl("WB", gen_W_stall(), gen_W_bubble());
}




/* generated by HCL */

int Simulator::gen_f_pc()
{
    return ((((ex_mem_curr->icode) == (I_JMP)) & !(ex_mem_curr->takebranch)
        ) ? (ex_mem_curr->vala) : ((mem_wb_curr->icode) == (I_RET)) ?
      (mem_wb_curr->valm) : (pc_curr->pc));
}

int Simulator::gen_f_icode()
{
    return ((imem_error) ? (I_NOP) : (imem_icode));
}

int Simulator::gen_f_ifun()
{
    return ((imem_error) ? (F_NONE) : (imem_ifun));
}

int Simulator::gen_instr_valid()
{
    return ((if_id_next->icode) == (I_NOP) || (if_id_next->icode) ==
      (I_HALT) || (if_id_next->icode) == (I_RRMOVL) || (if_id_next->icode)
       == (I_IRMOVL) || (if_id_next->icode) == (I_RMMOVL) ||
      (if_id_next->icode) == (I_MRMOVL) || (if_id_next->icode) ==
      (I_RMSWAP) || (if_id_next->icode) == (I_ALU) || (if_id_next->icode)
       == (I_JMP) || (if_id_next->icode) == (I_CALL) || (if_id_next->icode)
       == (I_RET) || (if_id_next->icode) == (I_PUSHL) ||
      (if_id_next->icode) == (I_POPL) || (if_id_next->icode) == (I_IADDL)
       || (if_id_next->icode) == (I_LEAVE));
}

int Simulator::gen_f_stat()
{
    return ((imem_error) ? (STAT_ADR) : !(instr_valid) ? (STAT_INS) : (
        (if_id_next->icode) == (I_HALT)) ? (STAT_HLT) : (STAT_AOK));
}

int Simulator::gen_need_regids()
{
    return ((if_id_next->icode) == (I_RRMOVL) || (if_id_next->icode) ==
      (I_ALU) || (if_id_next->icode) == (I_PUSHL) || (if_id_next->icode)
       == (I_POPL) || (if_id_next->icode) == (I_IRMOVL) ||
      (if_id_next->icode) == (I_RMMOVL) || (if_id_next->icode) ==
      (I_MRMOVL) || (if_id_next->icode) == (I_IADDL) || (if_id_next->icode)
       == (I_RMSWAP));
}

int Simulator::gen_need_valC()
{
    return ((if_id_next->icode) == (I_IRMOVL) || (if_id_next->icode) ==
      (I_RMMOVL) || (if_id_next->icode) == (I_MRMOVL) ||
      (if_id_next->icode) == (I_RMSWAP) || (if_id_next->icode) == (I_JMP)
       || (if_id_next->icode) == (I_CALL) || (if_id_next->icode) ==
      (I_IADDL));
}

int Simulator::gen_f_predPC()
{
    return (((if_id_next->icode) == (I_JMP) || (if_id_next->icode) ==
        (I_CALL)) ? (if_id_next->valc) : (if_id_next->valp));
}

int Simulator::gen_d_srcA()
{
    return (((if_id_curr->icode) == (I_RRMOVL) || (if_id_curr->icode) ==
        (I_RMMOVL) || (if_id_curr->icode) == (I_ALU) || (if_id_curr->icode)
         == (I_PUSHL) || (if_id_curr->icode) == (I_RMSWAP)) ?
      (if_id_curr->ra) : ((if_id_curr->icode) == (I_POPL) ||
        (if_id_curr->icode) == (I_RET)) ? (REG_ESP) : ((if_id_curr->icode)
         == (I_LEAVE)) ? (REG_EBP) : (REG_NONE));
}

int Simulator::gen_d_srcB()
{
    return (((if_id_curr->icode) == (I_ALU) || (if_id_curr->icode) ==
        (I_RMMOVL) || (if_id_curr->icode) == (I_MRMOVL) ||
        (if_id_curr->icode) == (I_IADDL) || (if_id_curr->icode) ==
        (I_RMSWAP)) ? (if_id_curr->rb) : ((if_id_curr->icode) == (I_PUSHL)
         || (if_id_curr->icode) == (I_POPL) || (if_id_curr->icode) ==
        (I_CALL) || (if_id_curr->icode) == (I_RET)) ? (REG_ESP) : (
        (if_id_curr->icode) == (I_LEAVE)) ? (REG_EBP) : (REG_NONE));
}

int Simulator::gen_d_dstE()
{
    return (((if_id_curr->icode) == (I_RRMOVL) || (if_id_curr->icode) ==
        (I_IRMOVL) || (if_id_curr->icode) == (I_ALU) || (if_id_curr->icode)
         == (I_IADDL)) ? (if_id_curr->rb) : ((if_id_curr->icode) ==
        (I_PUSHL) || (if_id_curr->icode) == (I_POPL) || (if_id_curr->icode)
         == (I_CALL) || (if_id_curr->icode) == (I_RET) ||
        (if_id_curr->icode) == (I_LEAVE)) ? (REG_ESP) : (REG_NONE));
}

int Simulator::gen_d_dstM()
{
    return (((if_id_curr->icode) == (I_MRMOVL) || (if_id_curr->icode) ==
        (I_POPL) || (if_id_curr->icode) == (I_RMSWAP)) ? (if_id_curr->ra)
       : ((if_id_curr->icode) == (I_LEAVE)) ? (REG_EBP) : (REG_NONE));
}

int Simulator::gen_d_valA()
{
    return (((if_id_curr->icode) == (I_CALL) || (if_id_curr->icode) ==
        (I_JMP)) ? (if_id_curr->valp) : ((id_ex_next->srca) ==
        (ex_mem_next->deste)) ? (ex_mem_next->vale) : ((id_ex_next->srca)
         == (ex_mem_curr->destm)) ? (mem_wb_next->valm) : (
        (id_ex_next->srca) == (ex_mem_curr->deste)) ? (ex_mem_curr->vale)
       : ((id_ex_next->srca) == (mem_wb_curr->destm)) ? (mem_wb_curr->valm)
       : ((id_ex_next->srca) == (mem_wb_curr->deste)) ? (mem_wb_curr->vale)
       : (d_regvala));
}

int Simulator::gen_d_valB()
{
    return (((id_ex_next->srcb) == (ex_mem_next->deste)) ?
      (ex_mem_next->vale) : ((id_ex_next->srcb) == (ex_mem_curr->destm)) ?
      (mem_wb_next->valm) : ((id_ex_next->srcb) == (ex_mem_curr->deste)) ?
      (ex_mem_curr->vale) : ((id_ex_next->srcb) == (mem_wb_curr->destm)) ?
      (mem_wb_curr->valm) : ((id_ex_next->srcb) == (mem_wb_curr->deste)) ?
      (mem_wb_curr->vale) : (d_regvalb));
}

int Simulator::gen_aluA()
{
    return (((id_ex_curr->icode) == (I_RRMOVL) || (id_ex_curr->icode) ==
        (I_ALU)) ? (id_ex_curr->vala) : ((id_ex_curr->icode) == (I_IRMOVL)
         || (id_ex_curr->icode) == (I_RMMOVL) || (id_ex_curr->icode) ==
        (I_MRMOVL) || (id_ex_curr->icode) == (I_IADDL) ||
        (id_ex_curr->icode) == (I_RMSWAP)) ? (id_ex_curr->valc) : (
        (id_ex_curr->icode) == (I_CALL) || (id_ex_curr->icode) == (I_PUSHL)
        ) ? -4 : ((id_ex_curr->icode) == (I_RET) || (id_ex_curr->icode) ==
        (I_POPL) || (id_ex_curr->icode) == (I_LEAVE)) ? 4 : 0);
}

int Simulator::gen_aluB()
{
    return (((id_ex_curr->icode) == (I_RMMOVL) || (id_ex_curr->icode) ==
        (I_MRMOVL) || (id_ex_curr->icode) == (I_ALU) || (id_ex_curr->icode)
         == (I_CALL) || (id_ex_curr->icode) == (I_RMSWAP) ||
        (id_ex_curr->icode) == (I_PUSHL) || (id_ex_curr->icode) == (I_RET)
         || (id_ex_curr->icode) == (I_POPL) || (id_ex_curr->icode) ==
        (I_IADDL) || (id_ex_curr->icode) == (I_LEAVE)) ? (id_ex_curr->valb)
       : ((id_ex_curr->icode) == (I_RRMOVL) || (id_ex_curr->icode) ==
        (I_IRMOVL)) ? 0 : 0);
}

int Simulator::gen_alufun()
{
    return (((id_ex_curr->icode) == (I_ALU)) ? (id_ex_curr->ifun) : (A_ADD)
      );
}

int Simulator::gen_set_cc()
{
    return ((((id_ex_curr->icode) == (I_ALU) || (id_ex_curr->icode) ==
          (I_IADDL)) & !((mem_wb_next->status) == (STAT_ADR) ||
          (mem_wb_next->status) == (STAT_INS) || (mem_wb_next->status) ==
          (STAT_HLT))) & !((mem_wb_curr->status) == (STAT_ADR) ||
        (mem_wb_curr->status) == (STAT_INS) || (mem_wb_curr->status) ==
        (STAT_HLT)));
}

int Simulator::gen_e_valA()
{
    return (id_ex_curr->vala);
}

int Simulator::gen_e_dstE()
{
    return ((((id_ex_curr->icode) == (I_RRMOVL)) & !
        (ex_mem_next->takebranch)) ? (REG_NONE) : (id_ex_curr->deste));
}

int Simulator::gen_mem_addr()
{
    return (((ex_mem_curr->icode) == (I_RMMOVL) || (ex_mem_curr->icode) ==
        (I_PUSHL) || (ex_mem_curr->icode) == (I_CALL) ||
        (ex_mem_curr->icode) == (I_MRMOVL) || (ex_mem_curr->icode) ==
        (I_RMSWAP)) ? (ex_mem_curr->vale) : ((ex_mem_curr->icode) ==
        (I_POPL) || (ex_mem_curr->icode) == (I_RET) || (ex_mem_curr->icode)
         == (I_LEAVE)) ? (ex_mem_curr->vala) : 0);
}

int Simulator::gen_mem_read()
{
    return ((ex_mem_curr->icode) == (I_MRMOVL) || (ex_mem_curr->icode) ==
      (I_POPL) || (ex_mem_curr->icode) == (I_RET) || (ex_mem_curr->icode)
       == (I_LEAVE) || (ex_mem_curr->icode) == (I_RMSWAP));
}

int Simulator::gen_mem_write()
{
    return ((ex_mem_curr->icode) == (I_RMMOVL) || (ex_mem_curr->icode) ==
      (I_PUSHL) || (ex_mem_curr->icode) == (I_CALL) || (ex_mem_curr->icode)
       == (I_RMSWAP));
}

int Simulator::gen_m_stat()
{
    return ((dmem_error) ? (STAT_ADR) : (ex_mem_curr->status));
}

int Simulator::gen_w_dstE()
{
    return (mem_wb_curr->deste);
}

int Simulator::gen_w_valE()
{
    return (mem_wb_curr->vale);
}

int Simulator::gen_w_dstM()
{
    return (mem_wb_curr->destm);
}

int Simulator::gen_w_valM()
{
    return (mem_wb_curr->valm);
}

int Simulator::gen_Stat()
{
    return (((mem_wb_curr->status) == (STAT_BUB)) ? (STAT_AOK) :
      (mem_wb_curr->status));
}

int Simulator::gen_F_bubble()
{
    return 0;
}

int Simulator::gen_F_stall()
{
    return ((((id_ex_curr->icode) == (I_RMSWAP)) | (((id_ex_curr->icode)
             == (I_MRMOVL) || (id_ex_curr->icode) == (I_POPL) ||
            (id_ex_curr->icode) == (I_LEAVE)) & ((id_ex_curr->destm) ==
            (id_ex_next->srca) || (id_ex_curr->destm) == (id_ex_next->srcb)
            ))) | ((I_RET) == (if_id_curr->icode) || (I_RET) ==
        (id_ex_curr->icode) || (I_RET) == (ex_mem_curr->icode)));
}

int Simulator::gen_D_stall()
{
    return (((id_ex_curr->icode) == (I_RMSWAP)) | (((id_ex_curr->icode) ==
          (I_MRMOVL) || (id_ex_curr->icode) == (I_POPL) ||
          (id_ex_curr->icode) == (I_LEAVE)) & ((id_ex_curr->destm) ==
          (id_ex_next->srca) || (id_ex_curr->destm) == (id_ex_next->srcb)))
      );
}

int Simulator::gen_D_bubble()
{
    return ((((id_ex_curr->icode) == (I_JMP)) & !(ex_mem_next->takebranch))
       | (!(((id_ex_curr->icode) == (I_RMSWAP)) | (((id_ex_curr->icode) ==
              (I_MRMOVL) || (id_ex_curr->icode) == (I_POPL) ||
              (id_ex_curr->icode) == (I_LEAVE)) & ((id_ex_curr->destm) ==
              (id_ex_next->srca) || (id_ex_curr->destm) ==
              (id_ex_next->srcb)))) & ((I_RET) == (if_id_curr->icode) ||
          (I_RET) == (id_ex_curr->icode) || (I_RET) == (ex_mem_curr->icode)
          )));
}

int Simulator::gen_E_stall()
{
    return 0;
}

int Simulator::gen_E_bubble()
{
    return (((((id_ex_curr->icode) == (I_JMP)) & !(ex_mem_next->takebranch)
          ) | ((id_ex_curr->icode) == (I_RMSWAP))) | (((id_ex_curr->icode)
           == (I_MRMOVL) || (id_ex_curr->icode) == (I_POPL) ||
          (id_ex_curr->icode) == (I_LEAVE)) & ((id_ex_curr->destm) ==
          (id_ex_next->srca) || (id_ex_curr->destm) == (id_ex_next->srcb)))
      );
}

int Simulator::gen_M_stall()
{
    return 0;
}

int Simulator::gen_M_bubble()
{
    return (((mem_wb_next->status) == (STAT_ADR) || (mem_wb_next->status)
         == (STAT_INS) || (mem_wb_next->status) == (STAT_HLT)) | (
        (mem_wb_curr->status) == (STAT_ADR) || (mem_wb_curr->status) ==
        (STAT_INS) || (mem_wb_curr->status) == (STAT_HLT)));
}

int Simulator::gen_W_stall()
{
    return ((mem_wb_curr->status) == (STAT_ADR) || (mem_wb_curr->status)
       == (STAT_INS) || (mem_wb_curr->status) == (STAT_HLT));
}

int Simulator::gen_W_bubble()
{
    return 0;
}







