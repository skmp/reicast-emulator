#include "types.h"

#define TRUE 1
#define FALSE 0

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

size_t HTTP_GET(string host, int port,string path, size_t offs, size_t len, void* pdata)
{
    string request;
    string response;

    struct sockaddr_in serveraddr;
    int sock;

    std::stringstream request2;

	if (len) {
		request2 << "GET " << path << " HTTP/1.1"<<endl;
		request2 << "User-Agent: reicastdc" << endl;
		//request2 << "" << endl;
		request2 << "Host: " << host << endl;
		request2 << "Accept: */*" << endl;
		request2 << "Range: bytes=" << offs << "-" << (offs + len-1) << endl;
		request2 << endl;
	}
	else {
		request2 << "HEAD " << path << " HTTP/1.1"<<endl;
		request2 << "User-Agent: reicastdc" << endl;
		//request2 << "" << endl;
		request2 << "Host: " << host << endl;
		request2 << endl;
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

    //open socket
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -1;

	
    //connect
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family      = AF_INET;
	serveraddr.sin_addr.s_addr = *(int*)gethostbyname( host.c_str() )->h_addr_list[0];
    serveraddr.sin_port        = htons((unsigned short) port);
    if (connect(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

#if HOST_OS == OS_WINDOWS
	BOOL v = TRUE;
#else
	int v = 1;
#endif

	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&v, sizeof(v));
    //send request
    if (send(sock, request.c_str(), request.length(), 0) != request.length())
        return -1;

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

	if (len == 0) {
		rv = content_length;
	}
	else {
		verify(len == content_length); //crash is a bit too harsh here perhaps?
		u8* ptr = (u8*)pdata;
		do
		{
			int rcv = recv(sock, (char*)ptr, len, 0);
			verify(rcv > 0 && len>= rcv);
			len -= rcv;
			ptr += rcv;
		}
		while (len >0);

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

string HTTP_GET(string host, int port, string path)
{
	auto size = HTTP_GET(host, port, path, 0, 0, nullptr);
	
	if (size == 0)
		return "";

	char* data = new char[size];

	auto size2 = HTTP_GET(host, port, path, 0, size, data);

	verify(size2 == size);

	string rv = string(data);

	delete[] data;

	return rv;
}