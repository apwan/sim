#include <cstdio>
#include <cstring>
#include "sim.h"
#include "utils.h"
#include "simulator.h"

#define HAS_GUI

#ifdef HAS_GUI


char tcl_msg[256];

/* Keep track of the TCL Interpreter */
Tcl_Interp *sim_interp = NULL;

mem_t post_load_mem; /* for reload code, should delete */


/******************************************************************************
 *	tcl functionality called from within C
 ******************************************************************************/



/* Provide mechanism for simulator to update condition code display */
void show_cc(cc_t scc)
{
    int code;
    sprintf(tcl_msg, "setCC %d %d %d",
        GET_ZF(scc), GET_SF(scc), GET_OF(scc));
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
void show_cpi(int instr, int cyc) {
    int code;
    double cpi = instr > 0 ? (double) cyc/instr : 1.0;
    sprintf(tcl_msg, "showCPI %d %d %.2f",
        cyc, instr, (double) cpi);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to display CPI\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}


#endif






/* Provide mechanism for simulator to report which instructions are in
   which stages */



/* Provide mechanism for simulator to update register display */
/* called in RegRec::setVal  */
void signal_register_update(int r, word_t val) {
    if(r>=REG_NONE){
        printf("invalid register update\n");
        return;
    }
    int code;
    sprintf(tcl_msg, "setReg %u %u 1", r, val);

    code = Tcl_Eval(sim_interp, tcl_msg);
    //printf("setReg 0x%.2x to 0x%.16x", r, val);//check
    if (code != TCL_OK) {
        fprintf(stderr, "Failed to signal register set\n");
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to update memory value */
void set_memory(int addr, int val, Simulator *s) {
    int code;
    int nminAddr = s? (s->minAddr): minAddr;
    int nmemCnt = s? (s->memCnt): memCnt;

    /* First see if we need to expand memory range */
    if (nmemCnt == 0) {
        nminAddr = addr;
        nmemCnt = 4;// previous memCnt may be wrong!(WU Yijie)
    } else if (addr < nminAddr) {
        nmemCnt = nminAddr + nmemCnt - addr;
        nminAddr = addr;
    } else if (addr >= nminAddr+nmemCnt) {
        nmemCnt = addr-nminAddr+4;
    }
    /* Now make sure nminAddr & nmemCnt are multiples of 16 */
    nmemCnt = ((nminAddr & 0xF) + nmemCnt + 0xF) & ~0xF;
    nminAddr = nminAddr & ~0xF;

    if(s && (nminAddr != s->minAddr || nmemCnt != s->memCnt)){
        s->minAddr = nminAddr;
        s->memCnt = nmemCnt;
        create_memory_display(nminAddr, nmemCnt, s);
    }else if((!s) && (nminAddr != minAddr || nmemCnt != memCnt)) {
        minAddr = nminAddr;
        memCnt = nmemCnt;
        create_memory_display(nminAddr, nmemCnt);
    } else {
        sprintf(tcl_msg, "setMem %u %u", addr, val);
        code = Tcl_Eval(sim_interp, tcl_msg);
        if (code != TCL_OK) {
            fprintf(stderr, "Couldn't set memory value 0x%x to 0x%x\n",
                    addr, val);
            fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
        }
    }
}


/* Provide mechanism for simulator to generate memory display */
void create_memory_display(int nminAddr, int nmemCnt, Simulator *s) {
    mem_t tmp = s? (s->mem): mem;
    int code;
    sprintf(tcl_msg, "createMem %u %u", nminAddr, nmemCnt);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Command '%s' failed\n", tcl_msg);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    } else {
    int i;
    for (i = 0; i < nmemCnt && code == TCL_OK; i+=4) {
        int addr = nminAddr+i;
        int val;
        if (!tmp->getWord(addr, &val)) {
            fprintf(stderr, "Out of bounds memory display\n");
            return;
        }
        sprintf(tcl_msg, "setMem %u %u", addr, val);
        code = Tcl_Eval(sim_interp, tcl_msg);
    }
    if (code != TCL_OK) {
        fprintf(stderr, "Couldn't set memory value\n");
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
    }
}






/* Provide mechanism for simulator to report instructions as they are
   read in
*/
/* C++ compatible */
/* called from MemRec::load */
void report_line(int line_no, int addr, char *hex, char *text) {
    int code;
    sprintf(tcl_msg, "addCodeLine %u %u {%s} {%s}", line_no, addr, hex, text);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Failed to report code line 0x%x\n", addr);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}




void report_pc(unsigned fpc, unsigned char fpcv,
           unsigned dpc, unsigned char dpcv,
           unsigned epc, unsigned char epcv,
           unsigned mpc, unsigned char mpcv,
           unsigned wpc, unsigned char wpcv)
{
    int status;
    char addr[10];
    char code[12];
    Tcl_DString cmd;
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, "simLabel ", -1);
    Tcl_DStringStartSublist(&cmd);
    if (fpcv) {
        sprintf(addr, "%u", fpc);
        Tcl_DStringAppendElement(&cmd, addr);
    }
    if (dpcv) {
        sprintf(addr, "%u", dpc);
        Tcl_DStringAppendElement(&cmd, addr);
    }
    if (epcv) {
        sprintf(addr, "%u", epc);
        Tcl_DStringAppendElement(&cmd, addr);
    }
    if (mpcv) {
        sprintf(addr, "%u", mpc);
        Tcl_DStringAppendElement(&cmd, addr);
    }
    if (wpcv) {
        sprintf(addr, "%u", wpc);
        Tcl_DStringAppendElement(&cmd, addr);
    }
    Tcl_DStringEndSublist(&cmd);
    Tcl_DStringStartSublist(&cmd);
    sprintf(code, "%s %s %s %s %s",
            fpcv ? "F" : "",
            dpcv ? "D" : "",
            epcv ? "E" : "",
            mpcv ? "M" : "",
            wpcv ? "W" : "");
    Tcl_DStringAppend(&cmd, code, -1);
    Tcl_DStringEndSublist(&cmd);
    /* Debug
       fprintf(stderr, "Code '%s'\n", Tcl_DStringValue(&cmd));
    */
    status = Tcl_Eval(sim_interp, Tcl_DStringValue(&cmd));
    if (status != TCL_OK) {
        fprintf(stderr, "Failed to report pipe code '%s'\n", code);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Report single line of pipeline state */
void report_state(char *id, int current, char *txt)
{
    int status;
    sprintf(tcl_msg, "updateStage %s %u {%s}", id, current,txt);
    status = Tcl_Eval(sim_interp, tcl_msg);
    if (status != TCL_OK) {
        fprintf(stderr, "Failed to report pipe status\n");
        fprintf(stderr, "\tStage %s.%s, status '%s'\n",
                id, current ? "current" : "next", txt);
        fprintf(stderr, "\tError Message was '%s'\n", sim_interp->result);
    }
}


char *rname[] = {"none", "ea", "eb", "me", "wm", "we"};

/* provide mechanism for simulator to specify source registers */
void signal_sources(mux_source_t a, mux_source_t b) {
    int code;
    sprintf(tcl_msg, "showSources %s %s",
        rname[a], rname[b]);
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





/* Hack for SunOS */
#ifndef __cplusplus
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif



/*
 * Tcl_AppInit - Called by TCL to perform application-specific initialization.
 */
int Tcl_AppInit(Tcl_Interp *interp)
{

    printf("ctrlId: %d\n", Simulator::showCoreId);
    Simulator *s = Simulator::simref[Simulator::showCoreId];

    /* Tell TCL about the name of the simulator so it can  */
    /* use it as the title of the main window */
    if(Use_Class){
        Tcl_SetVar(interp, "simname", s->simname, TCL_GLOBAL_ONLY);
    }else{
        Tcl_SetVar(interp, "simname", simname, TCL_GLOBAL_ONLY);
    }


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


/******************************************************************************
 *	registering the commands with tcl
 ******************************************************************************/

void addAppCommands(Tcl_Interp *interp)
{

    sim_interp = interp;
    if(Use_Class){
        Tcl_CreateCommand(interp, "simReset", (Tcl_CmdProc *) Simulator::simResetCmd,
                  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
        Tcl_CreateCommand(interp, "simCode", (Tcl_CmdProc *) Simulator::simLoadCodeCmd,
                  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
        Tcl_CreateCommand(interp, "simData", (Tcl_CmdProc *) Simulator::simLoadDataCmd,
                  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
        Tcl_CreateCommand(interp, "simRun", (Tcl_CmdProc *) Simulator::simRunCmd,
                  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
        Tcl_CreateCommand(interp, "setSimMode", (Tcl_CmdProc *) Simulator::simModeCmd,
                  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    }else{
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

}




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

    ((RegRec*)reg)->setReporter((void*)&signal_register_update);
    mem->setReporter((void*)&report_line);

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
    if(Use_Cache && mem->useCache())
        printf("gui mode using cahce\n");

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

    run_pipe(cycle_limit + 5, cycle_limit, &status, &cc);
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








