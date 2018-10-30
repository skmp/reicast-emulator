/*
	Basic gdrom syscall emulation
	Adapted from some (very) old pre-nulldc hle code
*/

#include <stdio.h>
#include "types.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"

#include "gdrom_hle.h"
#include "oslib/logging.h"

/* BEAUTIFUL */
#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

void GDROM_HLE_ReadSES(u32 addr)
{
	/* "Where's da READMEH sonny?" */
	u32 s = ReadMem32(addr + 0);
	u32 b = ReadMem32(addr + 4);
	u32 ba = ReadMem32(addr + 8);
	u32 bb = ReadMem32(addr + 12);

	LOG_I("gdrom_hle", "GDROM_HLE_ReadSES: doing nothing w/ %d, %d, %d, %d\n", s, b, ba, bb);
}

void GDROM_HLE_ReadTOC(u32 Addr)
{
	/* Description goEs here I think */
	u32 s = ReadMem32(Addr + 0);
	u32 b = ReadMem32(Addr + 4);

	u32* pDst = (u32*)GetMemPtr(b, 0);

	LOG_D( "gdrom_hle", "READ TOC : %X %X \n\n", s, b);

	libGDR_GetToc(pDst, s);

	/* The syscall swaps to LE(Little Endian) it seems */
	for (int i = 0; i < 102; ++i)
	{
		pDst[i] = SWAP32(pDst[i]);
	}
}

void read_sectors_to(u32 addr, u32 sector, u32 count)
{
	u8 * pDst = GetMemPtr(addr, 0);

	if (pDst)
	{
		libGDR_ReadSector(pDst, sector, count, 2048);
	}
	else
	{
		u8 temp[2048];

		while (count > 0)
		{
			libGDR_ReadSector(temp, sector, 1, 2048);

			for (int i = 0; i < 2048 / 4; i += 4)
			{
				WriteMem32(addr, temp[i]);
				addr += 4;
			}

			sector++;
			count--;
		}
	}

}

void GDROM_HLE_ReadDMA(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);



	LOG_D( "gdrom_hle", "\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);
	read_sectors_to(b, s, n);
}

void GDROM_HLE_ReadPIO(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	LOG_D( "gdrom_hle", "\tPIO READ Sector=%d, Num=%d, Buffer=0x%08X, Unk01=0x%08X\n", s, n, b, u);

	read_sectors_to(b, s, n);
}

void GDCC_HLE_GETSCD(u32 addr)
{
	u32 s = ReadMem32(addr + 0x00);
	u32 n = ReadMem32(addr + 0x04);
	u32 b = ReadMem32(addr + 0x08);
	u32 u = ReadMem32(addr + 0x0C);

	LOG_V( "gdrom_hle", "Doing nothing for GETSCD [0]=%d, [1]=%d, [2]=0x%08X, [3]=0x%08X\n", s, n, b, u);
}

#define r Sh4cntx.r
u32 SecMode[4];

void GD_HLE_Command(u32 cc, u32 prm)
{
	/* Oops, there goes a description */
	switch(cc)
	{
		case GDCC_GETTOC:
		{
			/* FIXME */
			LOG_W( "gdrom_hle", "*FIXME* CMD GETTOC CC:%X PRM:%X\n",cc,prm);
			break;
		}

		case GDCC_GETTOC2:
		{
			GDROM_HLE_ReadTOC(r[5]);
			break;
		}

		case GDCC_GETSES:
		{
			LOG_D( "gdrom_hle", "GETSES CC:%X PRM:%X\n", cc, prm);
			GDROM_HLE_ReadSES(r[5]);
			break;
		}

		case GDCC_INIT:
		{
			LOG_D( "gdrom_hle", "CMD INIT CC:%X PRM:%X\n", cc, prm);
			break;
		}

		case GDCC_PIOREAD:
		{
			GDROM_HLE_ReadPIO(r[5]);
			break;
		}

		case GDCC_DMAREAD:
		{
			LOG_D( "gdrom_hle", "CMD DMAREAD CC:%X PRM:%X\n", cc, prm);
			GDROM_HLE_ReadDMA(r[5]);
			break;
		}

		case GDCC_PLAY_SECTOR:
		{
			LOG_V( "gdrom_hle", "CMD PLAYSEC? CC:%X PRM:%X\n", cc, prm);
			break;
		}

		case GDCC_RELEASE:
		{
			LOG_V( "gdrom_hle", "CMD RELEASE? CC:%X PRM:%X\n", cc, prm);
			break;
		}

		case GDCC_STOP:	{ LOG_V( "gdrom_hle", "CMD STOP CC:%X PRM:%X\n", cc, prm);	break; }
		case GDCC_SEEK:	{ LOG_V( "gdrom_hle", "CMD SEEK CC:%X PRM:%X\n", cc, prm);	break; }
		case GDCC_PLAY:	{ LOG_V( "gdrom_hle", "CMD PLAY CC:%X PRM:%X\n", cc, prm);	break; }
		case GDCC_PAUSE:{ LOG_V( "gdrom_hle", "CMD PAUSE CC:%X PRM:%X\n", cc, prm);	break; }

		case GDCC_READ:
		{
			LOG_D( "gdrom_hle", "CMD READ CC:%X PRM:%X\n", cc, prm);
			break;
		}

		case GDCC_GETSCD:
		{
			LOG_D( "gdrom_hle", "GETSCD CC:%X PRM:%X\n",cc,prm);
			GDCC_HLE_GETSCD(r[5]);
			break;
		}

		default: { LOG_W( "gdrom_hle", "Unknown GDROM CC:%X PRM:%X\n", cc, prm); break; }
	}
}

