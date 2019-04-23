#include "types.h"
#include "maple_if.h"
#include "maple_helper.h"
#include "maple_devs.h"
#include "maple_cfg.h"

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
void UpdateVibration(u32 port, u32 value, u32 max_duration);

extern u32 kcode[4];
extern s8 joyx[4], joyy[4], joyrx[4], joyry[4];
extern u8 rt[4],lt[4];
extern bool enable_purupuru;
extern f32 mo_x_abs[4];
extern f32 mo_y_abs[4];
extern u32 mo_buttons[4];
extern f32 mo_x_delta[4];
extern f32 mo_y_delta[4];
extern f32 mo_wheel_delta[4];

const char* VMU_SCREEN_COLOR_NAMES[VMU_NUM_COLORS] = {
		"DEFAULT_ON",
		"DEFAULT_OFF",
		"BLACK",
		"BLUE",
		"LIGHT_BLUE",
		"GREEN",
		"GREEN_BLUE",
		"GREEN_LIGHT_BLUE",
		"LIGHT_GREEN",
		"LIGHT_GREEN_BLUE",
		"LIGHT_GREEN_LIGHT_BLUE",
		"RED",
		"RED_BLUE",
		"RED_LIGHT_BLUE",
		"RED_GREEN",
		"RED_GREEN_BLUE",
		"RED_GREEN_LIGHT_BLUE",
		"RED_LIGHT_GREEN",
		"RED_LIGHT_GREEN_BLUE",
		"RED_LIGHT_GREEN_LIGHT_BLUE",
		"LIGHT_RED",
		"LIGHT_RED_BLUE",
		"LIGHT_RED_LIGHT_BLUE",
		"LIGHT_RED_GREEN",
		"LIGHT_RED_GREEN_BLUE",
		"LIGHT_RED_GREEN_LIGHT_BLUE",
		"LIGHT_RED_LIGHT_GREEN",
		"LIGHT_RED_LIGHT_GREEN_BLUE",
		"WHITE"
};
const rgb_t VMU_SCREEN_COLOR_MAP[VMU_NUM_COLORS] = {
		{ DEFAULT_VMU_PIXEL_ON_R,   DEFAULT_VMU_PIXEL_ON_G ,   DEFAULT_VMU_PIXEL_ON_B  },
		{ DEFAULT_VMU_PIXEL_OFF_R,  DEFAULT_VMU_PIXEL_OFF_G ,  DEFAULT_VMU_PIXEL_OFF_B  },
		{ 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x7F },
		{ 0x00, 0x00, 0xFF },
		{ 0x00, 0x7F, 0x00 },
		{ 0x00, 0x7F, 0x7F },
		{ 0x00, 0x7F, 0xFF },
		{ 0x00, 0xFF, 0x00 },
		{ 0x00, 0xFF, 0x7F },
		{ 0x00, 0xFF, 0xFF },
		{ 0x7F, 0x00, 0x00 },
		{ 0x7F, 0x00, 0x7F },
		{ 0x7F, 0x00, 0xFF },
		{ 0x7F, 0x7F, 0x00 },
		{ 0x7F, 0x7F, 0x7F },
		{ 0x7F, 0x7F, 0xFF },
		{ 0x7F, 0xFF, 0x00 },
		{ 0x7F, 0xFF, 0x7F },
		{ 0x7F, 0xFF, 0xFF },
		{ 0xFF, 0x00, 0x00 },
		{ 0xFF, 0x00, 0x7F },
		{ 0xFF, 0x00, 0xFF },
		{ 0xFF, 0x7F, 0x00 },
		{ 0xFF, 0x7F, 0x7F },
		{ 0xFF, 0x7F, 0xFF },
		{ 0xFF, 0xFF, 0x00 },
		{ 0xFF, 0xFF, 0x7F },
		{ 0xFF, 0xFF, 0xFF }
};

vmu_screen_params_t vmu_screen_params[4] ;

