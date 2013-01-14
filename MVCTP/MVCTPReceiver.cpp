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

	keep_retrans_alive = false;
	mvctp_seq_num = 0;
	retrans_switch = true;

	read_ahead_header = (MvctpHeader*) read_ahead_buffer;
	read_ahead_data = read_ahead_buffer + MVCTP_HLEN;
	read_ahead_header->session_id = -1;
}

MVCTPReceiver::~MVCTPReceiver() {
	delete retrans_tcp_client;
	pthread_mutex_destroy(&retrans_list_mutex);
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



void MVCTPReceiver::SendHistoryStats() {
	char buf[1024];
	double retx_rate = recv_stats.total_recv_packets == 0 ? 0 :
							(recv_stats.total_retrans_packets * 100.0 / recv_stats.total_recv_packets);
	double robustness = recv_stats.num_recved_files == 0 ? 100.0 :
											(100.0 - recv_stats.num_failed_files * 100.0 / recv_stats.num_recved_files);  // in percentage

	sprintf(buf, "***** Statistics *****\nTotal received files: %d\nTotal received packets: %d\n"
			"Total retx packets: %d\nRetx rate:%.1f\%\nRobustness:%.2f\%\n", recv_stats.num_recved_files, recv_stats.total_recv_packets,
				recv_stats.total_retrans_packets, retx_rate, robustness);
	status_proxy->SendMessageLocal(INFORMATIONAL, buf);
}


void MVCTPReceiver::ResetHistoryStats() {
	recv_stats.total_recv_bytes = 0;
	recv_stats.total_recv_packets = 0;
	recv_stats.total_retrans_bytes = 0;
	recv_stats.total_retrans_packets = 0;
	recv_stats.num_recved_files = 0;
	recv_stats.num_failed_files = 0;
	recv_stats.last_file_recv_time = 0.0;
	recv_stats.session_stats_vec.clear();
	AccessCPUCounter(&recv_stats.reset_cpu_timer.hi, &recv_stats.reset_cpu_timer.lo);

	recv_status_map.clear();

	recv_stats.cpu_monitor.Stop();
	recv_stats.cpu_monitor.SetCPUFlag(true);
	recv_stats.cpu_monitor.SetInterval(200);
	recv_stats.cpu_monitor.Start();
}


void MVCTPReceiver::SendHistoryStatsToSender() {
	/*char buf[1024];
	double avg_throughput = recv_stats.total_recv_bytes / 1000.0 / 1000.0 * 8 / recv_stats.last_file_recv_time;
	double robustness = 100.0 - recv_stats.num_failed_files * 100.0 / recv_stats.num_recved_files;  // in percentage
	sprintf(buf, "%s,%.2f,%.2f,%d,%s", status_proxy->GetNodeId().c_str(), avg_throughput, robustness,
			recv_stats.cpu_monitor.GetAverageCpuUsage(), (packet_loss_rate > 0 ? "True" : "False"));

	status_proxy->SendMessageLocal(INFORMATIONAL, buf);

	int len = strlen(buf);
	char msg_packet[1024];
	//return;*/


	string res = "";
	int size = recv_stats.session_stats_vec.size();
	for (int i = 0; i < size; i++) {
		res += recv_stats.session_stats_vec[i];
	}

	char* msg_packet = new char[MVCTP_HLEN + res.size()];

	MvctpHeader* header = (MvctpHeader*)msg_packet;
	header->session_id = 0;
	header->seq_number = 0;
	header->data_len = res.size();
	header->flags = MVCTP_HISTORY_STATISTICS;

	memcpy(msg_packet + MVCTP_HLEN, res.c_str(), res.size());
	retrans_tcp_client->Send(msg_packet, MVCTP_HLEN + res.size());

	delete[] msg_packet;
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

	// reset the total missing bytes and current received retransmission bytes to zero
	total_missing_bytes = 0;
	received_retrans_bytes = 0;
	is_multicast_finished = false;
}


// Send session statistics to the sender through TCP connection
void MVCTPReceiver::SendSessionStatisticsToSender() {
	char buf[512];
	double send_rate = (recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes)
							/ 1000.0 / 1000.0 * 8 / recv_stats.session_total_time * SEND_RATE_RATIO;
	size_t total_bytes = recv_stats.session_recv_bytes + recv_stats.session_retrans_bytes;

	sprintf(buf, "%u,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%.4f,%s\n", session_id,
			status_proxy->GetNodeId().c_str(), recv_stats.session_total_time,
			recv_stats.session_trans_time, recv_stats.session_retrans_time,
			send_rate, recv_stats.session_recv_packets,
			recv_stats.session_retrans_packets,
			recv_stats.session_retrans_percentage,
			cpu_info.GetCPUMeasurements().c_str());

	int len = strlen(buf);
	retrans_tcp_client->Send(&len, sizeof(len));
	retrans_tcp_client->Send(buf, len);
}


// Format of a report entry:
// host_name, msg_id, file_size, transfer_time, retx bytes, success (1 or 0), is_slow_receiver
void MVCTPReceiver::AddSessionStatistics(uint msg_id) {
	MessageReceiveStatus& status = recv_status_map[msg_id];

	char buf[1024];
	sprintf(buf, "%s,%u,%lld,%.5f,%lld,%d,%s\n", status_proxy->GetNodeId().c_str(), msg_id,
			status.msg_length, GetElapsedSeconds(status.start_time_counter),
			status.retx_bytes, status.recv_failed ? 0 : 1,
			(packet_loss_rate > 0 ? "True" : "False"));

	recv_stats.session_stats_vec.push_back(buf);


//	double avg_throughput = recv_stats.total_recv_bytes / 1000.0 / 1000.0 * 8 / recv_stats.last_file_recv_time;
//	double robustness = 100.0 - recv_stats.num_failed_files * 100.0 / recv_stats.num_recved_files;  // in percentage
//	sprintf(buf, "%s,%.2f,%.2f,%d,%s", status_proxy->GetNodeId().c_str(), avg_throughput, robustness,
//			recv_stats.cpu_monitor.GetAverageCpuUsage(), (packet_loss_rate > 0 ? "True" : "False"));
//
//	status_proxy->SendMessageLocal(INFORMATIONAL, buf);
}


void MVCTPReceiver::ExecuteCommand(char* command) {
	string comm = command;
	if (comm.compare("SetSchedRR") == 0) {
		SetSchedRR(true);
	}
	else if  (comm.compare("SetNoSchedRR") == 0) {
		SetSchedRR(false);
	}
	else
		system(command);
}


// Set the process scheduling mode to SCHED_RR or SCHED_OTHER (the default process scheduling mode in linux)
void MVCTPReceiver::SetSchedRR(bool is_rr) {
	static int normal_priority = getpriority(PRIO_PROCESS, 0);

	struct sched_param sp;
	if (is_rr) {
		sp.__sched_priority = sched_get_priority_max(SCHED_RR);
		sched_setscheduler(0, SCHED_RR, &sp);
	}
	else {
		sp.__sched_priority = normal_priority;
		sched_setscheduler(0, SCHED_OTHER, &sp);
	}
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

	// start the retransmission request sending and data receiving threads
	StartRetransmissionThread();

	status_proxy->SendMessageLocal(INFORMATIONAL, "TCP server connected.");
	return 1;
}


void MVCTPReceiver::ReconnectSender() {
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
}


void MVCTPReceiver::Start() {
	StartReceivingThread();
}


// Main receiving thread functions
void MVCTPReceiver::StartReceivingThread() {
	pthread_create(&recv_thread, NULL, &MVCTPReceiver::StartReceivingThread, this);
}

void* MVCTPReceiver::StartReceivingThread(void* ptr) {
	((MVCTPReceiver*)ptr)->RunReceivingThread();
	return NULL;
}


// Main file receiving function
void MVCTPReceiver::RunReceivingThread() {
	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*) packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	MvctpSenderMessage *ptr_sender_msg = (MvctpSenderMessage *)packet_data;
	int 	recv_bytes = 0;
	fd_set 	read_set;
	map<uint, MessageReceiveStatus>::iterator it;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("TcpServer::SelectReceive::select() error");
		}

		if (FD_ISSET(multicast_sock, &read_set)) {
			if ( (recv_bytes = ptr_multicast_comm->RecvData(packet_buffer,
					MVCTP_PACKET_LEN, 0, NULL, NULL)) < 0)
				SysError("MVCTPReceiver::RunReceivingThread() multicast recv error");

			// Check for BOF and EOF
			if (header->flags & MVCTP_BOF) {
				HandleBofMessage(*ptr_sender_msg);
			}
			else if (header->flags & MVCTP_EOF) {
				HandleEofMessage(header->session_id);
			}
			else { // is a data packet
				if ( (it = recv_status_map.find(header->session_id)) == recv_status_map.end()) {
					cout << "Could not find the message ID in recv_status_map: " << header->session_id << endl << flush;
					continue;
				}

				MessageReceiveStatus& recv_status = it->second;
				// Write the packet into the file. Otherwise, just drop the packet (emulates errored packet)
				if (rand() % 1000 >= packet_loss_rate) {
					if (header->seq_number > recv_status.current_offset) {
						AddRetxRequest(header->session_id, recv_status.current_offset, header->seq_number);
						if (lseek(recv_status.file_descriptor, header->seq_number, SEEK_SET) < 0) {
							cout << "Error in file " << header->session_id << ":  " << endl << flush;
							SysError("MVCTPReceiver::RunReceivingThread()::lseek() error on multicast data");
						}
					}

					if (write(recv_status.file_descriptor, packet_data, header->data_len) < 0)
						SysError("MVCTPReceiver::RunReceivingThread()::write() error on multicast data");
					recv_status.current_offset = header->seq_number + header->data_len;

					// Update statistics
					recv_stats.total_recv_packets++;
					recv_stats.total_recv_bytes += header->data_len;
					recv_status.multicast_packets++;
					recv_status.multicast_bytes += header->data_len;
				}
			}
		}

		// check received data on the TCP connection
		if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			if (retrans_tcp_client->Receive(header, sizeof(MvctpHeader)) < 0) {
				SysError("MVCTPReceiver::RunReceivingThread()::recv() error");
			}

			if (header->flags & MVCTP_SENDER_MSG_EXP) {
				//status_proxy->SendMessageLocal(INFORMATIONAL, "I received a SENDER_MSG_EXP message");
				MvctpSenderMessage sender_msg;
				if (retrans_tcp_client->Receive(&sender_msg, header->data_len) < 0) {
					ReconnectSender();
					continue;
				}
				HandleSenderMessage(sender_msg);
			}
			else if (header->flags & MVCTP_RETRANS_DATA) {
				if (retrans_tcp_client->Receive(packet_data, header->data_len) < 0)
					SysError("MVCTPReceiver::RunningReceivingThread()::receive error on TCP");

				if ( (it = recv_status_map.find(header->session_id)) == recv_status_map.end()) {
					if (retrans_tcp_client->Receive(packet_data, header->data_len) < 0)
						SysError("MVCTPReceiver::RunningReceivingThread()::receive error on TCP");
					continue;
				}

				MessageReceiveStatus& recv_status = it->second; //recv_status_map[header->session_id];
				if (recv_status.retx_file_descriptor == -1) {
					recv_status.retx_file_descriptor = dup(recv_status.file_descriptor);
					if (recv_status.retx_file_descriptor < 0)
							SysError("MVCTPReceiver::RunReceivingThread() open file error");
				}
				if (lseek(recv_status.retx_file_descriptor, header->seq_number, SEEK_SET) == -1) {
					SysError("MVCTPReceiver::RunReceivingThread()::lseek() error on retx data");
				}
				if (write(recv_status.retx_file_descriptor, packet_data, header->data_len) < 0) {
					//SysError("MVCTPReceiver::ReceiveFile()::write() error");
					cout << "MVCTPReceiver::RunReceivingThread()::write() error on retx data" << endl;
				}

				// Update statistics
				recv_stats.total_recv_packets++;
				recv_stats.total_recv_bytes += header->data_len;
				recv_stats.total_retrans_packets++;
				recv_stats.total_retrans_bytes += header->data_len;
				recv_status.retx_packets++;
				recv_status.retx_bytes += header->data_len;
			}
			else if (header->flags & MVCTP_RETRANS_END) {
				//status_proxy->SendMessageLocal(INFORMATIONAL, "I received a RETX_END message");

				map<uint, MessageReceiveStatus>::iterator it = recv_status_map.find(header->session_id);
				if (it != recv_status_map.end()) {
					MessageReceiveStatus& recv_status = it->second; //recv_status_map[header->session_id];
					close(recv_status.file_descriptor);
					if (recv_status.retx_file_descriptor > 0)
						close(recv_status.retx_file_descriptor);
					//recv_status_map.erase(header->session_id);

					recv_stats.last_file_recv_time = GetElapsedSeconds(recv_stats.reset_cpu_timer);
					AddSessionStatistics(header->session_id);
					/*char str[256];
					sprintf(str, "File transfer finished.\n***** Statistics for File %d *****\n"
							"Transfer Time: %.2f seconds\nRetx. Packets: %lld\nRetx. Rate: %.2f",
							recv_status.msg_id, GetElapsedSeconds(recv_status.start_time_counter), recv_status.retx_packets,
								recv_status.retx_packets * 1.0 / (recv_status.multicast_packets + recv_status.retx_packets) );
					status_proxy->SendMessageLocal(INFORMATIONAL, str);*/
				}

			}
			else if (header->flags & MVCTP_RETRANS_TIMEOUT) {
				map<uint, MessageReceiveStatus>::iterator it = recv_status_map.find(header->session_id);
				if (it != recv_status_map.end()) {
					MessageReceiveStatus& recv_status = it->second; //recv_status_map[header->session_id];
					AddRetxRequest(recv_status.msg_id, recv_status.msg_length, recv_status.msg_length);
					close(recv_status.file_descriptor);
					if (recv_status.retx_file_descriptor > 0)
						close(recv_status.retx_file_descriptor);
					//recv_status_map.erase(header->session_id);

					recv_stats.num_failed_files++;
					recv_status_map[header->session_id].recv_failed = true;
					AddSessionStatistics(header->session_id);

					/*char str[256];
					sprintf(str, "Receiving file %d failed because of retransmission timeout.", recv_status.msg_id);
					status_proxy->SendMessageLocal(INFORMATIONAL, str); */
				}
			}
		}
	}
}


