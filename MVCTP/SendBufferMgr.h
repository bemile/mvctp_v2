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
#include <pthread.h>

class SendBufferMgr {
public:
	SendBufferMgr(int size, InetComm* mcomm);
	~SendBufferMgr();

	int SendData(const char* data, size_t length, void* dst_addr, bool send_out);
	void SendPacket(PacketBuffer* entry, void* dst_addr, bool send_out);
	void StartUdpThread();
	void SetBufferSize(size_t buff_size);
	void ResetBuffer();


private:
	MVCTPBuffer* 	send_buf;
	InetComm* 		comm;
	UdpComm*		udp_comm;
	sockaddr_in		sender_addr;
	socklen_t 		sender_socklen;

	int32_t				last_packet_id;			// packet ID assigned to the latest received packet

	pthread_t udp_thread;
	pthread_mutex_t buf_mutex;
	static void* StartUdpNackReceive(void* ptr);
	void ReceiveNack();
	void Retransmit(NackMsg* msg);
	void MakeRoomForNewPacket(size_t room_size);
};

#endif /* SENDBUFFERMGR_H_ */
