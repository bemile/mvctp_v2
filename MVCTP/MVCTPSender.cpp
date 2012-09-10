/*
 * MVCTPSender.cpp
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#include "MVCTPSender.h"


MVCTPSender::MVCTPSender(int buf_size) : MVCTPComm() {
	retrans_tcp_server = new TcpServer(BUFFER_TCP_SEND_PORT, this);
	cur_session_id = 0;
	bzero(&send_stats, sizeof(send_stats));

	// Initially set the maximum number of retransmission buffer to be 32 * 8 MB = 256 MB
	max_num_retrans_buffs = 32;
	// Initially set the send rate limit to 10Gpbs (effectively no limit)
	rate_shaper.SetRate(10000 * 1000000.0);

	// set default retransmission scheme
	retrans_scheme = RETRANS_SERIAL;
	num_retrans_threads = 1;

	//event_queue_manager = new MvctpEventQueueManager();
}

MVCTPSender::~MVCTPSender() {
	delete retrans_tcp_server;
	//delete event_queue_manager;

	map<int, pthread_t*>::iterator it = retrans_thread_map.begin();
	for (; it != retrans_thread_map.end(); it++) {
		delete (it->second);
	}
}

void MVCTPSender::SetSendRate(int num_mbps) {
	send_rate_in_mbps = num_mbps;
	rate_shaper.SetRate(num_mbps * 1000000.0);
}

int	MVCTPSender::GetSendRate() {
	return send_rate_in_mbps;
}


void MVCTPSender::SetRetransmissionBufferSize(int size_mb) {
	max_num_retrans_buffs = size_mb / 8;
	if (max_num_retrans_buffs == 0)
		max_num_retrans_buffs = 1;
}


void MVCTPSender::SetRetransmissionScheme(int scheme) {
	retrans_scheme = scheme;
}

void MVCTPSender::SetNumRetransmissionThreads(int num) {
	num_retrans_threads = num;
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

	status_proxy->SendMessageLocal(INFORMATIONAL, buf);
}

void MVCTPSender::SendSessionStatistics() {
	char buf[512];
	double send_rate = send_stats.session_sent_bytes / 1000.0 / 1000.0 * 8 / send_stats.session_total_time * SEND_RATE_RATIO;
	sprintf(buf, "***** Session Statistics *****\nTotal Sent Bytes: %u\nTotal Sent Packets: %d\nTotal Retrans. Packets: %d\n"
			"Retrans. Percentage: %.4f\nTotal Trans. Time: %.2f sec\nMulticast Trans. Time: %.2f sec\n"
			"Retrans. Time: %.2f sec\nOverall Throughput: %.2f Mbps\nAvg. CPU Usage: %d\%\n\n", send_stats.session_sent_bytes + send_stats.session_retrans_bytes,
			send_stats.session_sent_packets, send_stats.session_retrans_packets,
			send_stats.session_retrans_percentage, send_stats.session_total_time, send_stats.session_trans_time,
			send_stats.session_retrans_time, send_rate, send_stats.cpu_usage);
	status_proxy->SendMessageLocal(INFORMATIONAL, buf);
}


// Collect experiment results for file transfer from all receivers
void MVCTPSender::CollectExpResults() {
	// Send requests to all receivers

	char msg_packet[500];
	MvctpHeader* header = (MvctpHeader*)msg_packet;
	header->session_id = cur_session_id;
	header->seq_number = 0;
	header->data_len = sizeof(MvctpSenderMessage);
	header->flags = MVCTP_SENDER_MSG_EXP;

	MvctpSenderMessage* msg = (MvctpSenderMessage*)(msg_packet + MVCTP_HLEN);
	msg->msg_type = COLLECT_STATISTICS;
	msg->session_id = cur_session_id;
	msg->data_len = 0;
	retrans_tcp_server->SendToAll(&msg_packet, MVCTP_HLEN + sizeof(MvctpSenderMessage));

	char buf[512];
	int client_sock;
	list<int> sock_list = retrans_tcp_server->GetSocketList();
	int str_len;
	while (!sock_list.empty()) {
		int bytes = retrans_tcp_server->SelectReceive(&client_sock, &str_len, sizeof(str_len));
		if (bytes <= 0) {
			sock_list.remove(client_sock);
			continue;
		}
		else {
			int bytes = retrans_tcp_server->Receive(client_sock, buf, str_len);
			if (bytes >= 0) {
				buf[bytes] = '\0';
				cout << "I received exp report from socket " << client_sock << endl;
				status_proxy->SendMessageLocal(EXP_RESULT_REPORT, buf);
			}
			sock_list.remove(client_sock);
		}
	}
}


void MVCTPSender::ExecuteCommandOnReceivers(string command, int receiver_start, int receiver_end) {
	struct MvctpSenderMessage msg;
	msg.msg_type = EXECUTE_COMMAND;
	msg.session_id = cur_session_id;
	msg.data_len = command.length();
	memcpy(msg.text, command.c_str(), msg.data_len);
	msg.text[msg.data_len] = '\0';

	list<int> sock_list = retrans_tcp_server->GetSocketList();
	list<int>::iterator it;
	int sock_id = 0;
	for (it = sock_list.begin(); it != sock_list.end(); it++) {
		sock_id++;
		if (sock_id >= receiver_start && sock_id <= receiver_end)
			retrans_tcp_server->SelectSend(*it, &msg, sizeof(msg));
		else if (sock_id > receiver_end)
			break;
	}
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

int	MVCTPSender::RestartTcpServer() {
	delete retrans_tcp_server;
	retrans_tcp_server = new TcpServer(BUFFER_TCP_SEND_PORT, this);
	retrans_tcp_server->Start();
	return 1;
}


int	MVCTPSender::GetNumReceivers() {
	return retrans_tcp_server->GetSocketList().size();
}


void MVCTPSender::RemoveSlowNodes() {
	char buf[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)buf;
	header->session_id = cur_session_id;
	header->data_len = sizeof(MvctpSenderMessage);
	header->flags = MVCTP_SENDER_MSG_EXP;

	MvctpSenderMessage* sender_msg = (MvctpSenderMessage*)(buf + MVCTP_HLEN);
	sender_msg->msg_type = SPEED_TEST;
	sender_msg->session_id = cur_session_id;
	sender_msg->data_len = 0;
	retrans_tcp_server->SendToAll(buf, MVCTP_HLEN + sizeof(MvctpSenderMessage));
}



void MVCTPSender::SendMemoryData(void* data, size_t length) {
	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers before starting the memory transfer
	struct MvctpSenderMessage msg;
	msg.msg_type = MEMORY_TRANSFER_START;
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
	msg.msg_type = MEMORY_TRANSFER_FINISH;
	cout << "Memory to memory transfer finished." << endl;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));
	cout << "Start retransmission..." << endl;
	DoMemoryDataRetransmission(data);

	// collect experiment results from receivers
	CollectExpResults();

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
	ReceiveRetransRequestsSerial(missing_packet_map);
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
		uint data_size = remained_size < MVCTP_DATA_LEN ? remained_size
				: MVCTP_DATA_LEN;
		header->seq_number = offset + start_seq_num;
		header->data_len = data_size;
		memcpy(packet_data, (char*)data + offset, data_size);

		//Get tokens from the rate controller
		rate_shaper.RetrieveTokens(22 + MVCTP_HLEN + data_size);
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



uint MVCTPSender::SendFile(const char* file_name) {
	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Create metadata for the new file
	FileMessageMetadata* meta = new FileMessageMetadata();
	meta->msg_id = cur_session_id;
	meta->msg_length = file_size;
	meta->file_name = file_name;
	meta->start_time_count = cpu_counter;

	// add all current receiver sockets into the unfinished receiver map
	list<int> sock_list = retrans_tcp_server->GetSocketList();
	for (list<int>::iterator it = sock_list.begin(); it != sock_list.end(); it++) {
		meta->unfinished_recvers[*it] = false;
	}
	metadata.AddMessageMetadata(meta);

	/*multicast_task_info.type = DISK_TO_DISK_TRANSFER;
	strcpy(multicast_task_info.file_name, file_name);

	map<int, bool>::iterator it;
		for (it = retrans_finish_map.begin(); it != retrans_finish_map.end(); it++) {
			it->second = false;
	}*/




	// Send the BOF message to all receivers before starting the file transfer
	char msg_packet[500];
	MvctpHeader* header = (MvctpHeader*)msg_packet;
	header->session_id = cur_session_id;
	header->seq_number = 0;
	header->data_len = sizeof(MvctpSenderMessage);
	header->flags = MVCTP_BOF;

	MvctpSenderMessage* msg = (MvctpSenderMessage*)(msg_packet + MVCTP_HLEN);
	msg->session_id = cur_session_id;
	msg->msg_type = FILE_TRANSFER_START;
	msg->data_len = file_size;
	strcpy(msg->text, file_name);

	//retrans_tcp_server->SendToAll(&msg_packet, MVCTP_HLEN + sizeof(MvctpSenderMessage));
	if (ptr_multicast_comm->SendData(&msg_packet, MVCTP_HLEN + sizeof(MvctpSenderMessage), 0, NULL) < 0) {
		SysError("MVCTPSender::SendFile()::SendData() error");
	}


	//PerformanceCounter cpu_info(100);
	//cpu_info.SetCPUFlag(true);
	//cpu_info.Start();

	//cout << "Start file transferring..." << endl;
	// Transfer the file using memory mapped I/O
	int fd = open(file_name, O_RDWR);
	if (fd < 0) {
		SysError("MVCTPSender()::SendFile(): File open error!");
	}
	char* buffer;
	off_t offset = 0;
	while (remained_size > 0) {
		uint map_size = remained_size < MAX_MAPPED_MEM_SIZE ? remained_size
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

	// Record memory data multicast time
	send_stats.session_trans_time = GetElapsedSeconds(cpu_counter);
	meta->stats.session_trans_time = send_stats.session_trans_time;

	// TODO: remove this in real implementation
	// For test ONLY: clear system cache before doing retransmission
	// system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers to start retransmission
	header->flags = MVCTP_EOF;
	header->data_len = 0;
	//retrans_tcp_server->SendToAll(header, MVCTP_HLEN);
	if (ptr_multicast_comm->SendData(header, MVCTP_HLEN, 0, NULL) < 0) {
		SysError("MVCTPSender::SendFile()::SendData() error");
	}

	// wait for all retransmission to finish
//	bool is_retrans_finished = false;
//	while (!is_retrans_finished) {
//		is_retrans_finished = true;
//		for (it = retrans_finish_map.begin(); it != retrans_finish_map.end(); it++) {
//			if (!it->second) {
//				is_retrans_finished = false;
//				usleep(10000);
//				break;
//			}
//		}
//	}

	close(fd);
	//cout << "File multicast finished." << endl;

	//cpu_info.Stop();
	//cout << "Retransmission to all receivers finished." << endl;

	// collect experiment results from receivers
	//CollectExpResults();

	// Record total transfer and retransmission time
	//send_stats.cpu_usage = cpu_info.GetAverageCpuUsage();
	//send_stats.session_retrans_time = GetElapsedSeconds(cpu_counter); //send_stats.session_total_time - send_stats.session_trans_time;
	//send_stats.session_total_time = send_stats.session_trans_time + send_stats.session_retrans_time; //GetElapsedSeconds(cpu_counter);
	//send_stats.session_retrans_percentage = send_stats.session_retrans_packets  * 1.0
	//							/ (send_stats.session_sent_packets + send_stats.session_retrans_packets);


	// Increase the session id for the next transfer
	cur_session_id++;

	//SendSessionStatistics();
	return meta->msg_id;
}


