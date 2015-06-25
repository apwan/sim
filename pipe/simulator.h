#ifndef SIMULATOR_H
#define SIMULATOR_H
#include "sim.h"
#include "utils.h"
#include <cstdio>

#ifndef HAS_GUI
#define HAS_GUI
#endif


#ifdef HAS_GUI
#include <tk.h>
#endif


class Simulator{
public:
    Simulator();
    ~Simulator();
    void config(sim_config &conf);

    static Simulator sim0, sim1;
    static Simulator *simref[2];
    static int showCoreId;
    static void *simfunc0(void *arg);
    void *simfunc1(void *arg);


    void init();
    void reset();
    void setName(char *name);
    void setDumpFile(FILE *file);
    void log(const char *format, ...);
    int print(const char* ...);
    void report(int cyc);

    void tty_sim();


    Pipes pip;
    byte_t step_pipe(int max_instr, int ccount);

    void update_state(bool_t update_mem, bool_t update_cc);
    /* bubble stage (has effect at next update) */
    void bubble_stage(stage_id_t stage);
    /* stall stage (has effect at next update) */
    void stall_stage(stage_id_t stage);

    void do_if_stage();
    void do_id_wb_stages();
    void do_ex_stage();
    void do_mem_stage();

    void do_stall_check();


    /* Functions defined using HCL */
    int gen_f_pc();
    int gen_need_regids();
    int gen_need_valC();
    int gen_instr_valid();
    int gen_f_predPC();
    int gen_f_icode();
    int gen_f_ifun();
    int gen_f_stat();

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

    int gen_alufun();
    int gen_set_cc();
    int gen_Bch();
    int gen_aluA();
    int gen_aluB();
    int gen_e_valA();
    int gen_e_dstE();

    int gen_mem_addr();
    int gen_mem_read();
    int gen_mem_write();
    int gen_m_stat();



    int gen_F_stall();
    int gen_F_bubble();
    int gen_D_stall();
    int gen_D_bubble();
    int gen_E_stall();
    int gen_E_bubble();
    int gen_M_stall();
    int gen_M_bubble();
    int gen_W_stall();
    int gen_W_bubble();


    /* previous global */
    char simname[50] = "Y86 Processor: pipe-full.hcl";
    int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
    bool use_Cache = FALSE;
    char *object_filename;   /* The input object file name. */
    FILE *object_file;       /* Input file handle */
    char *output_filename = NULL;
    FILE *output_file = stdout;    // default set to stdout;
    int verbosity = 2;    /* Verbosity level [TTY only] (-v) */
    int instr_limit = 10000; /* Instruction limit [TTY only] (-l) */
    bool_t do_check = FALSE; /* Test with ISA simulator? [TTY only] (-t) */


    /* Simulator operating mode */
    sim_mode_t sim_mode = S_FORWARD;
    /* Log file */
    FILE *dumpfile = NULL;



#ifdef HAS_GUI
/* gui */




    static int simResetCmd(ClientData clientData, Tcl_Interp *interp,
            int argc, char *argv[]);

    static int simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
               int argc, char *argv[]);

    static int simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
               int argc, char *argv[]);

    static int simRunCmd(ClientData clientData, Tcl_Interp *interp,
              int argc, char *argv[]);

    static int simModeCmd(ClientData clientData, Tcl_Interp *interp,
               int argc, char *argv[]);


#endif

    /* data */
    int initialized = 0;
    /* Has simulator gotten past initial bubbles? */
    int starting_up = 1;
    /* How many cycles have been simulated? */
    int cycles = 0;
    /* How many instructions have passed through the WB stage? */
    int instructions = 0;
    /* Both instruction and data memory */

    mem_t post_load_mem = NULL;
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
    pipe_ptr pc_state, if_id_state, id_ex_state, ex_mem_state, mem_wb_state;



};

int run_pipe(int max_instr, int max_cycle, byte_t *statusp, cc_t *ccp, Simulator *s = NULL);
/* need to access glocal vars */
void create_memory_display(int nminAddr, int nmemCnt, Simulator *s = NULL);
void set_memory(int addr, int val, Simulator *s = NULL);



#endif // SIMULATOR_H

