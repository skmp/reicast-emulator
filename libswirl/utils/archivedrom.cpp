#include "license/bsd"
#include "fstream"
#include "archivedrom.h"
#include "deps/libzip/zip.h"
#include "deps/lzma/7z.h"
#include "deps/lzma/7zCrc.h"
#include "deps/lzma/Alloc.h"
#include "deps/lzma/7zFile.h"
#include "oslib/threading.h"

#include "stdclass.h"
#include "../stdclass.h"

#define kInputBufSize ((size_t)1 << 18)

static bool crc_tables_generated;

static bool extract_cancel = false;
static bool extract_error = false;
static bool extract_done =false;

static std::string arcPathT;
static std::string fileNameT;

void* extractArchive(void *p)
{
    ArchivedRomsCustom temp_item;

    int archive_check = temp_item.openArchive(arcPathT, fileNameT);

    if (archive_check == 1){

        if (temp_item.openZip()){
            if (temp_item.DecompressZip()){
                extract_done = true;
                return nullptr;

            }else{
                extract_error = true;
                return nullptr;

            }
        }else {
            extract_error = true;
            return nullptr;
        }
    }else if (archive_check == 0){
        if(temp_item.open7z()){
            if(temp_item.Decompress7z()){
                extract_done = true;
                return nullptr;
            }else{
                extract_error = true;
                return nullptr;
            }
        } else {
            extract_error = true;
            return nullptr;
        }
    }else{
        extract_error = true;
        return nullptr;
    }


}

static cThread extract_thread(extractArchive, NULL);

void start_Decompress(const std::string &path , const std::string &filename){
    extract_cancel = false;
    extract_error = false;
    extract_done = false;

    arcPathT = path;
    fileNameT = filename;

    extract_thread.Start();
}



void ArchivedRomsCustom::startExport(const std:: string &path, const std::string &filename){

    start_Decompress(path, filename);

}


int ArchivedRomsCustom::openArchive(const std::string &path, const std::string &filename ) { //checking what type of archive we have
    zipPath = path.c_str();
    std::string extension = filename.substr(filename.size() - 4).c_str();
    std::string extension_7z = filename.substr(filename.size() - 3).c_str();
    if (!stricmp(extension.c_str(), ".zip") && stricmp(extension_7z.c_str(), ".7z")) {
        return 1;
    }
    else if(stricmp(extension.c_str(), ".zip") && !stricmp(extension_7z.c_str(), ".7z")) {
        return 0;
    }
    else
        return -1;

}


bool ArchivedRomsCustom::openZip() {
    za = zip_open(zipPath.c_str(), 0, NULL);
    if (za == NULL) {
        return false;
    }else{
        return true;
    }
}

bool ArchivedRomsCustom::DecompressZip(){
    bool extraction = true;
    zipPath = zipPath.substr(0, zipPath.find_last_of("\\/") + 1);
    for (int i = 0; i < zip_get_num_files(za); i++) {
        std::string temp_name_holder = zip_get_name(za, i, 0);
        std::string extension = temp_name_holder.substr(temp_name_holder.size() - 4).c_str();
        if (!stricmp(extension.c_str(), ".cdi") || !stricmp(extension.c_str(), ".gdi") || !stricmp(extension.c_str(), ".chd") || !stricmp(extension.c_str(), ".cue")){
            name_holder = temp_name_holder;
            if(!stricmp(extension.c_str(), ".cdi")){
                index_holder = i;
                if(!isDecompressedZip())
                    extraction = false;
            }if(!stricmp(extension.c_str(), ".gdi") || !stricmp(extension.c_str(), ".cue")){
                std::string new_dir = name_holder.substr(0, name_holder.find_last_of("."));
                zipPath.append(new_dir).append("/");
                make_directory(zipPath);
                if(!gdi_chdZip())
                    extraction = false;
            }
        }
    }
    return extraction;
}

bool ArchivedRomsCustom::gdi_chdZip() {
    bool extraction = true;
    for (int j = 0; j < zip_get_num_files(za); j++){
        std::string temp_name_loop = zip_get_name(za, j, 0);
        index_holder = j;
        name_holder = temp_name_loop;
        std::string extension_loop = temp_name_loop.substr(temp_name_loop.size() - 4).c_str();
        if (!stricmp(extension_loop.c_str(), ".bin") || !stricmp(extension_loop.c_str(), ".raw") || !stricmp(extension_loop.c_str(), ".gdi") || !stricmp(extension_loop.c_str(), ".cue"))
            if(!isDecompressedZip())
                extraction = false;
    }

   return extraction;
}

bool ArchivedRomsCustom::isDecompressedZip(){
    struct zip_stat pre;
    zip_stat_init(&pre);
    zip_stat_index(za, index_holder, 0, &pre);
    char *contents = new char[pre.size];
    zip_file *sb = zip_fopen_index(za, index_holder, ZIP_FL_UNCHANGED);

    auto did_read = zip_fread(sb, contents, pre.size);
    if (did_read <= 0) {
        zip_fclose(sb);
        return false;
    }
    zip_fclose(sb);

    writeFile(contents, pre.size);

    delete[] contents;
    return true;
}

