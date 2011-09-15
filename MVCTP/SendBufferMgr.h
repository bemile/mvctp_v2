/*
 * SendBufferMgr.h
 *
 *  Created on: Jul 3, 2011
 *      Author: jie
 */

#ifndef SENDBUFFERMGR_H_
#define SENDBUFFERMGR_H_

#include "mvctp.h"
#include "MVCTPBuffer.h"
#include "InetComm.h"
#include "UdpComm.h"
#include "TcpServer.h"
#include <pthread.h>

class SendBufferMgr {
public:
	SendBufferMgr(int size, u_char* dst_addr, InetComm* mcomm);
	~SendBufferMgr();

	int SendData(const char* data, size_t length, void* dst_addr, bool send_out = true);
	void SendPacket(PacketBuffer* entry, void* dst_addr, bool send_out = true);
	void SetBufferSize(size_t buff_size);
	void ResetBuffer();
	void StartRetransmissionThread();
	void DoRetransmission();
	void StartUdpThread();


private:
	u_char 			dst_mac_addr[6];
	MVCTPBuffer* 	send_buf;
	InetComm* 		comm;
	UdpComm*		udp_comm;
	TcpServer*		retrans_tcp_server;
	sockaddr_in		sender_addr;
	socklen_t 		sender_socklen;

	int32_t				last_packet_id;			// packet ID assigned to the latest received packet

	pthread_t tcp_retrans_thread, udp_thread;
	pthread_mutex_t buf_mutex;
	map<int, list<int32_t> > nack_msg_map;

	void MakeRoomForNewPacket(size_t room_size);
	static void* StartTcpRetransThread(void* ptr);
	void RunTcpRetransThread();
	void RetransmitPacket(int sock, int32_t packet_id);

	static void* StartUdpNackReceive(void* ptr);
	void ReceiveNack();
	void Retransmit(NackMsg* msg);
};

#endif /* SENDBUFFERMGR_H_ */