u8 lightgun_img_crosshair[LIGHTGUN_CROSSHAIR_SIZE*LIGHTGUN_CROSSHAIR_SIZE] =
{
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,
	1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
};

u8 lightgun_palette[LIGHTGUN_COLORS_COUNT*3] =
{
	0xff,0xff,0xff, // LIGHTGUN_COLOR_OFF
	0xff,0xff,0xff, // LIGHTGUN_COLOR_WHITE
	0xff,0x10,0x10, // LIGHTGUN_COLOR_RED
	0x10,0xff,0x10, // LIGHTGUN_COLOR_GREEN
	0x10,0x10,0xff, // LIGHTGUN_COLOR_BLUE
};

lightgun_params_t lightgun_params[4] ;

MapleDeviceType maple_devices[MAPLE_PORTS] =
   { MDT_SegaController, MDT_SegaController, MDT_SegaController, MDT_SegaController };
bool use_lightgun = false;	// Naomi only

u8 GetBtFromSgn(s8 val)
{
	return val+128;
}

struct MapleConfigMap : IMapleConfigMap
{
	maple_device* dev;
	int player_num;

	MapleConfigMap(maple_device* dev, int player_num = -1)
	{
		this->dev=dev;
		this->player_num = player_num;
	}

   void SetVibration(u32 value, u32 max_duration)
   {
      UpdateVibration(player_num == -1 ? dev->bus_id : player_num, value, max_duration);
   }

	void GetInput(PlainJoystickState* pjs)
	{
	   int pnum = player_num == -1 ? dev->bus_id : player_num;
	   UpdateInputState(pnum);

	   pjs->kcode=kcode[pnum];
	   if (settings.System == DC_PLATFORM_DREAMCAST)
		  pjs->kcode |= 0xF901;
	   pjs->joy[PJAI_X1]=GetBtFromSgn(joyx[pnum]);
	   pjs->joy[PJAI_Y1]=GetBtFromSgn(joyy[pnum]);
	   pjs->joy[PJAI_X2]=GetBtFromSgn(joyrx[pnum]);
	   pjs->joy[PJAI_Y2]=GetBtFromSgn(joyry[pnum]);
	   pjs->trigger[PJTI_R]=rt[pnum];
	   pjs->trigger[PJTI_L]=lt[pnum];
	}
	void SetImage(void* img)
	{
		vmu_screen_params[player_num == -1 ? dev->bus_id : player_num].vmu_screen_needs_update = true ;
	}
	void GetAbsolutePosition(f32 *px, f32 *py)
	{
	   int pnum = player_num == -1 ? dev->bus_id : player_num;
	   *px = mo_x_abs[pnum];
	   *py = mo_y_abs[pnum];
	}
	void GetMouse(u32 *buttons, f32 *delta_x, f32 *delta_y, f32 *delta_wheel)
	{
	   int pnum = player_num == -1 ? dev->bus_id : player_num;
	   *buttons = mo_buttons[pnum];
	   *delta_x = mo_x_delta[pnum];
	   *delta_y = mo_y_delta[pnum];
	   *delta_wheel = mo_wheel_delta[pnum];

	   mo_x_delta[pnum] = 0;
	   mo_y_delta[pnum] = 0;
	   mo_wheel_delta[pnum] = 0;
	}
};

void mcfg_Create(MapleDeviceType type,u32 bus,u32 port, int player_num /* = -1 */)
{
	maple_device* dev=maple_Create(type);
	dev->Setup(maple_GetAddress(bus,port));
	dev->config = new MapleConfigMap(dev, player_num);
	dev->OnSetup();
	MapleDevices[bus][port]=dev;
}

