/*
 * MVCTPReceiver.cpp
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#include "MVCTPReceiver.h"


MVCTPReceiver::MVCTPReceiver(int buf_size) {
	//ptr_multicast_comm->SetBufferSize(10000000);
	retrans_tcp_client = NULL;
	multicast_sock = ptr_multicast_comm->GetSocket();

	srand(time(NULL));
	packet_loss_rate = 0;
	bzero(&recv_stats, sizeof(recv_stats));
}

MVCTPReceiver::~MVCTPReceiver() {
	delete retrans_tcp_client;
}


const struct MvctpReceiverStats MVCTPReceiver::GetBufferStats() {
	return recv_stats;
}

void MVCTPReceiver::SetPacketLossRate(int rate) {
	packet_loss_rate = rate;

	// tcp socket
//	double loss_rate = rate / 10.0;
//	char rate_str[25];
//	sprintf(rate_str, "%.2f%%", loss_rate);
//
//	char command[256];
//	sprintf(command, "sudo ./loss-rate-tcp.sh %s %d %s", GetInterfaceName().c_str(),
//						BUFFER_TCP_SEND_PORT, rate_str);
//	system(command);
}

int MVCTPReceiver::GetPacketLossRate() {
	return packet_loss_rate;
}

void MVCTPReceiver::SetBufferSize(size_t size) {
	ptr_multicast_comm->SetBufferSize(size);
}

void MVCTPReceiver::SetStatusProxy(StatusProxy* proxy) {
	status_proxy = proxy;
}

void MVCTPReceiver::SendSessionStatistics() {
	char buf[512];
	double send_rate = (recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes)
						/ 1000.0 / 1000.0 * 8 / recv_stats.session_total_time * SEND_RATE_RATIO;
	size_t total_bytes = recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes;

	sprintf(buf, "***** Session Statistics *****\nTotal Received Bytes: %u\nTotal Received Packets: %d\nTotal Retrans. Packets: %d\n"
			"Retrans. Percentage: %.4f\nTotal Transfer Time: %.2f sec\nMulticast Transfer Time: %.2f sec\n"
			"Retrans. Time: %.2f sec\nOverall Throughput: %.2f Mbps\n\n", total_bytes,
			recv_stats.session_recv_packets, recv_stats.session_retrans_packets,
			recv_stats.session_retrans_percentage, recv_stats.session_total_time, recv_stats.session_trans_time,
			recv_stats.session_retrans_time, send_rate);
	status_proxy->SendMessageLocal(INFORMATIONAL, buf);

	sprintf(buf, "%u,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%.4f\n", session_id, status_proxy->GetNodeId().c_str(), recv_stats.session_total_time, recv_stats.session_trans_time,
			recv_stats.session_retrans_time, send_rate, recv_stats.session_recv_packets, recv_stats.session_retrans_packets,
			recv_stats.session_retrans_percentage);
	status_proxy->SendMessageLocal(EXP_RESULT_REPORT, buf);
}

// Clear session related statistics
void MVCTPReceiver::ResetSessionStatistics() {
	recv_stats.session_recv_packets = 0;
	recv_stats.session_recv_bytes = 0;
	recv_stats.session_retrans_packets = 0;
	recv_stats.session_retrans_bytes = 0;
	recv_stats.session_retrans_percentage = 0.0;
	recv_stats.session_total_time = 0.0;
	recv_stats.session_trans_time = 0.0;
	recv_stats.session_retrans_time = 0.0;
}

// Send session statistics to the sender through TCP connection
void MVCTPReceiver::SendStatisticsToSender() {
	char buf[512];
	double send_rate = (recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes)
							/ 1000.0 / 1000.0 * 8 / recv_stats.session_total_time * SEND_RATE_RATIO;
	size_t total_bytes = recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes;

	sprintf(buf, "%u,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%.4f\n", session_id,
			status_proxy->GetNodeId().c_str(), recv_stats.session_total_time,
			recv_stats.session_trans_time, recv_stats.session_retrans_time,
			send_rate, recv_stats.session_recv_packets,
			recv_stats.session_retrans_packets,
			recv_stats.session_retrans_percentage);

	int len = strlen(buf);
	retrans_tcp_client->Send(&len, sizeof(len));
	retrans_tcp_client->Send(buf, len);
}


int MVCTPReceiver::JoinGroup(string addr, ushort port) {
	MVCTPComm::JoinGroup(addr, port);
	ConnectSenderOnTCP();
	return 1;
}

int MVCTPReceiver::ConnectSenderOnTCP() {
	status_proxy->SendMessageLocal(INFORMATIONAL, "Connecting TCP server at the sender...");

	if (retrans_tcp_client != NULL)
		delete retrans_tcp_client;

	retrans_tcp_client =  new TcpClient("10.1.1.2", BUFFER_TCP_SEND_PORT);
	retrans_tcp_client->Connect();
	retrans_tcp_sock = retrans_tcp_client->GetSocket();
	max_sock_fd = multicast_sock > retrans_tcp_sock ? multicast_sock : retrans_tcp_sock;
	FD_ZERO(&read_sock_set);
	FD_SET(multicast_sock, &read_sock_set);
	FD_SET(retrans_tcp_sock, &read_sock_set);

	status_proxy->SendMessageLocal(INFORMATIONAL, "TCP server connected.");
	return 1;
}

void MVCTPReceiver::Start() {
	// First set the process to SCHED_RR mode
	//struct sched_param sp;
	//sp.__sched_priority = sched_get_priority_max(SCHED_RR);
	//sched_setscheduler(0, SCHED_RR, &sp);


	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("MVCTPReceiver::Start()::select() error");
		}

		if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage msg;
			if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) <= 0) {
				//SysError("MVCTPReceiver::Start()::recv() error");
				status_proxy->SendMessageToManager(INFORMATIONAL, "Connection to the sender TCP server has broken. Reconnecting...");
				retrans_tcp_client->Connect();
				status_proxy->SendMessageToManager(INFORMATIONAL, "TCP server reconnected.");

				retrans_tcp_sock = retrans_tcp_client->GetSocket();
				FD_ZERO(&read_sock_set);
				FD_SET(multicast_sock, &read_sock_set);
				FD_SET(retrans_tcp_sock, &read_sock_set);
				if (max_sock_fd < retrans_tcp_sock)
					max_sock_fd = retrans_tcp_sock;

				continue;
			}

			switch (msg.event_type) {
			case MEMORY_TRANSFER_START:
			{
				char* buf = (char*)malloc(msg.data_len);
				ReceiveMemoryData(msg, buf);
				free(buf);
				break;
			}
			case FILE_TRANSFER_START:
				//ReceiveFileMemoryMappedIO(msg);
				ReceiveFileBufferedIO(msg);
				break;
			case TCP_MEMORY_TRANSFER_START: {
				char* buf = (char*) malloc(msg.data_len);
				TcpReceiveMemoryData(msg, buf);
				free(buf);
				break;
			}
			case TCP_FILE_TRANSFER_START:
				TcpReceiveFile(msg);
				break;
			case SPEED_TEST:
				if (recv_stats.session_retrans_percentage > 0.3) {
					status_proxy->SendMessageLocal(INFORMATIONAL, "I'm going offline because I'm a slow node...");
					system("sudo reboot");
				}
				break;
			case COLLECT_STATISTICS:
				SendStatisticsToSender();
				break;
			default:
				break;
			}

		} else if (FD_ISSET(multicast_sock, &read_set)) {

		}
	}
}



// Receive memory data from the sender
void MVCTPReceiver::ReceiveMemoryData(const MvctpTransferMessage & transfer_msg, char* mem_data) {
	retrans_info.open("retrans_info.txt", ofstream::out | ofstream::trunc);
	ResetSessionStatistics();

	char str[256];
	sprintf(str, "Started new memory data transfer. Size: %d", transfer_msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	uint32_t session_id = transfer_msg.session_id;
	list<MvctpNackMessage> nack_list;

	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	int recv_bytes;
	uint32_t offset = 0;
	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("MVCTPReceiver::ReceiveMemoryData()::select() error");
		}

		if (FD_ISSET(multicast_sock, &read_set)) {
			recv_bytes = ptr_multicast_comm->RecvData(packet_buffer,
					MVCTP_PACKET_LEN, 0, NULL, NULL);
			if (recv_bytes < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::RecvData() error");
			}

			if (header->session_id != session_id || header->seq_number < offset) {
				continue;
			}

			// Add the received packet to the buffer
			// When greater than packet_loss_rate, add the packet to the receive buffer
			// Otherwise, just drop the packet (emulates errored packet)
			if (rand() % 1000 >= packet_loss_rate) {
				if (header->seq_number > offset) {
					//cout << "Loss packets detected. Supposed Seq. #: " << offset << "    Received Seq. #: "
					//					<< header->seq_number << "    Lost bytes: " << (header->seq_number - offset) << endl;
					HandleMissingPackets(nack_list, offset, header->seq_number);
				}

				memcpy(mem_data + header->seq_number, packet_data,
						header->data_len);
				offset = header->seq_number + header->data_len;

				// Update statistics
				recv_stats.total_recv_packets++;
				recv_stats.total_recv_bytes += header->data_len;
				recv_stats.session_recv_packets++;
				recv_stats.session_recv_bytes += header->data_len;
			}

			continue;

		} else if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage t_msg;
			if (recv(retrans_tcp_sock, &t_msg, sizeof(t_msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::recv() error");
			}

			switch (t_msg.event_type) {
			case MEMORY_TRANSFER_FINISH: {
				//cout << "MEMORY_TRANSFER_FINISH signal received." << endl;
				usleep(10000);
				while ((recv_bytes = ptr_multicast_comm->RecvData(
						packet_buffer, MVCTP_PACKET_LEN, MSG_DONTWAIT,
						NULL, NULL)) > 0) {
					//cout << "Received a new packet. Seq No.: " << header->seq_number << "    Length: "
					//		<< header->data_len << endl;
					if (header->seq_number < offset)
						continue;
					else if (header->seq_number > offset) {
						HandleMissingPackets(nack_list, offset, header->seq_number);
					}

					memcpy(mem_data + header->seq_number, packet_data,
							header->data_len);
					offset = header->seq_number + header->data_len;
				}

				if (transfer_msg.data_len > offset) {
					HandleMissingPackets(nack_list, offset, transfer_msg.data_len);
				}

				// Record memory data multicast time
				recv_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

				DoMemoryDataRetransmission(mem_data, nack_list);

				// Record total transfer and retransmission time
				recv_stats.session_total_time = GetElapsedSeconds(cpu_counter);
				recv_stats.session_retrans_time = recv_stats.session_total_time - recv_stats.session_trans_time;
				recv_stats.session_retrans_percentage = recv_stats.session_retrans_packets  * 1.0
										/ (recv_stats.session_recv_packets + recv_stats.session_retrans_packets);

				status_proxy->SendMessageLocal(INFORMATIONAL, "Memory data transfer finished.");
				SendSessionStatistics();

				retrans_info.close();

				// Transfer finished, so return directly
				return;
			}
			default:
				break;
			}

		}
	}
}


void MVCTPReceiver::HandleMissingPackets(list<MvctpNackMessage>& nack_list, uint current_offset, uint received_seq) {
	retrans_info << GetElapsedSeconds(cpu_counter) << "    Start Seq. #: " << current_offset << "    End Seq. #: " << (received_seq - 1)
			     << "    Missing Block Size: " << (received_seq - current_offset) << endl;

	uint pos_start = current_offset;
	while (pos_start < received_seq) {
		uint len = (received_seq - pos_start) < MVCTP_DATA_LEN ?
					(received_seq - pos_start) : MVCTP_DATA_LEN;
		MvctpNackMessage msg;
		msg.seq_num = pos_start;
		msg.data_len = len;
		nack_list.push_back(msg);
		pos_start += len;
	}
}


//
void MVCTPReceiver::DoMemoryDataRetransmission(char* mem_data, const list<MvctpNackMessage>& nack_list) {
	SendNackMessages(nack_list);

	// Receive packets from the sender
	MvctpHeader header;
	char packet_data[MVCTP_DATA_LEN];
	int size = nack_list.size();
	int bytes;
	for (int i = 0; i < size; i++) {
		bytes = retrans_tcp_client->Receive(&header, MVCTP_HLEN);
		bytes = retrans_tcp_client->Receive(packet_data, header.data_len);
		memcpy(mem_data+header.seq_number, packet_data, header.data_len);

		// Update statistics
		recv_stats.total_retrans_packets++;
		recv_stats.total_retrans_bytes += header.data_len;
		recv_stats.session_retrans_packets++;
		recv_stats.session_retrans_bytes += header.data_len;

		//cout << "Retransmission packet received. Seq No.: " << header.seq_number <<
		//				"    Length: " << header.data_len << endl;
	}
}



// Send retransmission requests to the sender through tcp connection
void MVCTPReceiver::SendNackMessages(const list<MvctpNackMessage>& nack_list) {
	MvctpRetransMessage msg;
	bzero(&msg, sizeof(msg));

	list<MvctpNackMessage>::const_iterator it;
	for (it = nack_list.begin(); it != nack_list.end(); it++) {
		msg.seq_numbers[msg.num_requests] = it->seq_num;
		msg.data_lens[msg.num_requests] = it->data_len;
		msg.num_requests++;

		if (msg.num_requests == MAX_NUM_NACK_REQ) {
			retrans_tcp_client->Send(&msg, sizeof(msg));
			msg.num_requests = 0;
		}
	}

	// Send the last request
	if (msg.num_requests > 0) {
		retrans_tcp_client->Send(&msg, sizeof(msg));
	}

	// Send the request with #requests set to 0 to indicate the end of requests
	msg.num_requests = 0;
	retrans_tcp_client->Send(&msg, sizeof(msg));
}


void MVCTPReceiver::ReceiveFileBufferedIO(const MvctpTransferMessage & transfer_msg) {
	retrans_info.open("retrans_info.txt", ofstream::out | ofstream::trunc);

	char str[256];
	sprintf(str, "Started disk-to-disk file transfer. Size: %u",
			transfer_msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	uint32_t session_id = transfer_msg.session_id;

	// Create the disk file
	int fd = open(transfer_msg.text, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		SysError("MVCTPReceiver::ReceiveFile()::creat() error");
	}

	list<MvctpNackMessage> nack_list;
	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*) packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	int recv_bytes;
	uint32_t offset = 0;
	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("TcpServer::SelectReceive::select() error");
		}

		if (FD_ISSET(multicast_sock, &read_set)) {
			recv_bytes = ptr_multicast_comm->RecvData(packet_buffer,
					MVCTP_PACKET_LEN, 0, NULL, NULL);
			if (recv_bytes < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::RecvData() error");
			}

			if (header->session_id != session_id || header->seq_number < offset) {
				continue;
			}

			// Add the received packet to the buffer
			// When greater than packet_loss_rate, add the packet to the receive buffer
			// Otherwise, just drop the packet (emulates errored packet)
			if (rand() % 1000 >= packet_loss_rate) {
				if (header->seq_number > offset) {
					//cout << "Loss packets detected. Supposed Seq. #: " << offset << "    Received Seq. #: "
					//		<< header->seq_number << "    Lost bytes: " << (header->seq_number - offset) << endl;
					HandleMissingPackets(nack_list, offset, header->seq_number);
					if (lseek(fd, header->seq_number, SEEK_SET) == -1) {
						SysError("MVCTPReceiver::ReceiveFileBufferedIO()::lseek() error");
					}
				}

				write(fd, packet_data, header->data_len);
				offset = header->seq_number + header->data_len;

				// Update statistics
				recv_stats.total_recv_packets++;
				recv_stats.total_recv_bytes += header->data_len;
				recv_stats.session_recv_packets++;
				recv_stats.session_recv_bytes += header->data_len;
			}

			continue;
		}
		else if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage msg;
			if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveFileBufferedIO()::recv() error");
			}

			switch (msg.event_type) {
			case FILE_TRANSFER_FINISH:
				if (transfer_msg.data_len > offset) {
					cout    << "Missing packets in the end of transfer. Final offset: "
							<< offset << "    Transfer Size:"
							<< transfer_msg.data_len << endl;
					HandleMissingPackets(nack_list, offset,
							transfer_msg.data_len);
				}

				// Record memory data multicast time
				recv_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

				DoFileRetransmission(fd, nack_list);
				close(fd);

				// Record total transfer and retransmission time
				recv_stats.session_total_time = GetElapsedSeconds(cpu_counter);
				recv_stats.session_retrans_time = recv_stats.session_total_time - recv_stats.session_trans_time;
				recv_stats.session_retrans_percentage = recv_stats.session_retrans_packets * 1.0
										/ (recv_stats.session_recv_packets + recv_stats.session_retrans_packets);

				// TODO: Delete the file only for experiment purpose.
				//       Should comment out this in practical environments.
				//unlink(transfer_msg.text);
				char command[256];
				sprintf(command, "sudo rm %s", transfer_msg.text);
				system(command);
				system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");

				status_proxy->SendMessageLocal(INFORMATIONAL, "Memory data transfer finished.");
				SendSessionStatistics();

				// Transfer finished, so return directly
				return;
			default:
				break;
			}
		}
	}
}

// Receive a file from the sender
void MVCTPReceiver::ReceiveFileMemoryMappedIO(const MvctpTransferMessage & transfer_msg) {
	retrans_info.open("retrans_info.txt", ofstream::out | ofstream::trunc);

	// NOTE: the length of the memory mapped buffer should be a multiple of the page size
	static const int MAPPED_BUFFER_SIZE = MVCTP_DATA_LEN * 4096;

	char str[256];
	sprintf(str, "Started disk-to-disk file transfer. Size: %u",
			transfer_msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	session_id = transfer_msg.session_id;

	// Create the disk file
	int fd = open(transfer_msg.text, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		SysError("MVCTPReceiver::ReceiveFile()::creat() error");
	}
	if (lseek(fd, transfer_msg.data_len - 1, SEEK_SET) == -1) {
		SysError("MVCTPReceiver::ReceiveFile()::lseek() error");
	}
	if (write(fd, "", 1) != 1) {
		SysError("MVCTPReceiver::ReceiveFile()::write() error");
	}

	// Initialize the memory mapped file buffer
	off_t file_start_pos = 0;
	size_t mapped_size = (transfer_msg.data_len - file_start_pos) < MAPPED_BUFFER_SIZE ?
			(transfer_msg.data_len - file_start_pos) : MAPPED_BUFFER_SIZE;
	char* file_buffer = (char*) mmap(0, mapped_size, PROT_READ | PROT_WRITE,
			MAP_FILE | MAP_SHARED, fd, file_start_pos);
	if (file_buffer == MAP_FAILED) {
		SysError("MVCTPReceiver::ReceiveFile()::mmap() error");
	}
	//char* data_buffer = (char*)malloc(mapped_size);

	list<MvctpNackMessage> nack_list;
	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*) packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	int recv_bytes;
	uint32_t offset = 0;
	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("TcpServer::SelectReceive::select() error");
		}

		if (FD_ISSET(multicast_sock, &read_set)) {
			recv_bytes = ptr_multicast_comm->RecvData(packet_buffer, MVCTP_PACKET_LEN, 0, NULL, NULL);
			if (recv_bytes < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::RecvData() error");
			}

			// retrans_info << GetElapsedSeconds(cpu_counter) << "    Received a new packet. Seq. #: " << header->seq_number << endl;

			if (header->session_id != session_id || header->seq_number < offset) {
				if (header->seq_number < offset) {
					retrans_info << "Out-of-order packets received.    Offset: " << offset << "    Received: " << header->seq_number
							     << "    Total bytes: " << offset - header->seq_number << endl;
				}
				continue;
			}

			// Add the received packet to the buffer
			// When greater than packet_loss_rate, add the packet to the receive buffer
			// Otherwise, just drop the packet (emulates errored packet)
			if (rand() % 1000 >= packet_loss_rate) {
				if (header->seq_number > offset) {
					//cout << "Loss packets detected. Supposed Seq. #: " << offset << "    Received Seq. #: "
					//		<< header->seq_number << "    Lost bytes: " << (header->seq_number - offset) << endl;
					HandleMissingPackets(nack_list, offset, header->seq_number);
				}

				uint32_t pos = header->seq_number - file_start_pos;
				while (pos >= mapped_size) {
					//memcpy(file_buffer, data_buffer, mapped_size);
					munmap(file_buffer, mapped_size);

					file_start_pos += mapped_size;
					mapped_size = (transfer_msg.data_len - file_start_pos)
							< MAPPED_BUFFER_SIZE ? (transfer_msg.data_len
							- file_start_pos) : MAPPED_BUFFER_SIZE;
					file_buffer = (char*) mmap(0, mapped_size,
							PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd,
							file_start_pos);
					if (file_buffer == MAP_FAILED) {
						SysError("MVCTPReceiver::ReceiveFile()::mmap() error");
					}

					pos = header->seq_number - file_start_pos;
				}

				memcpy(file_buffer + pos, packet_data, header->data_len);
				offset = header->seq_number + header->data_len;

				// Update statistics
				recv_stats.total_recv_packets++;
				recv_stats.total_recv_bytes += header->data_len;
				recv_stats.session_recv_packets++;
				recv_stats.session_recv_bytes += header->data_len;
			}

			continue;
		} else if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage msg;
			if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveFile()::recv() error");
			}

			switch (msg.event_type) {
			case FILE_TRANSFER_FINISH:
				//memcpy(file_buffer, data_buffer, mapped_size);
				//free(data_buffer);
				munmap(file_buffer, mapped_size);

				if (transfer_msg.data_len > offset) {
					cout << "Missing packets in the end of transfer. Final offset: " << offset << "    Transfer Size:"
							<< transfer_msg.data_len << endl;
					HandleMissingPackets(nack_list, offset, transfer_msg.data_len);
				}

				// Record memory data multicast time
				recv_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

				DoFileRetransmission(fd, nack_list);
				close(fd);

				// Record total transfer and retransmission time
				recv_stats.session_total_time = GetElapsedSeconds(cpu_counter);
				recv_stats.session_retrans_time = recv_stats.session_total_time
						- recv_stats.session_trans_time;
				recv_stats.session_retrans_percentage = recv_stats.session_retrans_packets * 1.0
								/ (recv_stats.session_recv_packets + recv_stats.session_retrans_packets);


				// TODO: Delte the file only for experiment purpose.
				//       Shoule comment out this in practical environments.
				//unlink(transfer_msg.text);
				retrans_info.close();
				char command[256];
				sprintf(command, "sudo rm %s", transfer_msg.text);
				system(command);
				system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");


				status_proxy->SendMessageLocal(INFORMATIONAL, "Memory data transfer finished.");
				SendSessionStatistics();

				// Transfer finished, so return directly
				return;
				break;
			default:
				break;
			}
		}
	}
}


void MVCTPReceiver::DoFileRetransmission(int fd, const list<MvctpNackMessage>& nack_list) {
	SendNackMessages(nack_list);

	// Receive packets from the sender
	MvctpHeader header;
	char packet_data[MVCTP_DATA_LEN];
	int size = nack_list.size();
	int bytes;
	for (int i = 0; i < size; i++) {
		bytes = retrans_tcp_client->Receive(&header, MVCTP_HLEN);
		bytes = retrans_tcp_client->Receive(packet_data, header.data_len);

		lseek(fd, header.seq_number, SEEK_SET);
		write(fd, packet_data, header.data_len);

		// Update statistics
		recv_stats.total_retrans_packets++;
		recv_stats.total_retrans_bytes += header.data_len;
		recv_stats.session_retrans_packets++;
		recv_stats.session_retrans_bytes += header.data_len;
	}
}


void MVCTPReceiver::CheckReceivedFile(const char* file_name, size_t length) {
	int fd = open(file_name, O_RDWR);
	char buffer[256];
	int read_bytes;
	while ( (read_bytes = read(fd, buffer, 256) > 0)) {
		for (int i = 0; i < read_bytes; i++) {
			if (buffer[i] != i) {
				status_proxy->SendMessageLocal(INFORMATIONAL, "Invalid received file!");
				close(fd);
				return;
			}
		}
	}
	close(fd);
	status_proxy->SendMessageLocal(INFORMATIONAL, "Received file is valid!");
}


struct aio_info {
	char*	data_buffer;
	aiocb* 	ptr_aiocb;
};


void MVCTPReceiver::DoAsynchronousWrite(int fd, size_t offset, char* data_buffer, size_t length) {
	struct aiocb * my_aiocb = (struct aiocb *)malloc(sizeof(aiocb));
	struct aio_info* info = (struct aio_info*)malloc(sizeof(aio_info));
	info->ptr_aiocb = my_aiocb;
	info->data_buffer = data_buffer;

	/* Set up the AIO request */
	bzero(my_aiocb, sizeof(struct aiocb));
	my_aiocb->aio_fildes = fd;
	my_aiocb->aio_buf = data_buffer;
	my_aiocb->aio_nbytes = length;
	my_aiocb->aio_offset = offset;

	/* Link the AIO request with a thread callback */
	my_aiocb->aio_sigevent.sigev_notify = SIGEV_THREAD;
	my_aiocb->aio_sigevent.sigev_notify_function = HandleAsyncWriteCompletion;
	my_aiocb->aio_sigevent.sigev_notify_attributes = NULL;
	my_aiocb->aio_sigevent.sigev_value.sival_ptr = info;

	if (aio_write(my_aiocb) < 0) {
		perror("aio_write() error");
	}
}