void ArchivedRomsCustom::writeFile(char * contents, size_t tsize){ //common write function for both archive types
    std::string path_nofile = zipPath;
    path_nofile.append(name_holder);

    auto myfile = std::fstream(path_nofile.c_str(), std::ios::out | std::ios::binary);
    myfile.write(contents, tsize);
    myfile.close();

}

bool ArchivedRomsCustom::open7z(){
    SzArEx_Init(&szarchive);
    if (InFile_Open(&archiveStream.file, zipPath.c_str()))
        return false;
    FileInStream_CreateVTable(&archiveStream);
    LookToRead2_CreateVTable(&lookStream, false);
    lookStream.buf = (Byte *)ISzAlloc_Alloc(&g_Alloc, kInputBufSize);
    if (lookStream.buf == NULL)
        return false;
    lookStream.bufSize = kInputBufSize;
    lookStream.realStream = &archiveStream.vt;
    LookToRead2_Init(&lookStream);

    if (!crc_tables_generated)
    {
        CrcGenerateTable();
        crc_tables_generated = true;
    }
    SRes res = SzArEx_Open(&szarchive, &lookStream.vt, &g_Alloc, &g_Alloc);


    return (res == SZ_OK);
}

bool ArchivedRomsCustom::Decompress7z(){ //this IS slower than DecompressZip
    zipPath = zipPath.substr(0, zipPath.find_last_of("\\/") + 1);
    u16 fname[512];
    bool extraction = true;
    for (int i = 0; i < szarchive.NumFiles; i++)
    {
        unsigned isDir = SzArEx_IsDir(&szarchive, i);
        if (isDir)
            continue;

        int len = SzArEx_GetFileNameUtf16(&szarchive, i, fname);
        char szname[512];
        int j = 0;
        for (; j < len && j < sizeof(szname) - 1; j++)
            szname[j] = fname[j];
        szname[j] = 0;
        name_holder = "";
        for (int x = 0; x < len - 1; x++) {
            name_holder = name_holder + szname[x];
        }
        std::string extension = name_holder.substr(name_holder.size() - 4).c_str();
        if (stricmp(extension.c_str(), ".cdi") && stricmp(extension.c_str(), ".gdi") && stricmp(extension.c_str(), ".chd") && stricmp(extension.c_str(), ".cue"))
            continue;

        if(!stricmp(extension.c_str(), ".cdi") || !stricmp(extension.c_str(), ".chd")){
            if(!isDecompressed7z(i)){
                extraction = false;
            }
        }
        if(!stricmp(extension.c_str(), ".gdi") || !stricmp(extension.c_str(), ".cue")){
            std::string new_dir = name_holder.substr(0, name_holder.find_last_of("."));
            zipPath.append(new_dir).append("/");
            make_directory(zipPath);
            if(!gdi_chd7z())
                extraction = false;
        }



    }
    return extraction;
}

bool ArchivedRomsCustom::gdi_chd7z(){
    u16 fname[512];
    for (int j = 0; j < szarchive.NumFiles; j++)
    {
        unsigned isDir_loop = SzArEx_IsDir(&szarchive, j);
        if (isDir_loop)
            continue;

        int len_loop = SzArEx_GetFileNameUtf16(&szarchive, j, fname);
        char szname_loop[512];
        int z = 0;
        for (; z < len_loop && z < sizeof(szname_loop) - 1; z++)
            szname_loop[z] = fname[z];
        szname_loop[z] = 0;
        name_holder = "";
        for (int x = 0; x < len_loop - 1; x++) {
            name_holder = name_holder + szname_loop[x];
        }
        std::string extension_loop = name_holder.substr(name_holder.size() - 4).c_str();
        if (!stricmp(extension_loop.c_str(), ".bin") || !stricmp(extension_loop.c_str(), ".raw") || !stricmp(extension_loop.c_str(), ".gdi") || !stricmp(extension_loop.c_str(), ".cue"))
            if(!isDecompressed7z(j))
                return false;
    }


    return true;


}

bool ArchivedRomsCustom::isDecompressed7z( int i ){

    size_t offset = 0;
    size_t out_size_processed = 0;
    SRes res = SzArEx_Extract(&szarchive, &lookStream.vt, i, &block_idx, &out_buffer, &out_buffer_size, &offset, &out_size_processed, &g_Alloc, &g_Alloc);
    if (res != SZ_OK)
        return false;
    writeFile((char *) out_buffer + offset, out_size_processed);
    return true;

}


bool ArchivedRomsCustom::isExtracted(){
    return extract_done;
}

ArchivedRomsCustom::~ArchivedRomsCustom() {

    if (za != NULL){
        zip_close(za);
    }

    if (lookStream.buf != NULL)
    {
        File_Close(&archiveStream.file);
        ISzAlloc_Free(&g_Alloc, lookStream.buf);
        if (out_buffer != NULL)
            ISzAlloc_Free(&g_Alloc, out_buffer);
        SzArEx_Free(&szarchive, &g_Alloc);
    }

}