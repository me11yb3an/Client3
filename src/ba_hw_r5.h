#pragma once
#include <stdint.h>
int h5_mode(void); /* A=no graphics init; B=initialized idle; C=full; N=UI/minimap */
int h5_no_init(void);
int h5_silent(void);
int h5_measuring(void);
int h5_graph(unsigned kind); /* records attempts; 1 denies work in measured A/B */
void h5_init(void);
void h5_record(unsigned,uint64_t);
int h5_step(int ready,int x,int z); /* 1 = bounded benchmark completed */
void h5_mark(const char*);
int h5_requested_run(void);
int h5_pool_experiment(void);
void h5_scope(unsigned,uint64_t,uint64_t);
const char *h5_username(void);
double h5_random(void);
void h5_render_phase(int);
void h5_allocation(unsigned,uint64_t,int);
int h5_startup(void);
