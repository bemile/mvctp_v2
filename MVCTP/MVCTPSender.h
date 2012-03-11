/*
 * MVCTPSender.h
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#ifndef MVCTPSENDER_H_
#define MVCTPSENDER_H_

#include "mvctp.h"
#include "MVCTPComm.h"
#include "TcpServer.h"
#include "../CommUtil/PerformanceCounter.h"
#include "../CommUtil/StatusProxy.h"
#include "../CommUtil/RateShaper.h"
#include <pthread.h>

struct MvctpSenderStats {
	uint 	total_sent_packets;
	uint	total_sent_bytes;
	uint 	total_retrans_packets;
	uint	total_retrans_bytes;
	uint 	session_sent_packets;
	uint	session_sent_bytes;
	uint 	session_retrans_packets;
	uint	session_retrans_bytes;
	double	session_retrans_percentage;
	double	session_total_time;
	double	session_trans_time;
	double 	session_retrans_time;
};


#define	BUFFER_PACKET_SIZE	5480
struct MvctpRetransBuffer {
	char 	buffer[BUFFER_PACKET_SIZE * MVCTP_PACKET_LEN];  // 8MB buffer size
	char*	cur_pos;
	char*	end_pos;

	MvctpRetransBuffer() {
		cur_pos = buffer;
		end_pos = buffer + BUFFER_PACKET_SIZE * MVCTP_PACKET_LEN;
	}
};

class MVCTPSender : public MVCTPComm {
public:
	MVCTPSender(int buf_size);
	~MVCTPSender();

	void 	SetStatusProxy(StatusProxy* proxy);
	void    SetRetransmissionBufferSize(int size_mb);
	void	SetRetransmissionScheme(int scheme);
	void	SetNumRetransmissionThreads(int num);
	int 	JoinGroup(string addr, u_short port);
	void	RemoveSlowNodes();
	int		RestartTcpServer();
	int		GetNumReceivers();
	void 	SetSendRate(int num_mbps);
	void 	SendAllStatistics();
	void 	SendSessionStatistics();
	void	ResetSessionStatistics();
	// For memory-to-memory data tranfer
	void 	SendMemoryData(void* data, size_t length);
	// For disk-to-disk data transfer
	void 	SendFile(const char* file_name);
	void 	SendFileBufferedIO(const char* file_name);
	// Send data using TCP connections, for performance comparison
	void 	TcpSendMemoryData(void* data, size_t length);
	void 	TcpSendFile(const char* file_name);
	void 	CollectExpResults();
	void	ExecuteCommandOnReceivers(string command, int receiver_start, int receiver_end);

private:
	TcpServer*			retrans_tcp_server;
	u_int32_t			cur_session_id;		// the session ID for a new transfer
	MvctpSenderStats	send_stats;			// data transfer statistics
	CpuCycleCounter		cpu_counter;		// counter for elapsed CPU cycles
	StatusProxy*		status_proxy;
	RateShaper			rate_shaper;
	int					max_num_retrans_buffs;
	int					retrans_scheme;
	int					num_retrans_threads;


	void DoMemoryTransfer(void* data, size_t length, u_int32_t start_seq_num);
	void DoMemoryDataRetransmission(void* data);

	void DoFileRetransmissionSerial(int fd);
	void ReceiveRetransRequestsSerial(map<int, list<NACK_MSG> >* missing_packet_map);

	void DoFileRetransmissionSerialRR(int fd);
	void ReceiveRetransRequestsSerialRR(map <NACK_MSG, list<int> >* missing_packet_map);

	void DoFileRetransmissionParallel(const char* file_name);


	void SortSocketsByShortestJobs(int* ptr_socks, const map<int, list<NACK_MSG> >* missing_packet_map);

	void DoTcpMemoryTransfer(void* data, size_t length, u_int32_t start_seq_num);

	static void* StartRetransmissionThread(void* ptr);
	void	RunRetransmissionThread(const char* file_name, map<int, list<NACK_MSG> >* missing_packet_map);
	pthread_mutex_t sock_list_mutex;
	list<int>	retrans_sock_list;

	int send_rate_in_mbps;
};

#endif /* MVCTPSENDER_H_ */