/**
 * Handle a BOF message for a new file
 */
void MVCTPReceiver::HandleBofMessage(MvctpSenderMessage& sender_msg) {
	switch (sender_msg.msg_type) {
	case MEMORY_TRANSFER_START: {
		char* buf = (char*) malloc(sender_msg.data_len);
		ReceiveMemoryData(sender_msg, buf);
		free(buf);
		break;
	}
	case FILE_TRANSFER_START:
		PrepareForFileTransfer(sender_msg);
		break;
	case TCP_MEMORY_TRANSFER_START: {
		char* buf = (char*) malloc(sender_msg.data_len);
		TcpReceiveMemoryData(sender_msg, buf);
		free(buf);
		break;
	}
	case TCP_FILE_TRANSFER_START:
		TcpReceiveFile(sender_msg);
		break;
	default:
		break;
	}
}


// Create metadata for a new file that is to be received
void MVCTPReceiver::PrepareForFileTransfer(MvctpSenderMessage& sender_msg) {
	if (sender_msg.session_id % 100 == 1)
	{
		char str[500];
		sprintf(str, "Receiving file %d. File length: %d bytes\n\n",
							sender_msg.session_id, sender_msg.data_len);
		status_proxy->SendMessageLocal(INFORMATIONAL, str);
	}

	// First reset all session related counters
	ResetSessionStatistics();

	MessageReceiveStatus status;
	status.msg_id = sender_msg.session_id;
	status.msg_name = sender_msg.text;
	status.msg_length = sender_msg.data_len;
	status.is_multicast_done = false;
	status.msg_length = sender_msg.data_len;
	status.current_offset = 0;
	status.multicast_packets = 0;
	status.multicast_bytes = 0;
	status.retx_packets = 0;
	status.retx_bytes = 0;
	status.recv_failed = false;
	status.retx_file_descriptor = -1;
	status.file_descriptor = open(sender_msg.text, O_RDWR | O_CREAT | O_TRUNC);
	if (status.file_descriptor < 0)
		SysError("MVCTPReceiver::PrepareForFileTransfer open file error");

	AccessCPUCounter(&status.start_time_counter.hi, &status.start_time_counter.lo);
	if (read_ahead_header->session_id == sender_msg.session_id)
	{
		if (write(status.file_descriptor, read_ahead_data, read_ahead_header->data_len) < 0)
			SysError("MVCTPReceiver::ReceiveFileBufferedIO() write multicast data error");

		status.current_offset = read_ahead_header->seq_number + read_ahead_header->data_len;
		status.multicast_packets++;
		status.multicast_bytes += read_ahead_header->data_len;
		read_ahead_header->session_id = -1;
	}
	recv_status_map[status.msg_id] = status;

	recv_stats.current_msg_id = sender_msg.session_id;
	recv_stats.num_recved_files++;

	cout << "Added a new recv status for file " << status.msg_id << endl << flush;
 }


