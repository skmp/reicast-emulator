#include "common.h"
#include <ctype.h>
#include <algorithm>
#include <sstream>

static std::string dirname(std::string source)
{
	size_t pos_unix = source.find_last_of('/');
	size_t pos_dos = source.find_last_of('\\');
	size_t pos;

	if((pos_unix != std::string::npos) && (pos_dos != std::string::npos))
		pos = std::max(pos_unix, pos_dos) + 1;
	else
		pos = std::min(pos_unix, pos_dos) + 1;

	source.erase(pos);
	return source;
}

Disc* load_gdi(const char* file)
{
	u32 iso_tc;
	Disc* disc = new Disc();
	
	//memset(&gdi_toc,0xFFFFFFFF,sizeof(gdi_toc));
	//memset(&gdi_ses,0xFFFFFFFF,sizeof(gdi_ses));
	core_file* t=core_fopen(file);
	if (!t)
		return 0;

	size_t gdi_len = core_fsize(t);

	char gdi_data[8193] = { 0 };

	if (gdi_len >= sizeof(gdi_data))
	{
		core_fclose(t);
		return 0;
	}

	core_fread(t, gdi_data, gdi_len);
	core_fclose(t);

	istringstream gdi(gdi_data);

	gdi >> iso_tc;
	printf("\nGDI : %d tracks\n",iso_tc);

	string path_prefix = dirname(string(file));

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
			for(;;) {
				gdi >> std::noskipws >> last;
				if (last == '"')
					break;
				track_filename += last;
			}
		}
		else
		{
			gdi >> track_filename;
			track_filename = last + track_filename;
		}

		gdi >> OFFSET;
		
		printf("file[%d] \"%s\": FAD:%d, CTRL:%d, SSIZE:%d, OFFSET:%d\n", TRACK, track_filename.c_str(), FADS, CTRL, SSIZE, OFFSET);

		Track t;
		t.ADDR=0;
		t.StartFAD=FADS+150;
		t.EndFAD=0;		//fill it in
		t.file=0;

		if (SSIZE!=0)
		{
			string path = path_prefix + track_filename;
			t.file = new RawTrackFile(core_fopen(path.c_str()),OFFSET,t.StartFAD,SSIZE);
		}
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

void iso_term()
{
}
