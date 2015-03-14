#include <stdint.h>              //unit_8
#include <string.h>              //strlen
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>           //ioctl
#include <linux/types.h>
#include <linux/spi/spidev.h>    //struct spi_ioc_transfer
#include <pthread.h>
#include <time.h>                //nanosleep

#include "log.h"

/*Speeds
	Frequencies are 32MHz; 16MHz; 8MHz; 4MHz; 2MHz; 1MHz
	
	To process non last nibble
	Assume PIC needs 100 instructions to process a next nibble at 4MIPS or 250ns per instruction
	It needs 25uS to do this. There are 8 bits so each bit has about 3uS giving a frequency of 330kHz
	It was found that the practical limit on frequency was the interface itself which started to fail about 16MHz.
	The time taken by the Pi to process each nibble was enough for the PIC to store or retrieve one nibble.
	
	To process the last nibble
	PIC has to do extra work to fetch (ID_LAST_NO_DATA || ID_LAST_EXPECT_DATA) or save the data (DATA_LAST).
	An initial assumption that PIC needs 1000 instructions to cover this extra work would take an extra 250uS.
	Tests show that it actually needed about 500uS (2000 PIC instructions) for most requests and 2ms (8000 PIC istructions)
	to set the initial time.
	
	So time to retrieve an 8 byte message is about 500 * 2 + 9 * 25 = 1225uS = 1ms
*/

#define SPEED_HZ 5000000 //This will give 4MHz
#define INTER_NIBBLE_DELAY_US 200 //Time to allow PIC to store or read the nibble. Processing time is a parameter in SpiSend and SpiRead
#define DEVICE "/dev/spidev0.0"  //Device name

#define PROTOCOL  0
#define ID_ONLY   1
#define ID_START  2
#define ID_NEXT   3
#define ID_LAST   4
#define DATA_NEXT 5
#define DATA_LAST 6

#define CONT 0
#define ACKN 1
#define NACK 2

const char * types[] = {"PROTOCOL",
						"ID_ONLY",
						"ID_START",
						"ID_NEXT",
						"ID_LAST",
						"DATA_NEXT",
						"DATA_LAST" };
const char *   ids[] = {"CONT", "ACKN", "NACK"};

static void     idAsString(unsigned char id, char * buffer) {
	if (id >= CONT && id <= NACK)
		sprintf(buffer,"%s",  ids[id]);
	else
		sprintf(buffer, "Unknown id %X", id);
}
static void   typeAsString(unsigned char type, char * buffer) {
	if (type >= PROTOCOL && type <= DATA_LAST)
		sprintf(buffer, "%s",  types[type]);
	else
		sprintf(buffer, "Unknown type %X", type);
}
static void nibbleAsString(unsigned char type, unsigned char id, char * buffer) {
	if (type == PROTOCOL)
		idAsString(id, buffer);
	else
		typeAsString(type, buffer);
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int   lock(void) {
	int e = pthread_mutex_lock(&mutex);
	if (e)
	{
		Log('e', "ERROR - pthread_mutex_lock returned &d", e);
		return -1;
	}
	return 0;
}
static int unlock(void) {
	int e = pthread_mutex_unlock(&mutex);
	if (e)
	{
		Log('e', "ERROR - pthread_mutex_unlock returned &d", e);
		return -1;
	}
	return 0;
}

static int fd;
static unsigned char request;
static unsigned char response;
static unsigned char nibble;

static char makeParityOdd(char x) {
	char paritycount = 0;
	if (x & 0x01) paritycount++;
	if (x & 0x02) paritycount++;
	if (x & 0x04) paritycount++;
	if (x & 0x08) paritycount++;
	if (x & 0x10) paritycount++;
	if (x & 0x20) paritycount++;
	if (x & 0x40) paritycount++;
	char isOdd = paritycount & 0x01;
	if (!isOdd) x |= 0x80;
	return x;
}
static char   isParityOdd(char x) {
	char paritycount = 0;
	if (x & 0x01) paritycount++;
	if (x & 0x02) paritycount++;
	if (x & 0x04) paritycount++;
	if (x & 0x08) paritycount++;
	if (x & 0x10) paritycount++;
	if (x & 0x20) paritycount++;
	if (x & 0x40) paritycount++;
	if (x & 0x80) paritycount++;
	char isOdd = paritycount & 0x01;
	return isOdd;
}
static int transferNibble(char ignoreRead, int delay_us) {
	
	//SPI transfer structure
	struct spi_ioc_transfer tr;
	uint8_t x;                            //The address of x is given as the transmit and receive buffer in tr.
	tr.tx_buf        = (unsigned long)&x; //Holds pointer to userspace buffer with transmit data
	tr.rx_buf        = (unsigned long)&x; //Holds pointer to userspace buffer for receive data
	tr.len           =                 1; //Length of tx and rx buffers, in bytes.
	tr.speed_hz      =                 0; //Temporary override of the device's bitrate.
	tr.bits_per_word =                 0; //Temporary override of the device's wordsize.
	tr.delay_usecs   =          delay_us; //How long to delay after the last bit transfer before optionally deselecting the device before the next transfer.
	tr.cs_change     =                 1; //True to deselect device before starting the next transfer.
	
	//Send the nibble
	x = nibble & 0x0F;
	x |= request << 4;
	x = makeParityOdd(x); //Make the parity odd to detect FF or 00
	
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr); //Transfer x
	if (ret < 1)
	{
		LogErrno("ERROR transferring nibble - ioctl");
		return -1;
	}
	
	 //Treat the read nibble if required
	if (!ignoreRead)
	{
		if (!isParityOdd(x))
		{
			Log('e', "ERROR transferring nibble - parity");
			return -1;
		}
		nibble  = x & 0x0F;
		response = (x >> 4) & 0x07;
	}
	
	return 0;
}