bool MVCTPSender::IsTransferFinished(uint msg_id) {
	return metadata.IsTransferFinished(msg_id);
}


void MVCTPSender::SendFileBufferedIO(const char* file_name) {
	PerformanceCounter cpu_info(50);
	cpu_info.SetCPUFlag(true);
	cpu_info.Start();

	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Send a notification to all receivers before starting the memory transfer
	struct MvctpSenderMessage msg;
	msg.session_id = cur_session_id;
	msg.msg_type = FILE_TRANSFER_START;
	msg.data_len = file_size;
	strcpy(msg.text, file_name);
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	//cout << "Start file transferring..." << endl;
	// Transfer the file using memory mapped I/O
	int fd = open(file_name, O_RDWR);
	if (fd < 0) {
		SysError("MVCTPSender()::SendFile(): File open error!");
	}
	char* buffer = (char *)malloc(MVCTP_DATA_LEN);
	off_t offset = 0;
	while (remained_size > 0) {
		uint read_size = remained_size < MVCTP_DATA_LEN ? remained_size
				: MVCTP_DATA_LEN;
		ssize_t res = read(fd, buffer, read_size);
		if (res < 0) {
			SysError("MVCTPSender::SendFileBufferedIO()::read() error");
		}

		DoMemoryTransfer(buffer, read_size, offset);
		offset += read_size;
		remained_size -= read_size;
	}
	free(buffer);

	// Record memory data multicast time
	send_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers to start retransmission
	msg.msg_type = FILE_TRANSFER_FINISH;
	retrans_tcp_server->SendToAll(&msg, sizeof(msg));

	//cout << "File transfer finished. Start retransmission..." << endl;

	if (retrans_scheme == RETRANS_SERIAL)
		DoFileRetransmissionSerial(fd);
	else if (retrans_scheme == RETRANS_SERIAL_RR)
		DoFileRetransmissionSerialRR(fd);
	else if (retrans_scheme == RETRANS_PARALLEL)
		DoFileRetransmissionParallel(file_name);


	close(fd);

	// collect experiment results from receivers
	CollectExpResults();

	// Record total transfer and retransmission time
	send_stats.session_retrans_time = GetElapsedSeconds(cpu_counter); //send_stats.session_total_time - send_stats.session_trans_time;
	send_stats.session_total_time = send_stats.session_trans_time + send_stats.session_retrans_time; //GetElapsedSeconds(cpu_counter);
	send_stats.session_retrans_percentage = send_stats.session_retrans_packets  * 1.0
									/ (send_stats.session_sent_packets + send_stats.session_retrans_packets);
	// Increase the session id for the next transfer
	cur_session_id++;
	SendSessionStatistics();

	cpu_info.Stop();
}



