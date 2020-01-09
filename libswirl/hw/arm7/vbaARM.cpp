// nullAICA.cpp : Defines the entry point for the DLL application.
//

#include "types.h"
#include "arm7.h"
#include "arm_mem.h"

struct SoundCPU_impl : SoundCPU {

	//called when plugin is used by emu (you should do first time init here)
	s32 Init()
	{
		init_mem();
		arm_Init();

		return rv_ok;
	}

	//called when plugin is unloaded by emu, only if dcInit is called (eg, not called to enumerate plugins)
	void Term()
	{
		term_mem();
		//arm7_Term ?
	}

	//It's supposed to reset anything
	void Reset(bool Manual)
	{
		arm_Reset();
		arm_SetEnabled(false);
	}

	void SetResetState(u32 state)
	{
		arm_SetEnabled(state == 0);
	}

	//Mainloop
	void Update(u32 Cycles)
	{
		arm_Run(Cycles / arm_sh4_bias);
	}
};

SoundCPU* SoundCPU::Create() {
	return new SoundCPU_impl();
}
