#ifndef _EXC_H
#define _EXC_H

typedef struct {
    unsigned int epc;
    unsigned int at;
    unsigned int v0, v1;
    unsigned int a0, a1, a2, a3;
    unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
    unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
    unsigned int t8, t9;
    unsigned int hi, lo;
    unsigned int gp;
    unsigned int sp;
    unsigned int fp;
    unsigned int ra;
} context;

typedef void (*exc_fn)(unsigned int, unsigned int, context* context);

extern exc_fn exceptions[32];

void do_exceptions(unsigned int status, unsigned int cause, context* context);
void register_exception_handler(int index, exc_fn fn);
void init_exception();

#endif