/**
 * Handle a command message from the sender
 */
void MVCTPReceiver::HandleSenderMessage(MvctpSenderMessage& sender_msg) {
	switch (sender_msg.msg_type) {
		case SPEED_TEST:
			if (recv_stats.session_retrans_percentage > 0.3) {
				status_proxy->SendMessageLocal(INFORMATIONAL,
						"I'm going offline because I'm a slow node...");
				system("sudo reboot");
			}
			break;
		case COLLECT_STATISTICS:
			//SendStatisticsToSender();
			SendHistoryStatsToSender();
			break;
		case RESET_HISTORY_STATISTICS:
			ResetHistoryStats();
			break;
		case EXECUTE_COMMAND:
			ExecuteCommand(sender_msg.text);
			break;
	}
}


/**
 * Take actions after receiving an EOF message for a specific file
 */
void MVCTPReceiver::HandleEofMessage(uint msg_id) {
	map<uint, MessageReceiveStatus>::iterator it = recv_status_map.find(msg_id);
	if (it == recv_status_map.end())
		return;

	MessageReceiveStatus& status = it->second; //recv_status_map[msg_id];
	status.is_multicast_done = true;

	// Check data loss at the end
	if (status.current_offset < status.msg_length) {
		AddRetxRequest(msg_id, status.current_offset, status.msg_length);
		status.current_offset = status.msg_length;
	}

	// Add a RETX_END message to the end of the request list
	AddRetxRequest(msg_id, status.msg_length, status.msg_length);
}


