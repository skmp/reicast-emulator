#include "utils/http.h"
#include "utils/string.h"
#include "utils/cloudrom.h"
#include "oslib/threading.h"

string onlimeRomsHost = "cloudroms.reicast.com";
int onlimeRomsPort = 80;
string onlimeRomsList = "/v0.lst";

static float download_percent = 0;
static bool download_cancel = false;
static bool download_error = false;
static bool download_done = false;
static string download_id;
static string download_file;
static string download_name;

void* download_thread_func(void *p)
{
	string did = download_id;
	string dfile = download_file;
	string http_path = "/" + did;

	printf("Download thread\n");
	FILE* f = fopen(dfile.c_str(), "wb");
	u8* buffer = nullptr;

	if (!f)
	{
		download_error = true;
		return nullptr;
	}

	auto size = HTTP_GET(onlimeRomsHost, onlimeRomsPort, http_path, 0, 0, 0);

	if (size == 0)
	{
		download_error = true;
		goto cleanup;
	}

	{
		auto chunk_size = size / 200;

		if (chunk_size > 1024 * 1024)
			chunk_size = 1024 * 1024;
		else if (chunk_size < 64 * 1024)
			chunk_size = 64 * 1024;

		buffer = new u8[chunk_size];

		for (int offs = 0; offs < size; offs += chunk_size)
		{
			if (download_cancel)
			{
				download_error = true;
				goto cleanup;
			}

			auto len = chunk_size;
			auto rem = size - offs;

			if (len > rem)
				len = rem;

			auto xfer = HTTP_GET(onlimeRomsHost, onlimeRomsPort, http_path, offs, len, buffer);

			if (xfer != len)
			{
				download_error = true;
				goto cleanup;
			}

			download_percent = 100.0f * offs / size;

			fwrite(buffer, 1, len, f);
		}
		

		download_done = true;
	}

cleanup:
	fclose(f);
	if (buffer) delete[] buffer;

	return nullptr;
}

static cThread download_thread(download_thread_func, NULL);

void start_download(string did, string dfile, string dname)
{
	download_percent = 0;
	download_cancel = false;
	download_error = false;
	download_done = false;

	download_id = did;
	download_file = dfile;
	download_name = dname;

	download_thread.Start();
}




OnlineRomInfo* OnlineRomsProvider::findRom(string id)
{
	for (auto it = roms.begin(); it != roms.end(); it++)
	{
		if (it->id == id)
			return &(*it);
	}

	return nullptr;
}

string OnlineRomsProvider::romPath(string id)
{
	auto folder = settings.dreamcast.ContentPath[0];

	auto path = folder + "/" + id;

	return path;
}

bool OnlineRomsProvider::romExists(string id)
{
	auto rv = false;
	auto path = romPath(id);

	FILE* f = fopen(path.c_str(), "rb");
	if (f)
	{
		rv = true;
		fclose(f);
	}

	return rv;
}


//v0
//"type" "id/filename" "name" "sha256"
void OnlineRomsProvider::fetchRomList()
{
	status = "Loading list ...";
	roms.clear();

	auto list = HTTP_GET(onlimeRomsHost, onlimeRomsPort, onlimeRomsList);

	if (list.size() == 0)
		return;

	auto lines = SplitString(list, "\n");

	if (lines[0] == "v0")
	{
		for (int i = 1; i<lines.size(); i++)
		{
			auto line = lines[i];

			if (line.length() > 0)
			{
				auto parts = SplitString(line, "\"");

				if (parts.size() != 9)
					continue;

				
				auto type = parts[1];
				auto id = parts[3];
				auto name = parts[5];
				auto sha256 = parts[7];

				auto status = romExists(id) ? RS_DOWNLOADED : RS_MISSING;

				OnlineRomInfo rom;
				
				rom.status = status;
				rom.type = type;
				rom.id = id;
				rom.name = name;
				rom.sha256 = sha256;

				roms.push_back(rom);
			}
		}
	}

	status = "";
}

string OnlineRomsProvider::getStatus()
{
	return status;
}

vector<OnlineRomInfo> OnlineRomsProvider::getRoms()
{
	return roms;
}

void OnlineRomsProvider::download(string id)
{
	auto rom = findRom(id);

	if (rom)
	{
		printf("Downloading %s\n", rom->name.c_str());
		rom->status = RS_DOWNLOADING;

		start_download(id, romPath(id), rom->name);
	}
}

void OnlineRomsProvider::downloaded(string id)
{
	auto rom = findRom(id);

	if (rom)
	{
		printf("Downloaded %s\n", rom->name.c_str());
		rom->status = RS_DOWNLOADED;
	}
}

void OnlineRomsProvider::remove(string id)
{
	auto rom = findRom(id);

	if (rom)
	{
		printf("Removing %s\n", rom->name.c_str());
		rom->status = RS_MISSING;
		::remove(romPath(id).c_str());
	}
}

bool OnlineRomsProvider::hasDownloadErrored()
{
	return download_error;
}

bool OnlineRomsProvider::hasDownloadFinished()
{
	return download_done;
}


void OnlineRomsProvider::downloadCancel()
{
	download_cancel = true;
}

float OnlineRomsProvider::getDownloadPercent()
{
	return download_percent;
}

string OnlineRomsProvider::getDownloadName()
{
	return download_name;
}

string OnlineRomsProvider::getDownloadId()
{
	return download_id;
}