/*
 * ReceiveBufferMgr.cpp
 *
 *  Created on: Jul 3, 2011
 *      Author: jie
 */

#include "ReceiveBufferMgr.h"

ReceiveBufferMgr::ReceiveBufferMgr(int size, InetComm* mcomm) {
	last_recv_packet_id = 0;
	last_del_packet_id = 0;
	is_first_packet = true;
	memset(&buffer_stats, 0, sizeof(buffer_stats));

	recv_buf = new MVCTPBuffer(size);
	comm = mcomm;
	udp_comm = new UdpComm(BUFFER_UDP_RECV_PORT);

	//TODO: remove this after the problem of getting sender address info is solved
	//hostent * record = gethostbyname("node0.ldm-hs-lan.MVC.emulab.net");
	//in_addr* address = (in_addr *)record->h_addr_list[0];
	in_addr address;
	inet_aton("10.1.1.2", &address);
	sender_udp_addr.sin_port = htons(BUFFER_UDP_SEND_PORT);
	sender_udp_addr.sin_family = AF_INET;
	memcpy(&sender_udp_addr.sin_addr, &address, sizeof(address));

	packet_loss_rate = 0;

	CpuCycleCounter cc;
	AccessCPUCounter(&cc.hi, &cc.lo);
	srand(cc.lo);

//	int index = 0;
//	while( (address = (in_addr *)record->h_addr_list[index++]) != NULL) {
//		string ip = inet_ntoa(*address);
//		if (ip.find("10.1.") != ip.npos) {
//			sender_udp_addr.sin_port = htons(BUFFER_UDP_SEND_PORT);
//			sender_udp_addr.sin_family = AF_INET;
//			memcpy(&sender_udp_addr.sin_addr, address, sizeof(address));
//			cout << "Sender address: " << ip << endl;
//			break;
//		}
//	}
}


ReceiveBufferMgr::~ReceiveBufferMgr() {
	pthread_mutex_destroy(&buf_mutex);
	pthread_mutex_destroy(&nack_list_mutex);

	delete udp_comm;
	delete recv_buf;
}

void ReceiveBufferMgr::SetBufferSize(size_t buff_size) {
	recv_buf->SetMaxBufferSize(buff_size);
}

void ReceiveBufferMgr::SetSocketBufferSize(size_t buff_size) {
	comm->SetBufferSize(buff_size);
}

size_t ReceiveBufferMgr::GetBufferSize() {
	return recv_buf->GetMaxBufferSize();
}

void ReceiveBufferMgr::SetPacketLossRate(int rate) {
	packet_loss_rate = rate;
}

int ReceiveBufferMgr::GetPacketLossRate() {
	return packet_loss_rate;
}


const ReceiveBufferStats ReceiveBufferMgr::GetBufferStats() {
	return buffer_stats;
}


void ReceiveBufferMgr::ResetBuffer() {
	memset(&buffer_stats, 0, sizeof(buffer_stats));

	pthread_mutex_lock(&nack_list_mutex);
	missing_packets.clear();
	pthread_mutex_unlock(&nack_list_mutex);

	pthread_mutex_lock(&buf_mutex);
	recv_buf->Clear();
	pthread_mutex_unlock(&buf_mutex);

	last_recv_packet_id = 0;
	last_del_packet_id = 0;
	is_first_packet = true;
}

size_t ReceiveBufferMgr::GetData(void* buff, size_t len) {
	//wait until there are some data in the buffer
	while (recv_buf->IsEmpty() || recv_buf->Find(last_del_packet_id  + 1) == NULL) {
		usleep(5000);
	}

	char* pos = (char*)buff;
	size_t bytes_copied = 0;
	size_t bytes_remained = len;
	int32_t next_pid = last_del_packet_id  + 1;
	PacketBuffer * tmp = NULL;
	int sleep_turns = 0;
	while (true) {
		pthread_mutex_lock(&buf_mutex);
		while (bytes_remained > 0) {
			if ( (tmp = recv_buf->Find(next_pid)) == NULL)
				break;

			if (bytes_remained < tmp->data_len) {
				memcpy(pos, tmp->data, bytes_remained);
				pos += bytes_remained;
				bytes_copied += bytes_remained;
				bytes_remained = 0;
				recv_buf->ShrinkEntry(tmp, tmp->data_len - bytes_remained);
				break;
			} else {
				memcpy(pos, tmp->data, tmp->data_len);
				pos += tmp->data_len;
				bytes_copied += tmp->data_len;
				bytes_remained -= tmp->data_len;
			}

			next_pid++;
		}

		recv_buf->DeleteUntil(last_del_packet_id + 1, next_pid);
		last_del_packet_id = next_pid - 1;
		pthread_mutex_unlock(&buf_mutex);

		if (bytes_remained <= 0 || sleep_turns >= 5) {
			return bytes_copied;
		}
		else {
			usleep(5000);
			sleep_turns++;
		}
	}

	return bytes_copied;
}



