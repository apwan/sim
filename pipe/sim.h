#ifndef SIM_H
#define SIM_H

#include "isa.h"
#include "pipeline.h"
#include "stages.h"

#ifndef HAS_GUI
#define HAS_GUI
#endif

#define CMARK(msg) printf(msg);

/********** Defines **************/

/* Get ra out of one byte regid field */
#define GET_RA(r) HI4(r)

/* Get rb out of one byte regid field */
#define GET_RB(r) LO4(r)

#define DEFAULT_VERBOSITY 2
#define DEFAULT_INSTR_LIMIT 10000
#define MAXBUF 1024
#define DEFAULTNAME "Y86 Simulator: "
#define MAXARGS 128
#define MAXBUF 1024
#define TKARGS 3



/********** Typedefs ************/

/* EX stage mux settings */
typedef enum { MUX_NONE, MUX_EX_A, MUX_EX_B, MUX_MEM_E,
           MUX_WB_M, MUX_WB_E } mux_source_t;

/* Simulator operating modes */
typedef enum { S_WEDGED, S_STALL, S_FORWARD } sim_mode_t;

/* Pipeline stage identifiers for stage operation control */
typedef enum { IF_STAGE, ID_STAGE, EX_STAGE, MEM_STAGE, WB_STAGE } stage_id_t;


/************ Global state declaration ****************/
void report_pc(unsigned fpc, unsigned char fpcv,
           unsigned dpc, unsigned char dpcv,
           unsigned epc, unsigned char epcv,
           unsigned mpc, unsigned char mpcv,
           unsigned wpc, unsigned char wpcv);

void report_state(char *id, int current, char *txt);



/* How many cycles have been simulated? */
extern int cycles;
/* How many instructions have passed through the EX stage? */
extern int instructions;

/* Both instruction and data memory */
extern mem_t mem;

/* Keep track of range of addresses that have been written */
extern int minAddr;
extern int memCnt;

/* Register file */
extern mem_t reg;
/* Condition code register */
extern cc_t cc;
extern stat_t stat;


/* Operand sources in EX (to show forwarding) */
extern mux_source_t amux, bmux;


/*************** Bubbled version of stages *************/


/* Provide global access to current states of all pipeline registers */
#ifdef __cpluscplus
extern pipe_ptr pc_state, if_id_state, id_ex_state, ex_mem_state, mem_wb_state;
#else
//pipe_ptr pc_state, if_id_state, id_ex_state, ex_mem_state, mem_wb_state;
#endif

/* Current States */
extern pc_ptr pc_curr;
extern if_id_ptr if_id_curr;
extern id_ex_ptr id_ex_curr;
extern ex_mem_ptr ex_mem_curr;
extern mem_wb_ptr mem_wb_curr;

/* Next States */
extern pc_ptr pc_next;
extern if_id_ptr if_id_next;
extern id_ex_ptr id_ex_next;
extern ex_mem_ptr ex_mem_next;
extern mem_wb_ptr mem_wb_next;

/* Pending updates to state */
extern word_t cc_in;
extern word_t wb_destE;
extern word_t wb_valE;
extern word_t wb_destM;
extern word_t wb_valM;
extern word_t mem_addr;
extern word_t mem_data;
extern bool_t mem_write;


/* Intermdiate stage values that must be used by control functions */
extern word_t f_pc;
extern byte_t imem_icode;
extern byte_t imem_ifun;
extern bool_t imem_error;
extern bool_t instr_valid;
extern word_t d_regvala;
extern word_t d_regvalb;
extern word_t e_vala;
extern word_t e_valb;
extern bool_t e_bcond;
extern bool_t dmem_error;

extern Pipes pip;


/* Simulator operating mode */
extern sim_mode_t sim_mode;
/* Log file */
extern FILE *dumpfile;

struct sim_config{
    int use_Gui = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
    bool_t use_Class = FALSE;
    bool_t use_Cache = FALSE;
    char *input_filename = NULL;   /* The input object file name. */
    FILE *object_file;       /* Input file handle */
    char *output_filename = NULL;
    char *output_filename2 = NULL;
    int verbosity = DEFAULT_VERBOSITY;    /* Verbosity level [TTY only] (-v) */
    int instr_limit = DEFAULT_INSTR_LIMIT; /* Instruction limit [TTY only] (-l) */
    bool_t do_check = FALSE; /* Test with ISA simulator? [TTY only] (-t) */

    bool_t dual = FALSE;
};

void run_tty_sim(sim_config conf);
/* main */
int sim_main(int argc, char *argv[]);

/* Simulator name defined and initialized by the compiled HCL file */
/* according to the -n argument supplied to hcl2c */
extern  char simname[];

/* Parameters modifed by the command line */
extern int gui_mode;    /* Run in GUI mode instead of TTY mode? (-g) */
extern bool Use_Class;
extern bool Use_Cache;


/* End main */

/*************** Simulation Control Functions ***********/



/* Bubble next execution of specified stage */
void sim_bubble_stage(stage_id_t stage);

/* Stall stage (has effect at next update) */
void sim_stall_stage(stage_id_t stage);

/* Sets the simulator name (called from main routine in HCL file) */
void set_simname(char *name);

/* Initialize simulator */
void sim_init();

/* Reset simulator state, including register, instruction, and data memories */
void sim_reset();

/*
  Run pipeline until one of following occurs:
  - A status error is encountered in WB.
  - max_instr instructions have completed through WB
  - max_cycle cycles have been simulated

  Return number of instructions executed.
  if statusp nonnull, then will be set to status of final instruction
  if ccp nonnull, then will be set to condition codes of final instruction
*/

byte_t sim_step_pipe(int max_instr, int ccount);

/* If dumpfile set nonNULL, lots of status info printed out */
void sim_set_dumpfile(FILE *file);

/*
 * sim_log dumps a formatted string to the dumpfile, if it exists
 * accepts variable argument list
 */
void sim_log( const char *format, ... );


p_stat_t pipe_cntl(char *name, int stall, int bubble);


 
/******************* GUI Interface Functions **********************/
#ifdef HAS_GUI
#include <tk.h>
#endif

/* utils */
/* Print hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   bpw denotes bits per word.*/

void wprint(unsigned x, int bpd, int bpw, FILE *fp);
void wstring(unsigned x, int bpd, int bpw, char *str);
char *format_pc(pc_ptr state);
char *format_if_id(if_id_ptr state);
char *format_id_ex(id_ex_ptr state);
char *format_ex_mem(ex_mem_ptr state);
char *format_mem_wb(mem_wb_ptr state);

//#define HAS_GUI


#ifdef HAS_GUI
extern char tcl_msg[256];
/* Keep track of the TCL Interpreter */
extern Tcl_Interp *sim_interp;
extern mem_t post_load_mem;

void report_line(int line_no, int addr, char *hexcode, char *line);
void signal_register_update(int r, int val);

#endif


/* only access local vars */
void signal_sources(mux_source_t a, mux_source_t b);
void signal_register_clear();
void show_cc(cc_t scc);
void show_stat(stat_t stat);
void show_cpi(int instr, int cyc);















#ifdef HAS_GUI

/******************************************************************************
 *	function declarations
 ******************************************************************************/

int simResetCmd(ClientData clientData, Tcl_Interp *interp,
        int argc, char *argv[]);

int simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[]);

int simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[]);

int simRunCmd(ClientData clientData, Tcl_Interp *interp,
          int argc, char *argv[]);

int simModeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[]);

void addAppCommands(Tcl_Interp *interp);


#endif


								       
#endif
