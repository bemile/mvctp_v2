/*
 * ReceiveBufferMgr.h
 *
 *  Created on: Jul 3, 2011
 *      Author: jie
 */

#ifndef RECEIVEBUFFERMGR_H_
#define RECEIVEBUFFERMGR_H_

#include "mvctp.h"
#include "MVCTPBuffer.h"
#include "InetComm.h"
#include "UdpComm.h"
#include <netinet/in.h>
#include <pthread.h>

struct ReceiveBufferStats {
	uint num_received_packets;
	uint num_retrans_packets;
	uint num_dup_retrans_packets;
};

class ReceiveBufferMgr {
public:
	ReceiveBufferMgr(int size, InetComm* mcomm);
	~ReceiveBufferMgr();

	size_t 		GetData(void* buff, size_t len);
	void 		StartReceiveThread();
	void 		SetBufferSize(size_t buff_size);
	size_t 		GetBufferSize();
	void 		SetPacketLossRate(int rate);		// rate is #loss per thousand packets
	int 		GetPacketLossRate();
	void 		SetSocketBufferSize(size_t buff_size);
	void 		ResetBuffer();
	const struct ReceiveBufferStats GetBufferStats();

private:
	MVCTPBuffer* 	recv_buf;
	InetComm* 		comm;
	UdpComm*		udp_comm;
	sockaddr		sender_multicast_addr;
	sockaddr_in		sender_udp_addr;
	socklen_t 		sender_socklen;

	int32_t				last_recv_packet_id;	// packet ID assigned to the latest received packet
	int32_t				last_del_packet_id;
	bool 				is_first_packet;
	map<int32_t, NackMsgInfo> 	missing_packets;
	struct ReceiveBufferStats buffer_stats;
	int 			packet_loss_rate;

	int SendNackMsg(NackMsg& msg);

	pthread_t recv_thread, nack_thread, udp_thread;
	pthread_mutex_t buf_mutex, nack_list_mutex;
	static void* StartReceivingData(void* ptr);
	void Run();
	void AddNewEntry(MVCTP_HEADER* header, void* buf);
	void AddRetransmittedEntry(MVCTP_HEADER* header, void* buf);


	timer_t timer_id;
	struct sigevent signal_event;
	struct sigaction signal_action;
	struct itimerspec timer_specs;
	static void* StartNackThread(void* ptr);
	void NackRun();
	void StartNackRetransTimer();
	static void RetransmitNackHandler(int cause, siginfo_t *how_come, void *ucontext);
	void DoRetransmitNacks();
	void DeleteNackFromList(int32_t packet_id);


	static void* StartUdpReceiveThread(void* ptr);
	void UdpReceive();
};

#endif /* RECEIVEBUFFERMGR_H_ */
