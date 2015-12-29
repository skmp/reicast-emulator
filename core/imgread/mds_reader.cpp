#include "mds_reader.h"

session sessions[256];
session* mds_sessions = &sessions[0];

int nsessions;

int flen(FILE*f)
{
	long lpos=ftell(f);
	long epos;

	fseek(f,0,SEEK_END);
	epos=ftell(f);
	fseek(f,lpos,SEEK_SET);
	return epos;
}

void byteswap(void *pbuffer,int bytes)
{
	char t;
	int p=0;
	int l=bytes-1;
	char *b=(char*)pbuffer;
	while(p<l)
	{
		t=b[p];b[p]=b[l];b[l]=t;
		p++;l--;
	}
}

int mds_mode(int mode)
{
	if(mode>0xa9)
		return mode - 0xa9;
	printf("unknown track mode %d , guessing audio ....\nPress Any Key To Continue\n",mode);
	//getchar();
	return 0;
}

int nrg_mode(int mode)
{
    if(mode == 0x07) return 0;//audio
    if(mode == 0x00) return 1;
	if(mode == 0x05) return 1;
    if(mode == 0x01) return 2;
	if(mode == 0x03) return 2;//mode2 form 1
	if(mode == 0x06) return 2;
	printf("unknown track mode %d , guessing audio ....\nPress Any Key To Continue\n",mode);
	//getchar();
	return 0;
} 


#define read_binary(type,source,offset) (*(type*)((source)+(offset)))

