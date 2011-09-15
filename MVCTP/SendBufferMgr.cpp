/*
 * SendBufferMgr.cpp
 *
 *  Created on: Jul 3, 2011
 *      Author: jie
 */

#include "SendBufferMgr.h"

SendBufferMgr::SendBufferMgr(int size, u_char* dst_addr, InetComm* mcomm) {
	memcpy(dst_mac_addr, dst_addr, 6);
	last_packet_id = 0;

	send_buf = new MVCTPBuffer(size);
	comm = mcomm;

	retrans_tcp_server = new TcpServer(BUFFER_TCP_SEND_PORT);
	StartRetransmissionThread();

	udp_comm = new UdpComm(BUFFER_UDP_SEND_PORT);
	//StartUdpThread();

}

SendBufferMgr::~SendBufferMgr() {
	pthread_mutex_destroy(&buf_mutex);
	delete udp_comm;
	delete send_buf;
}


void SendBufferMgr::SetBufferSize(size_t buff_size) {
	send_buf->SetMaxBufferSize(buff_size);
}


void SendBufferMgr::ResetBuffer() {
	send_buf->Clear();
	last_packet_id = 0;
}

// Send out data through the multicast socket
// send_out: whether or not actually send out the packet (for testing reliability)
//              should be removed in real implementation
int SendBufferMgr::SendData(const char* data, size_t length, void* dst_addr, bool send_out) {
	int bytes_left = length;
	int bytes_sent = 0;
	const char* pos = data;

	pthread_mutex_lock(&buf_mutex);
	while (bytes_left > 0) {
		int len = bytes_left > MVCTP_DATA_LEN ? MVCTP_DATA_LEN : bytes_left;

		PacketBuffer* entry = send_buf->GetFreePacket();
		MVCTP_HEADER* header = (MVCTP_HEADER*) entry->mvctp_header;
		header->proto = MVCTP_PROTO_TYPE;
		header->packet_id = ++last_packet_id;
		header->data_len = len;
		header->flags = 0;

		entry->packet_id = header->packet_id;
		entry->packet_len = len + MVCTP_HLEN;
		entry->data_len = len;
		memcpy(entry->data, pos, len);

		SendPacket(entry, dst_addr, send_out);

		pos += len;
		bytes_left -= len;
		bytes_sent += len;
	}
	pthread_mutex_unlock(&buf_mutex);

	return bytes_sent;
}


// Send out a single MVCTP packet
void SendBufferMgr::SendPacket(PacketBuffer* entry, void* dst_addr, bool send_out) {
	size_t avail_buf_size = send_buf->GetAvailableBufferSize();
	if (avail_buf_size < entry->packet_len) {
		MakeRoomForNewPacket(entry->packet_len - avail_buf_size);
	}

	send_buf->Insert(entry);
	if (send_out) {
		if (comm->SendPacket(entry, 0, dst_addr) < 0) {
			SysError("SendBufferMgr::SendPacket()::SendData() error");
		}
	}
}


void SendBufferMgr::MakeRoomForNewPacket(size_t room_size) {
	size_t size = 0;
	int32_t pid;
	PacketBuffer* it;
	for (pid = send_buf->GetMinPacketId(); pid <= send_buf->GetMaxPacketId(); pid++) {
		if (size > room_size)
			break;

		if ( (it = send_buf->Find(pid)) != NULL)
			size += it->data_len;
	}

	send_buf->DeleteUntil(send_buf->GetMinPacketId(), pid);
}


void SendBufferMgr::DoRetransmission() {
	// First send EOF messages to all receivers so that they know
	// they should start receiving retransmission packets
	PacketBuffer* entry = send_buf->GetFreePacket();
	MVCTP_HEADER* header = (MVCTP_HEADER*) entry->mvctp_header;
	header->proto = MVCTP_PROTO_TYPE;
	header->packet_id = last_packet_id;
	header->data_len = 0;
	header->flags = MVCTP_EOF;

	entry->packet_id = header->packet_id;
	entry->packet_len = MVCTP_HLEN;
	entry->data_len = 0;
	SendPacket(entry, dst_mac_addr, true);
	header->flags = 0;

	//
	map<int, list<int32_t> >::iterator it;
	pthread_mutex_lock(&buf_mutex);
	for (it = nack_msg_map.begin(); it != nack_msg_map.end(); it++) {
		int sock = it->first;
		list<int32_t>::iterator list_it;
		for (list_it = it->second.begin(); list_it != it->second.end(); list_it++) {
			RetransmitPacket(sock, *list_it);
		}
		it->second.clear();
	}
	pthread_mutex_unlock(&buf_mutex);
}


