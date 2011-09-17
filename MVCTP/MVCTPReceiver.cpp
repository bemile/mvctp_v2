/*
 * MVCTPReceiver.cpp
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#include "MVCTPReceiver.h"

MVCTPReceiver::MVCTPReceiver(int buf_size) {
	retrans_tcp_client =  new TcpClient("10.1.1.2", BUFFER_TCP_SEND_PORT);
	retrans_tcp_client->Connect();

	multicast_sock = ptr_multicast_comm->GetSocket();
	retrans_tcp_sock = retrans_tcp_client->GetSocket();
	max_sock_fd = multicast_sock > retrans_tcp_sock ? multicast_sock : retrans_tcp_sock;
	FD_ZERO(&read_sock_set);
	FD_SET(multicast_sock, &read_sock_set);
	FD_SET(retrans_tcp_sock, &read_sock_set);

	srand(time(NULL));
	packet_loss_rate = 0;
}

MVCTPReceiver::~MVCTPReceiver() {
}


const struct MvctpReceiverStats MVCTPReceiver::GetBufferStats() {
	return stats;
}

void MVCTPReceiver::SetPacketLossRate(int rate) {
	packet_loss_rate = rate;
}

int MVCTPReceiver::GetPacketLossRate() {
	return packet_loss_rate;
}

void MVCTPReceiver::SetStatusProxy(StatusProxy* proxy) {
	status_proxy = proxy;
}



int MVCTPReceiver::JoinGroup(string addr, ushort port) {
	MVCTPComm::JoinGroup(group_id, port);
	return 1;
}


void MVCTPReceiver::Start() {
	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("MVCTPReceiver::Start()::select() error");
		}

		if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage msg;
			if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) < 0) {
				SysError("MVCTPReceiver::Start()::recv() error");
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
				ReceiveFile(msg);
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
	char str[500];
	sprintf(str, "Started new memory data transfer. Size: %d", transfer_msg.data_len);
	status_proxy->SendMessage(INFORMATIONAL, str);

	list<MvctpNackMessage> nack_list;

	char packet_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* header = (MvctpHeader*)packet_buffer;
	char* packet_data = packet_buffer + MVCTP_HLEN;

	int recv_bytes;
	int offset = 0;
	bool is_first_packet = true;

	fd_set read_set;
	while (true) {
		read_set = read_sock_set;
		if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
			SysError("MVCTPReceiver::ReceiveMemoryData()::select() error");
		}

		if (FD_ISSET(retrans_tcp_sock, &read_set)) {
			struct MvctpTransferMessage t_msg;
			if (recv(retrans_tcp_sock, &t_msg, sizeof(t_msg), 0) < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::recv() error");
			}

			switch (t_msg.event_type) {
			case MEMORY_TRANSFER_FINISH:
			{
				cout << "MEMORY_TRANSFER_FINISH signal received." << endl;

				while ((recv_bytes = ptr_multicast_comm->RecvData(packet_buffer,
									MVCTP_PACKET_LEN, 0 /*MSG_DONTWAIT*/, NULL, NULL)) > 0) {
					cout << "Received a new packet. Seq No.: " << header->seq_number
							<< "    Length: " << header->data_len << endl;

					if (header->seq_number > offset) {
						MvctpNackMessage msg;
						msg.seq_num = offset;
						msg.data_len = header->seq_number - offset;
						nack_list.push_back(msg);
					}

					memcpy(mem_data + header->seq_number, packet_data,
							header->data_len);
					offset = header->seq_number + header->data_len;
				}

				if (transfer_msg.data_len > offset) {
					int pos_start = offset;
					while (pos_start < transfer_msg.data_len) {
						int len = (transfer_msg.data_len - pos_start) < MVCTP_DATA_LEN ?
									(transfer_msg.data_len - pos_start) : MVCTP_DATA_LEN;
						MvctpNackMessage msg;
						msg.seq_num = pos_start; //offset;
						msg.data_len = len; //header->seq_number - offset;
						nack_list.push_back(msg);
						pos_start += len;
					}
				}

				DoMemoryDataRetransmission(mem_data, nack_list);
				status_proxy->SendMessage(INFORMATIONAL, "Memory data transfer finished.");
				// Transfer finished, so return directly
				return;
			}
			default:
				break;
			}

		} else if (FD_ISSET(multicast_sock, &read_set)) {
			recv_bytes = ptr_multicast_comm->RecvData(packet_buffer, MVCTP_PACKET_LEN, 0, NULL, NULL);
			if (recv_bytes < 0) {
				SysError("MVCTPReceiver::ReceiveMemoryData()::RecvData() error");
			}
			else {
				// Need to check that the first packet starts with sequence number 0
//				if (is_first_packet) {
//					if (header->seq_number == 0) {
//						is_first_packet = false;
//					}
//					else {
//						continue;
//					}
//				}

				// Add the received packet to the buffer
				// When greater than packet_loss_rate, add the packet to the receive buffer
				// Otherwise, just drop the packet (emulates errored packet)
				if (rand() % 1000 >= packet_loss_rate) {
					cout << "Received a new packet. Seq No.: " << header->seq_number
										<< "    Length: " << header->data_len << endl;
					if (header->seq_number > offset) {
						int pos_start = offset;
						while (pos_start < header->seq_number) {
							int len = (header->seq_number - pos_start) < MVCTP_DATA_LEN ?
										(header->seq_number - pos_start) : MVCTP_DATA_LEN;
							MvctpNackMessage msg;
							msg.seq_num = pos_start; //offset;
							msg.data_len = len; //header->seq_number - offset;
							nack_list.push_back(msg);
							pos_start += len;
						}
					}

					memcpy(mem_data + header->seq_number, packet_data, header->data_len);
					offset = header->seq_number + header->data_len;
				}

			}

		}
	}
}


