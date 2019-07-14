#pragma once

extern "C" void no_update();
extern "C" void intc_sched();
extern "C" void ngen_blockcheckfail();


extern "C" void ngen_LinkBlock_Generic_stub();
extern "C" void ngen_LinkBlock_cond_Branch_stub();
extern "C" void ngen_LinkBlock_cond_Next_stub();

extern "C" void ngen_FailedToFindBlock_();
extern "C" void ngen_mainloop(void*);