//==================== Parallel Retransmission Threads Management Functions ===================
void MVCTPSender::StartNewRetransThread(int sock_fd) {
	pthread_t * t = new pthread_t();

	retrans_thread_map[sock_fd] = t;
	retrans_switch_map[sock_fd] = true;
	retrans_finish_map[sock_fd] = false;

	StartRetransThreadInfo* retx_thread_info = new StartRetransThreadInfo();
	retx_thread_info->sender_ptr = this;
	retx_thread_info->sock_fd = sock_fd;
	thread_info_map[sock_fd] = retx_thread_info;
	pthread_create(t, NULL, &MVCTPSender::StartRetransThread, retx_thread_info);
}

void* MVCTPSender::StartRetransThread(void* ptr) {
	StartRetransThreadInfo* info = (StartRetransThreadInfo*)ptr;
	info->sender_ptr->RunRetransThread(info->sock_fd);
	return NULL;
}


// The execution function for the retransmission thread
void MVCTPSender::RunRetransThread(int sock) {
	int sock_fd = sock;
	cout << "Retx thread created for socket " << sock_fd << endl;

	// Opened file descriptor map for the retransmission.  Format: <msg_id, file_descriptor>
	map<uint, int> retrans_fd_map;

	char recv_buf[MVCTP_PACKET_LEN];
	MvctpHeader* recv_header = (MvctpHeader*)recv_buf;
	char* recv_packet_data = recv_buf + MVCTP_HLEN;
	MvctpRetransRequest* retx_request = (MvctpRetransRequest*)recv_packet_data;

	char send_buf[MVCTP_PACKET_LEN];
	MvctpHeader* send_header = (MvctpHeader*)send_buf;
	char* send_packet_data = send_buf + MVCTP_HLEN;

	while (true) {
		if (retrans_tcp_server->Receive(sock_fd, recv_header, sizeof(MvctpHeader)) <= 0) {
			break;
		}

		// Handle a retransmission request
		if (recv_header->flags & MVCTP_RETRANS_REQ) {
			if (retrans_tcp_server->Receive(sock_fd, retx_request, sizeof(MvctpRetransRequest)) < 0) {
				break;
			}

			MessageMetadata* meta = metadata.GetMetadata(recv_header->session_id);
			if (meta->is_disk_file) {	// is disk file transfer
				FileMessageMetadata* file_meta = (FileMessageMetadata*)meta;

				// get the file descriptor to read data from the file
				int fd;
				map<uint, int>::iterator it = retrans_fd_map.find(recv_header->session_id);
				if (it != retrans_fd_map.end()) {
					fd = it->second;
				}
				else {
					fd = open(file_meta->file_name.c_str(), O_RDONLY);
					if (fd < 0)
						SysError("MVCTPSender::RunRetransThread() file open error");
					else
						retrans_fd_map[recv_header->session_id] = fd;
				}


				// send the missing blocks to the receiver
				lseek(fd, retx_request->seq_num, SEEK_SET);
				size_t remained_size = retx_request->data_len;
				size_t curr_pos = retx_request->seq_num;
				send_header->session_id = recv_header->session_id;
				send_header->flags = MVCTP_RETRANS_DATA;
				while (remained_size > 0) {
					size_t data_length =
							remained_size > MVCTP_DATA_LEN ? MVCTP_DATA_LEN
									: remained_size;
					send_header->seq_number = curr_pos;
					send_header->data_len = data_length;

					read(fd, send_packet_data, send_header->data_len);
					retrans_tcp_server->SelectSend(sock_fd, send_buf,
							MVCTP_HLEN + send_header->data_len);

					curr_pos += data_length;
					remained_size -= data_length;

					// Update statistics
					send_stats.total_retrans_packets++;
					send_stats.total_retrans_bytes += send_header->data_len;
					//file_meta->stats.session_retrans_packets++;
					//file_meta->stats.session_retrans_bytes += header->data_len;
				}
			}
			else {	// is memory data transfer

			}
		}
		else if (recv_header->flags & MVCTP_RETRANS_END) {
			if (retrans_tcp_server->Receive(sock_fd, retx_request, recv_header->data_len) < 0) {
				break;
			}

			MessageMetadata* meta = metadata.GetMetadata(recv_header->session_id);
			map<uint, int>::iterator it = retrans_fd_map.find(recv_header->session_id);
			if (it != retrans_fd_map.end()) {
				close(it->second);
				retrans_fd_map.erase(it);
			}

			// send back the retransmission end message to the receiver
			send_header->session_id = recv_header->session_id;
			send_header->seq_number = 0;
			send_header->data_len = 0;
			send_header->flags = MVCTP_RETRANS_END;
			retrans_tcp_server->SelectSend(sock_fd, send_header, MVCTP_HLEN);

			// mark the completion of retransmission to one receiver
			metadata.RemoveFinishedReceiver(recv_header->session_id, sock_fd);
			/*if (metadata.IsTransferFinished(recv_header->session_id)) {
				char buf[200];
				sprintf(buf, "File transfer for message %d finished. Total transfer time: %.2f seconds",
								recv_header->session_id, GetElapsedSeconds(meta->start_time_count));
				status_proxy->SendMessageLocal(INFORMATIONAL, buf);
			}*/
			//cout << "Receive finishing mark request from sock " << sock_fd << endl;
		}
	}

	cout << "Retransmission thread exited for socket " << sock_fd  << endl;

}





