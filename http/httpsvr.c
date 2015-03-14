#include <pthread.h> //thread stuff
#include <semaphore.h>

#include "http.h"

#include "../lib/thread.h"
#include "../lib/socket.h"
#include "../lib/log.h"

#define PORT 80

static struct Thread  t;
static int    newsockfd;

//This is a back link to allow Http to read from the socket
int getData(char *buffer, int length) {
	return SocketRecv(newsockfd, buffer, length);
}

//Worker part
static void *worker(void *arg) {
	//Initialise thread part
	if (ThreadWorkerInit(&t)) return NULL;

	//Initialise local part
	int sockfd;
	if (SocketBindTcp(PORT, &sockfd)) return NULL;
	while(1)
	{
		//Wait for a request and acquire new socket
		if (SocketAccept(sockfd, &newsockfd)) continue;
		
		//Read and handle the request
		int length = HttpHandleTransaction();
		if (length < 0)
		{
			SocketClose(newsockfd);
			continue;
		}
		
		//Send the response
		if (SocketSend(newsockfd, HttpGetBufferStart(), length))
		{
			SocketClose(newsockfd);
			continue;
		}
		
		//Close the new socket
		if (SocketClose(newsockfd)) continue;
	}
    SocketClose(sockfd);
    return 0; 
}
void HttpSvrKill(void) {
	ThreadCancel(&t);
}
void HttpSvrInit(char *name, int priority) {
	HttpGetDataCallBack = getData;
	t.Name = name;
	t.Worker = worker;
	t.NormalPriority = priority;
	ThreadStart(&t);
}
void HttpSvrJoin(void) {
	ThreadJoin(&t);
}