void ReceiveBufferMgr::StartReceiveThread() {
	pthread_mutex_init(&buf_mutex, 0);
	pthread_mutex_init(&nack_list_mutex, 0);
	int res;
	if ( (res= pthread_create(&recv_thread, NULL, &ReceiveBufferMgr::StartReceivingData, this)) != 0) {
		SysError("ReceiveBufferMgr:: pthread_create() error");
	}

	if ( (res= pthread_create(&udp_thread, NULL, &ReceiveBufferMgr::StartUdpReceiveThread, this)) != 0) {
		SysError("ReceiveBufferMgr:: pthread_create() error");
	}

	//if ( (res= pthread_create(&nack_thread, NULL, &ReceiveBufferMgr::StartNackThread, this)) != 0) {
	//		SysError("ReceiveBufferMgr:: pthread_create() error");
	//}
}

// Helper function to start the multicast data receiving thread
void* ReceiveBufferMgr::StartReceivingData(void* ptr) {
	((ReceiveBufferMgr*)ptr)->Run();
	pthread_exit(NULL);
}


void ReceiveBufferMgr::Run() {
//	struct sched_param sp;
//	sp.__sched_priority = sched_get_priority_max(SCHED_RR);
//	sched_setscheduler(0, SCHED_RR, &sp);

	char buf[ETH_DATA_LEN];
	MVCTP_HEADER* header = (MVCTP_HEADER*)buf;
	int bytes;

	while (true) {
		if ( (bytes = comm->RecvData(buf, ETH_DATA_LEN, 0, (SA*)&sender_multicast_addr, &sender_socklen)) <= 0) {
			if (errno == EINTR)
				continue;
			else
				SysError("MVCTPBuffer error on receiving data");
		}
		//cout << "I received one packet. Packet length: " << bytes << endl;

		// Initialize the packet id information on receiving the first packet
		if (is_first_packet) {
			char ip[20];
			sockaddr_in * ptr_sock = (sockaddr_in *)&sender_multicast_addr;
			inet_ntop(AF_INET, (void*)&(ptr_sock->sin_addr), ip, 20);
			ip[15] = 0;
			//cout << "Sender IP address: " << ip << endl;
			//cout << "Sender Port: " << ntohs(ptr_sock->sin_port) << endl;

			last_recv_packet_id = header->packet_id - 1;
			last_del_packet_id = header->packet_id - 1;
			is_first_packet = false;
		}

		// Record missing packets if there is a gap in the packet_id
		if (header->packet_id > last_recv_packet_id + 1) {
			Log("%.6f    Packet loss detected. Received Packet ID: %d    Supposed ID:%d\n",
								GetCurrentTime(), header->packet_id, last_recv_packet_id + 1);

			pthread_mutex_lock(&nack_list_mutex);
			clock_t time = clock(); //- 0.5 * CLOCKS_PER_SEC;
			NackMsg msg;
			memset(&msg, 0, sizeof(msg));
			msg.proto = MVCTP_PROTO_TYPE;
			for (int32_t i = last_recv_packet_id + 1; i != header->packet_id; i++) {
				NackMsgInfo info;
				info.packet_id = i;
				info.time_stamp = time;
				info.num_retries = 0;
				info.packet_received = false;
				missing_packets.insert(pair<int, NackMsgInfo>(info.packet_id, info));

				msg.packet_ids[msg.num_missing_packets] = i;
				msg.num_missing_packets++;
				if (msg.num_missing_packets == MAX_NACK_IDS) {
					SendNackMsg(msg);
					msg.num_missing_packets = 0;
				}
			}

			if (msg.num_missing_packets > 0) {
				SendNackMsg(msg);
			}

			Log("%.6f    Retransmission requests sent for missing packet IDs. From: %d To: %d\n",
						GetCurrentTime(), last_recv_packet_id + 1, header->packet_id - 1);
			pthread_mutex_unlock(&nack_list_mutex);
		}

		// Add the received packet to the buffer
		// When greater than packet_loss_rate, add the packet to the receive buffer
		// Otherwise, just drop the packet (emulates errored packet)
		if (rand() % 1000 >= packet_loss_rate) {
			AddNewEntry(header, buf);
		}
	}
}