//=========== Functions related to TCP file transfer ================
void MVCTPSender::TcpSendMemoryData(void* data, size_t length) {
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	// Send a notification to all receivers before starting the memory transfer
	char msg_packet[500];
	MvctpHeader* header = (MvctpHeader*)msg_packet;
	header->session_id = cur_session_id;
	header->seq_number = 0;
	header->data_len = sizeof(MvctpSenderMessage);
	header->flags = MVCTP_SENDER_MSG_EXP;

	MvctpSenderMessage* msg = (MvctpSenderMessage*)(msg_packet + MVCTP_HLEN);
	msg->msg_type = TCP_MEMORY_TRANSFER_START;
	msg->session_id = cur_session_id;
	msg->data_len = length;
	retrans_tcp_server->SendToAll(&msg_packet, MVCTP_HLEN + sizeof(MvctpSenderMessage));

	retrans_tcp_server->SendToAll(data, length);

	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = length / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;
	char str[256];
	sprintf(str, "***** TCP Send Info *****\nTotal transfer time: %.2f\nThroughput: %.2f\n", trans_time, send_rate);
	status_proxy->SendMessageLocal(EXP_RESULT_REPORT, str);


	cur_session_id++;
}


struct TcpThreadInfo {
	MVCTPSender* ptr;
	int	 sock_fd;
	char file_name[256];
};


