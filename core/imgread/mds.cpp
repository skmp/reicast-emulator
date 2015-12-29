#include "mds_reader.h"
#include "common.h"

SessionInfo mds_ses;
TocInfo mds_toc;
DiscType mds_Disctype=CdRom;

struct file_TrackInfo
{
	u32 FAD;
	u32 Offset;
	u32 SectorSize;
};

file_TrackInfo mds_Track[101];
FILE* fp_mdf=0;
u8 mds_SecTemp[5120];
u32 mds_TrackCount;

u32 mds_ReadSSect(u8* p_out,u32 sector,u32 secsz)
{
	for (u32 i=0;i<mds_TrackCount;i++)
	{
		if (mds_Track[i+1].FAD>sector)
		{
			u32 fad_off=sector-mds_Track[i].FAD;
			fseek(fp_mdf,mds_Track[i].Offset+fad_off*mds_Track[i].SectorSize,SEEK_SET);
			fread(mds_SecTemp,mds_Track[i].SectorSize,1,fp_mdf);

			ConvertSector(mds_SecTemp,p_out,mds_Track[i].SectorSize,secsz,sector);
			return mds_Track[i].SectorSize;
		}
	}
	return 0;
}

void mds_DriveReadSector(u8 * buff,u32 StartSector,u32 SectorCount,u32 secsz)
{
//	printf("MDS/NRG->Read : Sector %d , size %d , mode %d \n",StartSector,SectorCount,secsz);
	while(SectorCount--)
	{
		mds_ReadSSect(buff,StartSector,secsz);
		buff+=secsz;
		StartSector++;
	}
}

void mds_CreateToc()
{
	//clear structs to 0xFF :)
	memset(mds_Track,0xFF,sizeof(mds_Track));
	memset(&mds_ses,0xFF,sizeof(mds_ses));
	memset(&mds_toc,0xFF,sizeof(mds_toc));

	printf("\n--GD toc info start--\n");
	int track=0;
	bool CD_DA=false;
	bool CD_M1=false;
	bool CD_M2=false;

	strack* last_track=&sessions[nsessions-1].tracks[sessions[nsessions-1].ntracks-1];

	mds_ses.SessionCount=nsessions;
	mds_ses.SessionsEndFAD=last_track->sector+last_track->sectors+150;
	mds_toc.LeadOut.FAD=last_track->sector+last_track->sectors+150;
	mds_toc.LeadOut.Addr=0;
	mds_toc.LeadOut.Control=0;
	mds_toc.LeadOut.Session=0;

	printf("Last Sector : %d\n",mds_ses.SessionsEndFAD);
	printf("Session count : %d\n",mds_ses.SessionCount);

	mds_toc.FistTrack=1;

	for (int s=0;s<nsessions;s++)
	{
		printf("Session %d:\n",s);
		session* ses=&sessions[s];

		printf("  Track Count: %d\n",ses->ntracks);
		for (int t=0;t< ses->ntracks ;t++)
		{
			strack* c_track=&ses->tracks[t];

			//pre gap
			
			if (t==0)
			{
				mds_ses.SessionFAD[s]=c_track->sector+150;
				mds_ses.SessionStart[s]=track+1;
				printf("  Session start FAD: %d\n",mds_ses.SessionFAD[s]);
			}

			//verify(cdi_track->dwIndexCount==2);
			printf("  track %d:\n",t);
			printf("    Type : %d\n",c_track->mode);

			if (c_track->mode>=2)
				CD_M2=true;
			if (c_track->mode==1)
				CD_M1=true;
			if (c_track->mode==0)
				CD_DA=true;
			
			//verify((c_track->mode==236) || (c_track->mode==169))
			 
			mds_toc.tracks[track].Addr=0;//hmm is that ok ?
			mds_toc.tracks[track].Session=s;
			mds_toc.tracks[track].Control=c_track->mode>0?4:0;//mode 1 , 2 , else are data , 0 is audio :)
			mds_toc.tracks[track].FAD=c_track->sector+150;


			mds_Track[track].FAD=mds_toc.tracks[track].FAD;
			mds_Track[track].SectorSize=c_track->sectorsize;
			mds_Track[track].Offset=(u32)c_track->offset;
			printf("    Start FAD : %d\n",mds_Track[track].FAD);
			printf("    SectorSize : %d\n",mds_Track[track].SectorSize);
			printf("    File Offset : %d\n",mds_Track[track].Offset);

			//main track data
			track++;
		}
	}

	//normal CDrom : mode 1 tracks .All sectors on the track are mode 1.Mode 2 was defined on the same book , but is it ever used? if yes , how can i detect
	//cd XA ???
	//CD Extra : session 1 is audio , session 2 is data
	//cd XA : mode 2 tracks.Form 1/2 are selected per sector.It allows mixing of mode1/mode2 tracks ?
	//CDDA  : audio tracks only <- thats simple =P
	/*
	if ((CD_M1==true) && (CD_DA==false) && (CD_M2==false))
		mds_Disctype = CdRom;
	else if (CD_M2)
		mds_Disctype = CdRom_XA;
	else if (CD_DA && CD_M1) 
		mds_Disctype = CdRom_Extra;
	else
		mds_Disctype=CdRom;//hmm?
	*/
	if (nsessions==1 && (CD_M1 | CD_M2))
		mds_Disctype = CdRom;		//hack so that non selfboot stuff works on utopia
	else
	{
		if ((CD_M1==true) && (CD_DA==false) && (CD_M2==false))
			mds_Disctype = CdRom;		//is that even correct ? what if its multysessions ? ehh ? what then ???
		else if (CD_M2)
			mds_Disctype = CdRom_XA;	// XA XA ! its mode 2 wtf ?
		else if (CD_DA && CD_M1) 
			mds_Disctype = CdRom_XA;	//data + audio , duno wtf as@!#$ lets make it _XA since it seems to boot
		else if (CD_DA && !CD_M1 && !CD_M2) 
			mds_Disctype = CdDA;	//audio
		else
			mds_Disctype=CdRom_XA;//and hope for the best
	}
/*
	bool data = CD_M1 | CD_M2;
	bool audio=CD_DA;

	if (data && audio)
		mds_Disctype = CdRom_XA;	//Extra/CdRom won't boot , so meh
	else if (data)
		mds_Disctype = CdRom;	//only data
	else
		mds_Disctype = CdDA;	//only audio
*/
	mds_toc.LastTrack=track;
	mds_TrackCount=track;
	printf("--GD toc info end--\n\n");
}

