
#include <stdarg.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "sim.h"
#include <functional>

#ifdef HAS_GUI
#include <tk.h>
#endif /* HAS_GUI */

#include "simulator.h"

#define DEFAULT_VERBOSITY 2
#define DEFAULT_INSTR_LIMIT 10000

/*
 * usage - print helpful diagnostic information
 */
static void usage(char *name)
{
    printf("Usage: %s [-htg] [-l m] [-v n] file.yo\n", name);
    printf("file.yo arg required in GUI mode, optional in TTY mode (default stdin)\n");
    printf("   -h     Print this message\n");
    printf("   -g     Run in GUI mode instead of TTY mode (default TTY)\n");
    printf("   -l m   Set instruction limit to m [TTY mode only] (default %d)\n", DEFAULT_INSTR_LIMIT);
    printf("   -v n   Set verbosity level to 0 <= n <= 2 [TTY mode only] (default %d)\n", DEFAULT_VERBOSITY);
    printf("   -t     Test result against ISA simulator [TTY mode only]\n");
    exit(0);
}

int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
bool Use_Class = true;

struct sim_config{
    //int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
    char *object_filename;   /* The input object file name. */
    FILE *object_file;       /* Input file handle */
    int verbosity = DEFAULT_VERBOSITY;    /* Verbosity level [TTY only] (-v) */
    int instr_limit = DEFAULT_INSTR_LIMIT; /* Instruction limit [TTY only] (-l) */
    bool_t do_check = FALSE; /* Test with ISA simulator? [TTY only] (-t) */
};



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
    //mem->c = init_cache(6, 8, mem);


    /* Emit simulator name */
    if (conf.verbosity >= 2)
    printf("%s\n", simname);

    byte_cnt = load_mem(mem, conf.object_file, 1);
    if (byte_cnt == 0) {
    fprintf(stderr, "No lines of code found\n");
    exit(1);
    } else if (conf.verbosity >= 2) {
    printf("%d bytes of code read\n", byte_cnt);
    }
    fclose(conf.object_file);
    if (conf.do_check) {
    isa_state = new_state(0);
    free_mem(isa_state->r);
    free_mem(isa_state->m);
    isa_state->m = copy_mem(mem);
    isa_state->r = copy_mem(reg);
    isa_state->cc = cc;
    }

    mem0 = copy_mem(mem);
    reg0 = copy_mem(reg);

    icount = sim_run_pipe(conf.instr_limit, 5*conf.instr_limit, &run_status, &result_cc);
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

    printf("start stepping\n");
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


/*******************************************************************
 * Part 1: This part is the initial entry point that handles general
 * initialization. It parses the command line and does any necessary
 * setup to run in either TTY or GUI mode, and then starts the
 * simulation.
 *******************************************************************/

//typedef std::function<int(Tcl_Interp*)> tclProc;




/*
 * sim_main - main simulator routine. This function is called from the
 * main() routine in the HCL file.
 */
int sim_main(int argc, char **argv)
{

    CMARK("sim_main start\n");


    int i;
    int c;
    char *myargv[MAXARGS];
    sim_config conf;


    /* Parse the command line arguments */
    while ((c = getopt(argc, argv, "htgl:v:")) != -1) {
    switch(c) {
    case 'h':
        usage(argv[0]);
        break;
    case 'l':
        conf.instr_limit = atoi(optarg);
        break;
    case 'v':{
        int tmp_v = atoi(optarg);

        if (tmp_v < 0 || tmp_v > 2) {
            printf("Invalid verbosity %d\n", conf.verbosity);
            usage(argv[0]);
        }else{
            conf.verbosity = tmp_v;
        }
    }
        break;
    case 't':
        conf.do_check = TRUE;
        break;
    case 'g':
        gui_mode = TRUE;
        break;
    default:
        printf("Invalid option '%c'\n", c);
        usage(argv[0]);
        break;
    }
    }


    /* Do we have too many arguments? */
    if (optind < argc - 1) {
        printf("Too many command line arguments:");
        for (i = optind; i < argc; i++)
            printf(" %s", argv[i]);
        printf("\n");
        usage(argv[0]);
    }


    /* The single unflagged argument should be the object file name */
    conf.object_filename = NULL;
    conf.object_file = NULL;
    if (optind < argc) {
    conf.object_filename = argv[optind];
    conf.object_file = fopen(conf.object_filename, "r");
    if (!conf.object_file) {
        fprintf(stderr, "Couldn't open object file %s\n", conf.object_filename);
        exit(1);
    }
    }


    Simulator::ctrlId = 0;
    Simulator &newsim = *Simulator::simref[Simulator::ctrlId];

    if(Use_Class){
        newsim.instr_limit = conf.instr_limit;
        newsim.verbosity = conf.verbosity;
        newsim.do_check = conf.do_check;
        newsim.gui_mode = gui_mode;
        newsim.object_filename = conf.object_filename;
        newsim.object_file = conf.object_file;
    }


    /* Run the simulator in GUI mode (-g flag) */
    if (gui_mode) {

#ifndef HAS_GUI
    printf("To run in GUI mode, you must recompile with the HAS_GUI constant defined.\n");
    exit(1);
#endif /* HAS_GUI */

    /* In GUI mode, we must specify the object file on command line */
    if (!conf.object_file) {
        printf("Missing object file argument in GUI mode\n");
        usage(argv[0]);
    }

    /* Build the command line for the GUI simulator */
    for (i = 0; i < TKARGS; i++) {
        if ((myargv[i] = (char*)malloc(MAXBUF*sizeof(char))) == NULL) {
        perror("malloc error");
        exit(1);
        }
    }
    strcpy(myargv[0], argv[0]);
    strcpy(myargv[1], "pipe.tcl");
    strcpy(myargv[2], conf.object_filename);
    myargv[3] = NULL;

    /* Start the GUI simulator */
#ifdef HAS_GUI
    if(Use_Class){
        Tk_Main(TKARGS, myargv, Simulator::Tcl_AppInit);
    }else{
        Tk_Main(TKARGS, myargv, Tcl_AppInit);
    }
#endif /* HAS_GUI */
    exit(0);
    }else{
        /* Otherwise, run the simulator in TTY mode (no -g flag) */
        if(Use_Class){
            newsim.tty_sim();
        }else{
            run_tty_sim(conf);
        }
        exit(0);
    }




}



