#include "license/bsd"
#include "deps/libzip/zip.h"
#include "deps/lzma/LzmaLib.h"
#include "deps/lzma/7z.h"
#include "deps/lzma/7zFile.h"
#include "types.h"


#pragma once

class ArchivedRomsCustom{


private:
    int index_holder;
    std::string name_holder;
    struct zip *za;
    std::string zipPath;
    CSzArEx szarchive;              /* original code for extracting 7z archives found at libswirl/archive */
    UInt32 block_idx;               /* it can have any value before first call (if outBuffer = 0) */
    Byte *out_buffer;               /* it must be 0 before first call for each new archive. */
    size_t out_buffer_size;         /* it can have any value before first call (if outBuffer = 0) */
    CFileInStream archiveStream;
    CLookToRead2 lookStream;



public:

    ArchivedRomsCustom() : out_buffer(NULL) {
        memset(&archiveStream, 0, sizeof(archiveStream));
        memset(&lookStream, 0, sizeof(lookStream));
    }
    virtual ~ArchivedRomsCustom();

    void startExport(const std:: string &path, const std::string &filename);

    int openArchive(const std::string& path, const std::string &filename);

    bool openZip();

    bool open7z();

    bool DecompressZip();

    bool isDecompressedZip();
    bool gdi_chdZip();

    bool Decompress7z();

    bool isDecompressed7z( int index );

    bool gdi_chd7z ();

    void writeFile(char *content, size_t tsize);

    bool isExtracted();




};