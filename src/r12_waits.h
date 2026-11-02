#pragma once
#ifndef BA_R12_DIAG
#define BA_R12_DIAG 0
#endif
#if BA_R12_DIAG
void r12_begin(void);void r12_before_present(void);void r12_end(void);
void r12_wait_record(unsigned,unsigned long long);
void r12_index_range(unsigned,unsigned,unsigned);
#else
#define r12_begin() ((void)0)
#define r12_before_present() ((void)0)
#define r12_end() ((void)0)
#define r12_wait_record(a,b) ((void)0)
#define r12_index_range(a,b,c) ((void)0)
#endif
