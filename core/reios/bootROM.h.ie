/* Various stuff for boot ROM */

#ifndef _BOOTROM_H_
#define _BOOTROM_H_

/* System variables */
struct _sysvars {
	uint32_t error_code[3];		/* 8c000000 - 8c000008 */
	uint16_t var1;			/* 8c00000c */
	uint16_t var2;			/* 8c00000e */
	uint32_t rte_code[2];		/* 8c000010 - 8c000014 */
	uint32_t rts_code[2];		/* 8c000018 - 8c00001c */
	volatile uint8_t irq_sem0;	/* 8c000020 */
	volatile uint8_t irq_sem1;	/* 8c000021 */
	volatile uint8_t irq_sem2;	/* 8c000022 */
	volatile uint8_t irq_sem3;	/* 8c000023 */
	uint8_t OS_type;		/* 8c000024 */
	uint8_t menu_param;		/* 8c000025 */
	uint8_t display_cable;		/* 8c000026 */
	uint8_t date_set;		/* 8c000027 */
	volatile uint32_t timer_count;	/* 8c000028 */
	uint8_t unknown0;		/* 8c00002c */
	uint8_t unknown1;		/* 8c00002d */
	uint16_t unknown2;		/* 8c00002e */
	struct {
		uint32_t stat0;		/* 8c000030 */
		uint32_t stat1;		/* 8c000034 */
		uint32_t stat2;		/* 8c000038 */
		uint32_t stat3;		/* 8c00003c */
	} gd_stat;
	uint32_t gdhn;			/* 8c000040 */
	uint16_t gd_unknown0;		/* 8c000044 */
	uint16_t gd_unknown1;		/* 8c000046 */
	struct {
		gd_drv_stat_t stat;	/* 8c000048 */
		gd_media_t media;	/* 8c00004c */
	} gd_drv;
	uint32_t *unknown_vector0;	/* 8c000050 */
	uint32_t *unknown_vector1;	/* 8c000054 */
	uint32_t *unknown_vector2;	/* 8c000058 */
	uint32_t *unknown_data_vector3;	/* 8c00005c */
	uint32_t current_color;		/* 8c000060 */
	uint32_t *IP_vector;		/* 8c000064 */
	uint8_t dreamcast_ID[8];	/* 8c000068 - 8c00006f */
	struct {
		union {
			uint8_t reserved[15];	/* 8c000070 - 8c000075 */
			struct {
				uint8_t unknown0;	/* 8c000070 */
				uint8_t unknown1;	/* 8c000071 */
				uint8_t levelHI;	/* 8c000072 */
				uint8_t unknown2;	/* 8c000073 */
				uint8_t levelLO;	/* 8c000074 */
				uint8_t unknown3;	/* 8c000075 */
				uint8_t debug_level;	/* 8c000076 */
				uint8_t reserved[9];	/* 8c000077-8c00007f */
			} switches;
		} u0;
	} debug_switches;
	uint32_t *irq_callback0;		/* 8c000080 */
	uint32_t *irq_callback1;		/* 8c000084 */
	uint32_t *irq_callback2;		/* 8c000088 */
	uint32_t *irq_callback3;		/* 8c00008c */
	uint16_t disc_type;		/* 8c000090 */
	uint16_t old_disc_type;		/* 8c000092 */
	uint32_t *old_gd_vector2;	/* 8c000094 */
	uint32_t irq_unknown0;		/* 8c000098 */
	uint8_t *boot_file;		/* 8c00009c */
	uint32_t *unknown3;		/* 8c0000a0 */
	uint32_t *menu_addr;		/* 8c0000a4 */
	uint32_t *flash_addr;		/* 8c0000a8 */
	uint32_t *gd_base_reg;		/* 8c0000ac */
	uint32_t *cfg_vector;		/* 8c0000b0 */
	uint32_t *fnt_vector;		/* 8c0000b4 */
	uint32_t *fl_vector;		/* 8c0000b8 */
	uint32_t *gd_vector;		/* 8c0000bc */
	uint32_t *gd_vector2;		/* 8c0000c0 */
	uint32_t unknown4;		/* 8c0000c4 */
	uint32_t unknown5;		/* 8c0000c8 */
	uint32_t unknown6;		/* 8c0000cc */
	uint32_t unknown7;		/* 8c0000d0 */
	uint32_t unknown8;		/* 8c0000d4 */
	uint32_t unknown9;		/* 8c0000d8 */
	uint32_t unknown10;		/* 8c0000dc */
	uint32_t *system_vector;	/* 8c0000e0 */
	uint32_t select_menu;		/* 8c0000e4 */
	sint32_t gd_cmd_stat;		/* 8c0000e8 */
	uint32_t gd_cmd;		/* 8c0000ec */
	uint32_t gd_param1;		/* 8c0000f0 */
	uint32_t gd_param2;		/* 8c0000f4 */
	uint32_t gd_param3;		/* 8c0000f8 */
	uint32_t gd_param4;		/* 8c0000fc */
} *sysvars = (struct _sysvars *)0x8c000000;

