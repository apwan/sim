/* Instruction set simulator for Y86 Architecture */

#include <stdio.h>
#include <stdlib.h>

#include "isa.h"

void usage(char *pname)
{
    printf("Usage: %s code_file [max_steps]\n", pname);
    exit(0);
}

int main(int argc, char *argv[])
{
    FILE *code_file;
    int max_steps = 10000;

    state_ptr s = new_state(MEM_SIZE);
    mem_t saver = copy_reg(s->r);
    mem_t savem;
    int step = 0;

    stat_t e = STAT_AOK;

    if (argc < 2 || argc > 3)
        usage(argv[0]);
    code_file = fopen(argv[1], "r");
    if (!code_file) {
        fprintf(stderr, "Can't open code file '%s'\n", argv[1]);
        exit(1);
    }

    if (!load_mem(s->m, code_file, 1)) {
        printf("Exiting\n");
        return 1;
    }

    savem = copy_mem(s->m);

    if (argc > 2){
        max_steps = atoi(argv[2]);
        s->m->useCache(); /* test cache */
    }



    /* run */
    for (step = 0; step < max_steps && e == STAT_AOK; step++){
        printf("step %d\n", step);
        e = step_state(s, stdout);

    }

    printf("Stopped in %d steps at PC = 0x%x.  Status '%s', CC %s\n",
           step, s->pc, stat_name(e), cc_name(s->cc));

    printf("Changes to registers:\n");
    diff_reg(saver, s->r, stdout);

    printf("\nChanges to memory:\n");
    diff_mem(savem, s->m, stdout);

    free_state(s);
    printf("state released\n");
    free_reg(saver);
    printf("saver released\n");
    free_mem(savem);
    printf("savem released\n");

    return 0;
}
