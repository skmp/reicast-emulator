// nullAICA.cpp : Defines the entry point for the DLL application.
//

#include "SoundCPU.h"
#include "arm7.h"
#include "arm_mem.h"
#include <memory>

struct SoundCPU_impl : SoundCPU {
	u8* aica_ram;
	u32 aram_size;
	unique_ptr<ARM7Backend> arm;

	SoundCPU_impl(u8* aica_ram, u32 aram_size) : aica_ram(aica_ram), aram_size(aram_size){
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

		if (backend == ARM7BE_INTERPRETER) {
			arm.reset(ARM7Backend::CreateInterpreter(aica_ram, aram_size));
			return true;
		}

		return false;
	}
};

SoundCPU* SoundCPU::Create(u8* aica_ram, u32 aram_size) {
	return new SoundCPU_impl(aica_ram, aram_size);
}

void libARM_SetResetState(bool Reset) {
	sh4_cpu->GetA0H<SoundCPU_impl>(A0H_SCPU)->SetResetState(Reset);
}
void libARM_InterruptChange(u32 bits, u32 L) {
	sh4_cpu->GetA0H<SoundCPU_impl>(A0H_SCPU)->arm->InterruptChange(bits, L);
}