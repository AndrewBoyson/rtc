
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 

#include "http.h"
#include "post.h"
#include "html.h"
#include "../lib/log.h"

#define BUF_SIZE 250000
#define HEADER_NAME_SIZE 20
#define HEADER_VALUE_SIZE 10
#define REQUEST_SIZE 10
#define RESOURCE_SIZE 100

static char buffer[BUF_SIZE];        //All pointers end up at the next available position, ie one past the end of the data
static char *pb = &buffer[0];        //Buffer begin
static char *p;                      //Current position
static char *pn;                     //Position of end of characters added from socket. Use addBuffer before reading the next character in the buffer.
static char *pe = &buffer[BUF_SIZE]; //Buffer end - don't actually write to this position: it's just for comparison and arithmetic

static int contentLength = 0;

int  HttpErrorType;                //Only 0 (no error), 400 Bad Request, 404 Not Found, 500 Internal Server Error, or 501 Not Implemented are understood
char HttpErrorMsg[ERROR_MSG_SIZE]; //This will be sent as the body of a warning message to supplement the above

char HttpSheet[20] = "Clock";

//Buffer
char *HttpGetBufferStart() {
	return pb;
}
static int addDataToBuffer() {
	int m = HttpGetDataCallBack(pn, pe - 1 - pn);
	if (m < 0) return -1;
	if (m == 0)
	{
		Log('i', "Client closed socket");
		return -1;
	}
	pn += m;
	if (pn >= pe)
	{
		Log('w', "Buffer overrun");
		return -1;
	}
	return 0;
}

//Read the request and work out what to do
static int handleRequestHeader    (char *name,    char *value)    {
	if (strcmp(name, "Content-Length") == 0)
	{
		sscanf(value, "%d", &contentLength);
	}
	return 0;
}
static int handleRequestTopLine   (char *request, char *resource) {

	char isRequest  = 1;
	char isResource = 0;
	int i = 0;

	while (1)
	{
		if (*p == '\n') { p++; break   ; } //Leave the pointer at the start of the next line
		if (*p == '\r') { p++; continue; } //Ignore carriage returns
		if (*p == ' ')
		{
			if (isResource) //Do it in reverse order or the isRequest will ripple through
			{
				isRequest   = 0;
				isResource  = 0;
				resource[i] = 0; //terminate string
			}
			if (isRequest)
			{
				isRequest   = 0;
				isResource  = 1;
				request[i]  = 0; //terminate string
			}
			i = 0;
		}
		else
		{
			if (isRequest  && i <  REQUEST_SIZE - 1)  request[i] = *p;
			if (isResource && i < RESOURCE_SIZE - 1) resource[i] = *p;
			i++;
		}
		p++;
	}
	return 0;
}
static int handleRequestHeaderLine(void)                          {
	char name[HEADER_NAME_SIZE];
	int n = 0;
	char value[HEADER_VALUE_SIZE];
	int v = 0;
	char isName = 1;
	while (1)
	{
		if (*p == '\r') { p++; continue; } //Leave the pointer at the start of the next line
		if (*p == '\n') { p++; break   ; } //Ignore carriage returns
		if (*p == ':')
		{
			isName = 0;
			if (*(p+1) == ' ') p++; //Remove any space
		}
		else
		{
			if (isName)
			{
				if (n < HEADER_NAME_SIZE - 1) name [n++] = *p;
			}
			else
			{
				if (v < HEADER_VALUE_SIZE - 1) value[v++] = *p;
			}
		}
		p++;
	}
	name [n] = 0;
	value[v] = 0;

	if (handleRequestHeader(name, value)) return -1;
	return n + v;
}
static int handleRequest          (char *request, char *resource) {

	//Add the first batch of data
	p  = pb; //Set working pointer to start
	pn = pb; //Set end pointer to start
	if (addDataToBuffer()) return -1;

	//Handle the headers
	contentLength = 0;
	if (handleRequestTopLine(request, resource)) return -1;
	while(1)
	{
		int lineLength = handleRequestHeaderLine();
		if (lineLength  < 0) return -1;
		if (lineLength == 0) break; //Reached the end of the request's header lines
	}
		
	//Read content from the socket and advance pn until reached content length pl
	char *pl = p + contentLength;
	while (pn < pl)	if (addDataToBuffer()) return -1;

	return 0;
}

//Read request, parse it, handle it, send response
static int handleError(void) {
	if (!HttpErrorType) return -1;
	else                return HtmlSendError(pb, BUF_SIZE);
}
int HttpHandleTransaction(void) {

	char  request[ REQUEST_SIZE]; // POST, GET, HEAD
	char resource[RESOURCE_SIZE]; // '/' or '/favicon.ico' etc
	
	HttpErrorType = 0;

	//Handle the request
	if (handleRequest(request, resource)) return handleError();
	
	//Handle the request top line
	char isGet  = strncmp(request, "GET" , 3) == 0;
	char isHead = strncmp(request, "HEAD", 4) == 0;
	char isPost = strncmp(request, "POST", 4) == 0;
	
	//Check request type is supported
	if (!isGet && !isHead && !isPost)
	{
		HttpErrorType = 501; // Not Implemented
		sprintf(HttpErrorMsg, "We only support GET, HEAD and POST; you sent %s", request);
		return handleError();
	}
	
	//Handle posted content
	if (isPost)
	{
		int postedLength = Post(p, contentLength);
		if (postedLength < 0) return handleError();
		
		//Check that the amount read corresponds with the buffer
		if (postedLength != contentLength)
		{
			HttpErrorType = 500; //Internal Server Error
			sprintf(HttpErrorMsg, "POST: %d bytes unused from the buffer", contentLength - postedLength);
			return -1;
		}
	}
	
	//Send the response
	int htmlLength = HtmlSend(pb, BUF_SIZE, resource, isHead);
	if (htmlLength < 0) return handleError();
	
	return htmlLength;
}
