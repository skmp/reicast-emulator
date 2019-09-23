#ifndef _NET_PLATFORM_H
#define _NET_PLATFORM_H

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef HAVE_LIBNX
#include <switch.h>
#include <arpa/inet.h>
#define SOL_TCP 6 // Shrug
#else
#include <netinet/ip.h>
#endif // HAVE_LIBNX

#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#else
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#define closesocket close
typedef int sock_t;
#define VALID(s) ((s) >= 0)
#define L_EWOULDBLOCK EWOULDBLOCK
#define L_EAGAIN EAGAIN
#define get_last_error() (errno)
#define INVALID_SOCKET (-1)
#define perror(s) do { INFO_LOG(MODEM, "%s: %s", (s) != NULL ? (s) : "", strerror(get_last_error())); } while (false)
#else
typedef SOCKET sock_t;
#define VALID(s) ((s) != INVALID_SOCKET)
#define L_EWOULDBLOCK WSAEWOULDBLOCK
#define L_EAGAIN WSAEWOULDBLOCK
#define get_last_error() (WSAGetLastError())
#define perror(s) do { INFO_LOG(MODEM, "%s: Winsock error: %d\n", (s) != NULL ? (s) : "", WSAGetLastError()); } while (false)
#define SHUT_WR SD_SEND
#define SHUT_RD SD_RECEIVE
#define SHUT_RDWR SD_BOTH
#endif

#endif