bool parse_mds(const char *mds_filename,bool verbose)
{
   /*"returns the supposable bin_filename and raw entries, if something went \
     wrong it throws an exception"*/
   // read the file

   FILE*mds_file = fopen (mds_filename, "rb");
   if(!mds_file)
   {
      fprintf(stderr,"Could not open the mds-file <%s>\n",mds_filename);
      return false;
   }

   int  mds_size = flen(mds_file);

   //if its too small ..
   if (mds_size<16)
      return false;

   //do NOT load whole file to memory before checking ... cna be deadly for .nrg/.iso/ anything :p
   char mds_key[16];
   fseek(mds_file,0,SEEK_SET);
   fread(mds_key,16,1,mds_file);

   if(memcmp(mds_key,"MEDIA DESCRIPTOR",16)!=0)
   {
      printf("Invalid data in <%s>. It is not an MDF/MDS file.\n",mds_filename);
      fclose(mds_file);
      return false;
   }

   fseek(mds_file,0,SEEK_SET);

   char *mds_content=(char*)malloc(mds_size);

   if(!mds_content)
   {
      printf("Could not allocate a buffer to read <%s>\n",mds_filename);
      fclose(mds_file);
      return false;
   }

   fread(mds_content,mds_size,1,mds_file);

   if(memcmp(mds_content,"MEDIA DESCRIPTOR",16)!=0)
   {
      printf("Invalid data in <%s>. It is not an MDF/MDS file.\n",mds_filename);
      free(mds_content);
      fclose(mds_file);
      return false;
   }

   // get some data from header
   int mds_header_size = 0x70;
   int mds_datablock_size = 0x50;
   int mds_extrablock_size = 0x08;
   int mds_footer_size = 0x16;
   int mds_version = read_binary(char, mds_content, 0x0010);
   int mds_revision = read_binary(char, mds_content, 0x0011);
   int mds_sessions = read_binary(unsigned char, mds_content, 0x0014);

   if(verbose)
   {
      printf("MDS/MDF version: %d.%d\n",mds_version, mds_revision);
   }

   nsessions=mds_sessions;

   int mds_session_offset = 0x58; 
   int mds_extrablocks_offset=0;

   for(int mds_current_session=0;mds_current_session<mds_sessions;mds_current_session++)
   {
      sessions[mds_current_session].pregap = read_binary(int, mds_content, mds_session_offset+0x0000);
      sessions[mds_current_session].sectors = read_binary(int, mds_content, mds_session_offset+0x0004);
      sessions[mds_current_session].something1 = read_binary(unsigned short, mds_content, mds_session_offset+0x0008);
      sessions[mds_current_session].datablocks = read_binary(unsigned char, mds_content, mds_session_offset+0x000a);
      sessions[mds_current_session].leadinblocks = read_binary(unsigned char, mds_content, mds_session_offset+0x000b);
      sessions[mds_current_session].session = read_binary(unsigned short, mds_content, mds_session_offset+0x000c);
      sessions[mds_current_session].last_track = read_binary(unsigned short, mds_content, mds_session_offset+0x000e);
      sessions[mds_current_session].something2 = read_binary(int, mds_content, mds_session_offset+0x0010);
      sessions[mds_current_session].datablocks_offset = read_binary(int, mds_content, mds_session_offset+0x0014);


      mds_extrablocks_offset=sessions[mds_current_session].datablocks_offset + mds_datablock_size*sessions[mds_current_session].datablocks;

      mds_session_offset = mds_session_offset + 0x18;
   }

   int mds_extrablocks_num=0;

   for(int mds_current_session=0;mds_current_session<mds_sessions;mds_current_session++)
   {
      sessions[mds_current_session].extrablocks_offset = mds_extrablocks_offset + mds_extrablock_size * mds_extrablocks_num;

      mds_extrablocks_num+=sessions[mds_current_session].datablocks;

   }

   for(int mds_current_session=0;mds_current_session<mds_sessions;mds_current_session++)
   {

      // making sure table is empty
      sessions[mds_current_session].ntracks=0;

      // read datablocks and extrablocks
      for(int datablock=0;datablock<sessions[mds_current_session].datablocks;datablock++)
      {
         int datablock_offset = sessions[mds_current_session].datablocks_offset+mds_datablock_size*datablock;
         int extrablock_offset = sessions[mds_current_session].extrablocks_offset+mds_extrablock_size*datablock;

         int mode = read_binary(unsigned char, mds_content, datablock_offset+0x0000);
         int smth1 = read_binary(unsigned char, mds_content, datablock_offset+0x0001);
         int flags = read_binary(unsigned short, mds_content, datablock_offset+0x0002);
         int track = read_binary(unsigned char, mds_content, datablock_offset+0x0004);
         int smth2 = read_binary(int, mds_content, datablock_offset+0x0005);
         int pmin = read_binary(unsigned char, mds_content, datablock_offset+0x0009);
         int psec = read_binary(unsigned char, mds_content, datablock_offset+0x000a);
         int pfrm = read_binary(unsigned char, mds_content, datablock_offset+0x000b);
         int smth3 = read_binary(int, mds_content, datablock_offset+0x000c);
         int sectorsize = read_binary(unsigned short, mds_content, datablock_offset+0x0010);
         int smth4 = read_binary(int, mds_content, datablock_offset+0x0014);
         int smth5 = read_binary(int, mds_content, datablock_offset+0x0018);
         int smth6 = read_binary(int, mds_content, datablock_offset+0x001c);
         int smth7 = read_binary(int, mds_content, datablock_offset+0x0020);
         int sector = read_binary(int, mds_content, datablock_offset+0x0024);
         __int64 offset = read_binary(__int64, mds_content, datablock_offset+0x0028);
         int pregap = read_binary(int, mds_content, extrablock_offset+0x0000);
         int sectors = read_binary(int, mds_content, extrablock_offset+0x0004);
         if(verbose)
            printf("datablock: %3d, track: %2x, mode: %2x, flags: %x, sector size: %d, MSF: %02d:%02d.%02d, sector: %d, offset: %d, pregap: %d, sectors: %d\n" ,
                  datablock, track, mode, flags, sectorsize, pmin, psec, pfrm, sector, (int)offset, pregap, sectors);
         // writing data to entries
         if (track < 0xa0)
         {
            strack *t=&(sessions[mds_current_session].tracks[sessions[mds_current_session].ntracks]);
            t->track     =track;
            t->mode      =mds_mode(mode);
            t->flags     =flags;

            t->pmin      = pmin;
            t->psec      = psec;
            t->pfrm      = pfrm;

            t->sectorsize=sectorsize;
            t->pregap    =pregap;
            t->sector    =sector;
            t->sectors   =sectors;
            t->offset    =offset;
            sessions[mds_current_session].ntracks++;
         }
      }
   }

   free(mds_content);
   fclose(mds_file);

   return true;
}