static int sendDataNibble(char isHighNibble, unsigned char *p, char isLast, int delay_us) {

	nibble = isHighNibble ? *p >> 4 : *p & 0x0F;
	request = isLast ? DATA_LAST : DATA_NEXT;

	if (transferNibble(0, delay_us)) return -1;
	
	if (response == PROTOCOL && nibble == CONT) return 0;
	
	//Had an enexpected response - bomb out
	char sNibble[20];
	nibbleAsString(response, nibble, sNibble);
	Log('e', "ERROR sending data - expected CONT but got %s", sNibble);
	return -1;

}
static int readAcknNibble(int delay_us) {
	//Receive the final ACKN
	request = PROTOCOL;
	nibble = CONT;
	if (transferNibble(0, delay_us)) return -1;
	if (response == PROTOCOL && nibble == ACKN) return 0;
	
	//Had an unxepected response - bomb out
	char sNibble[20];
	nibbleAsString(response, nibble, sNibble);
	Log('e', "ERROR sending data - expected final ACKN but got %s", sNibble);
	return -1;
}
static int readDataNibble(char isHighNibble, unsigned char *p, int delay_us) { //return -1 on error, 0 if unfinished and +1 if finished (ACKN) +2 if DATA_LAST

	request = PROTOCOL;
	nibble = CONT;

	if (transferNibble(0, delay_us)) return -1;

	if (response == PROTOCOL && nibble == ACKN) return +1; //No more data - stop fetching
	
	if (response == DATA_NEXT || response == DATA_LAST)
	{
		if (!isHighNibble) *p  = nibble;
		else               *p |= nibble << 4;
		
		if (response == DATA_LAST) return +2; //No more data - stop fetching
		if (response == DATA_NEXT) return  0; //More data
	}
	
	//Had an unxepected response - bomb out
	char sNibble[20];
	nibbleAsString(response, nibble, sNibble);
	Log('e', "ERROR reading data - expected DATA or ACKN but got %s", sNibble);
	return -1;
}

