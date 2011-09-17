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
	uint num_received_packets;
	uint num_retrans_packets;
	uint num_dup_retrans_packets;
};


class MVCTPReceiver : public MVCTPComm {
public:
	MVCTPReceiver(int buf_size);
	~MVCTPReceiver();

	int JoinGroup(string addr, u_short port);
	void 	Start();

	void 	SetPacketLossRate(int rate);
	int 	GetPacketLossRate();
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
	struct MvctpReceiverStats stats;
	StatusProxy*	status_proxy;

	// Memory-to-memory data tranfer
	void 	ReceiveMemoryData(const MvctpTransferMessage & msg, char* mem_data);
	void 	DoMemoryDataRetransmission(char* mem_data, const list<MvctpNackMessage>& nack_list);
	// Disk-to-disk data transfer
	void 	ReceiveFile(const MvctpTransferMessage & msg);
	void 	DoFileRetransmission();
};

#endif /* MVCTPRECEIVER_H_ */
