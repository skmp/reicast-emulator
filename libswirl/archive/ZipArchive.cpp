/*
	This file is part of libswirl
        original code by flyinghead
*/
#include "license/bsd"


#include "ZipArchive.h"

ZipArchive::~ZipArchive()
{
	zip_close(zip);
}

bool ZipArchive::Open(const char* path)
{
	zip = zip_open(path, 0, NULL);
	return (zip != NULL);
}

ArchiveFile* ZipArchive::OpenFile(const char* name)
{
	zip_file *zip_file = zip_fopen(zip, name, 0);
	if (zip_file == NULL)
		return NULL;

	return new ZipArchiveFile(zip_file);
}

u32 ZipArchiveFile::Read(void* buffer, u32 length)
{
	return (u32)zip_fread(zip_file, buffer, length);
}