/* GD Commands for BootROM API (eg. gdGdcReqCmd et al) */
typedef enum {
	CMD_PIOREAD	= 16,
	CMD_DMAREAD	= 17,
	CMD_GETTOC	= 18,
	CMD_GETTOC2	= 19,
	CMD_PLAY	= 20,
	CMD_PLAY2	= 21,
	CMD_PAUSE	= 22,
	CMD_RELEASE	= 23,
	CMD_INIT	= 24,
	CMD_SEEK	= 27,
	CMD_READ	= 28,
	CMD_STOP	= 33,
	CMD_GETSCD	= 34,
	CMD_GETTRACKS	= 35,
} gd_cmd_t;

/* Different media types */
typedef enum {
	TYPE_CDDA	= 0x00,
	TYPE_CDROM	= 0x10,
	TYPE_CDROMXA	= 0x20,
	TYPE_CDI	= 0x30,
	TYPE_GDROM	= 0x80,
} gd_media_t;

/* GD status */
typedef enum {
	STAT_BUSY	= 0,
	STAT_PAUSE	= 1,
	STAT_STANDBY	= 2,
	STAT_PLAY	= 3,
	STAT_SEEK	= 4,
	STAT_SCAN	= 5,
	STAT_OPEN	= 6,
	STAT_NODISK	= 7,
	STAT_RETRY	= 8,
	STAT_ERROR	= 9,
} gd_drv_stat_t;

/* security */
typedef enum {
	SECURITY_NG	= 0x16,
	SECURITY_OK	= 0x17,
} gd_security_t;

/* c000 commands */
typedef enum {
	INIT		= 0,
	UNKNOWN1	= 1,
	UNKNOWN2	= 2,
	SETDATE		= 3,
	OPENMENU	= 4,
} c000_cmd_t;

/* select_menu options */
typedef enum {
	UNKNOWN		-3,
	NORMAL		0,
	OPENMENU	1,
	OPENCDMENU	3,
} menu_select_t;

/* Hardware register access */
#define HW8(reg)	((volatile uint8_t *)reg)
#define HW16(reg)	((volatile uint16_t *)reg)
#define HW32(reg)	((volatile uint32_t *)reg)

/* bootstrap routine that copies bootROM to RAM */
uint16_t romcopy[] = {
	0xd204, 0xd106, 0x2122, 0xd204, 0x6106, 0x2312, 0x7304, 0x4210,
	0x8bfa, 0x8915, 0xffff, 0x001f, 0xffc0, 0x0007, 0x74e4, 0xa05f
};
/* The above is the following function:
 *
 * void
 * romcopy(src, dst)
 * 	uint32_t *src, *dst;
 * {
 * 	uint32_t i;
 *
 * 	*(uint32_t *)0xa05f74e4 = 0x001fffff;
 * 	for (i = 0; i < 0x0007ffc0; i++) {
 * 		*dst++ = *src++;
 * 	}
 * 	boot2(0);
 * }
 */

uint32_t *sys_callback[] = {
   boot3, _8c000408, syBtExit, boot5, syBtExit,
   syBtCheckDisc, syBtExit, system_reset
};

enum {
	BOOT3		= -3,
	_8C00408	= -2,
	SYBTEXIT1	= -1,
	BOOT5		= 0,
	SYBTEXIT	= 1,
	SYBTCHECKDISC	= 2,
	SYBTEXIT3	= 3,
	SYSTEM_RESET	= 4,
};

uint32_t irq_callback[64];		/* _8c0001c8 */

