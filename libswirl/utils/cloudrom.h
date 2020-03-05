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
};

class OnlineRomsProvider
{
	bool loggedIn = false;
	string status;

	vector<OnlineRomInfo> roms;

	OnlineRomInfo* findRom(string id);

	string romPath(string id);

	bool romExists(string id);

public:

	void fetchRomList();

	string getStatus();

	vector<OnlineRomInfo> getRoms();

	void download(string id);

	void downloaded(string id);

	void remove(string id);
	
	void downloadCancel();
	bool hasDownloadErrored();
	bool hasDownloadFinished();

	float getDownloadPercent();
	string getDownloadName();
	string getDownloadId();
};
