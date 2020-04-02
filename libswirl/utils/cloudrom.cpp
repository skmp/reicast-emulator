/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "utils/http.h"
#include "utils/string.h"
#include "utils/cloudrom.h"
#include "oslib/threading.h"

struct OnlineRomsProvider_impl: OnlineRomsProvider
{

	string onlimeRomsHost;
	string onlimeRomsList;

	float download_percent = 0;
	bool download_cancel = false;
	bool download_error = false;
	bool download_done = false;
	string download_id;
	string download_file;
	string download_name;

	cThread download_thread;

	OnlineRomsProvider_impl(string onlimeRomsHost, string onlimeRomsList): 
		onlimeRomsHost(onlimeRomsHost),
		onlimeRomsList(onlimeRomsList),
		download_thread(STATIC_FORWARD(OnlineRomsProvider_impl, download_thread_func), this)
	{
		
	}

	void* download_thread_func()
	{
		string did = download_id;
		string dfile = download_file;
		string url = did;

		printf("Download thread\n");
		FILE* f = fopen(dfile.c_str(), "wb");

		if (!f)
		{
			download_error = true;
			return nullptr;
		}

		auto size = HTTP(HM_HEAD, url, 0, 0, nullptr);

		if (size == 0)
		{
			download_error = true;

			fclose(f);
			return nullptr;
		}

		auto xfer = HTTP(HM_GET, url, 0, size, [this, f, size](void* data, size_t len){
			fwrite(data, 1, len, f);
			download_percent += 100.0f * len / size;

			if (download_cancel)
			{
				download_error = true;
				return false;
			}
			else
				return true;
		});

		if (xfer != 0 && !download_error)
		{
			download_done = true;
		}
		else
		{
			download_error = true;
		}

		fclose(f);
		return nullptr;
	}

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


	string status;

	vector<OnlineRomInfo> roms;

	OnlineRomInfo* findRom(string id)
	{
		for (auto it = roms.begin(); it != roms.end(); it++)
		{
			if (it->id == id)
				return &(*it);
		}

		return nullptr;
	}

	string romPath(string id)
	{
		auto folder = settings.dreamcast.ContentPath[0];

		auto path = folder + "/" + id;

		return path;
	}

	bool romExists(string id)
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
	void fetchRomList()
	{
		status = "Loading list ...";
		roms.clear();

		auto list = HTTP(HM_GET, onlimeRomsHost + onlimeRomsList);

		if (list.size() == 0)
		{
			status = "Cannot reach server";
			return;
		}

		auto lines = SplitString(list, "\n");

		if (lines[0] == "v1" || lines[0] == "v0")
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

					string rom_file = id;
					string url = id;

					if (url.rfind("http", 0) == 0) {
						// full http link, use the name for the file
						rom_file = name;
					} else {
						// add the http://... part
						url = onlimeRomsHost + "/" + id;
					}

					rom.filename = rom_file;
					rom.url = url;
					
					roms.push_back(rom);
				}
			}
		}

		status = "";
	}

	string getStatus()
	{
		return status;
	}

	vector<OnlineRomInfo> getRoms()
	{
		return roms;
	}

	bool downloadingAny()
	{
		for (const auto& rom : roms) {
			if (rom.status == RS_DOWNLOADING)
				return true;
		}
		
		return false;
	}

	void download(string id)
	{
		auto rom = findRom(id);

		if (rom)
		{
			printf("Downloading %s\n", rom->name.c_str());
			rom->status = RS_DOWNLOADING;

			string rom_file = id;

			if (id.rfind("http", 0) == 0) {
				// full htto link, use the name for the file
				rom_file = rom->name;
			} else {
				// add the http://... part
				id = onlimeRomsHost + "/" + id;
			}
			

			start_download(id, romPath(rom_file), rom->name);
		}
	}

	void downloaded(string id)
	{
		auto rom = findRom(id);

		if (rom)
		{
			printf("Downloaded %s\n", rom->name.c_str());
			rom->status = RS_DOWNLOADED;
		}
	}

	void remove(string id)
	{
		auto rom = findRom(id);

		if (rom)
		{
			printf("Removing %s\n", rom->name.c_str());
			rom->status = RS_MISSING;
			::remove(romPath(id).c_str());
		}
	}

	bool hasDownloadErrored()
	{
		return download_error;
	}

	void clearDownloadStatus()
	{
		download_done = download_error = false;

		for (auto& rom : roms)
		{
			rom.status = romExists(rom.id) ? RS_DOWNLOADED : RS_MISSING;
		}
	}

	bool hasDownloadFinished()
	{
		return download_done;
	}


	void downloadCancel()
	{
		download_cancel = true;
	}

	float getDownloadPercent()
	{
		return download_percent;
	}

	string getDownloadName()
	{
		return download_name;
	}

	string getDownloadId()
	{
		return download_id;
	}

};

OnlineRomsProvider* OnlineRomsProvider::CreateHttpProvider(string host, string path)
{
	return new OnlineRomsProvider_impl(host, path);
}