static int sendId  (char id, char expectDataMtoS, int pic_processing_us) {
	//See if have a two nibble or a one nibble id
	if (id & 0xF0)
	{
		//Transfer lower id nibble; throw away the rx nibble as it could be anything
		request = ID_START;
		nibble = id & 0x0F;
		if (transferNibble(1, INTER_NIBBLE_DELAY_US)) return -1;

		//Transfer upper id nibble
		request = ID_LAST;
		nibble = id >> 4;
		if (transferNibble(0, pic_processing_us + INTER_NIBBLE_DELAY_US)) return -1;
		
		//Check the response is CONT
		if (response != PROTOCOL || nibble != CONT) 
		{
			char sNibble[20];
			nibbleAsString(response, nibble, sNibble);
			Log('e', "ERROR sending id - expected CONT but got %s", sNibble);
			return -1;
		}

	}
	else
	{
		//Transfer lower id nibble; throw away the rx nibble as it could be anything
		request = ID_ONLY;
		nibble = id & 0x0F;
		if (transferNibble(1, pic_processing_us + INTER_NIBBLE_DELAY_US)) return -1;
	}
	
	return 0;
}
static int sendData(int size,       int pic_processing_us, void * pVoid) {
	unsigned char * p = pVoid;
	
	//Send the data
	for (int i = 0; i < size; i++, p++)
	{
		//Calculate info for the last high nibble
		char isLast = i >= size - 1;
		int delay_us = INTER_NIBBLE_DELAY_US;
		if (isLast) delay_us += pic_processing_us;
		
		//Send low nibble
		if (sendDataNibble(0, p,      0, INTER_NIBBLE_DELAY_US)) return -1;

		//Send high nibble
		if (sendDataNibble(1, p, isLast,              delay_us)) return -1;
	}
	
	//Receive the final ACKN
	if (readAcknNibble(INTER_NIBBLE_DELAY_US)) return -1;

	return 0;
}
static int readData(int size,                              void * pVoid) {  //Returns -1 on error or the number of bytes received
	unsigned char * p = pVoid;
	int r;
	int i = 0;
	for (i = 0; i < size; i++, p++)
	{
		//Transfer lower data nibble
		r = readDataNibble(0, p, INTER_NIBBLE_DELAY_US);
		if (r < 0)  return -1;
		if (r == 1) break;
		if (r > 1) {i++; break;}
		
		//Transfer upper data nibble
		r = readDataNibble(1, p, INTER_NIBBLE_DELAY_US);
		if (r < 0)  return -1;
		if (r == 1) break;
		if (r > 1) {i++; break;}
	}
	return i;
}

static int sendFixed(char id, int buffersize, int pic_processing_us, void * pVoid) { //send id; send each low and high data nibble; read final ack
	
	if (lock()) return -1;
		
	//Make sure the id MSB is reset when master is output for data
	id &= 0x7F;
	
	//Transfer id
	if (sendId(id, buffersize, pic_processing_us)) { unlock(); return -1; }
	
	//Transfer any data
	if (sendData(buffersize, pic_processing_us, pVoid)) {unlock(); return -1; }
	
	
	unlock();
	return 0;
}
static int readToEnd(char id, int buffersize, int pic_processing_us, void * pVoid) { //Returns length received or -1 on failure
	
	if (lock()) return -1;

	//Make sure the id MSB is set when master is input for data
	id |= 0x80;
	
	//Transfer id
	if (sendId(id, 0, pic_processing_us)) { unlock(); return -1; }
	
	//Transfer any data until have ACKN or DATA_LAST
	int size = readData(buffersize, pVoid);

	//Finish and return size received
	unlock();
	return size;
}
static int readFixed(char id, int buffersize, int pic_processing_us, void * pVoid) { //Returns 0 on success or -1 on failure
	
	//Read array
	int size = readToEnd(id, buffersize, pic_processing_us, pVoid);
	if (size < 0) return -1;

	//If received size is not the same as specified then bomb out
	if (size != buffersize)
	{
		Log('e', "ERROR reading message - expected %d but only received %d bytes", buffersize, size);
		return -1;
	}
	return 0;
}