struct _patch_data {
	uint32_t *addr;
	uint16_t opcode;
} patch_data[] = {
	{ 0xac0090d8, 0x5113, }
	{ 0xac00940a, 0x000b, }
	{ 0xac00940c, 0x0009, }
};

struct _gd_func {
	void (*gdGdcReqCmd)(),		/* 0 */
	void (*gdGdcGetCmdStat)(),	/* 1 */
	void (*gdGdcExecServer)(),	/* 2 */
	void (*gdGdcInitSystem)(),	/* 3 */
	void (*gdGdcGetDrvStat)(),	/* 4 */
	void (*gdGdcG1DmaEnd)(),	/* 5 */
	void (*gdGdcReqDmaTrans)(),	/* 6 */
	void (*gdGdcCheckDmaTrans)(),	/* 7 */
	void (*gdGdcReadAbort)(),	/* 8 */
	void (*gdGdcReset)(),		/* 9 */
	void (*gdGdcChangeDataType)(),	/* 10 */
	void (*_8c0032da)(),		/* 11 */
	void (*_8c003374)(),		/* 12 */
	void (*_8c00333c)(),		/* 13 */
	void (*_8c003774)(),		/* 14 */
	void (*_8c003774)(),		/* 15 */
} gd_func;

uint32_t *gd_cmd[] {
	_8c003774,		/*  0 */
	_8c003774,		/*  1 */
	_8c00262c,		/*  2 */
	_8c003774,		/*  3 */
	_8c00264c,		/*  4 */
	_8c003774,		/*  5 */
	_8c003774,		/*  6 */
	_8c003774,		/*  7 */
	_8c003774,		/*  8 */
	_8c003774,		/*  9 */
	_8c003774,		/* 10 */
	_8c003774,		/* 11 */
	_8c003774,		/* 12 */
	_8c003774,		/* 13 */
	_8c003774,		/* 14 */
	_8c003774,		/* 15 */
	_8c001aee,		/* 16 */
	gd_dmaread,		/* 17 _8c001b2c */
	gd_gettoc,		/* 18 _8c001c34 */
	gd_gettoc2,		/* 19 _8c001ca8 */
	gd_play,		/* 20 _8c001d3a */
	gd_play2,		/* 21 _8c001e3e */
	gd_pause,		/* 22 _8c001e7e */
	gd_release,		/* 23 _8c001ea8 */
	gd_init,		/* 24 _8c001fa8 */
	_8c00377c,		/* 25 */
	_8c00214a,		/* 26 */
	gd_seek,		/* 27 _8c00216c */
	gd_dmaread2,		/* 28 _8c0021fe */
	_8c00257a,		/* 29 */
	_8c0024a6,		/* 30 */
	_8c002520,		/* 31 */
	_8c002588,		/* 32 */
	gd_stop,		/* 33 _8c001ebc */
	gd_getscd,		/* 34 _8c0023a4 */
	gd_gettracks,	/* 35 _8c0025bc */
	_8c002404,		/* 36 */
	_8c002322,		/* 37 */
	_8c00221e,		/* 38 */
	_8c002342,		/* 39 */
	_8c001acc,		/* 40 */
	_8c003774,		/* 41 */
	_8c003774,		/* 42 */
	_8c003774,		/* 43 */
	_8c003774,		/* 44 */
	_8c003774,		/* 45 */
	_8c003774,		/* 46 */
	_8c003774,		/* 47 */
};

/* This from mkisofs */
#define ISODCL(from, to) (to - from + 1)

