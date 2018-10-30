#include "types.h"



#include "common.h"
#include "oslib/logging.h"
#include "deps/chdpsr/cdipsr.h"

Disc* cdi_parse(const wchar* file)
{
	core_file* fsource=core_fopen(file);

	if (!fsource)
		return 0;

	Disc* rv= new Disc();

	image_s image = { 0 };
	track_s track = { 0 };
	CDI_init(fsource,&image,0);

	CDI_get_sessions(fsource,&image);

	image.remaining_sessions = image.sessions;

	/////////////////////////////////////////////////////////////// Loop sessions

	bool ft=true, CD_M2=false,CD_M1=false,CD_DA=false;

	while(image.remaining_sessions > 0)
	{
		ft=true;
		image.global_current_session++;

		CDI_get_tracks (fsource, &image);

		image.header_position = core_ftell(fsource);

		LOG_I("imgread_cdi", "\nSession %d has %d track(s)\n",image.global_current_session,image.tracks);

		if (image.tracks == 0)
			LOG_V("imgread_cdi", "Open session\n");
		else
		{
			// Clear cuesheet
			image.remaining_tracks = image.tracks;

			///////////////////////////////////////////////////////////////// Loop tracks

			while(image.remaining_tracks > 0)
			{
				track.global_current_track++;
				track.number = image.tracks - image.remaining_tracks + 1;

				CDI_read_track (fsource, &image, &track);

				image.header_position = core_ftell(fsource);

				/* Show info */
				LOG_I("imgread_cdi", "Saving ~~> Track: %2d  Type: ", track.global_current_track);
				switch(track.mode)
				{
					case 0 : LOG_I("imgread_cdi", "Audio/%lu", track.sector_size); break;
					case 1 : LOG_I("imgread_cdi", "Mode1/%lu", track.sector_size); break;
					case 2 :
					default: LOG_I("imgread_cdi", "Mode2/%lu", track.sector_size); break;
				}
				LOG_I("imgread_cdi", "\tPregap: %-3ld \tSize: %-6ld  \tLBA: %-6ld\n", track.pregap_length, track.length, track.start_lba);

				if (ft)
				{
					ft = false;
					Session s;
					s.StartFAD = track.pregap_length + track.start_lba;
					s.FirstTrack = track.global_current_track;
					rv->sessions.push_back(s);
				}

				Track t;
				if (track.mode == 2)
					CD_M2 = true;
				if (track.mode == 1)
					CD_M1 = true;
				if (track.mode == 0)
					CD_DA = true;



				t.ADDR=1;//hmm is that ok ?

				t.CTRL=track.mode==0?0:4;
				t.StartFAD=track.start_lba+track.pregap_length;
				t.EndFAD=t.StartFAD+track.length-1;
				t.file = new RawTrackFile(core_fopen(file),track.position + track.pregap_length * track.sector_size,t.StartFAD,track.sector_size);

				rv->tracks.push_back(t);
				//       if (track.pregap_length != 150) LOG_W("imgread_cdi", "This track seems to have a non-standard pregap...\n");

				if (track.length < 0)
					LOG_E("imgread_cdi", "Negative track size found!\n"
					"You must extract image with /pregap option");

				//if (!opts.showinfo)
				{
					if (track.total_length < track.length + track.pregap_length)
					{
						LOG_I("imgread_cdi", "\nThis track seems truncated. Skipping...\n");
						core_fseek(fsource, track.position, SEEK_SET);
						core_fseek(fsource, track.total_length, SEEK_CUR);
						track.position = core_ftell(fsource);
					}
					else
					{

						LOG_I("imgread_cdi", "Track position: %lu\n", track.position + track.pregap_length * track.sector_size);
						core_fseek(fsource, track.position, SEEK_SET);
						//     fseek(fsource, track->pregap_length * track->sector_size, SEEK_CUR);
						//     fseek(fsource, track->length * track->sector_size, SEEK_CUR);
						core_fseek(fsource, track.total_length * track.sector_size, SEEK_CUR);

						//savetrack(fsource, &image, &track, &opts, &flags);
						track.position = core_ftell(fsource);

						rv->EndFAD=track.start_lba +track.total_length;
						// Generate cuesheet entries

						//if (flags.create_cuesheet && !(track.mode == 2 && flags.do_convert))  // Do not generate input if converted (obsolete)
						//	savecuesheet(fcuesheet, &image, &track, &opts, &flags);

					}
				}

				core_fseek(fsource, image.header_position, SEEK_SET);


				// Close loops

				image.remaining_tracks--;
			}

			//if (flags.create_cuesheet) fclose(fcuesheet);
		}

		CDI_skip_next_session (fsource, &image);

		image.remaining_sessions--;
	}

	rv->type=GuessDiscType(CD_M1,CD_M2,CD_DA);

	rv->LeadOut.StartFAD=rv->EndFAD;
	rv->LeadOut.ADDR=0;
	rv->LeadOut.CTRL=0;



	return rv;
}

#if 0
	SPfcToc* pstToc;
	HMODULE pfctoc_mod=NULL;
	pfctoc_mod=LoadLibrary("plugins\\pfctoc.dll");
	if (pfctoc_mod==NULL)
		pfctoc_mod=LoadLibrary("pfctoc.dll");
	if(pfctoc_mod==NULL)
		return false;

	PfcFreeTocFP* PfcFreeToc;
	PfcGetTocFP*  PfcGetToc;

	PfcFreeToc=(PfcFreeTocFP*)GetProcAddress(pfctoc_mod,"PfcFreeToc");
	PfcGetToc=(PfcGetTocFP*)GetProcAddress(pfctoc_mod,"PfcGetToc");
	verify(PfcFreeToc!=NULL && PfcGetToc!=NULL);

	DWORD dwSize;//
	DWORD dwErr = PfcGetToc(file, pstToc, dwSize);
	if (dwErr == PFCTOC_OK)
	{
		rv= new CDIDisc(pstToc,file);
	}

	if (pstToc)
		PfcFreeToc(pstToc);
	if (pfctoc_mod)
		FreeLibrary(pfctoc_mod);
	pstToc=0;
	pfctoc_mod=0;

	return rv;
#endif
