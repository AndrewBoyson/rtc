#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h> //ntohl

#include "../global.h"
#include "../lib/log.h"
#include "../lib/socket.h"
#include "../lib/datetime.h"
#include "../lib/rtc.h"
#include "../lib/cfg.h"
#include "../utc/utc.h"

//General
#define PORT 123
const uint16_t NtpEpoch       = 1900;
const uint64_t NtpTicksPerSec = SIZE_32_BITS;

struct Packet {
/*
	LI:  00 no warning; 01 last minute has 61 seconds; 10 last minute has 59 seconds; 11 alarm condition (clock not synchronized)
	VN:   3 = 011
	Mode: 3 = 011 for client request; 4 = 100 for server reply
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  |0 0 0 1|1 0 1 1|
	
	Mode: If the Mode field of the request is 3 (client), the reply is set to 4 (server).
          If this field is set to 1 (symmetric active), the reply is   set to 2 (symmetric passive).
		  This allows clients configured in either client (NTP mode 3) or symmetric active (NTP mode 1) to
		  interoperate successfully, even if configured in possibly suboptimal ways.
		  
	Poll: signed integer indicating the	minimum interval between transmitted messages, in seconds as a power of two.
	For instance, a value of six indicates a minimum interval of 64 seconds.
	
	Precision: signed integer indicating the precision of the various clocks, in seconds to the nearest power of two.
	The value must be rounded to the next larger power of two;
	for instance, a 50-Hz (20 ms) or 60-Hz (16.67 ms) power-frequency clock would be assigned the value -5 (31.25 ms),
	while a 1000-Hz (1 ms) crystal-controlled clock would be assigned the value -9 (1.95 ms).
	
	Root Delay (rootdelay): Total round-trip delay to the reference clock, in NTP short format (16bitseconds; 16bit fraction).

	Root Dispersion (rootdisp): Total dispersion to the reference clock, in NTP short format (16bitseconds; 16bit fraction)..
*/
	union
	{
		uint32_t FirstLine;
		struct
		{
			unsigned Mode : 3;
			unsigned VN   : 3;
			unsigned LI   : 2;
			uint8_t  Stratum; 
			 int8_t  Poll;
			 int8_t  Precision;
		};
	};
	uint32_t RootDelay;
	uint32_t Dispersion;
	uint32_t RefIdentifier;
	
	uint64_t RefTimeStamp;
	uint64_t OriTimeStamp;
	uint64_t RecTimeStamp;
	uint64_t TraTimeStamp;
};
static uint64_t ntohll(uint64_t n) {
	int testInt = 0x0001; //Big end contains 0x00; little end contains 0x01
	int *pTestInt = &testInt;
	char *pTestByte = (char*)pTestInt;
	char testByte = *pTestByte; //fetch the first byte
	if (testByte == 0x00) return n; //If the first byte is the big end then host and network have same endianess
	
	union ull
	{
		uint64_t Whole;
		char Bytes[8];
	};
	union ull h;
	h.Whole = n;
	
	char t;
	t = h.Bytes[7]; h.Bytes[7] = h.Bytes[0]; h.Bytes[0] = t;
	t = h.Bytes[6]; h.Bytes[6] = h.Bytes[1]; h.Bytes[1] = t;
	t = h.Bytes[5]; h.Bytes[5] = h.Bytes[2]; h.Bytes[2] = t;
	t = h.Bytes[4]; h.Bytes[4] = h.Bytes[3]; h.Bytes[3] = t;
	
	return h.Whole;
}

//Server stuff
static struct SocketUnconnectedStructure unconnected;
char     NtpLeapIndicator = 0; //Initialise to 'No warning'.       Set up in GetTime from the server LI
char     NtpRefStratum    = 0; //Initialise to 'Kiss of Death'.    Set up in GetTime from the server stratum
uint32_t NtpRefIdentifier = 0; //Initialise to 'Unknown'.          Set up in GetTime from the server IP4. It is in network byte order already.

