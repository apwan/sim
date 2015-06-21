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

mem_t post_load_mem;


/* Provide mechanism for simulator to report which instructions are in
   which stages */



void Simulator::report_pc(unsigned fpc, unsigned char fpcv,
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
void Simulator::report_state(char *id, int current, char *txt)
{
    int status;
    sprintf(tcl_msg, "updateStage %s %d {%s}", id, current,txt);
    status = Tcl_Eval(sim_interp, tcl_msg);
    if (status != TCL_OK) {
    fprintf(stderr, "Failed to report pipe status\n");
    fprintf(stderr, "\tStage %s.%s, status '%s'\n",
        id, current ? "current" : "next", txt);
    fprintf(stderr, "\tError Message was '%s'\n", sim_interp->result);
    }
}




/* Provide mechanism for simulator to update register display */
void Simulator::signal_register_update(int r, word_t val) {
    CMARK("signaling")
            int code;
    sprintf(tcl_msg, "setReg %d %d 1", (int) r, (int) val);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Failed to signal register set\n");
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to generate memory display */
void Simulator::create_memory_display() {
    int code;
    sprintf(tcl_msg, "createMem %d %d", minAddr, memCnt);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Command '%s' failed\n", tcl_msg);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    } else {
        int i;
        for (i = 0; i < memCnt && code == TCL_OK; i+=4) {
            int addr = minAddr+i;
            int val;
            if (!get_word_val(mem, addr, &val)) {
                fprintf(stderr, "Out of bounds memory display\n");
                return;
            }
            sprintf(tcl_msg, "setMem %d %d", addr, val);
            code = Tcl_Eval(sim_interp, tcl_msg);
        }
        if (code != TCL_OK) {
            fprintf(stderr, "Couldn't set memory value\n");
            fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
        }
    }
}

/* Provide mechanism for simulator to update memory value */
void Simulator::set_memory(int addr, int val) {
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
        CMARK("about to display mem\n")
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

/* Provide mechanism for simulator to update register display */
void signal_register_update(int r, word_t val) {
    if(Use_Class){
        Simulator *s = Simulator::simref[Simulator::ctrlId];
        s->signal_register_update(r, val);
        return;
    }
    int code;
    sprintf(tcl_msg, "setReg %d %d 1", (int) r, (int) val);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to signal register set\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to generate memory display */
void create_memory_display() {
    if(Use_Class){
        Simulator *s = Simulator::simref[Simulator::ctrlId];
        s->create_memory_display();
        return;
    }
    int code;
    sprintf(tcl_msg, "createMem %d %d", minAddr, memCnt);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Command '%s' failed\n", tcl_msg);
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    } else {
    int i;
    for (i = 0; i < memCnt && code == TCL_OK; i+=4) {
        int addr = minAddr+i;
        int val;
        if (!get_word_val(mem, addr, &val)) {
        fprintf(stderr, "Out of bounds memory display\n");
        return;
        }
        sprintf(tcl_msg, "setMem %d %d", addr, val);
        code = Tcl_Eval(sim_interp, tcl_msg);
    }
    if (code != TCL_OK) {
        fprintf(stderr, "Couldn't set memory value\n");
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
    }
}






/* Provide mechanism for simulator to update condition code display */
void Simulator::show_cc(cc_t cc)
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
void Simulator::show_stat(stat_t stat)
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
void Simulator::show_cpi() {
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

char * Simulator::rname[] = {"none", "ea", "eb", "me", "wm", "we"};

/* provide mechanism for simulator to specify source registers */
void Simulator::signal_sources() {
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
void Simulator::signal_register_clear() {
    int code;
    code = Tcl_Eval(sim_interp, "clearReg");
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to signal register clear\n");
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}

/* Provide mechanism for simulator to report instructions as they are
   read in
*/

void Simulator::report_line(int line_no, int addr, char *hex, char *text) {
    int code;
    sprintf(tcl_msg, "addCodeLine %d %d {%s} {%s}", line_no, addr, hex, text);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
        fprintf(stderr, "Failed to report code line 0x%x\n", addr);
        fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}


/* Provide mechanism for simulator to report instructions as they are
   read in
*/
/* C++ compatible */
void report_line(int line_no, int addr, char *hex, char *text) {
    if(Use_Class){
        Simulator *s = Simulator::simref[Simulator::ctrlId];
        return s->report_line(line_no, addr, hex, text);
    }
    int code;
    sprintf(tcl_msg, "addCodeLine %d %d {%s} {%s}", line_no, addr, hex, text);
    code = Tcl_Eval(sim_interp, tcl_msg);
    if (code != TCL_OK) {
    fprintf(stderr, "Failed to report code line 0x%x\n", addr);
    fprintf(stderr, "Error Message was '%s'\n", sim_interp->result);
    }
}










/******************************************************************************
 *	tcl command definitions
 ******************************************************************************/

/* Implement command versions of the simulation functions */
int Simulator::simResetCmd(ClientData clientData, Tcl_Interp *interp,
        int argc, char *argv[])
{

    Simulator *s = simref[ctrlId];
    s->sim_interp = interp;
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
    Simulator *s = simref[ctrlId];
    FILE *code_file;
    int code_count;
    s->sim_interp = interp;
    if (argc != 2) {
        interp->result = "One argument required";
        return TCL_ERROR;
    }

    code_file = fopen(argv[1], "r");

    if (!code_file) {
        sprintf(s->tcl_msg, "Couldn't open code file '%s'", argv[1]);
        interp->result = s->tcl_msg;
        return TCL_ERROR;
    }
    s->reset();


    code_count = load_mem(s->mem, code_file, 0);
    s->post_load_mem = copy_mem(s->mem);

    sprintf(s->tcl_msg, "%d", code_count);
    interp->result = s->tcl_msg;
    CMARK("finish load code\n")
    fclose(code_file);
    return TCL_OK;
}

int Simulator::simLoadDataCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    Simulator *s = simref[ctrlId];

    FILE *data_file;
    int word_count = 0;
    interp->result = "Not implemented";
    return TCL_ERROR;


    s->sim_interp = interp;
    if (argc != 2) {
        interp->result = "One argument required";
        return TCL_ERROR;
    }
    data_file = fopen(argv[1], "r");
    if (!data_file) {
        sprintf(s->tcl_msg, "Couldn't open data file '%s'", argv[1]);
        interp->result = s->tcl_msg;
        return TCL_ERROR;
    }
    sprintf(s->tcl_msg, "%d", word_count);
    interp->result = s->tcl_msg;
    fclose(data_file);
    return TCL_OK;
}


int Simulator::simRunCmd(ClientData clientData, Tcl_Interp *interp,
          int argc, char *argv[])
{
    Simulator *s = simref[ctrlId];
    int cycle_limit = 1;
    byte_t status;
    cc_t cc;
    s->sim_interp = interp;

    if (argc > 2) {
        interp->result = "At most one argument allowed";
        return TCL_ERROR;
    }
    if (argc >= 2 &&
            (sscanf(argv[1], "%d", &cycle_limit) != 1 ||
             cycle_limit < 0)) {
        sprintf(s->tcl_msg, "Cannot run for '%s' cycles!", argv[1]);
        interp->result = s->tcl_msg;
        return TCL_ERROR;
    }
    CMARK("run pipe\n")
    s->run_pipe(cycle_limit + 5, cycle_limit, &status, &cc);
    interp->result = stat_name((stat_t)status);
    return TCL_OK;
}

int Simulator::simModeCmd(ClientData clientData, Tcl_Interp *interp,
           int argc, char *argv[])
{
    Simulator *s = simref[ctrlId];
    s->sim_interp = interp;

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
        sprintf(s->tcl_msg, "Unknown mode '%s'", argv[1]);
        interp->result = s->tcl_msg;
        return TCL_ERROR;
    }
    return TCL_OK;
}


/******************************************************************************
 *	registering the commands with tcl
 ******************************************************************************/

void Simulator::addAppCommands(Tcl_Interp *interp)
{
    Simulator *s = simref[ctrlId];

    s->sim_interp = interp;
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
}


int Simulator::Tcl_AppInit(Tcl_Interp *interp)
{
    printf("ctrlId: %d\n", ctrlId);
    Simulator *s = simref[ctrlId];
    CMARK("get simulator instance\n")

    /* Tell TCL about the name of the simulator so it can  */
    /* use it as the title of the main window */
    Tcl_SetVar(interp, "simname", s->simname, TCL_GLOBAL_ONLY);

    if (Tcl_Init(interp) == TCL_ERROR)
        return TCL_ERROR;
    if (Tk_Init(interp) == TCL_ERROR)
        return TCL_ERROR;
    Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

    CMARK("about to add commands\n")

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
    sprintf(tcl_msg, "updateStage %s %d {%s}", id, current,txt);
    status = Tcl_Eval(sim_interp, tcl_msg);
    if (status != TCL_OK) {
    fprintf(stderr, "Failed to report pipe status\n");
    fprintf(stderr, "\tStage %s.%s, status '%s'\n",
        id, current ? "current" : "next", txt);
    fprintf(stderr, "\tError Message was '%s'\n", sim_interp->result);
    }
}



#endif