/**
 * Insert a new retransmission request to the request list
 */
void MVCTPReceiver::AddRetxRequest(uint msg_id, uint current_offset, uint received_seq) {
	MvctpRetransRequest req;
	req.msg_id = msg_id;
	req.seq_num = current_offset;
	req.data_len = received_seq - current_offset;

	pthread_mutex_lock(&retrans_list_mutex);
	retrans_list.push_back(req);
	pthread_mutex_unlock(&retrans_list_mutex);
}





//************************** Functions for the retransmission thread **************************
void MVCTPReceiver::StartRetransmissionThread() {
	keep_retrans_alive = true;
	pthread_mutex_init(&retrans_list_mutex, NULL);
	pthread_create(&retrans_thread, NULL, &MVCTPReceiver::StartRetransmissionThread, this);
}


void* MVCTPReceiver::StartRetransmissionThread(void* ptr) {
	((MVCTPReceiver*)ptr)->RunRetransmissionThread();
	return NULL;
}


void MVCTPReceiver::RunRetransmissionThread() {
	char buf[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)buf;
	header->data_len = sizeof(MvctpRetransRequest);

	char* data = (buf + MVCTP_HLEN);
	MvctpRetransRequest* request = (MvctpRetransRequest*)data;
	while (keep_retrans_alive) {
		if (!retrans_switch) {
			usleep(10000);
			continue;
		}

		pthread_mutex_lock(&retrans_list_mutex);
			while (!retrans_list.empty()) {
				MvctpRetransRequest& req = retrans_list.front();
				request->msg_id = req.msg_id;
				request->seq_num = req.seq_num;
				request->data_len = req.data_len;

				header->session_id = req.msg_id;
				header->seq_number = 0;
				header->flags = ((request->data_len == 0) ? MVCTP_RETRANS_END : MVCTP_RETRANS_REQ);


				retrans_tcp_client->Send(buf, MVCTP_HLEN + header->data_len);
				retrans_list.pop_front();
				cout << "Sent a retx request for file " << request->msg_id << endl << flush;
			}
		pthread_mutex_unlock(&retrans_list_mutex);
		usleep(10000);
	}
}












