/*
	This file is part of libswirl
*/
#include "license/bsd"

#pragma once
#include "types.h"

class ArchiveFile
{
public:
	virtual ~ArchiveFile() {}
	virtual u32 Read(void *buffer, u32 length) = 0;
};

class Archive
{
public:
	virtual ~Archive() {}
	virtual ArchiveFile *OpenFile(const char *name) = 0;

	friend Archive *OpenArchive(const char *path);

private:
	virtual bool Open(const char *name) = 0;
};

Archive *OpenArchive(const char *path);
