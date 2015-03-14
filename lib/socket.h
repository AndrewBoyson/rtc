#include <sys/socket.h>
#include <stdint.h> 


extern int SocketConnectUdp (char *server, char *port, int *sfd);
extern int SocketConnectTcp (char *server, char *port, int *sfd);
extern int SocketConnectPasv(char *ip, unsigned short port, int *sfd);

extern int SocketIp (int sfd, uint32_t *pIp);
extern int SocketIpA(int sfd, char *ip, int len);

extern int SocketBindUdp    (unsigned short port, int *sfd);
extern int SocketBindTcp    (unsigned short port, int *sfd);
extern int SocketAccept     (int listensfd, int *newsfd);

extern int SocketSendString (int sfd, char *text);
extern int SocketSend       (int sfd, void *buffer, int size);
extern int SocketRecv       (int sfd, void *buffer, int size);

extern int SocketClose      (int sfd);

struct SocketUnconnectedStructure{
	int             Socket;
	struct sockaddr DestAddress;
	socklen_t       DestAddressSize;
};
extern int SocketBindUnconnected(unsigned short port, struct SocketUnconnectedStructure *pUnconnected);
extern int SocketRecvUnconnected(struct SocketUnconnectedStructure *pUnconnected, void *pBuffer, int size);
extern int SocketSendUnconnected(struct SocketUnconnectedStructure *pUnconnected, void *pBuffer, int size);
extern int SocketDestUnconnected(struct SocketUnconnectedStructure *pUnconnected, char *name, char length);
extern int SocketCloseUnconnected(struct SocketUnconnectedStructure unconnected);

extern char *SocketGetLocalDomainName(void);