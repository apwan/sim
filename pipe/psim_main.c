
#include <stdarg.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "sim.h"
#include <pthread.h>

#ifdef HAS_GUI
#include <tk.h>
#endif /* HAS_GUI */

#include "simulator.h"

#ifndef HAS_GUI
#define WARN_NO_GUI \
    printf("To run in GUI mode, you must recompile with the HAS_GUI constant defined.\n");\
    exit(1);
#else
#define WARN_NO_GUI //printf("Has Gui!\n");
#endif

/*
 * usage - print helpful diagnostic information
 */
static void usage(char *name)
{
    printf("Usage: %s [-htgud] [-l m] [-v n] -i file.yo [-o outputfile] [-d outputfile2]\n", name);
    printf("   -i file.yo  required in GUI mode, optional in TTY mode (default stdin)\n");
    printf("   -h     Print this message\n");
    printf("   -g     Run in GUI mode instead of TTY mode (default TTY)\n");
    printf("   -l m   Set instruction limit to m [TTY mode only] (default %d)\n", DEFAULT_INSTR_LIMIT);
    printf("   -v n   Set verbosity level to 0 <= n <= 2 [TTY mode only] (default %d)\n", DEFAULT_VERBOSITY);
    printf("   -t     Test result against ISA simulator [TTY mode only]\n");
    printf("   -c     Use cache\n");
    printf("   -u     Use simulator class instead of global vars\n");
    printf("   -o file   Set output file, default stdout\n");
    printf("   -d file   Running dual core, set output of second core\n");
    exit(0);
}

int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
bool Use_Class = FALSE;
bool Use_Cache = FALSE;


bool_t parse_arg(sim_config *conf, int argc, char *argv[]){

    CMARK("parsing cmd line args\n")

    int c;
    /* Parse the command line arguments */
    while ((c = getopt(argc, argv, "htgcul:v:o:d:i:")) != -1) {
    switch(c) {
    case 'h':
        usage(argv[0]);
        break;
    case 'l':
        conf->instr_limit = atoi(optarg);
        break;
    case 'v':{
        conf->verbosity = atoi(optarg);
        if (conf->verbosity < 0 || conf->verbosity > 2) {
            printf("Invalid verbosity %d\n", conf->verbosity);
            usage(argv[0]);
        }
    }
        break;
    case 'o':{
        conf->output_filename = optarg;
    }
        break;
    case 'i':
        conf->input_filename = optarg;
    case 'u':
        conf->use_Class = TRUE;
        break;
    case 'c':
        conf->use_Cache = TRUE;
        break;
    case 't':
        conf->do_check = TRUE;
        break;
    case 'g':
        conf->use_Gui = TRUE;
        break;
    case 'd':{
        conf->dual = TRUE;
        conf->output_filename2 = optarg;
    }
        break;
    default:
        printf("Invalid option '%c'\n", c);
        usage(argv[0]);
        break;
    }
    }

    /* Do we have too many arguments? */
    int i;
    if (optind < argc) {
        printf("Too many command line arguments:");

        for (i = optind; i < argc; i++)
            printf(" %s", argv[i]);
        printf("\n");
        usage(argv[0]);
    }


    /* The single unflagged argument should be the object file name */

    conf->object_file = NULL;
}

int run_dual(char *argv1, sim_config &conf){

    conf.use_Class = TRUE;
    conf.use_Bus = TRUE;
    conf.do_check = FALSE;
    BusController bus(1<<12, CACHE_B);
    Simulator::bc = &bus;

    Simulator::sim0.config(conf);
    conf.output_filename = conf.output_filename2;
    Simulator::sim1.config(conf);


    pthread_t thrd1, thrd2;
    if(pthread_create(&thrd1,NULL,Simulator::simfunc0,(void*)0)){
        printf("thread 1 created err\n");
        exit(1);
    }
    if(pthread_create(&thrd2,NULL,Simulator::simfunc0,(void*)1)){
        printf("thread 2 created err\n");
        exit(1);
    }
    void *tret;
    scanf("%d", &tret);

    return 0;
}


/*******************************************************************
 * Part 1: This part is the initial entry point that handles general
 * initialization. It parses the command line and does any necessary
 * setup to run in either TTY or GUI mode, and then starts the
 * simulation.
 *******************************************************************/


/*
 * sim_main - main simulator routine. This function is called from the
 * main() routine in the HCL file.
 */
int sim_main(int argc, char **argv)
{
    CMARK("sim_main start\n");
    sim_config conf;
    parse_arg(&conf, argc, argv);

    Use_Class = conf.use_Class;
    gui_mode = conf.use_Gui;
    Use_Cache = conf.use_Cache;

    if(conf.dual){
        return run_dual(argv[0], conf);
    }

    if(conf.use_Class){
        Simulator::simref[0]->config(conf);
        Simulator::simref[1]->config(conf);
        Simulator::showCoreId = 0;
    }

    if(conf.input_filename){
        conf.object_file = fopen(conf.input_filename, "r");
        if (!conf.object_file) {
            fprintf(stderr, "Couldn't open object file %s\n", conf.input_filename);
            exit(1);
        }
    }


    /* Run the simulator in GUI mode (-g flag) */
    if (conf.use_Gui) {

WARN_NO_GUI

    /* In GUI mode, we must specify the object file on command line */
    if (!conf.object_file) {
        printf("Missing object file argument in GUI mode\n");
        usage(argv[0]);
    }

    /* Start the GUI simulator */
#ifdef HAS_GUI
    char *myargv[] = {argv[0], "pipe.tcl", conf.input_filename, NULL};
    Tk_Main(TKARGS, myargv, Tcl_AppInit);
#endif /* HAS_GUI */

    }else{
        /* Otherwise, run the simulator in TTY mode (no -g flag) */
        if(conf.use_Class){
            Simulator::simref[Simulator::showCoreId]->tty_sim();
        }else{
            run_tty_sim(conf);
        }
    }
    if(conf.object_file){
        fclose(conf.object_file);
    }
    exit(0);

}



