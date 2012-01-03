/*
 * mvctp.h
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#ifndef MVCTP_H_
#define MVCTP_H_


#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS   64

#include <aio.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <list>
#include <netdb.h>
#include <stdarg.h>
#include <string>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
//#include <linux/if_arp.h>



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

typedef struct MvctpHeader {
	u_int16_t	src_port;
	u_int16_t	dest_port;
	u_int32_t 	session_id;
	u_int32_t	seq_number;
	u_int32_t	data_len;
	u_int32_t	flags;
} MVCTP_HEADER, *PTR_MVCTP_HEADER;


// MVCTP structs
struct MVCTPHeader {
	int32_t 		proto;
	u_int32_t		group_id;
	u_int16_t		src_port;
	u_int16_t		dest_port;
	int32_t			packet_id;
	u_int32_t		data_len;
	u_int32_t		flags;
};

// MVCTP Header Flags
const u_int32_t MVCTP_EOF = 0x00000001;


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

struct MvctpNackMsg {
	int32_t 	proto;
	int32_t 	packet_id;
};


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

const static bool is_debug = true;

// Constant values
const static string group_id = "224.1.2.3";
const static unsigned char group_mac_addr[6] = {0x01, 0x00, 0x5e, 0x01, 0x02, 0x03};
const static ushort mvctp_port = 123;
const static ushort BUFFER_UDP_SEND_PORT = 12345;
const static ushort BUFFER_UDP_RECV_PORT = 12346;
const static ushort BUFFER_TCP_SEND_PORT = 12347;
const static ushort BUFFER_TCP_RECV_PORT = 12348;
const static int PORT_NUM = 11001;
const static int BUFF_SIZE = 10000;

const static ushort MVCTP_PROTO_TYPE = 0x0001;
// Force maximum MVCTP packet length to be 1460 bytes so that it won't cause fragmentation
// when using TCP for packet retransmission
const static int MVCTP_ETH_FRAME_LEN = 1460 + ETH_HLEN;
const static int MVCTP_PACKET_LEN = 1460; //ETH_FRAME_LEN - ETH_HLEN;
const static int MVCTP_HLEN = sizeof(MVCTP_HEADER);
const static int MVCTP_DATA_LEN = MVCTP_PACKET_LEN - sizeof(MVCTP_HEADER); //ETH_FRAME_LEN - ETH_HLEN - sizeof(MVCTP_HEADER);

// parameters for MVCTP over UDP
static const int UDP_MVCTP_PACKET_LEN = 1460;
static const int UDP_MVCTP_HLEN = sizeof(MVCTP_HEADER);
static const int UDP_MVCTP_DATA_LEN = 1200 - sizeof(MVCTP_HEADER);
static const int UDP_PACKET_LEN = ETH_DATA_LEN;

static const int INIT_RTT	= 50;		// in milliseconds


// parameters for data transfer
static const double SEND_RATE_RATIO = (MVCTP_PACKET_LEN + 8 + ETH_HLEN) * 1.0 / MVCTP_DATA_LEN;
static const int MAX_NUM_RECEIVERS = 200;
static const int MAX_MAPPED_MEM_SIZE = 4096 * MVCTP_DATA_LEN;

// message types for MVCTP data transfer
static const int STRING_TRANSFER_START = 1;
static const int STRING_TRANSFER_FINISH = 2;
static const int MEMORY_TRANSFER_START = 3;
static const int MEMORY_TRANSFER_FINISH = 4;
static const int FILE_TRANSFER_START = 5;
static const int FILE_TRANSFER_FINISH = 6;
static const int DO_RETRANSMISSION = 7;
// message types related to TCP transfer (for performance comparison)
static const int TCP_MEMORY_TRANSFER_START = 8;
static const int TCP_MEMORY_TRANSFER_FINISH = 9;
static const int TCP_FILE_TRANSFER_START = 10;
static const int TCP_FILE_TRANSFER_FINISH = 11;
static const int SPEED_TEST = 12;


struct MvctpTransferMessage {
	int32_t		event_type;
	uint32_t	session_id;
	uint32_t 	data_len;
	char       	text[256];
};

const int MAX_NUM_NACK_REQ = 50;
struct MvctpRetransMessage {
	int32_t		num_requests;
	u_int32_t	seq_numbers[MAX_NUM_NACK_REQ];
	u_int32_t	data_lens[MAX_NUM_NACK_REQ];
};

typedef struct MvctpNackMessage {
	u_int32_t 	seq_num;
	u_int32_t	data_len;
} NACK_MSG, * PTR_NACK_MSG;


class MVCTP {
public:
	static double CPU_MHZ;
	static FILE*  log_file;
	static bool is_log_enabled;
	static struct CpuCycleCounter start_time_counter;
};

#endif /* MVCTP_H_ */
