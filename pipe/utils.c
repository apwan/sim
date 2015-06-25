#include <cstdio>
#include "sim.h"

/* Representations of digits */
static char digits[16] =
   {'0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/* Print hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   pbw denotes bits per word.*/
void wprint(unsigned x, int bpd, int bpw, FILE *fp)
{
  int digit;
  unsigned mask = (1 << bpd) - 1;
  for (digit = (bpw-1)/bpd; digit >= 0; digit--) {
    unsigned val = (x >> (digit * bpd)) & mask;
    putc(digits[val], fp);
  }
}

/* Create string in hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   pbw denotes bits per word.*/
void wstring(unsigned x, int bpd, int bpw, char *str)
{
  int digit;
  unsigned mask = (1 << bpd) - 1;
  for (digit = (bpw-1)/bpd; digit >= 0; digit--) {
    unsigned val = (x >> (digit * bpd)) & mask;
    *str++ = digits[val];
  }
  *str = '\0';
}


/* used for formatting instructions */
static char status_msg[128];



char *format_pc(pc_ptr state){
    char pstring[9];
    wstring(state->pc, 4, 32, pstring);
    sprintf(status_msg, "%s %s", stat_name(state->status), pstring);
    return status_msg;
}

char *format_if_id(if_id_ptr state){
    char valcstring[9];
    char valpstring[9];
    wstring(state->valc, 4, 32, valcstring);
    wstring(state->valp, 4, 32, valpstring);
    sprintf(status_msg, "%s %s %s %s %s %s",
        stat_name(state->status),
        iname(HPACK(state->icode,state->ifun)),
        reg_name(state->ra),
        reg_name(state->rb),
        valcstring,
        valpstring);
    return status_msg;
}

char *format_id_ex(id_ex_ptr state){
    char valcstring[9];
    char valastring[9];
    char valbstring[9];
    wstring(state->valc, 4, 32, valcstring);
    wstring(state->vala, 4, 32, valastring);
    wstring(state->valb, 4, 32, valbstring);
    sprintf(status_msg, "%s %s %s %s %s %s %s %s %s",
        stat_name(state->status),
        iname(HPACK(state->icode, state->ifun)),
        valcstring,
        valastring,
        valbstring,
        reg_name(state->deste),
        reg_name(state->destm),
        reg_name(state->srca),
        reg_name(state->srcb));
    return status_msg;
}

char *format_ex_mem(ex_mem_ptr state){
    char valestring[9];
    char valastring[9];
    wstring(state->vale, 4, 32, valestring);
    wstring(state->vala, 4, 32, valastring);
    sprintf(status_msg, "%s %s %c %s %s %s %s",
        stat_name(state->status),
        iname(HPACK(state->icode, state->ifun)),
        state->takebranch ? 'Y' : 'N',
        valestring,
        valastring,
        reg_name(state->deste),
        reg_name(state->destm));

    return status_msg;
}

char *format_mem_wb(mem_wb_ptr state){
    char valestring[9];
    char valmstring[9];
    wstring(state->vale, 4, 32, valestring);
    wstring(state->valm, 4, 32, valmstring);
    sprintf(status_msg, "%s %s %s %s %s %s",
        stat_name(state->status),
        iname(HPACK(state->icode, state->ifun)),
        valestring,
        valmstring,
        reg_name(state->deste),
        reg_name(state->destm));

    return status_msg;
}
