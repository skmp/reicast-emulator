#include "disc_common.h"

extern signed int sns_asc;
extern signed int sns_ascq;
extern signed int sns_key;


u8 q_subchannel[96];		//latest q subcode

u32 NullDriveDiscType;
Disc* disc;

void PatchRegion_0(u8* sector,int size)
{
	if (settings.imgread.PatchRegion==0)
		return;

	u8* usersect=sector;

	if (size!=2048)
	{
		printf("PatchRegion_0 -> sector size %d , skipping patch\n",size);
	}

	//patch meta info
	u8* p_area_symbol=&usersect[0x30];
	memcpy(p_area_symbol,"JUE     ",8);
}
void PatchRegion_6(u8* sector,int size)
{
#ifndef NOT_REICAST
	if (settings.imgread.PatchRegion==0)
		return;
#else
	return;
#endif

	u8* usersect=sector;

	if (size!=2048)
	{
		printf("PatchRegion_6 -> sector size %d , skipping patch\n",size);
	}

	//patch area symbols
	u8* p_area_text=&usersect[0x700];
	memcpy(&p_area_text[4],"For JAPAN,TAIWAN,PHILIPINES.",28);
	memcpy(&p_area_text[4 + 32],"For USA and CANADA.         ",28);
	memcpy(&p_area_text[4 + 32 + 32],"For EUROPE.                 ",28);
}

void DiscTerm();

bool DiscInit_(wchar* fn)
{
	DiscTerm();

	//try all drivers
	disc = OpenDisc(fn);

	if (disc!=0)
	{
		if (disc->type != GdRom)
		{
			const wchar warn_str[] = "Warning: CD Image Loaded!\n  Many CD/CDI/NRG images are known to be defective, GDI, CUE or CHD format is preferred. "
				"Please only file bug reports when using images known to be good (GDI, CUE or CHD).";
			printf("%s\n", warn_str);
		}
		printf("gdrom: Opened image \"%s\"\n",fn);
		NullDriveDiscType=Busy;

		libCore_gdrom_disc_change();

//		Sleep(400); //busy for a bit // what, really ?
		return true;
	}
	else
	{
		printf("gdrom: Failed to open image \"%s\"\n",fn);
		NullDriveDiscType=NoDisk; //no disc :)
	}
	return false;
}

bool DiscInit()
{
	if (settings.imgread.LoadDefaultImage)
	{
		printf("Loading default image \"%s\"\n",settings.imgread.DefaultImage);
		if (!DiscInit_(settings.imgread.DefaultImage))
		{
			msgboxf("Default image \"%s\" failed to load",MBX_ICONERROR,settings.imgread.DefaultImage);
			return false;
		}
		else
			return true;
	}

	// FIXME: Data loss if buffer is too small
	wchar fn[512];
	strncpy(fn,settings.imgread.LastImage, sizeof(fn));
	fn[sizeof(fn) - 1] = '\0';

	int gfrv=GetFile(fn);

	if (gfrv == 0)
	{
		NullDriveDiscType=NoDisk;
		gd_setdisc();
		sns_asc=0x29;
		sns_ascq=0x00;
		sns_key=0x6;
		return true;
	}
	else if (gfrv == -1)
	{
		return false;
	}

	// FIXME: Data loss if buffer is too small
	strncpy(settings.imgread.LastImage, fn, sizeof(settings.imgread.LastImage));
	settings.imgread.LastImage[sizeof(settings.imgread.LastImage) - 1] = '\0';

	SaveSettings();

	if (!DiscInit_(fn))
	{
		//msgboxf("Selected image failed to load",MBX_ICONERROR);
			NullDriveDiscType=NoDisk;
			gd_setdisc();
			sns_asc=0x29;
			sns_ascq=0x00;
			sns_key=0x6;
		return true;
	}
	else
	{
		return true;
	}
}
bool DiscSwap()
{
	// These Additional Sense Codes mean "The lid was closed"
	sns_asc = 0x28;
	sns_ascq = 0x00;
	sns_key = 0x6;
	if (settings.imgread.LoadDefaultImage)
	{
		printf("Loading default image \"%s\"\n",settings.imgread.DefaultImage);
		if (!DiscInit_(settings.imgread.DefaultImage))
		{
			msgboxf("Default image \"%s\" failed to load",MBX_ICONERROR,settings.imgread.DefaultImage);
			return false;
		}
		else
			return true;
	}

	// FIXME: Data loss if buffer is too small
	wchar fn[512];
	strncpy(fn, settings.imgread.LastImage, sizeof(fn));
	fn[sizeof(fn) - 1] = '\0';



	int gfrv=GetFile(fn);

	if (gfrv == 0)
	{
		NullDriveDiscType=Open;
		gd_setdisc();
		return true;
	}
	else if (gfrv == -1)
	{
		return false;
	}

	// FIXME: Data loss if buffer is too small
	strncpy(settings.imgread.LastImage, fn, sizeof(settings.imgread.LastImage));
	settings.imgread.LastImage[sizeof(settings.imgread.LastImage) - 1] = '\0';


	SaveSettings();

	if (!DiscInit_(fn))
	{
		//msgboxf("Selected image failed to load",MBX_ICONERROR);
		NullDriveDiscType=Open;
		gd_setdisc();
	}

	return true;
}


void DiscTerm()
{
	if (disc!=0)
		delete disc;

	disc=0;
}


//
//convert our nice toc struct to dc's native one :)