void MVCTPSender::TcpSendFile(const char* file_name) {
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Send a notification to all receivers before starting the memory transfer
	char msg_packet[500];
	MvctpHeader* header = (MvctpHeader*)msg_packet;
	header->session_id = cur_session_id;
	header->seq_number = 0;
	header->data_len = sizeof(MvctpSenderMessage);
	header->flags = MVCTP_SENDER_MSG_EXP;

	MvctpSenderMessage* msg = (MvctpSenderMessage*)(msg_packet + MVCTP_HLEN);
	msg->msg_type = TCP_FILE_TRANSFER_START;
	msg->session_id = cur_session_id;
	msg->data_len = file_size;
	strcpy(msg->text, file_name);
	retrans_tcp_server->SendToAll(&msg_packet, MVCTP_HLEN + sizeof(MvctpSenderMessage));


	PerformanceCounter cpu_info(100);
	cpu_info.SetCPUFlag(true);
	cpu_info.Start();


	cout << "Start TCP file transferring..." << endl;
	list<int> sock_list = retrans_tcp_server->GetSocketList();
	list<TcpThreadInfo*> thread_info_list;
	list<pthread_t*> thread_list;
	int file_name_len = strlen(file_name);
	for (list<int>::iterator it = sock_list.begin(); it != sock_list.end(); it++) {
		TcpThreadInfo* info = new TcpThreadInfo();
		info->ptr = this;
		info->sock_fd = *it;
		memcpy(info->file_name, file_name, file_name_len);
		thread_info_list.push_back(info);

		pthread_t * t = new pthread_t();
		pthread_create(t, NULL, &MVCTPSender::StartTcpSendThread, info);
		thread_list.push_back(t);
	}

	for (list<pthread_t*>::iterator it = thread_list.begin(); it != thread_list.end(); it++) {
		pthread_join(**it, NULL);
	}

	for (list<pthread_t*>::iterator it = thread_list.begin(); it != thread_list.end(); it++) {
		delete (*it);
	}

	for (list<TcpThreadInfo*>::iterator it = thread_info_list.begin(); it != thread_info_list.end(); it++) {
		delete (*it);
	}

	cpu_info.Stop();
	int cpu_usage = cpu_info.GetAverageCpuUsage();

	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = file_size / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;
	char str[256];
	sprintf(str, "***** TCP Send Info *****\nTotal transfer time: %.2f seconds\nThroughput: %.2f Mbps\nAvg. CPU Usage: %d\%\n",
					trans_time, send_rate, cpu_usage);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);


	cur_session_id++;
}


