/*
	This file is part of libswirl
*/
#include "license/bsd"



#ifndef CORE_ARCHIVE_ZIPARCHIVE_H_
#define CORE_ARCHIVE_ZIPARCHIVE_H_
#include "archive.h"
#include "deps/libzip/zip.h"

class ZipArchive : public Archive
{
public:
	ZipArchive() : zip(NULL) {}
	virtual ~ZipArchive();

	virtual ArchiveFile* OpenFile(const char* name) override;

private:
	virtual bool Open(const char* path) override;

	struct zip *zip;
};

class ZipArchiveFile : public ArchiveFile
{
public:
	ZipArchiveFile(struct zip_file *zip_file) : zip_file(zip_file) {}
	virtual ~ZipArchiveFile() { zip_fclose(zip_file); }
	virtual u32 Read(void* buffer, u32 length) override;

private:
	struct zip_file *zip_file;
};

#endif /* CORE_ARCHIVE_ZIPARCHIVE_H_ */
