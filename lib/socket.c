#include <stdio.h>      //snprintf
#include <errno.h>      //errno
#include <string.h>     //strerror
#include <sys/socket.h> //connect read write
#include <unistd.h>     //close
#include <netdb.h>      //addrinfo
#include <arpa/inet.h>  //inet_pton
#include <resolv.h>


#include "log.h"
#include "socket.h"

#define LISTEN_LIMIT 5

/* Explanation
Connect connects a socket to a remote server: it is only used by a client. It can be used for both UDP and TCP.
Bind binds the socket to a port: it is only used by a server. It is used for both UDP and TCP.

       |         Datagram          |  Stream
       |            UDP            |    TCP
       |  Connected  | Unconnected |  Connected
-------+-------------+-------------+-------------
Server |             |    bind     |   bind
       |             |             |   listen
	   |             |             |   accept
	   |             |   recvfrom  |   recv/read
	   |             |   sendto    |   send/write
-------+-------------+-------------+-------------
Client |  connect    |             |   connect
       |  send/write |   sendto    |   send/write
	   |  recv/read  |  recvfrom   |   recv/read



*/

static struct addrinfo * getAddresses(char *server_name, char *port, int protocol) { //returns a pointer to a list of addresses or NULL
    struct addrinfo hints;
    struct addrinfo *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_INET;  // Allow IPv4 only
    hints.ai_socktype = protocol;
    hints.ai_flags    = 0;
    hints.ai_protocol = 0;        // Any protocol
    int s = getaddrinfo(server_name, port, &hints, &result);
    if (s != 0)
	{
		Log('e', "getAddresses: %s", gai_strerror(s));
        return NULL;
    }
	return result;
}
static int setSocketTimeout(int sfd, int seconds) {
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof tv))
	{
		LogErrno("setSocketTimeout-rcvtimeo");
		return -1;
	}
	if (setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,  sizeof tv))
	{
        LogErrno("setSocketTimeout-sndtimeo");
		return -1;
	}
	return 0;
}
static int setSocketNoTimeWaitAfterClose(int sfd) {
	int optval = 1;
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval,  sizeof(int)))
	{
		LogErrno("setSocketNoTimeWaitAfterClose");
		return -1;
	}
	return 0;
}
static int connectSocket(struct addrinfo *addresses, int *sfd) {

	struct addrinfo *rp;
    for (rp = addresses; rp != NULL; rp = rp->ai_next)
	{
		*sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (*sfd == -1) continue;

		if (connect(*sfd, rp->ai_addr, rp->ai_addrlen) != -1) break; // Success

		close(*sfd);
    }
    if (rp == NULL)   // No address succeeded
	{
		*sfd = -1;
        Log('e', "No address found");
		return -1;
    }
	return 0;
}
static int connectSocketProtocol(char *server_name, char *port, int protocol, int timeout, int *sfd) { //Returns 0 if successful or -1 if not
    // Obtain addresses matching the host and port, remember to free it
    struct addrinfo *result = getAddresses(server_name, port, protocol);
	if (result == NULL) return -1;
	
    //Try each address until we successfully connect
	int r = connectSocket(result, sfd);
	freeaddrinfo(result); //Whatever happens free the addresses
	if (r) return -1;
 	
	//Set socket to timeout in 1 second
	r = setSocketTimeout(*sfd, timeout);
	if (r)
	{
		close(*sfd);
		return -1;
	}
	return 0;
}
int SocketConnectUdp(char *server_name, char *port, int *sfd) { //Returns the 0 if successful or -1 if not
	return connectSocketProtocol(server_name, port, SOCK_DGRAM, 1, sfd);
}
int SocketConnectTcp(char *server_name, char *port, int *sfd) { //Returns the 0 if successful or -1 if not
	return connectSocketProtocol(server_name, port, SOCK_STREAM, 1, sfd);
}
int SocketConnectPasv(char *ip, unsigned short port, int *sfd) { //Returns the 0 if successful or -1 if not
/* IPv4 AF_INET sockets:
struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // load with inet_pton()
    char             sin_zero[8];  // zero this if you want to
	
	int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
*/
	//Make socket
	*sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sfd == -1)
	{
		LogErrno("SocketConnectPasv-socket");
		return -1;
	}

	//Make address
	struct sockaddr_in sa;
	bzero((char *) &sa, sizeof(sa));
	inet_pton(AF_INET, ip, &(sa.sin_addr));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	//Connect
	if (connect(*sfd, (struct sockaddr *) &sa, sizeof(sa)))
	{
		*sfd = -1;
		close(*sfd);
		LogErrno("SocketConnectPasv-connect");
		return -1;
    }
	
	//Set socket to timeout in 1 second
	int r = setSocketTimeout(*sfd, 1);
	if (r < 0)
	{
		*sfd = -1;
		close(*sfd);
		return -1;
	}

	return 0;
}
int SocketIp(int sfd, uint32_t *pIp) {
	struct sockaddr sockaddr;
	socklen_t sockaddrlen = sizeof(struct sockaddr);
	if (getpeername(sfd, &sockaddr, &sockaddrlen))
	{
		LogErrno("ERROR getpeername");
		return -1;
	}
	if (sockaddr.sa_family != AF_INET)
	{
		Log('e', "Socket address is not IPv4");
		return -1;
	}
	struct sockaddr_in *psockaddrIP4 = (struct sockaddr_in *)&sockaddr;
	struct sockaddr_in sockaddr_in = *psockaddrIP4;
	*pIp = sockaddr_in.sin_addr.s_addr; //s_addr is stored in network byte order
	return 0;
}
int SocketIpA(int sfd, char *ip, int len) {
	uint32_t uip;
	if (SocketIp(sfd, &uip)) return -1;
    if (inet_ntop(AF_INET, &uip, ip, len) == NULL)
	{
		LogErrno("ERROR inet_ntop");
		return -1;
	}
	return 0;
}

