/*
 * MVCTPSender.cpp
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#include "MVCTPSender.h"


MVCTPSender::MVCTPSender(int buf_size) : MVCTPComm() {
	//ptr_send_buf_mgr = new SendBufferMgr(buf_size, ptr_multicast_comm);
	ptr_send_buf_mgr = new SendBufferMgr(buf_size, ptr_raw_sock_comm);
}

MVCTPSender::~MVCTPSender() {
	delete ptr_send_buf_mgr;
}


void MVCTPSender::SetSendRate(int num_mbps) {
	send_rate_in_mbps = num_mbps;
	ptr_raw_sock_comm->SetSendRate(num_mbps);
}

void MVCTPSender::SetBufferSize(size_t buff_size) {
	ptr_send_buf_mgr->SetBufferSize(buff_size);
}


void MVCTPSender::ResetBuffer() {
	ptr_send_buf_mgr->ResetBuffer();
}

// Send data through RAW socket
int MVCTPSender::RawSend(const char* data, size_t length, bool send_out) {
	return ptr_send_buf_mgr->SendData(data, length, mac_group_addr, send_out);
	//return ptr_mvctp_comm->Send(data, length);
}

ssize_t MVCTPSender::RawReceive(void* buff, size_t len) {
	//return ptr_mvctp_comm->Receive(buff, len);
	return -1;
}

// Send data through UDP multicast socket
int MVCTPSender::IPSend(const char* data, size_t length, bool send_out) {
	return ptr_send_buf_mgr->SendData(data, length, mac_group_addr, send_out);
	//return comm->SendData(data, length, 0);
}

