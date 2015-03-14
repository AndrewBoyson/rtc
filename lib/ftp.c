#include <stdio.h>   //fopen printf
#include <errno.h>   //errno
#include <string.h>  //strerror
#include <stdlib.h>  //free strdupa
#include <pthread.h> //rwlock
#include <stdint.h>  //uint64_t

#include <sys/socket.h> //connect read write
#include <unistd.h>     //close
#include <netdb.h>      //addrinfo
#include <arpa/inet.h>  //inet_ntop
#include <stdarg.h>     //
#include <sys/stat.h>   //File date

#include "log.h"
#include "socket.h"
#include "ftp.h"
#include "file.h"

#define SERVICE_PORT           21
#define FILEBUF_SIZE         1000

static   char databuf[FILEBUF_SIZE];
static   void makePasvAddress(char ** ip, unsigned short * port) {
//227 Entering Passive Mode (128,138,140,44,161,238)
//                              1   2   3  4   5
//strtol reads up to first non valid character
	int commaCount = 0;
	for (char *p = databuf; *p != 0; p++)
	{
		if (*p == '(') *ip = ++p; //Record first character after the '(' as the start of the ip address

		if (*p == ',')
		{
			switch (++commaCount)
			{
				case 1:
				case 2:
				case 3: 
					*p = '.'; //Change commas to dots for the ip address
					break;
				case 4:
					*p =  0 ; //Terminate ip address
					*port = strtol(++p, NULL, 10) << 8; //Load the high byte of the port
					break;
				case 5:
					*port += strtol(++p, NULL, 10); //Load the low byte of the port
					return;
			}
		}
	}
}
static time_t parseMDTM(void) {
	//213 20120807154341
	//012345678901234567
	//0         1
	char start[5];
	struct tm t;
	if (sscanf(databuf,
		"%4c%4d%2d%2d%2d%2d%2d",
		&start[0],
		&t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) == EOF)
	{
		LogErrno("Error sscanf");
		return 0;
	} 
	t.tm_mon  -= 1;    // month is 0 to 11 while day is 1 to 31!!!
	t.tm_year -= 1900; // struct tm year is from 1900
	return mktime(&t);
}
static    int readFtpControl(int sck, char expected) { //returns 0 on success or -1 if not
    while (1)
	{
		int bytes = SocketRecv(sck, databuf, FILEBUF_SIZE);
		if (bytes <= 0) return -1;

        databuf[bytes] = 0; //Set end of string
		int start = 0;
		for (int i = 0; i < bytes; i++)
		{
			if (databuf[i] == '\r') databuf[i] = 0;
			if (databuf[i] == '\n')
			{
				Log('i', "<-- %s", &databuf[start]);
				if (i == bytes - 1) break; //if this is the end then exit now
				start = i + 1;
			}
		}
		if (databuf[start] != expected)
		{
			Log('e', "Wrong response");
			return -1;
		}
		if (databuf[start + 3] == ' ') break; //Finish if have space after 200, otherwise keep reading
    }
	return 0;
}
static    int sendFtpControl(int sck, char *format, ...) {
	//Get the stuff to print and put it into buffer
	char text[256];
	va_list args;
	va_start (args, format);
	vsprintf (text, format, args);
	va_end (args);

	if (SocketSendString(sck, text  )) return -1;
	if (SocketSendString(sck, "\r\n")) return -1;
	Log('i', "--> %s", text);
	return 0;
}
static    int xchgFtpControl(int sck, char expected, char *format, ...) {
	char text[256];
	va_list args;
	va_start (args, format);
	vsprintf (text, format, args);
	va_end (args);
	
	if (sendFtpControl(sck, text)) return -1;
	if (readFtpControl(sck, expected)) return -1;
	return 0;
}
static    int readToFile    (         int sckData,               char *localfilename) {
	FILE *outfile = fopen(localfilename, "w");
	if (!outfile)
	{
		LogErrno("fetchData - fopen");
		return -1;
	}
	int bytesread = 0;
	while (1)
	{
		int bytes = read(sckData, databuf, FILEBUF_SIZE);
		if (bytes == 0) break;
		bytesread += bytes;
		write(fileno(outfile), databuf, bytes);
	}
	fclose(outfile);
	Log('i', "<++ %d bytes read from data port into file %s", bytesread, localfilename);
	return 0;
}

int    FtpLogin      (char *host, int *pSck) {
	if (SocketConnectTcp(host, "21", pSck            )) return FTP_NOT_CONNECTED;
	if (readFtpControl  (*pSck, '2'                  )) return FTP_CONNECTED;
	if (xchgFtpControl  (*pSck, '3', "USER anonymous")) return FTP_CONNECTED;
	if (xchgFtpControl  (*pSck, '2', "PASS guest"    )) return FTP_CONNECTED;
	return FTP_LOGGED_IN;
}
void   FtpLogout     (int sck, int state) {
	if (state >= FTP_LOGGED_IN) xchgFtpControl(sck, '2', "QUIT");
	if (state >= FTP_CONNECTED) SocketClose   (sck);
}
int    FtpCwd        (int sck, char *path) {
	if (xchgFtpControl(sck, '2', "CWD /%s", path )) return -1;
	return 0;
}
time_t FtpGetFileTime(int sck, char *file) {
	if (xchgFtpControl(sck, '2', "MDTM %s", file     )) return -1;
	return parseMDTM();
}
int    FtpDownload   (int sck, char *file, char *localfilename) {
	if (xchgFtpControl(sck, '2', "TYPE A"       )) return -1;
	if (xchgFtpControl(sck, '2', "PASV"         )) return -1;
	
	unsigned short port;
	char * ip;
	makePasvAddress(&ip, &port);
	Log('i', "--- Connecting to data port IP %s:%d", ip, port);

	int sckData;
	if (SocketConnectPasv(ip, port, &sckData    )) return -1;
	
	if (xchgFtpControl(sck, '1', "RETR %s", file)) { SocketClose(sckData); return -1; }
	if (readToFile(sckData, localfilename))        { SocketClose(sckData); return -1; }
	if (readFtpControl(sck, '2'                 )) return -1;
	if (SocketClose   (sckData                  )) return -1;


	return 0;
}