// Thread functions for TCP transfer
void* MVCTPSender::StartTcpSendThread(void* ptr) {
	TcpThreadInfo* info_ptr = (TcpThreadInfo*)ptr;
	info_ptr->ptr->RunTcpSendThread(info_ptr->file_name, info_ptr->sock_fd);
}

void MVCTPSender::RunTcpSendThread(const char* file_name, int sock_fd) {
	cout << "Start sending file " << file_name << " to socket " << sock_fd << endl;
	struct stat file_status;
	stat(file_name, &file_status);
	ulong file_size = file_status.st_size;
	ulong remained_size = file_size;

	// Transfer the file using memory mapped I/O
	int fd = open(file_name, O_RDWR);
	char* buffer = (char* )malloc(MAX_MAPPED_MEM_SIZE);
	off_t offset = 0;
	while (remained_size > 0) {
		int map_size = remained_size < MAX_MAPPED_MEM_SIZE ? remained_size
				: MAX_MAPPED_MEM_SIZE;
		read(fd, buffer, map_size);

		//retrans_tcp_server->SendToAll(buffer, map_size);
		retrans_tcp_server->SelectSend(sock_fd, buffer, map_size);
		offset += map_size;
		remained_size -= map_size;
	}
	close(fd);
	free(buffer);
}





