/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"
#include <functional>

string url_encode(const string &value);

enum HTTP_METHOD
{
	HM_GET,
	HM_HEAD
};

size_t HTTP(HTTP_METHOD method, string host, int port,string path, size_t offs, size_t len, std::function<bool(void* data, size_t len)> cb);
size_t HTTP(HTTP_METHOD method, string host, int port,string path, size_t offs, size_t len, void* pdata);
string HTTP(HTTP_METHOD method, string host, int port, string path);