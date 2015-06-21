#ifndef PSIM_GUI_HPP
#define PSIM_GUI_HPP


/**********************
 * End Part 3 globals
 **********************/


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


/******************************************************************************
 *	tcl command definitions
 ******************************************************************************/

/* Implement command versions of the simulation functions */
int simResetCmd(ClientData clientData, Tcl_Interp *interp,
        int argc, char *argv[])
{
    sim_interp = interp;
    if (argc != 1) {
    interp->result = "No arguments allowed";
    return TCL_ERROR;
    }
    sim_reset();
    if (post_load_mem) {
    free_mem(mem);
    mem = copy_mem(post_load_mem);
    }
    interp->result = stat_name(STAT_AOK);
    return TCL_OK;
}

int simLoadCodeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
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
    sim_reset();
    code_count = load_mem(mem, code_file, 0);
    post_load_mem = copy_mem(mem);
    sprintf(tcl_msg, "%d", code_count);
    interp->result = tcl_msg;
    fclose(code_file);
    return TCL_OK;
}

int simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
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


int simRunCmd(ClientData clientData, Tcl_Interp *interp,
          int argc, char *argv[])
{
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
    sim_run_pipe(cycle_limit + 5, cycle_limit, &status, &cc);
    interp->result = stat_name((stat_t)status);
    return TCL_OK;
}

int simModeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    sim_interp = interp;
    if (argc != 2) {
    interp->result = "One argument required";
    return TCL_ERROR;
    }
    interp->result = argv[1];
    if (strcmp(argv[1], "wedged") == 0)
    sim_mode = S_WEDGED;
    else if (strcmp(argv[1], "stall") == 0)
    sim_mode = S_STALL;
    else if (strcmp(argv[1], "forward") == 0)
    sim_mode = S_FORWARD;
    else {
    sprintf(tcl_msg, "Unknown mode '%s'", argv[1]);
    interp->result = tcl_msg;
    return TCL_ERROR;
    }
    return TCL_OK;
}


/******************************************************************************
 *	registering the commands with tcl
 ******************************************************************************/

void addAppCommands(Tcl_Interp *interp)
{
    sim_interp = interp;
    Tcl_CreateCommand(interp, "simReset", (Tcl_CmdProc *) simResetCmd,
              (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simCode", (Tcl_CmdProc *) simLoadCodeCmd,
              (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simData", (Tcl_CmdProc *) simLoadDataCmd,
              (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "simRun", (Tcl_CmdProc *) simRunCmd,
              (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "setSimMode", (Tcl_CmdProc *) simModeCmd,
              (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
}

/******************************************************************************
 *	tcl functionality called from within C
 ******************************************************************************/


/* Provide mechanism for simulator to update memory value */
void set_memory(int addr, int val) {
    int code;
    int nminAddr = minAddr;
    int nmemCnt = memCnt;

    /* First see if we need to expand memory range */
    if (memCnt == 0) {
    nminAddr = addr;
    memCnt = 4;
    } else if (addr < minAddr) {
    nminAddr = addr;
    nmemCnt = minAddr + memCnt - addr;
    } else if (addr >= minAddr+memCnt) {
    nmemCnt = addr-minAddr+4;
    }
    /* Now make sure nminAddr & nmemCnt are multiples of 16 */
    nmemCnt = ((nminAddr & 0xF) + nmemCnt + 0xF) & ~0xF;
    nminAddr = nminAddr & ~0xF;

    if (nminAddr != minAddr || nmemCnt != memCnt) {
    minAddr = nminAddr;
    memCnt = nmemCnt;
    create_memory_display();
    } else {
    sprintf(tcl_msg, "setMem %d %d", addr, val);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Couldn't set memory value 0x%x to 0x%x\n",
            addr, val);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
    }
}

/* Provide mechanism for simulator to update condition code display */
void show_cc(cc_t cc)
{
    int code;
    sprintf(tcl_msg, "setCC %d %d %d",
        GET_ZF(cc), GET_SF(cc), GET_OF(cc));
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to display condition codes\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to update status display */
void show_stat(stat_t stat)
{
    int code;
    sprintf(tcl_msg, "showStat %s", stat_name(stat));
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to display status\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}



/* Provide mechanism for simulator to update performance information */
void show_cpi() {
    int code;
    double cpi = instructions > 0 ? (double) cycles/instructions : 1.0;
    sprintf(tcl_msg, "showCPI %d %d %.2f",
        cycles, instructions, (double) cpi);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to display CPI\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

char *rname[] = {"none", "ea", "eb", "me", "wm", "we"};

/* provide mechanism for simulator to specify source registers */
void signal_sources() {
    int code;
    sprintf(tcl_msg, "showSources %s %s",
        rname[amux], rname[bmux]);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to signal forwarding sources\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to clear register display */
void signal_register_clear() {
    int code;
    code = Tcl_Eval(sim_interp, "clearReg");
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to signal register clear\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}












/*
 * Tcl_AppInit - Called by TCL to perform application-specific initialization.
 */
int Tcl_AppInit(Tcl_Interp *interp)
{
    /* Tell TCL about the name of the simulator so it can  */
    /* use it as the title of the main window */
    Tcl_SetVar(interp, "simname", simname, TCL_GLOBAL_ONLY);

    if (Tcl_Init(interp) == TCL_ERROR)
    return TCL_ERROR;
    if (Tk_Init(interp) == TCL_ERROR)
    return TCL_ERROR;
    Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

    /* Call procedure to add new commands */
    addAppCommands(interp);

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.wishrc", TCL_GLOBAL_ONLY);
    return TCL_OK;

}

#endif // PSIM_GUI_HPP

