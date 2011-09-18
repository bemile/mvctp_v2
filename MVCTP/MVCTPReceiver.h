/*
 * MVCTPReceiver.h
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#ifndef MVCTPRECEIVER_H_
#define MVCTPRECEIVER_H_

#include "mvctp.h"
#include "MVCTPComm.h"
#include "TcpClient.h"
#include "../CommUtil/StatusProxy.h"

struct MvctpReceiverStats {
	uint 	total_recv_packets;
	uint 	total_retrans_packets;
	uint 	session_recv_packets;
	uint 	session_retrans_packets;
	double	session_retrans_percentage;
	double	session_total_time;
	double	session_trans_time;
	double 	session_retrans_time;
};


class MVCTPReceiver : public MVCTPComm {
public:
	MVCTPReceiver(int buf_size);
	~MVCTPReceiver();

	int 	JoinGroup(string addr, u_short port);
	void 	Start();

	void 	SetPacketLossRate(int rate);
	int 	GetPacketLossRate();
	void 	SendSessionStatistics();
	void 	SetStatusProxy(StatusProxy* proxy);
	const struct MvctpReceiverStats GetBufferStats();


private:
	TcpClient*		retrans_tcp_client;
	// used in the select() system call
	int		max_sock_fd;
	int 	multicast_sock;
	int		retrans_tcp_sock;
	fd_set	read_sock_set;

	int 	packet_loss_rate;
	MvctpReceiverStats 	recv_stats;
	CpuCycleCounter		cpu_counter;
	StatusProxy*		status_proxy;

	// Memory-to-memory data tranfer
	void 	ReceiveMemoryData(const MvctpTransferMessage & msg, char* mem_data);
	void 	DoMemoryDataRetransmission(char* mem_data, const list<MvctpNackMessage>& nack_list);
	// Disk-to-disk data transfer
	void 	ReceiveFile(const MvctpTransferMessage & msg);
	void 	DoFileRetransmission();
};

#endif /* MVCTPRECEIVER_H_ */