void mcfg_CreateDevices()
{
   int bus ;

   for ( bus = 0 ; bus < MAPLE_PORTS; bus++)
      vmu_screen_params[bus].vmu_lcd_screen = NULL ;


   if (settings.System == DC_PLATFORM_DREAMCAST)
   {
	  for (bus = 0; bus < MAPLE_PORTS; ++bus)
	  {
		 switch (maple_devices[bus])
		 {
		 case MDT_SegaController:
			mcfg_Create(MDT_SegaController, bus, 5);
			mcfg_Create(MDT_SegaVMU, bus, 0);

			if (enable_purupuru)
			   mcfg_Create(MDT_PurupuruPack, bus, 1);
			else
			   mcfg_Create(MDT_SegaVMU, bus, 1);
			break;

		 case MDT_Keyboard:
			 mcfg_Create(MDT_Keyboard, bus, 5);
			 break;

		 case MDT_Mouse:
			 mcfg_Create(MDT_Mouse, bus, 5);
			 break;

		 case MDT_LightGun:
			 mcfg_Create(MDT_LightGun, bus, 5);
			 if (enable_purupuru)
				mcfg_Create(MDT_PurupuruPack, bus, 0);
			 else
				mcfg_Create(MDT_SegaVMU, bus, 0);
			 break;
		 }
	  }
   }
   else if (settings.System == DC_PLATFORM_NAOMI)
   {
      use_lightgun = false;
	  for (int i = 0; i < MAPLE_PORTS; i++)
		 if (maple_devices[i] == MDT_LightGun)
			use_lightgun = true;

	  bus = 0;
      mcfg_Create(MDT_NaomiJamma, bus++, 5);
	  for (int i = 0; i < MAPLE_PORTS; i++)
	  {
		 switch (maple_devices[i])
		 {
		 case MDT_Keyboard:
			mcfg_Create(MDT_Keyboard, bus++, 5);
			break;
		 default:
			break;
		 }
	  }
   }
   else if (settings.System == DC_PLATFORM_ATOMISWAVE)
   {
	  mcfg_Create(MDT_SegaController, 0, 5);
	  mcfg_Create(MDT_SegaController, 1, 5);
	  switch (settings.mapping.JammaSetup)
	  {
	  case 5:
		 // 2 players with analog axes
		 mcfg_Create(MDT_SegaController, 2, 5, 0);
		 mcfg_Create(MDT_SegaController, 3, 5, 1);
		 break;
	  case 6:
		 // Light guns
		 mcfg_Create(MDT_LightGun, 2, 5, 0);
		 mcfg_Create(MDT_LightGun, 3, 5, 1);
		 break;
	  case 2:
		 // Track-ball
		 mcfg_Create(MDT_Mouse, 2, 5, 0);
		 break;
	  default:
		 // 4 players
		 mcfg_Create(MDT_SegaController, 2, 5);
		 mcfg_Create(MDT_SegaController, 3, 5);
		 break;
	  }
   }
}


void mcfg_DestroyDevices()
{
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j=0;j<=5;j++)
		{
			delete MapleDevices[i][j];
			MapleDevices[i][j] = NULL;
		}
}

void mcfg_DestroyDevice(int i, int j)
{
	delete MapleDevices[i][j];
	MapleDevices[i][j] = NULL;
}

void mcfg_SerializeDevices(void **data, unsigned int *total_size)
{
	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 **p = (u8 **)data;
			if (MapleDevices[i][j] != NULL)
			{
				if (*p != NULL)
				{
					**p = MapleDevices[i][j]->get_device_type();
					*p = *p + 1;
				}
				MapleDevices[i][j]->maple_serialize(data, total_size);
			}
			else if (*p != NULL)
			{
				**p = MDT_None;
				*p = *p + 1;
			}
			*total_size = *total_size + 1;
		}
}

void mcfg_UnserializeDevices(void **data, unsigned int *total_size)
{
	mcfg_DestroyDevices();

	for (int i = 0; i < MAPLE_PORTS; i++)
		for (int j = 0; j < 6; j++)
		{
			u8 **p = (u8 **)data;
			MapleDeviceType device_type = (MapleDeviceType)**p;
			*p = *p + 1;
			*total_size = *total_size + 1;
			if (device_type != MDT_None)
			{
				mcfg_Create(device_type, i, j);
				MapleDevices[i][j]->maple_unserialize(data, total_size);
			}
		}
}