struct _iso_primary_descriptor {
	uint8_t type			[ISODCL (  1,   1)]; /* 711 */
	uint8_t id			[ISODCL (  2,   6)];
	uint8_t version			[ISODCL (  7,   7)]; /* 711 */
	uint8_t unused1			[ISODCL (  8,   8)];
	uint8_t system_id		[ISODCL (  9,  40)]; /* achars */
	uint8_t volume_id		[ISODCL ( 41,  72)]; /* dchars */
	uint8_t unused2			[ISODCL ( 73,  80)];
	uint8_t volume_space_size	[ISODCL ( 81,  88)]; /* 733 */
	uint8_t unused3			[ISODCL ( 89, 120)];
	uint8_t volume_set_size		[ISODCL (121, 124)]; /* 723 */
	uint8_t volume_sequence_number	[ISODCL (125, 128)]; /* 723 */
	uint8_t logical_block_size	[ISODCL (129, 132)]; /* 723 */
	uint8_t path_table_size		[ISODCL (133, 140)]; /* 733 */
	uint8_t type_l_path_table	[ISODCL (141, 144)]; /* 731 */
	uint8_t opt_type_l_path_table	[ISODCL (145, 148)]; /* 731 */
	uint8_t type_m_path_table	[ISODCL (149, 152)]; /* 732 */
	uint8_t opt_type_m_path_table	[ISODCL (153, 156)]; /* 732 */
	uint8_t root_directory_record	[ISODCL (157, 190)]; /* 9.1 */
	uint8_t volume_set_id		[ISODCL (191, 318)]; /* dchars */
	uint8_t publisher_id		[ISODCL (319, 446)]; /* achars */
	uint8_t preparer_id		[ISODCL (447, 574)]; /* achars */
	uint8_t application_id		[ISODCL (575, 702)]; /* achars */
	uint8_t copyright_file_id	[ISODCL (703, 739)]; /* 7.5 dchars */
	uint8_t abstract_file_id	[ISODCL (740, 776)]; /* 7.5 dchars */
	uint8_t bibliographic_file_id	[ISODCL (777, 813)]; /* 7.5 dchars */
	uint8_t creation_date		[ISODCL (814, 830)]; /* 8.4.26.1 */
	uint8_t modification_date	[ISODCL (831, 847)]; /* 8.4.26.1 */
	uint8_t expiration_date		[ISODCL (848, 864)]; /* 8.4.26.1 */
	uint8_t effective_date		[ISODCL (865, 881)]; /* 8.4.26.1 */
	uint8_t file_structure_version	[ISODCL (882, 882)]; /* 711 */
	uint8_t unused4			[ISODCL (883, 883)];
	uint8_t application_data	[ISODCL (884, 1395)];
	uint8_t unused5			[ISODCL (1396, 2048)];
};

struct _iso_directory_record {
	unsigned char length		[ISODCL (1, 1)]; /* 711 */
	char ext_attr_length		[ISODCL (2, 2)]; /* 711 */
	char extent1			[ISODCL (3, 6)]; /* 733 */
	char extent2			[ISODCL (7, 10)];
	char size1			[ISODCL (11, 14)];
	char size2			[ISODCL (15, 18)];
	char date			[ISODCL (19, 25)]; /* 7 by 711 */
	char flags			[ISODCL (26, 26)];
	char file_unit_size		[ISODCL (27, 27)]; /* 711 */
	char interleave			[ISODCL (28, 28)]; /* 711 */
	char volume_sequence_number	[ISODCL (29, 32)]; /* 723 */
	unsigned char name_len		[ISODCL (33, 33)]; /* 711 */
	char name			[34]; /* Not really, but we need something here */
};

typedef struct {
	int unknown;		/* offset _0000 */
	int unknown;		/* offset _0004 */
	int gd_cmd;		/* offset _0010 */
	int gd_cmd_stat;	/* offset _0014 */
	int param[16];	/* offset _0060 to _0088 */
	int gd_hw_base;	/* offset _00a0 */
	int transfered;	/* offset _00a4 */
	int drv_stat;	/* offset _00ac */
	int drv_media;	/* offset _00b0 */
	int cmd_abort;	/* offset _00b8 */
	int gdchn;		/* offset _00c0 */
	int gdplaying;	/* offset _00c4 */
	int sector_mode;	/* offset _00c8 */
	int sector_size;	/* offset _00cc */
	short *pioaddr;		/* offset _00e0 */
	int piosize;		/* offset _00e4 */
	int cmd_stuff[48];	/* offset _00e8 to _01a8 */
	TOC TOCS[2];		/* offset _01a8 to _0344; from _0348 to _04e4 */
	int cmdp[48];	/* offset _04e8 to _05a8 */
} GDS;

GDS gd_gds;		/* _8c0012e8 */

typedef struct {
	char toc_buf[408];
	int type;
	char *buf;
} toc_t;

typedef struct {
	char hardware_ID[16];
	char maker_ID[16];
	char device_info[16];
	char country_codes[8];
	char ctrl[4];
	char dev[1];
	char VGA[1];
	char WinCE[1];
	char product_ID[9];
	char product_version[6];
	char release_date[16];
	char boot_file[16];
	char software_maker_info[16];
	char title[32];
} ip_t;

#endif	/* !_BOOTROM_H_ */
