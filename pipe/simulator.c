#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include "simulator.h"
#include <pthread.h>

Simulator Simulator::sim0, Simulator::sim1;
Simulator *Simulator::simref[2] = {&Simulator::sim0, &Simulator::sim1};
int Simulator::showCoreId = 0;

int Simulator::print(const char* format ...){
    va_list arg;
    va_start( arg, format );
    vfprintf( output_file, format, arg );
    va_end( arg );
    return 0;
}

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
    if(!output_file)
        output_file = stdout;

    if (verbosity >= 2){
        if(output_file){
            setDumpFile(output_file);
        }else{
            setDumpFile(stdout);
        }
    }


    init();


    /* Emit simulator name */
    if (verbosity >= 2)
        print("%s\n", simname);



    log("objectfile: %s\n", this->object_filename);
    byte_cnt = mem->load(object_file, 1);
    CMARK("finish load mem\n")


    if (byte_cnt == 0) {
        fprintf(stderr, "No lines of code found\n");
        exit(1);
    } else if (verbosity >= 2) {
        log("%d bytes of code read\n", byte_cnt);
    }
    fclose(object_file);


    if (do_check) {
        isa_state = new StateRec(mem,reg,cc);
    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);

    if(use_Cache && mem->useCache()){
        log("tty mode Using Cache\n");
    }

    icount = run_pipe(instr_limit, 5*instr_limit, &run_status, &result_cc, this);
    if (verbosity > 0) {


        print("%d instructions executed\n", icount);
        print("Status = %s\n", stat_name((stat_t)run_status));
        print("Condition Codes: %s\n", cc_name(result_cc));
        print("Changed Register State:\n");
        diff_reg(reg0, reg, output_file);
        print("Changed Memory State:\n");
        diff_mem(mem0, mem, output_file);
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
                print("ISA Register != Pipeline Register File\n");
                diff_reg(isa_state->r, reg, output_file);
            }
        }
        if (diff_mem(isa_state->m, mem, NULL)) {
            match = FALSE;
            if (verbosity > 0) {
                print("ISA Memory != Pipeline Memory\n");
                diff_mem(isa_state->m, mem, output_file);
            }
        }
        if (isa_state->cc != result_cc) {
            match = FALSE;
            if (verbosity > 0) {
                print("ISA Cond. Codes (%s) != Pipeline Cond. Codes (%s)\n",
                       cc_name(isa_state->cc), cc_name(result_cc));
            }
        }
        if (match) {
            print("ISA Check Succeeds\n");
        } else {
            print("ISA Check Fails\n");
        }
    }

    /* Emit CPI statistics */
    {
        double cpi = instructions > 0 ? (double) cycles/instructions : 1.0;
        print("CPI: %d cycles/%d instructions = %.2f\n",
           cycles, instructions, cpi);
    }

}



Simulator::Simulator(){
}
Simulator::~Simulator(){
    if(output_filename && output_file != stdout){
        fclose(output_file);
        output_file = NULL;
        printf("close output_file: %s\n", output_filename);
    }
    if(mem){
        delete mem;
        mem = NULL;
    }
    if(reg){
        delete reg;
        reg = NULL;
    }
    if(object_file){
        fclose(object_file);
    }
    if(post_load_mem){
        printf("delete post load mem\n");
        delete post_load_mem;
        post_load_mem = NULL;
    }
}

void Simulator::config(sim_config &conf){
    instr_limit = conf.instr_limit;
    verbosity = conf.verbosity;
    do_check = conf.do_check;
    gui_mode = conf.use_Gui;
    use_Cache = conf.use_Cache;
    object_filename = conf.input_filename;
    if(object_filename){
        object_file = fopen(object_filename,"r");
        if(!object_file){
            printf("open input_file err!");
            //exit(1);
        }
    }
    if(conf.output_filename){
        output_filename = conf.output_filename;
        // may need to check previous output_file
        output_file = fopen(output_filename, "w+");
        if(!output_file){
            printf("open output_file err, set to stdout\n");
            output_file = stdout;
        }
    }

}

void *Simulator::simfunc0(void *arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    int id = (long)arg;
    printf("I am simfunc0 running at simulator %d\n", id);
    if(id == 0){
        sim0.tty_sim();
    }else{
        sim1.tty_sim();
    }

    pthread_exit((void*)0);
}

void Simulator::init(){
    initialized = 1;
    mem = new MemRec(MEM_SIZE);
    reg = new RegRec();

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

    reset();
    mem->clear();

}

void Simulator::reset(){
    if (!initialized)
        init();
    pip.clear();
    reg->clear();
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
    if(gui_mode){
        report(0); //GUI
    }

}