int NtpBind(void) {
	if (SocketBindUnconnected(PORT, &unconnected)) return -1;
	return 0;
}
static int recordClient(char rtcIsSet, int64_t clientTicks, int64_t rtcTicks) {

	//Get the filename and check if wanted
	char *filename = NULL;
	CfgLock();
		if (CfgNtpClientLogFileName != NULL) filename = strdupa(CfgNtpClientLogFileName);
	CfgUnlock();
	if (filename == NULL) return 0;
	if (filename[0] == 0) return 0;

	//Get the time
	char time[30];
	if (DateTimeNowS(30, time)) return -1;

	//Get the client
	char dest[64];
	if (SocketDestUnconnected(&unconnected, dest, 64)) return -1;
		
	//Get the host name part of the client - ie the bit before the '.'
	char *localdomain = SocketGetLocalDomainName();
	int i = strlen(dest) - 1;                                          //Position i at the last character of the destination
	while (dest[i] != '.' && i >= 0) i--;                              //Position i at the last '.' in destination or at -1 if no '.' found
	if (i >= 0 && strcmp(dest + i + 1, localdomain) == 0) dest[i] = 0; //If found then remove the local domain part by replacing the last '.' with a 0.
	
	//Open the log
	FILE *fp = fopen(filename, "a");
	if (fp == NULL)
	{
		LogErrno("ERROR opening NTP client log file: ");
		return -1;
	}
	
	//Append the date and client
	if (fprintf(fp, "%s\t%s\t", time, dest) < 0) { LogErrno("Error appending to NTP client log file"); fclose(fp); return -1; }

	//Append the offset
	if (rtcIsSet)
	{
		if (clientTicks)
		{
			//Get the offset in ms
			 int64_t clientMinusRtc = clientTicks - rtcTicks;
			 int64_t clientMinusRtcMs;
			if (DateTimeScaleTicksToTicks(SIZE_32_BITS, clientMinusRtc, 1000,  &clientMinusRtcMs)) return -1;
		
			//Record the client offset
			if (fprintf(fp, "%+lld", clientMinusRtcMs) < 0) { LogErrno("Error appending to NTP client log file"); fclose(fp); return -1; }
		}
		else
		{
			if (fprintf(fp, "---") < 0) { LogErrno("Error appending to NTP client log file"); fclose(fp); return -1; }
		}
	}
	else
	{
		if (fprintf(fp, "RTC not set") < 0) { LogErrno("Error appending to NTP client log file"); fclose(fp); return -1; }
	}
	
	//Append a LF
	if (fprintf(fp, "\n") < 0) { LogErrno("Error appending to NTP client log file"); fclose(fp); return -1; }

	//Close the file
	if (fclose(fp))
	{
		fclose(fp);
		LogErrno("Error closing NTP client log file: ");
		return -1;
	}

	return 0;
}
int NtpSendTime(void) {

	struct Packet packet;
	char isLeap;

	//Wait for request
	int nRead = SocketRecvUnconnected(&unconnected, &packet, sizeof(packet));
	if (nRead < sizeof(packet))
	{
		Log('e', "Invalid packet length of %d bytes", nRead);
		return -1;
	}
	
	char version = packet.VN;
	char mode    = packet.Mode;
	int8_t poll  = packet.Poll;
		
	//Check the request is reasonable
	if (mode   != 1 && mode != 3) { Log('w', "Received mode '%d' but can only accept 3 (client) or 1 (symmetric active)", mode); return -1; }
	
	//Set up the reply
	char rtcIsSet = RtcIsSet();
	packet.LI   = rtcIsSet ? NtpLeapIndicator : 3;          //Leap indicator: pass on reference indicator if it is synchronised or an alarm if not
	packet.VN   = version;                                  //Send back the same version as the request
	packet.Mode =  mode == 3 ? 4 : 2;                       //Client --> Server; symmetric active --> symmetric passive mode
	
	packet.Stratum       = NtpRefStratum + 1;               //Our stratum is the reference stratum plus one
	packet.Poll          = poll;                            //Send back the request poll
	packet.Precision     = -15;                             //32Khz is 15 bit
	packet.RootDelay     = ntohl(20 * SIZE_16_BITS / 1000); //Delay from reference clock 20ms
	packet.Dispersion    = ntohl(10 * SIZE_16_BITS / 1000); //Dispersion around ref clock 10ms
	packet.RefIdentifier = NtpRefIdentifier;                //Upsteam server IP. Note that this is already in network byte order
	
	//Set up time stamp of last sample from the upstream clock
	uint64_t lastSampleRtc;
	if (RtcGetSampleThisAbsNs(&lastSampleRtc)) return -1;
	uint64_t lastSampleTai;
	if (RtcToTicks(lastSampleRtc, UtcEpoch, UtcTicksPerSec, &lastSampleTai)) return -1;
	uint64_t lastSampleUtc;
	if (TaiToUtc(lastSampleTai, &lastSampleUtc, &isLeap)) return -1;
	uint64_t lastSampleNtp = UtcToNtp(lastSampleUtc);
	
	packet.RefTimeStamp  = ntohll(lastSampleNtp); //Timestamp of last sample
	
	//Set up times
	packet.OriTimeStamp = packet.TraTimeStamp;
	uint64_t clientNtp = ntohll(packet.OriTimeStamp);
	uint64_t rtcNtp = 0;
	if (rtcIsSet)
	{
		uint64_t tai;
		if (RtcGetNowTicks(UtcEpoch, UtcTicksPerSec, &tai)) return -1;
		uint64_t utc;
		if (TaiToUtc(tai, &utc, &isLeap)) return -1;
		rtcNtp = UtcToNtp(utc);
	}
	packet.RecTimeStamp = ntohll(rtcNtp);
	packet.TraTimeStamp = ntohll(rtcNtp);
	
	//Send
	if (SocketSendUnconnected(&unconnected, &packet, sizeof(packet))) return -1;
	
	//Record the client
	if (recordClient(rtcIsSet, clientNtp, rtcNtp)) return -1;
	
	return 0;
}
int NtpUnbind(void) {
	return SocketCloseUnconnected(unconnected);
}


