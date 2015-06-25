/******************************************************************************
 *	pipe.h
 *
 *	Code for implementing pipelined processor simulators
 ******************************************************************************/

#ifndef PIPE_H
#define PIPE_H

/******************************************************************************
 *	#includes
 ******************************************************************************/

#include <cstdio>

/* Different control operations for pipeline register */
/* LOAD:   Copy next state to current   */
/* STALL:  Keep current state unchanged */
/* BUBBLE: Set current state to nop     */
/* ERROR:  Occurs when both stall & load signals set */

#define MAX_STAGE 10

/******************************************************************************
 *	typedefs
 ******************************************************************************/

typedef enum { P_LOAD, P_STALL, P_BUBBLE, P_ERROR } p_stat_t;
// forward declaration
class Pipes;

class PipeRec{
public:
    PipeRec(int c, void *bv);
    ~PipeRec();

    void clear();
    void update();

    /* Current and next register state */
    void *current;
    void *next;
    /* Contents of register when bubble occurs */
    void *bubble_val;
    /* Number of state bytes */
    int count;
    /* How should state be updated next time? */
    p_stat_t op;

    friend class Pipes;

private:
    static PipeRec* pipes[MAX_STAGE];
    static int pipe_count;

};
typedef PipeRec pipe_ele;
typedef PipeRec* pipe_ptr;

class Pipes{
public:
    int count;
    pipe_ptr pipes[MAX_STAGE];

    Pipes();
    ~Pipes();
/* Create new pipe with count bytes of state */
/* bubble_val indicates state corresponding to pipeline bubble */
    pipe_ptr add(int c, void *bubble_val);
/* Update all pipes */
    void update();
/* Set all pipes to bubble values */
    void clear();
    pipe_ptr& operator[](int i);

};

#endif /* PIPE_H */