//************************************* Old unused functions ********************************

// Receive memory data from the sender
void MVCTPReceiver::ReceiveMemoryData(const MvctpSenderMessage & transfer_msg, char* mem_data) {
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
			struct MvctpSenderMessage t_msg;
			if (recv(retrans_tcp_sock, &t_msg, sizeof(t_msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::recv() error");
			}

			switch (t_msg.msg_type) {
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



// Handle missing packets by inserting retransmission requests into the request list
void MVCTPReceiver::HandleMissingPackets(list<MvctpNackMessage>& nack_list, uint current_offset, uint received_seq) {
	retrans_info << GetElapsedSeconds(cpu_counter) << "    Start Seq. #: " << current_offset << "    End Seq. #: " << (received_seq - 1)
			     << "    Missing Block Size: " << (received_seq - current_offset) << endl;

	MvctpRetransRequest req;
	req.seq_num = current_offset;
	req.data_len = received_seq - current_offset;
	total_missing_bytes += req.data_len;

	pthread_mutex_lock(&retrans_list_mutex);
	retrans_list.push_back(req);
	pthread_mutex_unlock(&retrans_list_mutex);

//	uint pos_start = current_offset;
//	while (pos_start < received_seq) {
//		uint len = (received_seq - pos_start) < MVCTP_DATA_LEN ?
//					(received_seq - pos_start) : MVCTP_DATA_LEN;
//		MvctpNackMessage msg;
//		msg.seq_num = pos_start;
//		msg.data_len = len;
//		nack_list.push_back(msg);
//		pos_start += len;
//	}
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


void MVCTPReceiver::ReceiveFileBufferedIO(const MvctpSenderMessage & transfer_msg) {
	retrans_info.open("retrans_info.txt", ofstream::out | ofstream::trunc);

	MessageReceiveStatus status;
	status.msg_id = transfer_msg.session_id;
	status.msg_length = transfer_msg.data_len;
	status.multicast_bytes = 0;
	recv_status_map[status.msg_id] = status;

	is_multicast_finished = false;
	received_retrans_bytes = 0;
	total_missing_bytes = 0;
	recv_stats.current_msg_id = transfer_msg.session_id;

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
			struct MvctpSenderMessage msg;
			if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveFileBufferedIO()::recv() error");
			}

			switch (msg.msg_type) {
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
void MVCTPReceiver::ReceiveFileMemoryMappedIO(const MvctpSenderMessage & transfer_msg) {
	// NOTE: the length of the memory mapped buffer should be a multiple of the page size
	static const int MAPPED_BUFFER_SIZE = MVCTP_DATA_LEN * 4096;

	retrans_info.open("retrans_info.txt", ofstream::out | ofstream::trunc);

	//PerformanceCounter udp_buffer_info(50);
	//udp_buffer_info.SetUDPRecvBuffFlag(true);
	//udp_buffer_info.Start();

	MessageReceiveStatus status;
	status.msg_id = transfer_msg.session_id;
	status.msg_length = transfer_msg.data_len;
	status.multicast_bytes = 0;
	recv_status_map[status.msg_id] = status;

	cpu_info.SetInterval(500);
    cpu_info.SetCPUFlag(true);
	cpu_info.Start();

	is_multicast_finished = false;
	received_retrans_bytes = 0;
	total_missing_bytes = 0;

	char str[256];
	sprintf(str, "Started disk-to-disk file transfer. Size: %u",
			transfer_msg.data_len);
	status_proxy->SendMessageLocal(INFORMATIONAL, str);

	ResetSessionStatistics();
	AccessCPUCounter(&cpu_counter.hi, &cpu_counter.lo);
	session_id = transfer_msg.session_id;

	// Create the disk file
	int recv_fd = open(transfer_msg.text, O_RDWR | O_CREAT | O_TRUNC);
	if (recv_fd < 0) {
		SysError("MVCTPReceiver::ReceiveFile()::creat() error");
	}
	if (lseek(recv_fd, transfer_msg.data_len - 1, SEEK_SET) == -1) {
		SysError("MVCTPReceiver::ReceiveFile()::lseek() error");
	}
	if (write(recv_fd, "", 1) != 1) {
		SysError("MVCTPReceiver::ReceiveFile()::write() error");
	}

	// Initialize the memory mapped file buffer
	off_t file_start_pos = 0;
	size_t mapped_size = (transfer_msg.data_len - file_start_pos) < MAPPED_BUFFER_SIZE ?
			(transfer_msg.data_len - file_start_pos) : MAPPED_BUFFER_SIZE;
	char* file_buffer = (char*) mmap(0, mapped_size, PROT_READ | PROT_WRITE,
			MAP_FILE | MAP_SHARED, recv_fd, file_start_pos);
	if (file_buffer == MAP_FAILED) {
		SysError("MVCTPReceiver::ReceiveFile()::mmap() error");
	}
	//char* data_buffer = (char*)malloc(mapped_size);

	list<MvctpNackMessage> nack_list;
	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*) packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	cout << "Start receiving file..." << endl;
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
			//if (rand() % 1000 >= packet_loss_rate) {
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
					mapped_size = ((transfer_msg.data_len - file_start_pos)
							< MAPPED_BUFFER_SIZE ? (transfer_msg.data_len
							- file_start_pos) : MAPPED_BUFFER_SIZE);
					file_buffer = (char*) mmap(0, mapped_size,
							PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, recv_fd,
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
			//}

			continue;
		} else if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			if (recv(retrans_tcp_sock, header, sizeof(MvctpHeader), 0) <= 0) {
				SysError("MVCTPReceiver::ReceiveFile()::recv() error");
			}

			if (header->flags & MVCTP_EOF) {
				munmap(file_buffer, mapped_size);
				if (transfer_msg.data_len > offset) {
					cout << "Missing packets in the end of transfer. Final offset: "
						<< offset << "    Transfer Size:" << transfer_msg.data_len << endl;
					HandleMissingPackets(nack_list, offset, transfer_msg.data_len);
				}

				// Add one dummy request to indicate the end of requests to the sender
				HandleMissingPackets(nack_list, transfer_msg.data_len, transfer_msg.data_len);

				// Record data multicast time
				recv_stats.session_trans_time = GetElapsedSeconds(cpu_counter);

				cout << "EOF received." << endl;
				is_multicast_finished = true;
				if (received_retrans_bytes == total_missing_bytes)
					break;
				else
					cout << "There are more retransmission packets to come." << endl;
			}
			else if (header->flags & MVCTP_RETRANS_DATA) {
				retrans_tcp_client->Receive(packet_data, header->data_len);

				if (lseek(recv_fd, header->seq_number, SEEK_SET) == -1) {
					SysError("MVCTPReceiver::ReceiveFile()::lseek() error");
				}
				if (write(recv_fd, packet_data, header->data_len) < 0) {
					//SysError("MVCTPReceiver::ReceiveFile()::write() error");
					cout << "MVCTPReceiver::ReceiveFile()::write() error" << endl;
				}

				// Update statistics
				recv_stats.total_retrans_packets++;
				recv_stats.total_retrans_bytes += header->data_len;
				recv_stats.session_retrans_packets++;
				recv_stats.session_retrans_bytes += header->data_len;

				received_retrans_bytes += header->data_len;
				//cout << "Received one retransmission pakcet.  Remained bytes:" <<
				//							(total_missing_bytes - received_retrans_bytes) << endl;
				if (is_multicast_finished && (received_retrans_bytes == total_missing_bytes) )
					break;
			}
		}
	}


	//DoFileRetransmission(recv_fd, nack_list);
	close(recv_fd);

	// Record total transfer and retransmission time
	recv_stats.session_total_time = GetElapsedSeconds(cpu_counter);
	recv_stats.session_retrans_time = recv_stats.session_total_time - recv_stats.session_trans_time;
	recv_stats.session_retrans_percentage = recv_stats.session_retrans_packets
					* 1.0 / (recv_stats.session_recv_packets + recv_stats.session_retrans_packets);

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

	cpu_info.Stop();

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
void MVCTPReceiver::TcpReceiveMemoryData(const MvctpSenderMessage & msg, char* mem_data) {
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


void MVCTPReceiver::TcpReceiveFile(const MvctpSenderMessage & transfer_msg) {
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