void ReceiveBufferMgr::AddNewEntry(MVCTP_HEADER* header, void* buf) {
	pthread_mutex_lock(&buf_mutex);
	PacketBuffer* entry = recv_buf->GetFreePacket();
	entry->packet_id = header->packet_id;
	entry->packet_len = MVCTP_HLEN + header->data_len;
	entry->data_len = header->data_len;
	memcpy(entry->mvctp_header, buf, entry->packet_len);

	if (entry->packet_len <= recv_buf->GetAvailableBufferSize()) {
		if (recv_buf->Insert(entry))
			buffer_stats.num_received_packets++;
		//Log("One new packet received. Packet ID: %d\n", header->packet_id);
	}
	else {
		//Log("Not enough buffer space for a new packet. Packet ID: %d\n", header->packet_id);
		clock_t time = clock();
		NackMsgInfo info;
		info.packet_id = header->packet_id;
		info.time_stamp = time;
		info.num_retries = 0;
		info.packet_received = false;
		pthread_mutex_lock(&nack_list_mutex);
		missing_packets.insert(pair<int, NackMsgInfo>(info.packet_id, info));
		pthread_mutex_unlock(&nack_list_mutex);
	}
	pthread_mutex_unlock(&buf_mutex);

	last_recv_packet_id = header->packet_id;
}


void* ReceiveBufferMgr::StartNackThread(void* ptr) {
	((ReceiveBufferMgr*)ptr)->NackRun();
	pthread_exit(NULL);
}

// Keep checking the missing list for retransmission request
void ReceiveBufferMgr::NackRun() {
	map<int32_t, NackMsgInfo>::iterator it;
	clock_t max_rto = 400;

	while (true) {
		clock_t cur_time = clock();
		NackMsg msg;
		memset(&msg, 0, sizeof(msg));
		msg.proto = MVCTP_PROTO_TYPE;


		pthread_mutex_lock(&nack_list_mutex);
		for (it = missing_packets.begin(); it != missing_packets.end(); it++) {
			clock_t rto = it->second.num_retries * INIT_RTT * CLOCKS_PER_SEC / 1000;
			if (rto > max_rto)
				rto = max_rto;

			if ((cur_time - it->second.time_stamp) > rto) {
				//		&& it->second.num_retries < 20) {
				msg.packet_ids[msg.num_missing_packets] = it->first;
				msg.num_missing_packets++;
				if (msg.num_missing_packets == MAX_NACK_IDS) {
					SendNackMsg(msg);
					msg.num_missing_packets = 0;
				}

				it->second.num_retries++;
				it->second.time_stamp = cur_time;
				//cout << "One NACK message sent." << endl;
			}
		}

		pthread_mutex_unlock(&nack_list_mutex);

		if (msg.num_missing_packets > 0) {
			SendNackMsg(msg);
		}

		usleep(5000);
	}
}


void ReceiveBufferMgr::StartNackRetransTimer() {
	/* Install our SIGPROF signal handler */
	signal_action.sa_sigaction = ReceiveBufferMgr::RetransmitNackHandler;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_flags = SA_SIGINFO; /* we want a siginfo_t */
	if (sigaction(SIGALRM, &signal_action, 0)) {
		SysError("ReceiveBufferMgr::StartNackRetransTimer()::sigaction()");
	}

	// Define sigEvent
	// This information will be forwarded to the signal-handler function
	memset(&signal_event, 0, sizeof(signal_event));
	// With the SIGEV_SIGNAL flag we say that there is sigev_value
	signal_event.sigev_notify = SIGEV_SIGNAL;
	// Now it's possible to give a pointer to the object
	signal_event.sigev_value.sival_ptr = (void*) this;
	// Declare this signal as Alarm Signal
	signal_event.sigev_signo = SIGALRM;

	// Install the Timer
	if (timer_create(CLOCK_REALTIME, &signal_event, &timer_id) != 0) {
		SysError("ReceiveBufferMgr::StartNackRetransTimer()::timer_create()");
	}

	/* Request SIGPROF */
	timer_specs.it_interval.tv_sec = 0;
	timer_specs.it_interval.tv_nsec = 5 * 1000 * 1000;
	timer_specs.it_value.tv_sec = 0;
	timer_specs.it_value.tv_nsec = 5 * 1000 * 1000;
	//setitimer(SIGALRM, &timer_specs, NULL);

	if (timer_settime(timer_id, 0, &timer_specs, NULL) == -1) {
		SysError("Could not start timer:");
	}
}