//
void MVCTPReceiver::DoMemoryDataRetransmission(char* mem_data, const list<MvctpNackMessage>& nack_list) {
	char str[500];
	sprintf(str, "Start retransmission. Total missing packets: %d", nack_list.size());
	status_proxy->SendMessage(INFORMATIONAL, str);

	MvctpRetransMessage msg;
	bzero(&msg, sizeof(msg));

	// Send retransmission requests
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

	status_proxy->SendMessage(INFORMATIONAL, "Retransmission requests sent.");

	// Receive packets from the sender
	MvctpHeader header;
	char packet_data[MVCTP_DATA_LEN];
	int size = nack_list.size();
	int bytes;
	for (int i = 0; i < size; i++) {
		bytes = retrans_tcp_client->Receive(&header, MVCTP_HLEN);
		cout << "Received header: " << bytes << " bytes." << endl;
		bytes = retrans_tcp_client->Receive(packet_data, header.data_len);
		cout << "Received payload: " << bytes << " bytes." << endl;
		memcpy(mem_data+header.seq_number, packet_data, header.data_len);

		//cout << "Retransmission packet received. Seq No.: " << header.seq_number <<
		//				"    Length: " << header.data_len << endl;
	}
}


// Receive a file from the sender
void MVCTPReceiver::ReceiveFile(const MvctpTransferMessage & msg) {
	fd_set read_set;
		while (true) {
			read_set = read_sock_set;
			if (select(max_sock_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
				SysError("TcpServer::SelectReceive::select() error");
			}

			if (FD_ISSET(retrans_tcp_sock, &read_set)) {
				struct MvctpTransferMessage msg;
				if (recv(retrans_tcp_sock, &msg, sizeof(msg), 0) < 0) {
					switch (msg.event_type) {
					case FILE_TRANSFER_FINISH:

						break;
					default:
						break;
					}
				}

			} else if (FD_ISSET(multicast_sock, &read_set)) {

			}
		}
}




