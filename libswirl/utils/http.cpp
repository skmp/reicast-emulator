/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "types.h"

#define TRUE 1
#define FALSE 0

#include "http.h"

#include <sstream>
#include <iomanip>
#include <cctype>

#if HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/ip.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <unistd.h>
#else
	#include <windows.h>
	#pragma comment (lib, "wsock32.lib")
#endif

string url_encode(const string &value) {
	ostringstream escaped;
	escaped.fill('0');
	escaped << hex;

	for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c  == '/' || c =='%' ) {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << '%' << setw(2) << int((unsigned char)c);
	}

	return escaped.str();
}

size_t HTTP(HTTP_METHOD method, string host, int port, string path, size_t offs, size_t len, std::function<bool(void* data, size_t len)> cb)
{
    string request;
    string response;

    struct sockaddr_in serveraddr;
    int sock;

    std::stringstream request2;

    //printf("HTTP: %d\n", method);

	if (method == HM_GET)
	{
		request2 << "GET " << path << " HTTP/1.1"<<endl;
		request2 << "User-Agent: reicastdc" << endl;
		request2 << "Host: " << host << endl;
		request2 << "Accept: */*" << endl;
		if (offs != 0 || len != 0)
		{
			//printf("HTTP: Range Request");
			request2 << "Range: bytes=" << offs << "-" << (offs + len-1) << endl;	
		}
		request2 << endl;
	}
	else if (method == HM_HEAD)
	{
		verify(offs == 0 && len == 0);

		request2 << "HEAD " << path << " HTTP/1.1"<<endl;
		request2 << "User-Agent: reicastdc" << endl;
		request2 << "Host: " << host << endl;
		request2 << endl;
	}
	else
	{
		die("HTTP: Unsupported method\n");
	}

	request = request2.str();
    //init winsock

	static bool init = false;
	if (!init) {
#if HOST_OS == OS_WINDOWS
		static WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
			die("WSAStartup fail");
#endif
		init = true;
	}

	// resolve the ip address
	auto host_ip = gethostbyname(host.c_str());

	if (!host_ip)
		return 0;

    //open socket
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return 0;

	
    //connect
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family      = AF_INET;
	serveraddr.sin_addr.s_addr = *(int*)host_ip->h_addr_list[0];
    serveraddr.sin_port        = htons((unsigned short) port);
    if (connect(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return 0;

#if HOST_OS == OS_WINDOWS
	BOOL v = TRUE;
#else
	int v = 1;
#endif

	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&v, sizeof(v));
    //send request
    if (send(sock, request.c_str(), request.length(), 0) != request.length())
        return 0;

	/*
    //get response
    response = "";
    resp_leng= BUFFERSIZE;
    
	*/

	/*
		parse headers ..
	*/
	int content_length = 0;
	
	size_t rv = 0;

	for (;;) {
		stringstream ss;
		for (;;) {
			char t;
			if (recv(sock, &t, 1, 0) <= 0)
				goto _cleanup;

			if (t != '\n'){
				ss << t;
				continue;
			}

			string ln = ss.str();

			if (ln.size() == 1)
				goto _data;
			string CL = "Content-Length:";

			if (ln.substr(0, CL.size()) == CL) {
				sscanf(ln.substr(CL.size(), ln.npos).c_str(),"%d", &content_length);
			}

			break;
		}
	}

_data:

	if (len == 0)
	{
		rv = content_length;
	}
	else
	{
		//printf("%d vs %d\n", len, content_length);
		verify(len == content_length); //crash is a bit too harsh here perhaps?

		u32 chunk_max = 64 * 1024;

		u8* pdata = new u8[chunk_max];

		do
		{
			u32 chunk_remaining = chunk_max;

			if (chunk_remaining > len)
				chunk_remaining = len;

			u8* ptr = pdata;
			do
			{
				int rcv = recv(sock, (char*)ptr, chunk_remaining, 0);
				//printf("%d > %d\n", chunk_remaining, rcv);
				verify(rcv >= 0);
				verify(chunk_remaining > 0 && chunk_remaining>= rcv);
				chunk_remaining -= rcv;
				len -= rcv;
				ptr += rcv;
			}
			while (chunk_remaining >0);

			if (!cb(pdata, ptr - pdata))
				break;

		} while (len >0);

		delete[] pdata;

		rv = content_length;
	}

	_cleanup:
    //disconnect
#if HOST_OS == OS_WINDOWS
    closesocket(sock);
#else
	close(sock);
#endif

	/*
    //cleanup
    WSACleanup();
	*/

    return  rv;
}

size_t HTTP(HTTP_METHOD method, string host, int port, string path, size_t offs, size_t len, void* pdata)
{
	return HTTP(method, host, port, path, offs, len, [&pdata](void* data, size_t len) {
		memcpy(pdata, data, len);
		(u8*&)pdata += len;

		return true;
	});
}

string HTTP(HTTP_METHOD method, string host, int port, string path)
{
	auto size = HTTP(method, host, port, path, 0, 0, (void*)nullptr);
	
	if (size == 0)
		return "";

	char* data = new char[size];

	auto size2 = HTTP(method, host, port, path, 0, size, data);

	verify(size2 == size);

	string rv = string(data);

	delete[] data;

	return rv;
}