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
#include "../CommUtil/StatusProxy.h"

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


class MVCTPSender : public MVCTPComm {
public:
	MVCTPSender(int buf_size);
	~MVCTPSender();

	void 	SetStatusProxy(StatusProxy* proxy);
	int 	JoinGroup(string addr, u_short port);
	void 	SetSendRate(int num_mbps);
	void 	SendAllStatistics();
	void 	SendSessionStatistics();
	// For memory-to-memory data tranfer
	void 	SendMemoryData(void* data, size_t length);
	// For disk-to-disk data transfer
	void 	SendFile(const char* file_name);


private:
	TcpServer*			retrans_tcp_server;
	u_int32_t			cur_session_id;		// the session ID for a new transfer
	MvctpSenderStats	send_stats;			// data transfer statistics
	CpuCycleCounter		cpu_counter;		// counter for elapsed CPU cycles
	StatusProxy*		status_proxy;

	void DoMemoryTransfer(void* data, size_t length, u_int32_t start_seq_num);
	void DoMemoryDataRetransmission(void* data);
	void DoFileRetransmission(int fd);
	void ReceiveRetransRequests(map<int, list<NACK_MSG> >* missing_packet_map);
	void SortSocketsByShortestJobs(int* ptr_socks, const map<int, list<NACK_MSG> >* missing_packet_map);


	int send_rate_in_mbps;
};

#endif /* MVCTPSENDER_H_ */
