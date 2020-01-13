// nullAICA.cpp : Defines the entry point for the DLL application.
//

#include "SoundCPU.h"
#include "arm7.h"
#include "arm_mem.h"
#include <memory>

struct SoundCPU_impl : SoundCPU {
	unique_ptr<ARM7Backend> arm;

	SoundCPU_impl() {
		setBackend(ARM7BE_INTERPRETER);
	}

	//called when plugin is used by emu (you should do first time init here)
	bool Init()
	{
		arm->Init();

		return true;
	}

	//called when plugin is unloaded by emu, only if dcInit is called (eg, not called to enumerate plugins)
	void Term()
	{
		//arm7_Term ?
	}

	//It's supposed to reset anything
	void Reset(bool Manual)
	{
		arm->Reset();
		arm->SetEnabled(false);
	}

	void SetResetState(u32 state)
	{
		arm->SetEnabled(state == 0);
	}

	//Mainloop
	void Update(u32 Cycles)
	{
		arm->Run(Cycles / arm_sh4_bias);
	}

	bool setBackend(Arm7Backends backend) {

		arm.reset(Create_ARM7Interpreter());

		return true; 
	}
};

SoundCPU* SoundCPU::Create() {
	return new SoundCPU_impl();
}

void libARM_InterruptChange(u32 bits, u32 L) {
	sh4_cpu->GetA0H<SoundCPU_impl>(A0H_SCPU)->arm->InterruptChange(bits, L);
}