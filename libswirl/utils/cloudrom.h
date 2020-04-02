/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

enum RomStatus
{
	RS_MISSING,
	RS_DOWNLOADING,
	RS_DOWNLOADED
};

struct OnlineRomInfo
{
	RomStatus status = RS_MISSING;

	string type;
	string id;
	string name;
	string sha256;
	string url;
	string filename;
};

struct OnlineRomsProvider
{
	virtual void fetchRomList() = 0;

	virtual string getStatus() = 0;
	virtual bool downloadingAny() = 0;

	virtual vector<OnlineRomInfo> getRoms() = 0;

	virtual void download(string id) = 0;

	virtual void downloaded(string id) = 0;

	virtual void remove(string id) = 0;
	
	virtual void downloadCancel() = 0;
	virtual bool hasDownloadErrored() = 0;
	virtual bool hasDownloadFinished() = 0;
	virtual void clearDownloadStatus() = 0;

	virtual float getDownloadPercent() = 0;
	virtual string getDownloadName() = 0;
	virtual string getDownloadId() = 0;

	static OnlineRomsProvider* CreateHttpProvider(string host, string path);
};