bool parse_nrg(const char *nrg_filename,bool verbose)
{
   /*"returns the supposable bin_filename and raw entries, if something went \
     wrong it throws an exception"*/
   // read the file
   FILE*nrg_file = fopen (nrg_filename, "rb");
   if(!nrg_file)
   {
      printf("Could not open the nrg-file <%s>\n",nrg_filename);
      return false;
   }

   // last 8 or 12 bytes gives us important information, like the signature
   // and where to find the rest of the image information.
   int nrg_filesize = flen(nrg_file);


   int nrg_signature;
   __int64 nrg_trailer_offset=0;
   int oldformat;

   fseek(nrg_file,-12,SEEK_END);
   fread(&nrg_signature,4,1,nrg_file);

#define NER5 0x3552454E
#define NERO 0x3552454E

   if(nrg_signature == NER5) // Nero 5.5.x or newer
   {
      if(verbose) printf("File format: NER5.\n");
      fread(&nrg_trailer_offset,8,1,nrg_file);
      byteswap(&nrg_trailer_offset,8);
      oldformat = 0; // Offsets are 64 bit.
   }
   else
   {
      fseek(nrg_file,-8,SEEK_END);
      fread(&nrg_signature,4,1,nrg_file);
      if(nrg_signature == NERO) // Nero older than 5.5.x
      {
         if(verbose) printf("File format: NERO.\n");
         fread(&nrg_trailer_offset,4,1,nrg_file);
         byteswap(&nrg_trailer_offset,4);
         oldformat = 1; // Offsets are 32 bit.
      }
      else
      {
         printf("Invalid data in <%s>. It is not an NRG file.",nrg_filename);
         return false;
      }
   }
   fseek(nrg_file,(u32)nrg_trailer_offset, SEEK_SET);

   char *nrg_content = (char*)malloc((u32)nrg_filesize - (u32)nrg_trailer_offset);
   if(!nrg_content)
   {
      printf("Could not allocate a buffer to read the nrg info from <%s>\n",nrg_filename);
      fclose(nrg_file);
      return false;
   }

   fread(nrg_content,(nrg_filesize - nrg_trailer_offset),1,nrg_file);

   nsessions=1;

   // find relative offsets for different blocks
   __int64 offs = 0;

   // CUEX is 64-bit variant of CUES, same goes for DAOX vs. DAOI and ETN2 vs. ETNF.
   // DAO is DiscAtOnce, ETN is TrackAtOnce. SINF is Session Information, and MTYP is Media Type.
   // CDTX is CD-Text. END! is a marker for the last block.
   char block_ids[10][5] = {"CUEX", "CUES", "DAOX", "DAOI", "CDTX", "ETNF", "ETN2", "SINF", "MTYP", "END!"};

   enum block_nums {CUEX, CUES, DAOX, DAOI, CDTX, ETNF, ETN2, SINF, MTYP, END};

   struct _block {
      struct info {
         __int64 offset;
         __int64 size;
      } infos[255];
      int found;
   } block[10];

   for(int block_num=0;block_num<10;block_num++)
      block[block_num].found=0;

   while(offs < (nrg_filesize - nrg_trailer_offset - 12))
   {
      bool found=false;
      for(int block_num=0;block_num<10;block_num++)
      {
         char *block_id = block_ids[block_num]; 
         if(memcmp(block_id,nrg_content+offs,4)==0)
         {
            int size = read_binary(unsigned int, nrg_content, offs+4);
            byteswap(&size,4);
            block[block_num].infos[block[block_num].found].offset=offs;
            block[block_num].infos[block[block_num].found].size=size;
            if(verbose) printf("%s [0x%x:0x%x]\n",block_id, offs, size);
            offs += size+8;
            block[block_num].found++;
            found=true;
            break;
         }
      }
      if(!found)
      {
         fprintf(stderr,"Error in parsing Nero CD-Image.\n");
         free(nrg_content);
         fclose(nrg_file);
         return false;
      }
   }

   bool is_dao;

   // DiscAtOnce or TrackAtOnce?
   if(oldformat)
   {
      if(block[DAOI].found && (block[DAOI].found==block[CUES].found)) 
      {
         is_dao = true;
         nsessions=block[DAOI].found;
      }
      else 
         if(block[ETNF].found && (block[SINF].found==block[ETNF].found))
         {
            is_dao = false;
            nsessions=block[ETNF].found;
         }
         else
         {
            fprintf(stderr,"Error in checking for DAO / TAO.\n");
            free(nrg_content);
            fclose(nrg_file);
            return false;
         }
   }
   else
   {
      if(block[DAOX].found && (block[DAOX].found==block[CUEX].found)) 
      {
         is_dao = true;
         nsessions=block[DAOX].found;
      }
      else 
         if(block[ETN2].found && (block[SINF].found==block[ETN2].found))
         {
            is_dao = false;
            nsessions=block[ETN2].found;
         }
         else
         {
            fprintf(stderr,"Error in checking for DAO / TAO.\n");
            free(nrg_content);
            fclose(nrg_file);
            return false;
         }
   }
   if(verbose)
   {
      if(is_dao) printf("CD-Image is Disc-At-Once.\n");
      else printf("CD-Image is Track-At-Once.\n");
   }       

   // making sure table is empty
   int entry=0;
   int sess=0;

   if(verbose) printf("TRACK ENTRIES:");

   for(sess=0;sess<nsessions;sess++)
   {
      sessions[sess].session=sess+1;

      if(is_dao) // Read DAO information.
      {
         entry = 1; // entry 0 is block-header entry so we start on entry 1
         for(;;)
         {
            __int64 cue_offset;
            __int64 dao_offset;
            if(oldformat)
            {
               cue_offset = block[CUES].infos[sess].offset;
               dao_offset = block[DAOI].infos[sess].offset;
            }
            else
            {
               cue_offset = block[CUEX].infos[sess].offset;
               dao_offset = block[DAOX].infos[sess].offset;
            }

            // tracktype. 0x40 = datatrack, 0x01 = copyright?
            int tracktype = read_binary(unsigned char, nrg_content, cue_offset+8+entry*16+0);
            int track_raw = read_binary(unsigned char, nrg_content, cue_offset+8+entry*16+1);
            int track = (track_raw >> 4) * 10 + (track_raw & 0x0f);

            // addr_ctrl. addresstype MSF or LBA in lower 4 bits, Q-channel control field in upper 4 bits.
            int addr_ctrl = read_binary(unsigned char, nrg_content, cue_offset+8+entry*16+2);
            int start_sector = read_binary(int, nrg_content, cue_offset+8+entry*16+4);
            byteswap(&start_sector,4);
            int nexttrack_raw = read_binary(unsigned char, nrg_content, cue_offset+8+entry*16+9);
            int nexttrack = (nexttrack_raw >> 4) * 10 + (nexttrack_raw & 0x0f);
            int end_sector = read_binary(int, nrg_content, cue_offset+8+entry*16+12);
            byteswap(&end_sector,4);

            char isrc[12];
            memcpy(isrc,nrg_content+dao_offset-12+entry*42,12);

            int sector_size = read_binary(unsigned short, nrg_content, dao_offset-12+entry*42+12);
            byteswap(&sector_size,2);
            int mode = read_binary(unsigned char, nrg_content, dao_offset-12+entry*42+14);

            __int64 start_offset;
            __int64 end_offset;

            if(oldformat)
            {
               start_offset = read_binary(unsigned int, nrg_content, dao_offset-12+entry*42+22);
               end_offset = read_binary(unsigned int, nrg_content, dao_offset-12+entry*42+26);
               byteswap(&start_offset,4);
               byteswap(&end_offset,4);
            }
            else
            {
               start_offset = read_binary(__int64, nrg_content, dao_offset-12+entry*42+26);
               end_offset = read_binary(__int64, nrg_content, dao_offset-12+entry*42+34);
               byteswap(&start_offset,8);
               byteswap(&end_offset,8);
            }

            if(verbose)
            {
               printf("[%3i] track: %3i, next track: %3i, start sector: %8i, end sector: %8i\n      sector size: %i, mode: %i, start: 0x%08x, end: 0x%08x.\n",
                     entry, track, nexttrack, start_sector, end_sector, sector_size, mode, start_offset, end_offset);
               printf("      tracktype: 0x%x, addr_ctrl: 0x%x, isrc: %s.\n",tracktype, addr_ctrl, isrc);
            }

            sessions[sess].tracks[entry-1].mode=nrg_mode(mode);
            sessions[sess].tracks[entry-1].track=track;
            sessions[sess].tracks[entry-1].sectorsize=sector_size;
            sessions[sess].tracks[entry-1].sector=start_sector;
            sessions[sess].tracks[entry-1].sectors=end_sector-start_sector;
            sessions[sess].tracks[entry-1].offset=start_offset;

            entry += 1;
            if(nexttrack_raw == 0xAA) break; // lead-out
         }
         sessions[sess].ntracks=entry-1;
      }
      else // Read TAO information.
      {
         int tracks_in_session = read_binary(int, nrg_content, block[SINF].infos[sess].offset+8+0);
         byteswap(&tracks_in_session,4);
         if(verbose) printf("Tracks in Session %i: %i.\n",sess,tracks_in_session);

         for(entry=0;entry<tracks_in_session;entry++)
         {
            __int64 offset;
            __int64 size;
            int mode;
            int sector;

            if(oldformat)
            {
               offset = read_binary(unsigned int, nrg_content, block[ETNF].infos[sess].offset+8+entry*20+0);
               size = read_binary(unsigned int, nrg_content, block[ETNF].infos[sess].offset+8+entry*20+4);
               mode = read_binary(unsigned char, nrg_content, block[ETNF].infos[sess].offset+8+entry*20+11);
               sector = read_binary(int, nrg_content, block[ETNF].infos[sess].offset+8+entry*20+12);
               byteswap(&size,4);
               byteswap(&offset,4);
               byteswap(&sector,4);
            }
            else
            {
               offset = read_binary(__int64, nrg_content, block[ETN2].infos[sess].offset+8+entry*32+0);
               size = read_binary(__int64, nrg_content, block[ETN2].infos[sess].offset+8+entry*32+8);
               mode = read_binary(unsigned char, nrg_content, block[ETN2].infos[sess].offset+8+entry*32+19);
               sector = read_binary(int, nrg_content, block[ETN2].infos[sess].offset+8+entry*32+20);
               byteswap(&size,8);
               byteswap(&offset,8);
               byteswap(&sector,4);
            }
            if(verbose)
               printf("[%3i] offset: 0x%08I64x, size: 0x%08I64x, sector: %i, mode: %x\n",entry+1, offset, size, sector, mode);

            sessions[sess].tracks[entry].mode=nrg_mode(mode);
            sessions[sess].tracks[entry].track=entry+1;
            sessions[sess].tracks[entry].sector=sector;
            sessions[sess].tracks[entry].sectorsize=2048;
            sessions[sess].tracks[entry].sectors=(s32)(size>>11); // size/2048 (user data sectors)
            sessions[sess].tracks[entry].offset=offset;
         }
         sessions[sess].ntracks=tracks_in_session;
      }
   }

   return true;
}
