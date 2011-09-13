/*
 * mvctp.h
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#ifndef MVCTP_H_
#define MVCTP_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
//#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctime>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdarg.h>
#include <string>
#include <string.h>
#include <list>
#include "ConfigInfo.h"


using namespace std;

struct CpuCycleCounter {
	unsigned hi;
	unsigned lo;
};

//global functions
void MVCTPInit();
void AccessCPUCounter(unsigned *hi, unsigned *lo);
double GetElapsedCycles(unsigned hi, unsigned lo);
double GetElapsedSeconds(CpuCycleCounter lastCount);
double GetCPUMhz();
double GetCurrentTime();
void SysError(string s);
void Log(char* format, ...);
void CreateNewLogFile(const char* file_name);

// Linux network struct typedefs
typedef struct sockaddr SA;
typedef struct ifreq	IFREQ;


// MVCTP structs
typedef struct MVCTPHeader {
	int32_t 		proto;
	u_int32_t		group_id;
	u_int16_t		src_port;
	u_int16_t		dest_port;
	int32_t			packet_id;
	u_int32_t		data_len;
} MVCTP_HEADER, *PTR_MVCTP_HEADER;


/*#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl:4;
    unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version:4;
    unsigned int ihl:4;
#else
# error "Please fix <bits/endian.h>"
#endif
*/

// buffer entry for a single packet
typedef struct PacketBuffer {
	int32_t 	packet_id;
	size_t		packet_len;
	size_t		data_len;
	char*		eth_header;
	char*		mvctp_header;
	char*		data;
	char* 		packet_buffer;
} BUFFER_ENTRY, * PTR_BUFFER_ENTRY;


const int MAX_NACK_IDS = 10;
struct NackMsg {
	int32_t 	proto;
	int32_t 	num_missing_packets;
	int32_t 	packet_ids[MAX_NACK_IDS];
};

struct NackMsgInfo {
	int32_t		packet_id;
	clock_t		time_stamp;
	short		num_retries;
	bool		packet_received;
};

// Macros
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

const bool is_debug = true;

// Constant values
const string group_id = "224.1.2.3";
const unsigned char group_mac_addr[6] = {0x01, 0x00, 0x5e, 0x01, 0x02, 0x03};
const ushort mvctp_port = 123;
const ushort BUFFER_UDP_SEND_PORT = 12345;
const ushort BUFFER_UDP_RECV_PORT = 12346;
const int PORT_NUM = 11001;
const int BUFF_SIZE = 10000;

const ushort MVCTP_PROTO_TYPE = 0x0001;
// Force maximum MVCTP packet length to be 1460 bytes so that it won't cause fragmentation
// when using TCP for packet retransmission
const int MVCTP_ETH_FRAME_LEN = 1460 + ETH_HLEN;
const int MVCTP_PACKET_LEN = 1460; //ETH_FRAME_LEN - ETH_HLEN;
const int MVCTP_HLEN = sizeof(MVCTP_HEADER);
const int MVCTP_DATA_LEN = MVCTP_PACKET_LEN - sizeof(MVCTP_HEADER); //ETH_FRAME_LEN - ETH_HLEN - sizeof(MVCTP_HEADER);

// parameters for MVCTP over UDP
const int UDP_MVCTP_PACKET_LEN = 1460;
const int UDP_MVCTP_HLEN = sizeof(MVCTP_HEADER);
const int UDP_MVCTP_DATA_LEN = 1200 - sizeof(MVCTP_HEADER);
const int UDP_PACKET_LEN = ETH_DATA_LEN;

const int INIT_RTT	= 50;		// in milliseconds


class MVCTP {
public:
	static double CPU_MHZ;
	static FILE*  log_file;
	static bool is_log_enabled;
	static struct CpuCycleCounter start_time_counter;
};

#endif /* MVCTP_H_ */