void ReceiveBufferMgr::RetransmitNackHandler(int cause, siginfo_t *si, void *ucontext) {
	ReceiveBufferMgr* ptr = (ReceiveBufferMgr*)si->si_value.sival_ptr;
	ptr->DoRetransmitNacks();
}

void ReceiveBufferMgr::DoRetransmitNacks() {
	map<int32_t, NackMsgInfo>::iterator it;
	clock_t max_rto = 400;
	clock_t cur_time = clock();

	NackMsg msg;
	memset(&msg, 0, sizeof(msg));
	msg.proto = MVCTP_PROTO_TYPE;

	pthread_mutex_lock(&nack_list_mutex);
	for (it = missing_packets.begin(); it != missing_packets.end(); it++) {
		clock_t rto = it->second.num_retries * INIT_RTT * CLOCKS_PER_SEC / 1000;
		if (rto > max_rto)
			rto = max_rto;

		if ((cur_time - it->second.time_stamp) > rto) {
			//		&& it->second.num_retries < 20) {
			msg.packet_ids[msg.num_missing_packets] = it->first;
			msg.num_missing_packets++;
			if (msg.num_missing_packets == MAX_NACK_IDS) {
				SendNackMsg(msg);
				msg.num_missing_packets = 0;
			}

			it->second.num_retries++;
			it->second.time_stamp = cur_time;
			//cout << "One NACK message sent." << endl;
		}
	}
	pthread_mutex_unlock(&nack_list_mutex);

	if (msg.num_missing_packets > 0) {
		SendNackMsg(msg);
	}
}

// Send a retransmission request to the sender
int ReceiveBufferMgr::SendNackMsg(NackMsg& msg) {
	return udp_comm->SendTo((void *)&msg, sizeof(msg), 0,
			(SA*)&sender_udp_addr, sizeof(sender_udp_addr));
}


// Helper function to start the unicast UDP thread
void* ReceiveBufferMgr::StartUdpReceiveThread(void* ptr) {
	((ReceiveBufferMgr*)ptr)->UdpReceive();
	pthread_exit(NULL);
}


// Receive retransmitted packets from the sender through UDP
void ReceiveBufferMgr::UdpReceive() {
	char buf[UDP_PACKET_LEN];
	MVCTP_HEADER* header = (MVCTP_HEADER*) buf;
	int bytes;

	StartNackRetransTimer();

	while (true) {
		if ( (bytes = udp_comm->RecvFrom(buf, UDP_PACKET_LEN, 0, NULL, NULL)) <= 0) {
			if (errno == EINTR)
				continue;
			else
				SysError("ReceiveBufferMgr::UdpReceive()::RecvData() error");
		}

		Log("%.6f    One retransmission packet received. Packet ID: %d\n", GetCurrentTime(), header->packet_id);
		// Discard duplicated packet that has already been used and deleted from the buffer
		if (header->packet_id <= last_del_packet_id) {
			DeleteNackFromList(header->packet_id);
			buffer_stats.num_dup_retrans_packets++;
			continue;
		}

		AddRetransmittedEntry(header, buf);
		DeleteNackFromList(header->packet_id);
	}
}

void ReceiveBufferMgr::AddRetransmittedEntry(MVCTP_HEADER* header, void* buf) {
	pthread_mutex_lock(&buf_mutex);
		PacketBuffer* entry = recv_buf->GetFreePacket();
		entry->packet_id = header->packet_id;
		entry->packet_len = MVCTP_HLEN + header->data_len;
		entry->data_len = header->data_len;
		memcpy(entry->mvctp_header, buf, entry->packet_len);

		if (recv_buf->Insert(entry)) {
			//last_recv_packet_id = header->packet_id;
			buffer_stats.num_retrans_packets++;
			buffer_stats.num_received_packets++;
		}
	pthread_mutex_unlock(&buf_mutex);

	Log("%.6f    Retransmitted packet added to the buffer. Packet ID: %d\n", GetCurrentTime(), header->packet_id);
}



void ReceiveBufferMgr::DeleteNackFromList(int32_t packet_id) {
//	map<int32_t, NackMsgInfo>::iterator it = missing_packets.find(packet_id);
//	if (it != missing_packets.end())
//		it->second.packet_received = true;

	pthread_mutex_lock(&nack_list_mutex);
	missing_packets.erase(packet_id);
	pthread_mutex_unlock(&nack_list_mutex);
}