void SendBufferMgr::RetransmitPacket(int sock, int32_t packet_id) {
	PacketBuffer* entry = send_buf->Find(packet_id);
	if (entry != NULL) {
		retrans_tcp_server->SelectSend(sock,(void *)entry->mvctp_header, entry->packet_len);
	}
}


//======================== TCP Retransmission Thread Functions ========================
void SendBufferMgr::StartRetransmissionThread() {
	pthread_mutex_init(&buf_mutex, 0);

	int res;
	if ( (res= pthread_create(&tcp_retrans_thread, NULL, &SendBufferMgr::StartTcpRetransThread, this)) != 0) {
		SysError("SendBufferMgr:: pthread_create() error");
	}
}

void* SendBufferMgr::StartTcpRetransThread(void* ptr) {
	((SendBufferMgr*)ptr)->RunTcpRetransThread();
	pthread_exit(NULL);
}


void SendBufferMgr::RunTcpRetransThread() {
	retrans_tcp_server->Start();
	int client_sock;
	struct MvctpNackMsg nack_msg;
	int size = sizeof(nack_msg);
	while (true) {
		retrans_tcp_server->SelectReceive(&client_sock, &nack_msg, size);
		nack_msg_map[client_sock].push_back(nack_msg.packet_id);
	}
}



//======================== UDP Retransmission Thread Functions ========================
void SendBufferMgr::StartUdpThread() {
	pthread_mutex_init(&buf_mutex, 0);

	int res;
	if ( (res= pthread_create(&udp_thread, NULL, &SendBufferMgr::StartUdpNackReceive, this)) != 0) {
		SysError("SendBufferMgr:: pthread_create() error");
	}
}


void* SendBufferMgr::StartUdpNackReceive(void* ptr) {
	((SendBufferMgr*)ptr)->ReceiveNack();
	pthread_exit(NULL);
}


void SendBufferMgr::ReceiveNack() {
	char buf[UDP_PACKET_LEN];
	NackMsg*	nack_msg = (NackMsg*)buf;
	int bytes;

	while (true) {
		if ( (bytes = udp_comm->RecvFrom(buf, UDP_PACKET_LEN, 0, (SA*)&sender_addr, &sender_socklen)) <= 0) {
			SysError("ReceiveBufferMgr::UdpReceive()::RecvData() error");
		}

		if (nack_msg->proto == MVCTP_PROTO_TYPE) {
			Log("%.6f    One retransmission request received. Packet Number: %d    Receiver: %s\n",
					GetCurrentTime(), nack_msg->num_missing_packets,  inet_ntoa(sender_addr.sin_addr));
			Retransmit(nack_msg);
		}
	}
}


void SendBufferMgr::Retransmit(NackMsg* ptr_msg) {
	pthread_mutex_lock(&buf_mutex);
	for (int i = 0; i < ptr_msg->num_missing_packets; i++) {
		int32_t packet_id = ptr_msg->packet_ids[i];
		PacketBuffer* entry = send_buf->Find(packet_id);
		if (entry != NULL) {
			udp_comm->SendTo((void *)entry->mvctp_header, entry->packet_len, 0,
						(SA*)&sender_addr, sender_socklen);
			Log("%.6f    One packet retransmitted. Packet ID: %d\n",
					GetCurrentTime(), packet_id);
		}
		else {
			Log("%.6f    Could not find packet for retransmission. Packet ID: %d\n",
					GetCurrentTime(), packet_id);
		}
	}
	pthread_mutex_unlock(&buf_mutex);
}