//Client stuff
static int  sfd;

int NtpConnect(char *server_name) {
	Log('i', "Connecting to %s", server_name);
	if (SocketConnectUdp(server_name, "123", &sfd)) return -1;
	return 0;
}
int NtpClose(void) {
	Log('i', "Closing NTP socket");
	if (SocketClose(sfd)) return -1;
	return 0;
}
int NtpGetTime(int64_t *pRtcMinusNtpTai, int64_t *pTravelTimeTai) {
	struct Packet packet;
	memset(&packet, 0, sizeof(packet));
	packet.LI   = 0;
	packet.VN   = 1;
	packet.Mode = 3; //Client
		
	//Get the time
	uint64_t tai24_1; //We retain this value for use later on - it prevents the era being stripped from the RTC
	if (RtcGetNowTicks(UtcEpoch, UtcTicksPerSec, &tai24_1)) return -1;
	uint64_t utc24_1;
	char isLeap;
	if (TaiToUtc(tai24_1, &utc24_1, &isLeap)) return -1;
	uint64_t ntp32_1 = UtcToNtp(utc24_1);  //This step loses the era into ntp
    packet.TraTimeStamp = ntohll(ntp32_1); //The server puts this into the OriTimeStamp but we don't use it
	
	//Send datagram
	int resultSend = SocketSend(sfd, &packet, sizeof(packet));
	if (resultSend < 0) return -1;
	
	//Receive reply
    int resultRead = SocketRecv(sfd, &packet, sizeof(packet));
	if (resultRead < 0) return -1;
	
	//Handle the reply	
	char leap    = packet.LI;
	char version = packet.VN;
	char mode    = packet.Mode;
	char stratum = packet.Stratum;
	if (leap    == 3) { Log('w', "Remote clock has a fault");                     return -1; }
	if (version  < 1) { Log('w', "Version is %d", version);                       return -1; }
	if (mode    != 4) { Log('w', "Mode is %d", mode);                             return -1; }
	if (stratum == 0) { Log('w', "Received Kiss of Death packet (stratum is 0)"); return -1; }
	
/*
	See http://www.eecis.udel.edu/~mills/time.html for timestamp calculations
        Ori
	----t1---------t4---- RTC
	      \       /
	-------t2---t3------- NTP
	       Rec  Tra
	offset (RTC - NTP) = (t1 + t4)/2 - (t2 + t3)/2 ==> [(t1 - t2) + (t4 - t3)] / 2
	delay              = (t4 - t1)   - (t3 - t2)
*/
	uint64_t ntp32_2 = ntohll(packet.RecTimeStamp);
	uint64_t ntp32_3 = ntohll(packet.TraTimeStamp);
	
	//Convert ntp times to 24 bit utc with era
	uint64_t utc24_2 = NtpToUtc(ntp32_2);
	uint64_t utc24_3 = NtpToUtc(ntp32_3);
	
	//Convert utc times to tai in case they straddle a leap second (as does happen when the RTC is not set at start up) If any is a leap second then returns failure.
	uint64_t tai24_2;
	uint64_t tai24_3;
	if (UtcToTai(utc24_2, -1, &tai24_2)) return -1;
	if (UtcToTai(utc24_3, -1, &tai24_3)) return -1;
	
	//Get the rtc
	uint64_t tai24_4;
	if (RtcGetNowTicks(UtcEpoch, UtcTicksPerSec, &tai24_4)) return -1;
	
	uint64_t utc24_4;
	if (TaiToUtc(tai24_4, &utc24_4, &isLeap)) return -1;
	if (isLeap)
	{
		Log('e', "Ambiguous second");
		return -1;
	}
	
	/*
	Log('d', "utc1 %15llu", utc24_1 >> 24);
	Log('d', "utc2 %15llu", utc24_2 >> 24);
	Log('d', "utc3 %15llu", utc24_3 >> 24);
	Log('d', "utc4 %15llu", utc24_4 >> 24);

	Log('d', "tai1 %15llu", tai24_1 >> 24);
	Log('d', "tai2 %15llu", tai24_2 >> 24);
	Log('d', "tai3 %15llu", tai24_3 >> 24);
	Log('d', "tai4 %15llu", tai24_4 >> 24);
	*/
	
	//Calculate the differences in 24bit form
	int64_t t1_minus_t2 = tai24_1 - tai24_2;
	int64_t t4_minus_t3 = tai24_4 - tai24_3;
	int64_t t4_minus_t1 = tai24_4 - tai24_1;
	int64_t t3_minus_t2 = tai24_3 - tai24_2;

	*pRtcMinusNtpTai = (t1_minus_t2 + t4_minus_t3) >> 1; //RTC - NTP as 24 bit
	*pTravelTimeTai  =  t4_minus_t1 - t3_minus_t2;       //Travel time as 24 bit
	
	//Save any reference information
	NtpLeapIndicator = leap;
	NtpRefStratum = stratum;
	if (SocketIp(sfd, &NtpRefIdentifier)) return -1;
	
	char ip[20];
	if (SocketIpA(sfd, ip, 20)) return -1;
	
	//Log('d', "LI %u, VN %u, Mode %u, Stratum %u, IP4 %s, IP4 %x, TimeStamp %llx", leap, version, mode, stratum, ip, NtpRefIdentifier, NtpRefTimeStamp);
	
	return 0;
}

