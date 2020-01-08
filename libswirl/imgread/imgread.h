#pragma once

#include "types.h"


struct TocTrackInfo
{
	u32 FAD;    //fad, Intel format
	u8 Control; //control info
	u8 Addr;    //address info
	u8 Session; //Session where the track belongs
};
struct TocInfo
{
	//0-98 ->1-99
	TocTrackInfo tracks[99];

	u8 FistTrack;
	u8 LastTrack;

	TocTrackInfo LeadOut; //session set to 0 on that one
};

struct SessionInfo
{
	u32 SessionsEndFAD;   //end of Disc (?)
	u8 SessionCount;      //must be at least 1
	u32 SessionStart[99]; //start track for session
	u32 SessionFAD[99];   //for sessions 1-99 ;)
};

/*
Mode2 Subheader:

"1" file number for identification of nested files (0 = not interleaver.)

"2" channel number, the infantry of the various channels are selectable for playback

"3" SUBMODE byte:

7: last sector of file (EOF)
6: Real-time sector (f.Echtzeitwiedergabe without error correction)
5: Form 2 (bit = 1), form 1 (bit = 0)
4: Trigger on (depending on OS)
3: data sector (Submodebyte 3 or 2 or 1)
2: ADPCM audio sector "
1: Video-sector "
0: last sector of a record (EOR)
"4" Encoding type of audio (eg mono / stereo) and video data (in this byte data sectors is set to 0)

"5" to "8" is the repetition of "1" through "4"


RAW: 2352
MODE1:
SYNC (12) | HEAD (4) | data (2048) | edc (4) | space (8) | ecc (276)
MODE2:
SYNC (12) | HEAD (4) | sub-head (8) | sector_data (2328)
  -form1 sector_data:
   data (2048) | edc (4) | ecc (276)

  -form2 sector_data:
   data (2324) |edc(4)
*/

enum SectorFormat
{
	SECFMT_2352,				//full sector
	SECFMT_2048_MODE1,			//2048 user byte, form1 sector
	SECFMT_2048_MODE2_FORM1,	//2048 user bytes, form2m1 sector
	SECFMT_2336_MODE2,			//2336 user bytes, 
	SECFMT_2448_MODE2,			//2048 user bytes, ? SYNC (12) | HEAD (4) | sub-head (8) | data (2048) | edc (4) | ecc (276) + subcodes (96) ?
};

enum SubcodeFormat
{
	SUBFMT_NONE,				//No subcode info
	SUBFMT_96					//raw 96-byte subcode info
};

struct Session
{
	u32 StartFAD;			//session's start fad
	u8 FirstTrack;			//session's first track
};

struct TrackFile
{
	virtual void Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type) = 0;
	virtual ~TrackFile() {};
};

struct Track
{
	TrackFile* file;	//handler for actual IO
	u32 StartFAD;		//Start FAD
	u32 EndFAD;			//End FAD
	u8 CTRL;
	u8 ADDR;

	Track()
	{
		file = 0;
		StartFAD = 0;
		EndFAD = 0;
		CTRL = 0;
		ADDR = 0;
	}
	bool Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
	{
		if (FAD >= StartFAD && (FAD <= EndFAD || EndFAD == 0) && file)
		{
			file->Read(FAD, dst, sector_type, subcode, subcode_type);
			return true;
		}
		else
			return false;
	}
	void Destroy() { if (file) delete file; file = 0; }
};

struct Disc
{
	wstring path;
	vector<Session> sessions;	//info for sessions
	vector<Track> tracks;		//info for tracks
	Track LeadOut;				//info for lead out track (can't read from here)
	u32 EndFAD;					//Last valid disc sector
	DiscType type;

