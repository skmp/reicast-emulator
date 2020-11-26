#include "common.h"
#include <ctype.h>
#include <sstream>

Disc* load_gdi(const char* file)
{
	core_file* t=core_fopen(file);
	if (!t)
		return nullptr;

	size_t gdi_len = core_fsize(t);

	char gdi_data[8193] = { 0 };

	if (gdi_len >= sizeof(gdi_data))
	{
		WARN_LOG(GDROM, "GDI: file too big");
		core_fclose(t);
		return nullptr;
	}

	core_fread(t, gdi_data, gdi_len);
	core_fclose(t);

	istringstream gdi(gdi_data);

	u32 iso_tc = 0;
	gdi >> iso_tc;
	if (iso_tc == 0)
	{
		WARN_LOG(GDROM, "GDI: empty or invalid GDI file");
		return nullptr;
	}
	INFO_LOG(GDROM, "GDI : %d tracks", iso_tc);

	char path[512];
	strcpy(path,file);
	ssize_t len=strlen(file);
	while (len>=0)
	{
		if (path[len]=='\\' || path[len]=='/')
			break;
		len--;
	}
	len++;
	char* pathptr=&path[len];

	Disc* disc = new Disc();
	u32 TRACK=0,FADS=0,CTRL=0,SSIZE=0;
	s32 OFFSET=0;
	for (u32 i=0;i<iso_tc;i++)
	{
		string track_filename;

		//TRACK FADS CTRL SSIZE file OFFSET
		gdi >> TRACK;
		gdi >> FADS;
		gdi >> CTRL;
		gdi >> SSIZE;

		char last;

		do {
			gdi >> last;
		} while (isspace(last));
		
		if (last == '"')
		{
			gdi >> std::noskipws;
			for(;;) {
				gdi >> last;
				if (last == '"')
					break;
				track_filename += last;
			}
			gdi >> std::skipws;
		}
		else
		{
			gdi >> track_filename;
			track_filename = last + track_filename;
		}

		gdi >> OFFSET;
		
		DEBUG_LOG(GDROM, "file[%d] \"%s\": FAD:%d, CTRL:%d, SSIZE:%d, OFFSET:%d", TRACK, track_filename.c_str(), FADS, CTRL, SSIZE, OFFSET);

		Track t;
		t.ADDR=0;
		t.StartFAD=FADS+150;
		t.EndFAD=0;		//fill it in
		t.file=0;
		t.CTRL = CTRL;

		if (SSIZE!=0)
		{
			strcpy(pathptr, track_filename.c_str());
			t.file = new RawTrackFile(core_fopen(path),OFFSET,t.StartFAD,SSIZE);	
		}
		if (!disc->tracks.empty())
			disc->tracks.back().EndFAD = t.StartFAD - 1;
		disc->tracks.push_back(t);
	}

	disc->FillGDSession();

	return disc;
}


Disc* gdi_parse(const char* file)
{
	size_t len=strlen(file);
	if (len>4)
	{
		if (stricmp( &file[len-4],".gdi")==0)
		{
			return load_gdi(file);
		}
	}
	return 0;
}