int SpiSend0(char id,                 int pic_processing_us              ) {
	return sendFixed(id, 0, pic_processing_us, NULL);
}
int SpiSend1(char id,                 int pic_processing_us, void * pVoid) {
	return sendFixed(id, 1, pic_processing_us, pVoid);
}
int SpiRead1(char id,                 int pic_processing_us, void * pVoid) {
	return readFixed(id, 1, pic_processing_us, pVoid);
}
int SpiSend2(char id,                 int pic_processing_us, void * pVoid) {
	return sendFixed(id, 2, pic_processing_us, pVoid);
}
int SpiRead2(char id,                 int pic_processing_us, void * pVoid) {
	return readFixed(id, 2, pic_processing_us, pVoid);
}
int SpiSend4(char id,                 int pic_processing_us, void * pVoid) {
	return sendFixed(id, 4, pic_processing_us, pVoid);
}
int SpiRead4(char id,                 int pic_processing_us, void * pVoid) {
	return readFixed(id, 4, pic_processing_us, pVoid);
}
int SpiSend8(char id,                 int pic_processing_us, void * pVoid) {
	return sendFixed(id, 8, pic_processing_us, pVoid);
}
int SpiRead8(char id,                 int pic_processing_us, void * pVoid) {
	return readFixed(id, 8, pic_processing_us, pVoid);
}
int SpiSendS(char id,                 int pic_processing_us, char * pChar) {
	int length = strlen(pChar);
	return sendFixed(id, length, pic_processing_us, pChar);
}
int SpiReadS(char id, int buffersize, int pic_processing_us, char * pChar) {
	int length = readToEnd(id, buffersize - 1, pic_processing_us, pChar);
	if (length < 0) return -1;

	*(pChar + length) = 0; //Add the terminating null to the string
	
	return 0;
}
int SpiSendB(char id, int buffersize, int pic_processing_us, void * pVoid) {
	return sendFixed(id, buffersize, pic_processing_us, pVoid);
}
int SpiReadE(char id, int buffersize, int pic_processing_us, void * pVoid) { //Returns length received or -1 on failure
	return readToEnd(id, buffersize, pic_processing_us, pVoid);
}
int SpiReadB(char id, int buffersize, int pic_processing_us, void * pVoid) { //Returns 1 on failure
	return readFixed(id, buffersize, pic_processing_us, pVoid);
}

int SpiInit () {

	
	//Open
	fd = open(DEVICE, O_RDWR);
	if (fd < 0)
	{
		LogErrno("ERROR opening SPI device");
		return -1;
	}
	int ret;
	/*spi mode
	#define SPI_CPHA                0x01
	#define SPI_CPOL                0x02

	#define SPI_MODE_0              (0|0)
	#define SPI_MODE_1              (0|SPI_CPHA)
	#define SPI_MODE_2              (SPI_CPOL|0)
	#define SPI_MODE_3              (SPI_CPOL|SPI_CPHA)

	#define SPI_CS_HIGH             0x04
	#define SPI_LSB_FIRST           0x08
	#define SPI_3WIRE               0x10
	#define SPI_LOOP                0x20
	#define SPI_NO_CS               0x40
	#define SPI_READY               0x80
	*/
	uint8_t mode = SPI_MODE_3;
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
	{
		LogErrno("ERROR writing SPI mode device");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
	{
		LogErrno("ERROR reading SPI mode device");
		return -1;
	}

	//bits per word
	uint8_t bits = 8;
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		LogErrno("ERROR writing SPI bits per word");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		LogErrno("ERROR reading SPI bits per word");
		return -1;
	}

	//max speed hz
	int speed = SPEED_HZ;
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		LogErrno("ERROR writing SPI speed");
		return -1;
	}

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		LogErrno("ERROR reading SPI speed");
		return -1;
	}

	Log('i', "spi mode: %d", mode);
	Log('i', "bits per word: %d", bits);
	Log('i', "max speed: %d Hz (%d KHz)", speed, speed/1000);
	
	return 0;
}
int SpiClose() {
	if (close(fd))
	{
		LogErrno("ERROR closing SPI device");
		return -1;
	}
	return 0;
}
