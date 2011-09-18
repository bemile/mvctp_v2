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


class MVCTPSender : public MVCTPComm {
public:
	MVCTPSender(int buf_size);
	~MVCTPSender();

	void 	SetStatusProxy(StatusProxy* proxy);
	int 	JoinGroup(string addr, u_short port);
	void 	SetSendRate(int num_mbps);
	// For memory-to-memory data tranfer
	void 	SendMemoryData(void* data, size_t length);
	// For disk-to-disk data transfer
	void 	SendFile(const char* file_name);


private:
	TcpServer*		retrans_tcp_server;
	u_int32_t		last_packet_id;		// packet ID assigned to the latest received packet
	u_int32_t		cur_session_id;		// the session ID for a new transfer

	void DoMemoryTransfer(void* data, size_t length, u_int32_t start_seq_num);
	void DoMemoryDataRetransmission(void* data);
	void DoFileRetransmission(int fd);
	void ReceiveRetransRequests(map<int, list<NACK_MSG> >& missing_packet_map);

	StatusProxy*	status_proxy;
	int send_rate_in_mbps;
};

#endif /* MVCTPSENDER_H_ */
