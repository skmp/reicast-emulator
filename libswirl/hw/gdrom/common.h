#pragma once
#include "imgread/imgread.h"

#include <vector>
using namespace std;

#include "deps/coreio/coreio.h"

extern u32 NullDriveDiscType;

bool ConvertSector(u8* in_buff , u8* out_buff , int from , int to,int sector);

bool InitDrive(u32 fileflags=0);
void TermDrive();
bool DiscSwap(u32 fileflags=0);
extern signed int sns_asc;
extern signed int sns_ascq;
extern signed int sns_key;


void PatchRegion_0(u8* sector,int size);
void PatchRegion_6(u8* sector,int size);
void ConvToc(u32* to,TocInfo* from);
void GetDriveToc(u32* to,DiskArea area);
void GetDriveSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz);

void GetDriveSessionInfo(u8* to,u8 session);
int GetFile(char *szFileName, char *szParse=0,u32 flags=0);
int msgboxf(wchar* text,unsigned int type,...);
void printtoc(TocInfo* toc,SessionInfo* ses);
extern u8 q_subchannel[96];

extern Disc* disc;


extern void gd_setdisc();