static int bindProtocol(unsigned short port, int protocol, int *sfd) {
    *sfd = socket(AF_INET, protocol, 0);
    if (*sfd < 0)
	{
		LogErrno("ERROR opening socket");
		return -1;
	}
	struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (setSocketNoTimeWaitAfterClose(*sfd)) return -1;
	if (bind(*sfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		LogErrno("ERROR on binding");
		return -1;
	}
	return 0;
}
int SocketBindTcp(unsigned short port, int *sfd) {
	if (bindProtocol(port, SOCK_STREAM, sfd)) return -1;
	listen(*sfd, LISTEN_LIMIT);
	return 0;
}
int SocketAccept(int listenSocket, int *newSocket) {
	struct sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	*newSocket = accept(listenSocket, (struct sockaddr *) &cli_addr, &clilen);
	if (*newSocket < 0)
	{
		LogErrno("ERROR on accept");
		return -1;
	}
	return 0;
}

int SocketSend(int sfd, void *pBuffer, int size) { //Returns 0 if successful or -1 if not
	if (sfd == -1)
	{
		Log('e', "SocketSend socket not connected");
		return -1;
	}
	while (size > 0)
	{
		int nwrite = write(sfd, pBuffer, size);
		if (nwrite <= 0)
		{
			LogErrno("SocketSend-write");
			return -1;
		}
		pBuffer += nwrite;
		size    -= nwrite;
	}
	return 0;
}
int SocketSendString(int sfd, char *text) {
	return SocketSend(sfd, text, strlen(text));
}
int SocketRecv(int sfd, void *pBuffer, int size) { //Returns number of characters read if successful or -1 if not
	if (sfd == -1)
	{
		Log('e', "SocketRead socket not connected");
		return -1;
	}
    int nread = read(sfd, pBuffer, size);
	if (nread == -1)
	{
		LogErrno("SocketRead-read");
		return -1;
	}
	return nread;
}

int SocketClose(int sfd) {

	if (shutdown(sfd, SHUT_RDWR))
	{
		LogErrno("SocketClose-shutdown");
		return -1;
	}
	if (close(sfd))
	{
		LogErrno("SocketClose-close");
		return -1;
	}
	return 0;
}

int SocketBindUnconnected(unsigned short port, struct SocketUnconnectedStructure *pUnconnected) {
	if (bindProtocol(port, SOCK_DGRAM, &pUnconnected->Socket)) return -1;
	return 0;
}
int SocketRecvUnconnected(struct SocketUnconnectedStructure *pUnconnected, void *pBuffer, int size) {
	pUnconnected->DestAddressSize = sizeof(struct sockaddr_storage);
    int nread = recvfrom(pUnconnected->Socket, pBuffer, size, 0, &(pUnconnected->DestAddress), &(pUnconnected->DestAddressSize));
	if (nread == -1)
	{
		LogErrno("SocketRecvFrom-recvfrom");
		return -1;
	}
	return nread;
}
int SocketSendUnconnected(struct SocketUnconnectedStructure *pUnconnected, void *pBuffer, int size) {
	int nwrite = sendto(pUnconnected->Socket, pBuffer, size, 0, &(pUnconnected->DestAddress), pUnconnected->DestAddressSize);
	if (nwrite <= 0)
	{
		LogErrno("SocketSendTo-sendto");
		return -1;
	}
	return 0;
}
int SocketDestUnconnected(struct SocketUnconnectedStructure *pUnconnected, char *name, char length) {
	int r = getnameinfo(&pUnconnected->DestAddress, pUnconnected->DestAddressSize, name, length, NULL, 0, 0);
	if (r)
	{
		Log('e', "ERROR getnameinfo - %i - %s", r, gai_strerror(r));
		return -1;
	}
	return 0;
}
int SocketCloseUnconnected(struct SocketUnconnectedStructure unconnected) {
	SocketClose(unconnected.Socket);
	return 0;
}

//Utilities
char *SocketGetLocalDomainName(void) {
	if (res_init())
	{
		Log('e', "ERROR - res_init");
		return NULL;
	}
	return _res.defdname;
}