u32 CreateTrackInfo(u32 ctrl,u32 addr,u32 fad)
{
	u8 p[4];
	p[0]=(ctrl<<4)|(addr<<0);
	p[1]=fad>>16;
	p[2]=fad>>8;
	p[3]=fad>>0;

	return *(u32*)p;
}
u32 CreateTrackInfo_se(u32 ctrl,u32 addr,u32 tracknum)
{
	u8 p[4];
	p[0]=(ctrl<<4)|(addr<<0);
	p[1]=tracknum;
	p[2]=0;
	p[3]=0;
	return *(u32*)p;
}


void DiscGetDriveSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz)
{
	//printf("GD: read %08X, %d\n",StartSector,SectorCount);
	if (disc)
	{
		disc->ReadSectors(StartSector,SectorCount,buff,secsz, q_subchannel);
		if (disc->type == GdRom && StartSector==45150 && SectorCount==7)
		{
			PatchRegion_0(buff,secsz);
			PatchRegion_6(buff+2048*6,secsz);
		}
	}
}
void DiscGetDriveToc(u32* to,DiskArea area)
{
	if (!disc)
		return;
	memset(to,0xFFFFFFFF,102*4);

	//can't get toc on the second area on discs that don't have it
	verify(area != DoubleDensity || disc->type == GdRom);

	//normal CDs: 1 .. tc
	//GDROM: area0 is 1 .. 2, area1 is 3 ... tc

	u32 first_track=1;
	u32 last_track=disc->tracks.size();
	if (area==DoubleDensity)
		first_track=3;
	else if (disc->type==GdRom)
	{
		last_track=2;
	}

	//Generate the TOC info

	//-1 for 1..99 0 ..98
	to[99]=CreateTrackInfo_se(disc->tracks[first_track-1].CTRL,disc->tracks[first_track-1].ADDR,first_track);
	to[100]=CreateTrackInfo_se(disc->tracks[last_track-1].CTRL,disc->tracks[last_track-1].ADDR,last_track);

	if (disc->type==GdRom)
	{
		//use smaller LEADOUT
		if (area==SingleDensity)
			to[101]=CreateTrackInfo(disc->LeadOut.CTRL,disc->LeadOut.ADDR,13085);
	}
	else
	{
		to[101] = CreateTrackInfo(disc->LeadOut.CTRL, disc->LeadOut.ADDR, disc->LeadOut.StartFAD);
	}

	for (u32 i=first_track-1;i<last_track;i++)
	{
		to[i]=CreateTrackInfo(disc->tracks[i].CTRL,disc->tracks[i].ADDR,disc->tracks[i].StartFAD);
	}
}

void DiscGetDriveSessionInfo(u8* to,u8 session)
{
	if (!disc)
		return;
	to[0]=2;//status, will get overwritten anyway
	to[1]=0;//0's

	if (session==0)
	{
		to[2]=disc->sessions.size();//count of sessions
		to[3]=disc->EndFAD>>16;//fad is sessions end
		to[4]=disc->EndFAD>>8;
		to[5]=disc->EndFAD>>0;
	}
	else
	{
		to[2]=disc->sessions[session-1].FirstTrack;//start track of this session
		to[3]=disc->sessions[session-1].StartFAD>>16;//fad is session start
		to[4]=disc->sessions[session-1].StartFAD>>8;
		to[5]=disc->sessions[session-1].StartFAD>>0;
	}
}

void printtoc(TocInfo* toc,SessionInfo* ses)
{
	printf("Sessions %d\n",ses->SessionCount);
	for (u32 i=0;i<ses->SessionCount;i++)
	{
		printf("Session %d: FAD %d,First Track %d\n",i+1,ses->SessionFAD[i],ses->SessionStart[i]);
		for (u32 t=toc->FistTrack-1;t<=toc->LastTrack;t++)
		{
			if (toc->tracks[t].Session==i+1)
			{
				printf("\tTrack %d : FAD %d CTRL %d ADR %d\n",t,toc->tracks[t].FAD,toc->tracks[t].Control,toc->tracks[t].Addr);
			}
		}
	}
	printf("Session END: FAD END %d\n",ses->SessionsEndFAD);
}


void libGDR_ReadSubChannel(u8* buff, u32 format, u32 len)
{
	if (format == 0)
	{
		memcpy(buff, q_subchannel, len);
	}
}

void libGDR_ReadSector(u8* buff, u32 StartSector, u32 SectorCount, u32 secsz)
{
	DiscGetDriveSector(buff, StartSector, SectorCount, secsz);
	//if (CurrDrive)
	//	CurrDrive->ReadSector(buff,StartSector,SectorCount,secsz);
}

void libGDR_GetToc(u32* toc, u32 area)
{
	DiscGetDriveToc(toc, (DiskArea)area);
}
//TODO : fix up
u32 libGDR_GetDiscType()
{
	if (disc)
		return disc->type;
	else
		return NullDriveDiscType;
}

void libGDR_GetSessionInfo(u8* out, u8 ses)
{
	DiscGetDriveSessionInfo(out, ses);
}

//It's supposed to reset everything (if not a manual reset)
void libGDR_Reset(bool Manual)
{
	libCore_gdrom_disc_change();
}

//called when entering sh4 thread , from the new thread context (for any thread specific init)
s32 libGDR_Init()
{
	if (!DiscInit())
		return rv_serror;
	libCore_gdrom_disc_change();
	settings.imgread.PatchRegion = true;
	return rv_ok;
}

//called when exiting from sh4 thread , from the new thread context (for any thread specific init) :P
void libGDR_Term()
{
	DiscTerm();
}
