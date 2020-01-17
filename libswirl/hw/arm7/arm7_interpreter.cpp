#include "arm7.h"
#include "arm_mem.h"
#include "arm7_context.h"

/*
void CPUSwitchMode(int mode, bool saveState, bool breakLoop=true);
extern "C" void CPUFiq();
void CPUUpdateCPSR();
void CPUUpdateFlags();
void CPUSoftwareInterrupt(int comment);
void CPUUndefinedException();
*/

struct Arm7Interpreter_impl : ARM7Backend {

	Arm7Context* ctx;
	
	Arm7Interpreter_impl(Arm7Context* ctx) : ctx(ctx) {	}

	void UpdateInterrupts()
	{
		ARM7Backend::UpdateInterrupts(ctx);
	}

	void Run(u32 CycleCount)
	{
		StepMany(ctx, CycleCount);
	}

	void InvalidateJitCache() {

	}
};

ARM7Backend* ARM7Backend::CreateInterpreter(Arm7Context* ctx) {
	return new Arm7Interpreter_impl(ctx);
}