void Simulator::setName(char *name){
    strcpy(simname, name);

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
        show_cpi(instructions, cycles);

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
    pip.update();

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

            set_memory(align_addr, val, this);
            align_addr+=4;
            get_word_val(mem, align_addr, &val);

            set_memory(align_addr, val, this);
        } else {
            set_memory(mem_addr, mem_data, this);
        }

#endif
            }

        }
    }
    if (update_cc)
        cc = cc_in;
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


#ifdef HAS_GUI



/******************************************************************************
 *	tcl command definitions
 ******************************************************************************/

/* Implement command versions of the simulation functions */
int Simulator::simResetCmd(ClientData clientData, Tcl_Interp *interp,
        int argc, char *argv[])
{

    Simulator *s = simref[showCoreId];
    sim_interp = interp;
    if (argc != 1) {
    interp->result = "No arguments allowed";
    return TCL_ERROR;
    }
    s->reset();
    if (s->post_load_mem) {
        free_mem(s->mem);
        s->mem = copy_mem(s->post_load_mem);
    }
    interp->result = stat_name(STAT_AOK);
    return TCL_OK;
}

int Simulator::simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    Simulator *s = simref[showCoreId];
    FILE *code_file;
    int code_count;
    sim_interp = interp;
    if (argc != 2) {
        interp->result = "One argument required";
        return TCL_ERROR;
    }

    code_file = fopen(argv[1], "r");

    if (!code_file) {
        sprintf(tcl_msg, "Couldn't open code file '%s'", argv[1]);
        interp->result = tcl_msg;
        return TCL_ERROR;
    }
    s->reset();


    ((RegRec*)s->reg)->setReporter((void*)&signal_register_update);
    s->mem->setReporter((void*)&report_line);//set before loading code
    code_count = s->mem->load(code_file, 0);
    if(s->post_load_mem){
        delete (s->post_load_mem);
    }
    s->post_load_mem = new MemRec (*(s->mem));

    sprintf(tcl_msg, "%d", code_count);
    interp->result = tcl_msg;
    CMARK("finish load code\n")
    if(s->use_Cache && s->mem->useCache())
          printf("gui mode using cache");
    fclose(code_file);

    return TCL_OK;
}

int Simulator::simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    Simulator *s = simref[showCoreId];

    FILE *data_file;
    int word_count = 0;
    interp->result = "Not implemented";
    return TCL_ERROR;


    sim_interp = interp;
    if (argc != 2) {
        interp->result = "One argument required";
        return TCL_ERROR;
    }
    data_file = fopen(argv[1], "r");
    if (!data_file) {
        sprintf(tcl_msg, "Couldn't open data file '%s'", argv[1]);
        interp->result = tcl_msg;
        return TCL_ERROR;
    }
    sprintf(tcl_msg, "%d", word_count);
    interp->result = tcl_msg;
    fclose(data_file);
    return TCL_OK;
}


int Simulator::simRunCmd(ClientData clientData, Tcl_Interp *interp,
          int argc, char *argv[])
{
    Simulator *s = simref[showCoreId];
    int cycle_limit = 1;
    byte_t status;
    cc_t cc;
    sim_interp = interp;

    if (argc > 2) {
        interp->result = "At most one argument allowed";
        return TCL_ERROR;
    }
    if (argc >= 2 &&
            (sscanf(argv[1], "%d", &cycle_limit) != 1 ||
             cycle_limit < 0)) {
        sprintf(tcl_msg, "Cannot run for '%s' cycles!", argv[1]);
        interp->result = tcl_msg;
        return TCL_ERROR;
    }

    CMARK("run pipe\n")

    run_pipe(cycle_limit + 5, cycle_limit, &status, &cc, s);
    interp->result = stat_name((stat_t)status);
    return TCL_OK;
}

int Simulator::simModeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    Simulator *s = simref[showCoreId];
    sim_interp = interp;

    if (argc != 2) {
        interp->result = "One argument required";
        return TCL_ERROR;
    }
    interp->result = argv[1];
    if (strcmp(argv[1], "wedged") == 0)
        s->sim_mode = S_WEDGED;
    else if (strcmp(argv[1], "stall") == 0)
        s->sim_mode = S_STALL;
    else if (strcmp(argv[1], "forward") == 0)
        s->sim_mode = S_FORWARD;
    else {
        sprintf(tcl_msg, "Unknown mode '%s'", argv[1]);
        interp->result = tcl_msg;
        return TCL_ERROR;
    }
    return TCL_OK;
}










#endif













