/*
 * MVCTPSender.cpp
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#include "MVCTPSender.h"


MVCTPSender::MVCTPSender(int buf_size) : MVCTPComm() {
	retrans_tcp_server = new TcpServer(BUFFER_TCP_SEND_PORT);
	cur_session_id = 0;
	bzero(&send_stats, sizeof(send_stats));
}

MVCTPSender::~MVCTPSender() {
}

void MVCTPSender::SetSendRate(int num_mbps) {
	send_rate_in_mbps = num_mbps;
}

void MVCTPSender::SetStatusProxy(StatusProxy* proxy) {
	status_proxy = proxy;
}

void MVCTPSender::SendAllStatistics() {
	char buf[512];
	sprintf(buf, "***** Sender Statistics *****\nTotal Sent Packets:\t\t%u\nTotal Retrans. Packets:\t\t%d\t"
			"Session Sent Packets:\t\t%d\nSession Retrans. Packets:\t\t%d\t"
			"Retrans. Percentage:\t\t%.4f\nTotal Trans. Time:\t\t%.2f sec\nMulticast Trans. Time:\t\t%.2f sec\n"
			"Retrans. Time:\t\t\t%.2f sec\n", send_stats.total_sent_packets, send_stats.total_retrans_packets,
			send_stats.session_sent_packets, send_stats.session_retrans_packets,
			send_stats.session_retrans_percentage, send_stats.session_total_time, send_stats.session_trans_time,
			send_stats.session_retrans_time);

	status_proxy->SendMessage(INFORMATIONAL, buf);
}

void MVCTPSender::SendSessionStatistics() {
	char buf[512];
	double send_rate = send_stats.session_sent_bytes / 1000.0 / 1000.0 * 8 / send_stats.session_total_time * SEND_RATE_RATIO;
	sprintf(buf, "***** Session Statistics *****\nTotal Sent Bytes: %u\nTotal Sent Packets: %d\nTotal Retrans. Packets: %d\n"
			"Retrans. Percentage: %.4f\nTotal Trans. Time: %.2f sec\nMulticast Trans. Time: %.2f sec\n"
			"Retrans. Time: %.2f sec\nOverall Throughput: %.2f Mbps\n\n", send_stats.session_sent_bytes + send_stats.session_retrans_bytes,
			send_stats.session_sent_packets, send_stats.session_retrans_packets,
			send_stats.session_retrans_percentage, send_stats.session_total_time, send_stats.session_trans_time,
			send_stats.session_retrans_time, send_rate);
	status_proxy->SendMessage(INFORMATIONAL, buf);
}


// Clear session related statistics
void MVCTPSender::ResetSessionStatistics() {
	send_stats.session_sent_packets = 0;
	send_stats.session_sent_bytes = 0;
	send_stats.session_retrans_packets = 0;
	send_stats.session_retrans_bytes = 0;
	send_stats.session_retrans_percentage = 0.0;
	send_stats.session_total_time = 0.0;
	send_stats.session_trans_time = 0.0;
	send_stats.session_retrans_time = 0.0;
}


// After binding the multicast address, the sender also needs to
// start the thread to accept incoming connection requests
int MVCTPSender::JoinGroup(string addr, u_short port) {
	MVCTPComm::JoinGroup(addr, port);
	retrans_tcp_server->Start();
	return 1;
}


void MVCTPSender::ReceiveRetransRequests(map<int, list<NACK_MSG> >* missing_packet_map) {
	int client_sock;
	MvctpRetransMessage retrans_msg;
	int msg_size = sizeof(retrans_msg);

	list<int> sock_list = retrans_tcp_server->GetSocketList();
	while (!sock_list.empty()) {
		int bytes = retrans_tcp_server->SelectReceive(&client_sock, &retrans_msg, msg_size);
		if (bytes < 0)
			cout << "Bad socket: " << client_sock << endl;
		if (retrans_msg.num_requests == 0 || bytes < 0) {
			sock_list.remove(client_sock);
			continue;
		}

		for (int i = 0; i < retrans_msg.num_requests; i++) {
			NACK_MSG packet_info;
			packet_info.seq_num = retrans_msg.seq_numbers[i];
			packet_info.data_len = retrans_msg.data_lens[i];
			if (missing_packet_map->find(client_sock) != missing_packet_map->end()) {
				(*missing_packet_map)[client_sock].push_back(packet_info);
			}
			else {
				list<NACK_MSG> new_list;
				new_list.push_back(packet_info);
				missing_packet_map->insert(pair<int, list<NACK_MSG> >(client_sock, new_list) );
			}
		}
	}
}



// Use selection sort to reorder the sockets according to their retransmission request numbers
// Should use better sorting algorithm is this becomes a bottleneck
void MVCTPSender::SortSocketsByShortestJobs(int* ptr_socks,
			const map<int, list<NACK_MSG> >* missing_packet_map) {
	map<int, int> num_sockets;
	map<int, list<NACK_MSG> >::const_iterator it;
	for (it = missing_packet_map->begin(); it != missing_packet_map->end(); it++) {
		num_sockets[it->first] = it->second.size();
	}

	map<int, int>::iterator num_it;
	int min_sock;
	int min_num;
	int pos = 0;
	while (!num_sockets.empty()) {
		min_num = 0x7fffffff;
		for (num_it = num_sockets.begin(); num_it != num_sockets.end(); num_it++) {
			if (min_num > num_it->second) {
				min_sock = num_it->first;
				min_num = num_it->second;
			}
		}

		ptr_socks[pos++] = min_sock;
		num_sockets.erase(min_sock);
	}

}



void MVCTPSender::SendMemoryData(void* data, size_t length) {
	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers before starting the memory transfer
	struct MvctpTransferMessage msg;
	msg.event_type = MEMORY_TRANSFER_START;
	msg.session_id = cur_session_id;
	msg.data_len = length;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	DoMemoryTransfer(data, length, 0);

	// Record memory data multicast time
	send_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

	// Sleep for a few milliseconds to allow receivers to
	// empty their multicast socket buffers
	//usleep(50000);

	// Send a notification to all receivers to start retransmission
	msg.event_type = MEMORY_TRANSFER_FINISH;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));
	DoMemoryDataRetransmission(data);

	// Record total transfer and retransmission time
	send_stats.session_total_time = GetElapsedSeconds(cpu_counter);
	send_stats.session_retrans_time = send_stats.session_total_time - send_stats.session_trans_time;
	send_stats.session_retrans_percentage = send_stats.session_retrans_packets  * 1.0
								/ (send_stats.session_sent_packets + send_stats.session_retrans_packets);
	// Increase the session id for the next transfer
	cur_session_id++;

	SendSessionStatistics();
}



// TODO: Add scheduling logic
void MVCTPSender::DoMemoryDataRetransmission(void* data) {
	// first: client socket; second: list of NACK_MSG info
	map<int, list<NACK_MSG> >* missing_packet_map = new map<int, list<NACK_MSG> >();
	ReceiveRetransRequests(missing_packet_map);
	cout << "Retransmission requests received." << endl;

	int num_socks = missing_packet_map->size();
	if (num_socks == 0)
		return;

	int* sorted_socks = new int[num_socks];
	SortSocketsByShortestJobs(sorted_socks, missing_packet_map);

	char buffer[MVCTP_PACKET_LEN];
	char* packet_data = buffer + MVCTP_HLEN;
	MvctpHeader* header = (MvctpHeader*) buffer;
	bzero(header, MVCTP_HLEN);
	header->session_id = cur_session_id;

	for (int i = 0; i < num_socks; i++) {
		int sock = sorted_socks[i];

		list<NACK_MSG>* retrans_list = &(*missing_packet_map)[sock];
		cout << "Socket " << sock << " has " << retrans_list->size() << " retransmission requests." << endl;

		list<NACK_MSG>::iterator list_it;
		for (list_it = retrans_list->begin(); list_it != retrans_list->end(); list_it++) {
			header->seq_number = list_it->seq_num;
			header->data_len = list_it->data_len;
			memcpy(packet_data, (char*)data + list_it->seq_num, list_it->data_len);
			retrans_tcp_server->SelectSend(sock, buffer, MVCTP_HLEN + list_it->data_len);

			// Update statistics
			send_stats.total_retrans_packets++;
			send_stats.total_retrans_bytes += header->data_len;
			send_stats.session_retrans_packets++;
			send_stats.session_retrans_bytes += header->data_len;

			//cout << "Retransmission packet sent. Seq No.: " << list_it->seq_num <<
			//	"    Length: " << list_it->data_len << endl;
		}
	}

	delete missing_packet_map;
	delete[] sorted_socks;
}

// Multicast data in a memory buffer, given the specific start sequence number
void MVCTPSender::DoMemoryTransfer(void* data, size_t length, u_int32_t start_seq_num) {
	char buffer[MVCTP_PACKET_LEN];
	char* packet_data = buffer + sizeof(MvctpHeader);
	MvctpHeader* header = (MvctpHeader*) buffer;
	header->session_id = cur_session_id;
	header->src_port = 0;
	header->dest_port = 0;
	header->seq_number = 0;
	header->flags = 0;

	size_t remained_size = length;
	size_t offset = 0;
	while (remained_size > 0) {
		int data_size = remained_size < MVCTP_DATA_LEN ? remained_size
				: MVCTP_DATA_LEN;
		header->seq_number = offset + start_seq_num;
		header->data_len = data_size;
		memcpy(packet_data, (char*)data + offset, data_size);
		if (ptr_multicast_comm->SendData(buffer, MVCTP_HLEN + data_size, 0, NULL) < 0) {
			SysError("MVCTPSender::DoMemoryTransfer()::SendPacket() error");
		}

		remained_size -= data_size;
		offset += data_size;

		// Update statistics
		send_stats.total_sent_packets++;
		send_stats.total_sent_bytes += data_size;
		send_stats.session_sent_packets++;
		send_stats.session_sent_bytes += data_size;
	}
}



void MVCTPSender::SendFile(const char* file_name) {
	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Send a notification to all receivers before starting the memory transfer
	struct MvctpTransferMessage msg;
	msg.session_id = cur_session_id;
	msg.event_type = FILE_TRANSFER_START;
	msg.data_len = file_size;
	strcpy(msg.text, file_name);
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	cout << "Start file transferring..." << endl;
	// Transfer the file using memory mapped I/O
	int fd = open(file_name, O_RDWR);
	if (fd < 0) {
		SysError("MVCTPSender()::SendFile(): File open error!");
	}
	char* buffer;
	off_t offset = 0;
	while (remained_size > 0) {
		int map_size = remained_size < MAX_MAPPED_MEM_SIZE ? remained_size
				: MAX_MAPPED_MEM_SIZE;
		buffer = (char*) mmap(0, map_size, PROT_READ, MAP_FILE | MAP_SHARED, fd,
				offset);
		if (buffer == MAP_FAILED) {
			SysError("MVCTPSender::SendFile()::mmap() error");
		}

		DoMemoryTransfer(buffer, map_size, offset);

		munmap(buffer, map_size);

		offset += map_size;
		remained_size -= map_size;
	}
	cout << "File transfer finished. Start retransmission..." << endl;
	// Record memory data multicast time
	send_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

	// Sleep for a few milliseconds to allow receivers to
	// empty their multicast socket buffers
	//usleep(200000);

	// TODO: remove this in real implementation
	// For test ONLY: clear system cache before doing retransmission
	//system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers to start retransmission
	msg.event_type = FILE_TRANSFER_FINISH;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	DoFileRetransmission(fd);

	close(fd);

	// Record total transfer and retransmission time
	send_stats.session_retrans_time = GetElapsedSeconds(cpu_counter); //send_stats.session_total_time - send_stats.session_trans_time;
	send_stats.session_total_time = send_stats.session_trans_time + send_stats.session_retrans_time; //GetElapsedSeconds(cpu_counter);
	send_stats.session_retrans_percentage = send_stats.session_retrans_packets  * 1.0
								/ (send_stats.session_sent_packets + send_stats.session_retrans_packets);
	// Increase the session id for the next transfer
	cur_session_id++;

	SendSessionStatistics();
}



///
void MVCTPSender::DoFileRetransmission(int fd) {
	// first: client socket; second: list of NACK_MSG info
	map<int, list<NACK_MSG> >* missing_packet_map = new map<int, list<NACK_MSG> >();
	ReceiveRetransRequests(missing_packet_map);

	int num_socks = missing_packet_map->size();
	if (num_socks == 0)
			return;

	int* sorted_socks = new int[num_socks];
	SortSocketsByShortestJobs(sorted_socks, missing_packet_map);

	list<MvctpRetransBuffer *> retrans_cache_list;
	MvctpRetransBuffer * ptr_cache = new MvctpRetransBuffer();
	retrans_cache_list.push_back(ptr_cache);

	MvctpHeader* header;
	char* packet_data;
	// first: sequence number of a packet; second: pointer to the packet in the cache
	map<uint32_t, char*>* packet_map = new map<uint32_t, char*>();
	map<uint32_t, char*>::iterator packet_map_it;

	for (int i = 0; i < num_socks; i++) {
		int sock = sorted_socks[i];

		list<NACK_MSG>* retrans_list = &(*missing_packet_map)[sock];
		cout << "Socket " << sock << " has " << retrans_list->size() << " retransmission requests." << endl;

		list<NACK_MSG>::iterator list_it;
		for (list_it = retrans_list->begin(); list_it != retrans_list->end(); list_it++) {
			// First check if the packet is already in the cache
			if ( (packet_map_it = packet_map->find(list_it->seq_num)) != packet_map->end()) {
				retrans_tcp_server->SelectSend(sock, packet_map_it->second, MVCTP_HLEN + list_it->data_len);
				continue;
			}

			// If not, read the packet in from the disk file
			if (ptr_cache->cur_pos == ptr_cache->end_pos) {
				ptr_cache = new MvctpRetransBuffer();
				retrans_cache_list.push_back(ptr_cache);
			}

			header = (MvctpHeader *)ptr_cache->cur_pos;
			header->session_id = cur_session_id;
			header->seq_number = list_it->seq_num;
			header->data_len = list_it->data_len;

			packet_data = ptr_cache->cur_pos + MVCTP_HLEN;
			lseek(fd, list_it->seq_num, SEEK_SET);
			read(fd, packet_data, list_it->data_len);
			retrans_tcp_server->SelectSend(sock, ptr_cache->cur_pos, MVCTP_HLEN + list_it->data_len);

			(*packet_map)[list_it->seq_num] = ptr_cache->cur_pos;
			ptr_cache->cur_pos += MVCTP_PACKET_LEN;

			// Update statistics
			send_stats.total_retrans_packets++;
			send_stats.total_retrans_bytes += header->data_len;
			send_stats.session_retrans_packets++;
			send_stats.session_retrans_bytes += header->data_len;
		}
	}

	// Clean up
	delete missing_packet_map;
	delete[] sorted_socks;
	list<MvctpRetransBuffer *>::iterator it;
	for (it = retrans_cache_list.begin(); it != retrans_cache_list.end(); it++) {
		delete (*it);
	}
	delete packet_map;
}




//=========== Functions related to data transfer using TCP ================
void MVCTPSender::TcpSendMemoryData(void* data, size_t length) {
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers before starting the memory transfer
	struct MvctpTransferMessage msg;
	msg.event_type = TCP_MEMORY_TRANSFER_START;
	msg.session_id = cur_session_id;
	msg.data_len = length;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	retrans_tcp_server->SendToAll(data, length);

	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = length / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;
	char str[256];
	sprintf(str, "***** TCP Send Info *****\nTotal transfer time: %.2f\nThroughput: %.2f\n", trans_time, send_rate);
	status_proxy->SendMessage(EXP_RESULT_REPORT, str);


	cur_session_id++;
}


void MVCTPSender::TcpSendFile(const char* file_name) {
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Send a notification to all receivers before starting the memory transfer
	struct MvctpTransferMessage msg;
	msg.session_id = cur_session_id;
	msg.event_type = TCP_FILE_TRANSFER_START;
	msg.data_len = file_size;
	strcpy(msg.text, file_name);
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	cout << "Start file transferring..." << endl;
	// Transfer the file using memory mapped I/O
	int fd = open(file_name, O_RDWR);
	char* buffer = (char* )malloc(MAX_MAPPED_MEM_SIZE);
	off_t offset = 0;
	while (remained_size > 0) {
		int map_size = remained_size < MAX_MAPPED_MEM_SIZE ? remained_size
				: MAX_MAPPED_MEM_SIZE;
		read(fd, buffer, map_size);

		retrans_tcp_server->SendToAll(buffer, map_size);

		offset += map_size;
		remained_size -= map_size;
	}
	close(fd);
	free(buffer);

	cout << "File transfer finished. Start retransmission..." << endl;
	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = file_size / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;
	char str[256];
	sprintf(str, "***** TCP Send Info *****\nTotal transfer time: %.2f\nThroughput: %.2f\n", trans_time, send_rate);
	status_proxy->SendMessage(INFORMATIONAL, str);


	cur_session_id++;
}

