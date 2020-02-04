/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "archive.h"
#include "7zArchive.h"
#ifndef _MSC_VER
#include "ZipArchive.h"
#endif

Archive *OpenArchive(const char *path)
{
	std::string base_path(path);

#ifndef _MSC_VER
	Archive *sz_archive = new SzArchive();
	if (sz_archive->Open(base_path.c_str()) || sz_archive->Open((base_path + ".7z").c_str()) || sz_archive->Open((base_path + ".7Z").c_str()))
		return sz_archive;
	delete sz_archive;

	Archive *zip_archive = new ZipArchive();
	if (zip_archive->Open(base_path.c_str()) || zip_archive->Open((base_path + ".zip").c_str()) || zip_archive->Open((base_path + ".ZIP").c_str()))
		return zip_archive;
	delete zip_archive;
#endif

	return NULL;
}




