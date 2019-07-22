#pragma once
#include "types.h"

string url_encode(const string &value);
size_t HTTP_GET(string host, int port,string path, size_t offs, size_t len, void* pdata);
string HTTP_GET(string host, int port, string path);