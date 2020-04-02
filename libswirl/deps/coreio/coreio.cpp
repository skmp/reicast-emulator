
#include "coreio.h"

#include "utils/http.h"

struct CoreFile
{
	size_t seek_ptr = 0;

	virtual void seek() = 0;
	virtual size_t read(void* buff, size_t len) = 0;
	virtual size_t size() = 0;

	virtual ~CoreFile() { }
};

struct CoreFileLocal: CoreFile
{
	FILE* f = nullptr;

	static CoreFile* open(const char* path)
	{
		FILE* f = fopen(path, "rb");

		if (!f)
			return nullptr;

		CoreFileLocal* rv = new CoreFileLocal();

		rv->f = f;

		return rv;
	}

	size_t read(void* buff, size_t len)
	{
		return fread(buff,1,len,f);
	}

	void seek()
	{
		fseek(f, seek_ptr, SEEK_SET);
	}

	size_t size()
	{
		size_t p = ftell(f);
		fseek(f, 0, SEEK_END);

		size_t rv = ftell(f);
		fseek(f, p, SEEK_SET);

		return rv;
	}

	~CoreFileLocal() { fclose(f); }
};

struct CoreFileHTTP: CoreFile
{
	string url;
	
	static CoreFile* open(const char* path)
	{
		string p = path;

		if (p.substr(0,7)!="http://" && p.substr(0,8)!="https://")
			return nullptr;

		CoreFileHTTP* rv = new CoreFileHTTP();

		rv->url = p;

		return rv;
	}

	size_t read(void* buff, size_t len)
	{
		return HTTP(HM_GET, url, seek_ptr, len, buff);
	}

	void seek() { }

	size_t size()
	{
		return HTTP(HM_HEAD, url, 0, 0,0);
	}

	~CoreFileHTTP() { }
};

static CoreFile* (*vfs[])(const char* path) =
{
	&CoreFileHTTP::open,
	&CoreFileLocal::open,
	nullptr
};

core_file* core_fopen(const char* filename)
{
	CoreFile* rv = nullptr;
	for (int i = 0; vfs[i] != nullptr; i++)
	{
		auto fs_open = vfs[i];

		rv = fs_open(filename);
		if (rv)
			break;
	}

	if (rv)
	{
		core_fseek((core_file*)rv, 0, SEEK_SET);
		return (core_file*)rv;
	}
	else
	{
		printf("CoreIO: Failed to open %s\n", filename);
		return nullptr;
	}
}

size_t core_fseek(core_file* fc, size_t offs, size_t origin) {
	CoreFile* f = (CoreFile*)fc;
	
	if (origin == SEEK_SET)
		f->seek_ptr = offs;
	else if (origin == SEEK_CUR)
		f->seek_ptr += offs;
	else
		die("Invalid code path");

	f->seek();

	return 0;
}

size_t core_ftell(core_file* fc) {
	CoreFile* f = (CoreFile*)fc;
	return f->seek_ptr;
}

int core_fread(core_file* fc, void* buff, size_t len)
{
	CoreFile* f = (CoreFile*)fc;

	auto rv = f->read(buff, len);

	f->seek_ptr += rv;

	return (int)rv;
}

int core_fclose(core_file* fc)
{
	CoreFile* f = (CoreFile*)fc;

	delete f;

	return 0;
}

size_t core_fsize(core_file* fc)
{
	CoreFile* f = (CoreFile*)fc;

    return f->size();
}