///===============================  Obseleted Functions =================================
void MVCTPSender::DoFileRetransmissionSerial(int fd) {
	// first: client socket; second: list of NACK_MSG info
	map<int, list<NACK_MSG> >* missing_packet_map = new map<int, list<NACK_MSG> >();
	ReceiveRetransRequestsSerial(missing_packet_map);

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

		// double start_time = GetElapsedSeconds(cpu_counter);
		list<NACK_MSG>::iterator list_it;
		for (list_it = retrans_list->begin(); list_it != retrans_list->end(); list_it++) {
			// First check if the packet is already in the cache
			if ( (packet_map_it = packet_map->find(list_it->seq_num)) != packet_map->end()) {
				retrans_tcp_server->SelectSend(sock, packet_map_it->second, MVCTP_HLEN + list_it->data_len);
				continue;
			}

			// If not, read the packet in from the disk file
			if (ptr_cache->cur_pos == ptr_cache->end_pos) {
				if (retrans_cache_list.size() > max_num_retrans_buffs) {
					list<MvctpRetransBuffer *>::iterator it;
					for (it = retrans_cache_list.begin(); it != retrans_cache_list.end(); it++) {
						delete (*it);
					}

					retrans_cache_list.clear();
					packet_map->clear();
				}

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

//		double read_finish_time = GetElapsedSeconds(cpu_counter);
//		cout << "Time to read all retransmission data: " << (read_finish_time - start_time) << " Seconds" << endl;
//
//
//		for (list_it = retrans_list->begin(); list_it != retrans_list->end(); list_it++) {
//			if ( (packet_map_it = packet_map->find(list_it->seq_num)) != packet_map->end()) {
//				retrans_tcp_server->SelectSend(sock, packet_map_it->second, MVCTP_HLEN + list_it->data_len);
//			}
//		}
//		double send_finish_time = GetElapsedSeconds(cpu_counter);
//		cout << "Time to send all retransmission data: " << (send_finish_time - read_finish_time) << " Seconds" << endl;
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


void MVCTPSender::ReceiveRetransRequestsSerial(map<int, list<NACK_MSG> >* missing_packet_map) {
	int client_sock;
	MvctpRetransMessage retrans_msg;
	int msg_size = sizeof(retrans_msg);

	list<int> sock_list = retrans_tcp_server->GetSocketList();
	while (!sock_list.empty()) {
		int bytes = retrans_tcp_server->SelectReceive(&client_sock, &retrans_msg, msg_size);
		if (retrans_msg.num_requests == 0 || bytes <= 0) {
			sock_list.remove(client_sock);
			continue;
		}

		for (int i = 0; i < retrans_msg.num_requests; i++) {
			NACK_MSG packet_info;
			packet_info.seq_num = retrans_msg.seq_numbers[i];
			packet_info.data_len = retrans_msg.data_lens[i];
			(*missing_packet_map)[client_sock].push_back(packet_info);
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


//
void MVCTPSender::DoFileRetransmissionSerialRR(int fd) {
	// first: sequence number
	// second: list of sockets
	map<NACK_MSG, list<int> >* missing_packet_map = new map<NACK_MSG, list<int> >();
	ReceiveRetransRequestsSerialRR(missing_packet_map);


	char packet_buf[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)packet_buf;
	char* packet_data = packet_buf + MVCTP_HLEN;

	map<NACK_MSG, list<int> >::iterator it;
	for (it = missing_packet_map->begin(); it != missing_packet_map->end(); it++) {
		const NACK_MSG& nack_msg = it->first;
		list<int>& sock_list = it->second;

		list<int>::iterator sock_it;
		for (sock_it = sock_list.begin(); sock_it != sock_list.end(); sock_it++) {
			header->session_id = cur_session_id;
			header->seq_number = nack_msg.seq_num;
			header->data_len = nack_msg.data_len;

			lseek(fd, header->seq_number, SEEK_SET);
			read(fd, packet_data, header->data_len);
			retrans_tcp_server->SelectSend(*sock_it, packet_buf, MVCTP_HLEN + header->data_len);

			// Update statistics
			send_stats.total_retrans_packets++;
			send_stats.total_retrans_bytes += header->data_len;
			send_stats.session_retrans_packets++;
			send_stats.session_retrans_bytes += header->data_len;
		}
	}

	delete missing_packet_map;
}


void MVCTPSender::ReceiveRetransRequestsSerialRR(map <NACK_MSG, list<int> >* missing_packet_map) {
	int client_sock;
	MvctpRetransMessage retrans_msg;
	int msg_size = sizeof(retrans_msg);

	list<int> sock_list = retrans_tcp_server->GetSocketList();
	while (!sock_list.empty()) {
		int bytes = retrans_tcp_server->SelectReceive(&client_sock, &retrans_msg, msg_size);
		if (retrans_msg.num_requests == 0 || bytes <= 0) {
			sock_list.remove(client_sock);
			continue;
		}

		for (int i = 0; i < retrans_msg.num_requests; i++) {
			NACK_MSG packet_info;
			packet_info.seq_num = retrans_msg.seq_numbers[i];
			packet_info.data_len = retrans_msg.data_lens[i];
			(*missing_packet_map)[packet_info].push_back(client_sock);
		}
	}
}


struct RetransThreadStartInfo {
	const char*	file_name;
	MVCTPSender* sender_ptr;
	map<int, list<NACK_MSG> >* missing_packet_map;

	RetransThreadStartInfo(const char* fname): file_name(fname) {}
};

//
void MVCTPSender::DoFileRetransmissionParallel(const char* file_name) {
	// first: client socket; second: list of NACK_MSG info
	map<int, list<NACK_MSG> >* missing_packet_map = new map<int, list<NACK_MSG> > ();
	ReceiveRetransRequestsSerial(missing_packet_map);

	int num_socks = missing_packet_map->size();
	if (num_socks == 0)
		return;

	int* sorted_socks = new int[num_socks];
	SortSocketsByShortestJobs(sorted_socks, missing_packet_map);

	retrans_sock_list.clear();
	for (int i = 0; i < num_socks; i++) {
		retrans_sock_list.push_back(sorted_socks[i]);
	}


	RetransThreadStartInfo start_info(file_name);
	start_info.sender_ptr = this;
	start_info.missing_packet_map = missing_packet_map;
	//start_info.file_name = file_name;

	pthread_mutex_init(&sock_list_mutex, NULL);
	pthread_t* retrans_threads = new pthread_t[num_retrans_threads];
	for (int i = 0; i < num_retrans_threads; i++) {
		pthread_create(&retrans_threads[i], NULL, &MVCTPSender::StartRetransmissionThread, &start_info);
	}

	for (int i = 0; i < num_retrans_threads; i++) {
		pthread_join(retrans_threads[i], NULL);
	}

	delete missing_packet_map;
	delete[] retrans_threads;
	pthread_mutex_destroy(&sock_list_mutex);
}


void* MVCTPSender::StartRetransmissionThread(void* ptr) {
	RetransThreadStartInfo* start_info = (RetransThreadStartInfo*)ptr;
	start_info->sender_ptr->RunRetransmissionThread(start_info->file_name, start_info->missing_packet_map);
	return NULL;
}


void MVCTPSender::RunRetransmissionThread(const char* file_name, map<int, list<NACK_MSG> >* missing_packet_map) {
	int fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		SysError("MVCTPSender()::RunRetransmissionThread(): File open error!");
	}

	char packet_buf[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)packet_buf;
	char* packet_data = packet_buf + MVCTP_HLEN;

	while (true) {
		int sock;
		list<NACK_MSG>* msg_list;
		pthread_mutex_lock(&sock_list_mutex);
		if (retrans_sock_list.size() == 0) {
			pthread_mutex_unlock(&sock_list_mutex);
			return;
		}
		else {
			sock = retrans_sock_list.front();
			retrans_sock_list.pop_front();
			pthread_mutex_unlock(&sock_list_mutex);
		}

		msg_list = &(*missing_packet_map)[sock];
		list<NACK_MSG>::iterator it;
		for (it = msg_list->begin(); it != msg_list->end(); it++) {
			header->session_id = cur_session_id;
			header->seq_number = it->seq_num;
			header->data_len = it->data_len;

			lseek(fd, header->seq_number, SEEK_SET);
			read(fd, packet_data, header->data_len);
			retrans_tcp_server->SelectSend(sock, packet_buf, MVCTP_HLEN + header->data_len);

			// Update statistics
			send_stats.total_retrans_packets++;
			send_stats.total_retrans_bytes += header->data_len;
			send_stats.session_retrans_packets++;
			send_stats.session_retrans_bytes += header->data_len;
		}
	}
}