bool mds_init(const char* file)
{
	char fn[512];
	
	bool rv=false;
	if (rv==false && parse_mds(file,false))
	{
		bool found=false;
		if (strlen(file) > 4)
		{
			strcpy(&fn[0],file);
			size_t len= strlen(fn);
			strcpy(&fn[len-4],".mdf");
			
			fp_mdf = fopen(fn,"rb");
			found  = fp_mdf!=0;
		}
		if (!found)
		{
			if (GetFile(fn,L"mds images (*.mds) \0*.mdf\0\0")==1)
			{
				fp_mdf= fopen(fn, "rb");
				found=true;
			}
		}
			
		if (!found)
			return false;
		rv=true;
	}
	
	if (rv==false && parse_nrg(file,false))
	{
		rv=true;
		fp_mdf= fopen(file, "rb");
	}
	if (rv==false)
		return false;
	/*
	for(int j=0;j<nsessions;j++)
	for(int i=0;i<sessions[j].ntracks;i++)
	{
		printf("Session %d Track %d mode %d/%d sector %d count %d offset %I64d\n",
			sessions[j].session_,
			sessions[j].tracks[i].track,
			sessions[j].tracks[i].mode,
			sessions[j].tracks[i].sectorsize,
			sessions[j].tracks[i].sector,
			sessions[j].tracks[i].sectors,
			sessions[j].tracks[i].offset);
	}*/
	
	mds_CreateToc();
	return true;
}
void mds_term()
{
	if (fp_mdf)
		fclose(fp_mdf);
	fp_mdf=0;
}

u32 mds_DriveGetDiscType()
{
	return mds_Disctype;
}
void mds_DriveGetTocInfo(TocInfo* toc,DiskArea area)
{
	verify(area==SingleDensity);
	memcpy(toc,&mds_toc,sizeof(TocInfo));
}
void mds_GetSessionsInfo(SessionInfo* sessions)
{
	memcpy(sessions,&mds_ses,sizeof(SessionInfo));
}

struct MDSDiskWrapper : Disc
{
	MDSDiskWrapper() {
	}

	bool TryOpen(const char *file)
   {
      if (mds_init(file))
      {
         //printf("Session count %d:\n",nsessions);
         //s32 tr_c = 1;
         for (s32 s=0;s<nsessions;++s)
         {
            //printf("Session %d:\n",s);
            session* ses=&mds_sessions[s];
            //printf("  Track Count: %d\n",ses->ntracks);

            Track tr;
            for (s32 t=0;t< ses->ntracks;++t)
            {
               strack* c_track=&ses->tracks[t];

               if (t==0)
               {
                  Session ts;
                  ts.FirstTrack = t + 1;//(tr_c++);
                  ts.StartFAD = c_track->sector+150;
                  sessions.push_back(ts);
                  //printf("  Session start FAD: %d\n",mds_ses.SessionFAD[s]);
               }

               tr.ADDR = 0;
               tr.StartFAD = c_track->sector+150;
               tr.EndFAD = 0;
               tr.CTRL = c_track->mode>0?4:0;

               //printf("SECTOR SIZE %u\n",c_track->sectorsize);
               tr.file = new RawTrackFile(fp_mdf,(u32)c_track->offset,tr.StartFAD,c_track->sectorsize);

               tracks.push_back(tr);
            }
         }

         type=mds_Disctype;
         LeadOut.ADDR=0;
         LeadOut.CTRL=0;
         LeadOut.StartFAD=549300;

         EndFAD=549300;

         return true;
      }

      return false;
   }

	~MDSDiskWrapper() {
		mds_term();
	}
};
 
Disc* mds_parse(const char * fname)
{
	MDSDiskWrapper* dsk = new MDSDiskWrapper();

	if (!dsk)
		return 0;

	if (dsk->TryOpen(fname))
		fprintf("Loaded %s\n", fname);
   else
   {
      fprintf("Unable to load %s \n",fname);
      delete dsk;
      return 0;
   }

	return dsk;
}