	//functions !
	bool ReadSector(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
	{
		for (size_t i = tracks.size(); i-- > 0;)
		{
			*subcode_type = SUBFMT_NONE;
			if (tracks[i].Read(FAD, dst, sector_type, subcode, subcode_type))
				return true;
		}

		return false;
	}

	void ReadSectors(u32 FAD, u32 count, u8* dst, u32 fmt, u8* q_subchannel = nullptr)
	{
		u8 temp[2448];
		SectorFormat secfmt;
		SubcodeFormat subfmt;

		while (count)
		{
			if (ReadSector(FAD, temp, &secfmt, q_subchannel, &subfmt))
			{
				//TODO: Proper sector conversions
				if (secfmt == SECFMT_2352)
				{
					ConvertSector(temp, dst, 2352, fmt, FAD, q_subchannel);
				}
				else if (fmt == 2048 && secfmt == SECFMT_2336_MODE2)
					memcpy(dst, temp + 8, 2048);
				else if (fmt == 2048 && (secfmt == SECFMT_2048_MODE1 || secfmt == SECFMT_2048_MODE2_FORM1))
				{
					memcpy(dst, temp, 2048);
				}
				else if (fmt == 2352 && (secfmt == SECFMT_2048_MODE1 || secfmt == SECFMT_2048_MODE2_FORM1))
				{
					printf("GDR:fmt=2352;secfmt=2048\n");
					memcpy(dst, temp, 2048);
				}
				else if (fmt == 2048 && secfmt == SECFMT_2448_MODE2)
				{
					// Pier Solar and the Great Architects
					ConvertSector(temp, dst, 2448, fmt, FAD, q_subchannel);
				}
				else
				{
					printf("ERROR: UNABLE TO CONVERT SECTOR. THIS IS FATAL. Format: %d Sector format: %d\n", fmt, secfmt);
					//verify(false);
				}
			}
			else
			{
				printf("Sector Read miss FAD: %d\n", FAD);
			}
			dst += fmt;
			FAD++;
			count--;
		}
	}
	virtual ~Disc()
	{
		for (size_t i = 0; i < tracks.size(); i++)
			tracks[i].Destroy();
	};

	void FillGDSession()
	{
		Session ses;

		//session 1 : start @ track 1, and its fad
		ses.FirstTrack = 1;
		ses.StartFAD = tracks[0].StartFAD;
		sessions.push_back(ses);

		//session 2 : start @ track 3, and its fad
		ses.FirstTrack = 3;
		ses.StartFAD = tracks[0].StartFAD;
		sessions.push_back(ses);

		//this isn't always true for gdroms, depends on area look @ the get-toc code
		type = GdRom;
		LeadOut.ADDR = 0;
		LeadOut.CTRL = 0;
		LeadOut.StartFAD = 549300;

		EndFAD = 549300;
	}

	void Dump(const string& path)
	{
		for (u32 i = 0; i < tracks.size(); i++)
		{
			u32 fmt = tracks[i].CTRL == 4 ? 2048 : 2352;
			char fsto[1024];
			sprintf(fsto, "%s%s%d.img", path.c_str(), ".track", i);

			FILE* fo = fopen(fsto, "wb");

			for (u32 j = tracks[i].StartFAD; j <= tracks[i].EndFAD; j++)
			{
				u8 temp[2352];
				ReadSectors(j, 1, temp, fmt);
				fwrite(temp, fmt, 1, fo);
			}
			fclose(fo);
		}
	}

	private:
		bool ConvertSector(u8* in_buff, u8* out_buff, int from, int to, int sector, u8* q_subchannel)
		{
			//get subchannel data, if any
			if (from == 2448)
			{
				if (q_subchannel) {
					memcpy(q_subchannel, in_buff + 2352, 96);
				}
				from -= 96;
			}
			//if no conversion
			if (to == from)
			{
				memcpy(out_buff, in_buff, to);
				return true;
			}
			switch (to)
			{
			case 2340:
			{
				verify((from == 2352));
				memcpy(out_buff, &in_buff[12], 2340);
			}
			break;
			case 2328:
			{
				verify((from == 2352));
				memcpy(out_buff, &in_buff[24], 2328);
			}
			break;
			case 2336:
				verify(from >= 2336);
				verify((from == 2352));
				memcpy(out_buff, &in_buff[0x10], 2336);
				break;
			case 2048:
			{
				verify(from >= 2048);
				verify((from == 2448) || (from == 2352) || (from == 2336));
				if ((from == 2352) || (from == 2448))
				{
					if (in_buff[15] == 1)
					{
						memcpy(out_buff, &in_buff[0x10], 2048); //0x10 -> mode1
					}
					else
						memcpy(out_buff, &in_buff[0x18], 2048); //0x18 -> mode2 (all forms ?)
				}
				else
					memcpy(out_buff, &in_buff[0x8], 2048);	//hmm only possible on mode2.Skip the mode2 header
			}
			break;
			case 2352:
				//if (from >= 2352)
			{
				memcpy(out_buff, &in_buff[0], 2352);
			}
			break;
			default:
				printf("Sector conversion from %d to %d not supported \n", from, to);
				break;
			}

			return true;
		}
};

Disc* OpenDisc(const wchar* fn);