void gdrom_hle_op()
{
	/* If only a Description could be added, mhmm.. */

	static u32 last_cmd = 0xFFFFFFFF;		// only works for last cmd, might help somewhere
	static u32 dwReqID = 0xF0FFFFFF;		// ReqID, starting w/ high val

	/* GDROM SYSCALL */
	if( SYSCALL_GDROM == r[6] )
	{
		/* COMMAND MODE */
		switch(r[7])
		{
			/* *FIXME* NEED RET */

			/* cSEND GDROM COMMAND RET: - if failed + req id */
			case GDROM_SEND_COMMAND:
			{
				LOG_D("gdrom_hle", "HLE SEND COMMAND CC:%X  param ptr: %X\n", r[4], r[5]);
				GD_HLE_Command(r[4],r[5]);

				/* RET Request ID */
				last_cmd = r[0] = --dwReqID;

				break;
			}
			case GDROM_CHECK_COMMAND:
			{
				/* RET Finished: Invalid */
				r[0] = last_cmd == r[4] ? 2 : 0;

				LOG_D("gdrom_hle", "HLE CHECK COMMAND REQID:%X  param ptr: %X -> %X\n", r[4], r[5], r[0]);

				/* INVALIDATE CHECK CMD */
				last_cmd = 0xFFFFFFFF;

				break;
			}

			/* NO return, NO params */
			case GDROM_MAIN:
			{
				LOG_D("gdrom_hle", "HLE GDROM_MAIN\n");
				break;
			}

			case GDROM_INIT:	{ LOG_V("gdrom_hle", "HLE GDROM_INIT\n");		break; }
			case GDROM_RESET:	{ LOG_V("gdrom_hle", "HLE GDROM_RESET\n");	break; }

			case GDROM_CHECK_DRIVE:
			{
				LOG_D("gdrom_hle", "HLE GDROM_CHECK_DRIVE r4:%X\n",r[4],r[5]);

				/* STANDBY */
				WriteMem32(r[4] + 0, 0x02);

				/* CDROM | 0x80 for GDROM */
				WriteMem32(r[4] + 4, libGDR_GetDiscType());

				/* RET SUCCESS */
				r[0] = 0;
				break;
			}

			case GDROM_ABORT_COMMAND:
			{
				LOG_D("gdrom_hle", "HLE GDROM_ABORT_COMMAND r4:%X\n",r[4],r[5]);

				/* RET FAILURE */
				r[0] = -1;
				break;
			}

			case GDROM_SECTOR_MODE:
			{
				/* THERE GOES YOUR DESCRIPTION!!! */
				LOG_V( "gdrom_hle", "HLE GDROM_SECTOR_MODE PTR_r4:%X\n",r[4]);
				for(int i = 0; i < 4; i++)
				{
					SecMode[i] = ReadMem32(r[4] +( i << 2));
					LOG_V("gdrom_hle", "%08X%s", SecMode[i], ((3 == i) ? "\n" : "\t"));
				}

				/* RET SUCCESS */
				r[0] = 0;
				break;
			}

			default: { LOG_D("gdrom_hle", "Unknown SYSCALL: %X\n", r[7]); break; }
		}
	}

	/*  ~~~ MISC ~~~ */
	else
	{
		LOG_D("gdrom_hle", "SYSCALL:\tSYSCALL: %X\n",r[7]);
	}
}
