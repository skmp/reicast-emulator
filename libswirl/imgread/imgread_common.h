/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "imgread.h"
#include "coreio/coreio.h"

struct RawTrackFile : TrackFile
{
	core_file* file;
	s32 offset;
	u32 fmt;
	bool cleanup;

	RawTrackFile(core_file* file, u32 file_offs, u32 first_fad, u32 secfmt)
	{
		verify(file != 0);
		this->file = file;
		this->offset = file_offs - first_fad * secfmt;
		this->fmt = secfmt;
		this->cleanup = true;
	}

	virtual void Read(u32 FAD, u8* dst, SectorFormat* sector_type, u8* subcode, SubcodeFormat* subcode_type)
	{
		//for now hackish
		if (fmt == 2352)
			*sector_type = SECFMT_2352;
		else if (fmt == 2048)
			*sector_type = SECFMT_2048_MODE2_FORM1;
		else if (fmt == 2336)
			*sector_type = SECFMT_2336_MODE2;
		else if (fmt == 2448)
			*sector_type = SECFMT_2448_MODE2;
		else
		{
			verify(false);
		}

		core_fseek(file, offset + FAD * fmt, SEEK_SET);
		core_fread(file, dst, fmt);
	}
	virtual ~RawTrackFile()
	{
		if (cleanup && file)
			core_fclose(file);
	}
};

DiscType GuessDiscType(bool m1, bool m2, bool da);