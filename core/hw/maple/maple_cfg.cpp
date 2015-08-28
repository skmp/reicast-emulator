#include "types.h"
#include "maple_if.h"
#include "maple_helper.h"
#include "maple_devs.h"
#include "maple_cfg.h"
#include "maple_controller.h"	

#define HAS_VMU
/*
bus_x=0{p0=1{config};p1=2{config};config;}
Plugins:
	Input Source
		EventMap -- 'Raw' interface, source_name[seid]:mode
		KeyMap -- translated chars ( no re-mapping possible)
	Output
		Image
		
*/
/*
	MapleConfig:
		InputUpdate(&fmt);
		ImageUpdate(data);
*/
void UpdateInputState(u32 port);

u8 GetBtFromSgn(s8 val)
{
	return val+128;
}

struct MapleConfigMap : IMapleConfigMap
{
	maple_device* dev;

	MapleConfigMap(maple_device* dev)
	{
		this->dev=dev;
	}

	void GetInput(PlainJoystickState* pjs)
	{
		UpdateInputState(dev->bus_id);

		pjs->kcode           = maple_controller[dev->bus_id].buttons | 0xF901;
		pjs->joy[PJAI_X1]    = GetBtFromSgn(maple_controller[dev->bus_id].stick_x);
		pjs->joy[PJAI_Y1]    = GetBtFromSgn(maple_controller[dev->bus_id].stick_y);
		pjs->trigger[PJTI_R] = maple_controller[dev->bus_id].trigger_right;
		pjs->trigger[PJTI_L] = maple_controller[dev->bus_id].trigger_left;
	}
	void SetImage(void* img)
	{
		//?
	}
};

void mcfg_Create(MapleDeviceType type,u32 bus,u32 port)
{
	maple_device* dev=maple_Create(type);
	dev->Setup(maple_GetAddress(bus,port));
	dev->config = new MapleConfigMap(dev);
	dev->OnSetup();
	MapleDevices[bus][port]=dev;
}

void mcfg_CreateDevices()
{
	#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
		for(int port = 0; port < MAPLE_NUM_PORTS; port++)
		{
			if(maple_controller[port].enabled)
			{
				mcfg_Create(MDT_SegaController, port, 5);
				mcfg_Create(MDT_SegaVMU,        port, 0);
				mcfg_Create(MDT_SegaVMU,        port, 1);
			}
		}
	#else
		mcfg_Create(MDT_NaomiJamma, 0, 5);
	#endif
}

void mcfg_DestroyDevices()
{
	for (int i=0;i<=3;i++)
		for (int j=0;j<=5;j++)
			delete MapleDevices[i][j];
}