void MVCTPReceiver::HandleAsyncWriteCompletion(sigval_t sigval) {
	struct aio_info *info = (struct aio_info *)sigval.sival_ptr;
	cout << "Async write completed. fd: " << info->ptr_aiocb->aio_fildes << "    Offset: "
				<< info->ptr_aiocb->aio_offset << "    Length: " << info->ptr_aiocb->aio_nbytes << endl;


	int errno;
	if ( (errno = aio_error(info->ptr_aiocb)) == 0) {
		/* Request completed successfully, get the return status */
		int ret = aio_return(info->ptr_aiocb);
		if (ret != info->ptr_aiocb->aio_nbytes) {
			cout << "Incomplete AIO write. Return value:" << ret << endl;
		}
	}
	else {
		cout << "AIO write error! Error #: " << errno << endl;
		size_t ret = aio_return(info->ptr_aiocb);
		if (ret != info->ptr_aiocb->aio_nbytes) {
			cout << "Incomplete AIO write. Return value:" << ret << endl;
		}
	}

	// Free the memory buffer
	free(info->data_buffer);
	free(info->ptr_aiocb);
	free(info);
	return;
}




// ============ Functions related to TCP data transfer =============
void MVCTPReceiver::TcpReceiveMemoryData(const MvctpTransferMessage & msg, char* mem_data) {
	char str[256];
	sprintf(str, "Started memory-to-memory transfer using TCP. Size: %d", msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	retrans_tcp_client->Receive(mem_data, msg.data_len);

	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = msg.data_len / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;

	sprintf(str, "***** TCP Receive Info *****\nTotal transfer time: %.2f\nThroughput: %.2f\n", trans_time, send_rate);
	status_proxy->SendMessageLocal(EXP_RESULT_REPORT, str);
}


void MVCTPReceiver::TcpReceiveFile(const MvctpTransferMessage & transfer_msg) {
	// NOTE: the length of the memory mapped buffer should be a multiple of the page size
	static const size_t RECV_BUFFER_SIZE = MVCTP_DATA_LEN * 4096;

	char str[256];
	sprintf(str, "Started disk-to-disk file transfer using TCP. Size: %u",
			transfer_msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);

	// Create the disk file
	char* buffer = (char*)malloc(RECV_BUFFER_SIZE);
	int fd = open(transfer_msg.text, O_RDWR | O_CREAT | O_TRUNC);
	if (fd < 0) {
		SysError("MVCTPReceiver::ReceiveFile()::creat() error");
	}

	size_t remained_size = transfer_msg.data_len;
	while (remained_size > 0) {
		size_t map_size = remained_size < RECV_BUFFER_SIZE ? remained_size
				: RECV_BUFFER_SIZE;
		retrans_tcp_client->Receive(buffer, map_size);
		write(fd, buffer, map_size);

		remained_size -= map_size;
	}
	close(fd);
	free(buffer);
	unlink(transfer_msg.text);

	// Record memory data multicast time
	double trans_time = GetElapsedSeconds(cpu_counter);
	double send_rate = transfer_msg.data_len / 1024.0 / 1024.0 * 8.0 * 1514.0 / 1460.0 / trans_time;

	sprintf(str, "***** TCP Receive Info *****\nTotal transfer time: %.2f\nThroughput: %.2f\n\n", trans_time, send_rate);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	sprintf(str, "%u,%.2f,%.2f\n", transfer_msg.data_len, trans_time, send_rate);
	status_proxy->SendMessageLocal(EXP_RESULT_REPORT, str);

}




