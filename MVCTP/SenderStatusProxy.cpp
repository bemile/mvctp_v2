/*
 * SenderStatusProxy.cpp
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#include "SenderStatusProxy.h"

SenderStatusProxy::SenderStatusProxy(string addr, int port, MVCTPSender* psender)
			: StatusProxy(addr, port) {
	ptr_sender = psender;
}

int SenderStatusProxy::HandleCommand(char* command) {
	string s = command;
	/*int length = s.length();
	int index = s.find(' ');
	string comm_name = s.substr(0, index);
	string args = s.substr(index + 1, length - index - 1);
	 */

	list<string> parts;
	Split(s, ' ', parts);
	if (parts.size() == 0)
		return 0;

	char msg[512];
	if (parts.front().compare("Send") == 0) {
		parts.erase(parts.begin());
		HandleSendCommand(parts);
	}
	else if (parts.front().compare("SetRate") == 0) {
		if (parts.size() == 2) {
			int rate = atoi(parts.back().c_str());
			ptr_sender->SetSendRate(rate);
			sprintf(msg, "Data sending rate has been set to %d Mbps.", rate);
			SendMessage(COMMAND_RESPONSE, msg);
		}
	}
	else if (parts.front().compare("SetBufferSize") == 0) {
		if (parts.size() == 2) {
			int size = atoi(parts.back().c_str());
			ptr_sender->SetBufferSize(size);
			sprintf(msg, "Buffer size has been set to %d bytes.", size);
			SendMessage(COMMAND_RESPONSE, msg);
		}
	}
	else if (parts.front().compare("ResetBuffer") == 0) {
		ptr_sender->ResetBuffer();
		SendMessage(COMMAND_RESPONSE, "Buffer has been reset.");
	}
	else if (parts.front().compare("CreateLogFile") == 0) {
		if (parts.size() == 2) {
			CreateNewLogFile(parts.back().c_str());
			SendMessage(COMMAND_RESPONSE, "New log file created.");
		}
	}
	else if (parts.front().compare("SetLogSwitch") == 0) {
		if (parts.size() == 2) {
			if (parts.back().compare("On") == 0) {
				MVCTP::is_log_enabled = true;
				SendMessage(COMMAND_RESPONSE, "Log switch set to ON.");
			}
			else if (parts.back().compare("Off") == 0) {
				MVCTP::is_log_enabled = false;
				SendMessage(COMMAND_RESPONSE, "Log switch set to OFF.");
			}
		}
	}
	else {
		StatusProxy::HandleCommand(command);
	}

	return 1;
}




//
int SenderStatusProxy::HandleSendCommand(list<string>& slist) {
	bool memory_transfer = false;
	bool file_transfer = false;
	bool send_out_packets = true;

	int 	mem_transfer_size = 0;

	string arg = "";
	list<string>::iterator it;
	for (it = slist.begin(); it != slist.end(); it++) {
		if ((*it)[0] == '-') {
			switch ((*it)[1]) {
			case 'm':
				it++;
				memory_transfer = true;
				mem_transfer_size = atoi((*it).c_str());	// in Megabytes
				break;
			case 'f':
				file_transfer = true;
				break;
			case 'n':
				send_out_packets = false;
				break;
			default:
				break;
			}
		}
		else {
			arg.append(*it);
			arg.append(" ");
		}
	}

	//ptr_sender->IPSend(&command[index + 1], args.length(), true);
	if (memory_transfer) {
		TransferMemoryData(mem_transfer_size);
	}
	else if (file_transfer) {

	}
	else {
		TransferString(arg, send_out_packets);
	}

	// Send result status back to the monitor
//	if (send_out_packets) {
//		SendMessage(COMMAND_RESPONSE, "Data sent.");
//	}
//	else {
//		SendMessage(COMMAND_RESPONSE, "Data recorded but not sent out (simulate packet loss).");
//	}

	return 1;
}

// Transfer memory-to-memory data to all receivers
// size: the size of data to transfer (in megabytes)
int SenderStatusProxy::TransferMemoryData(int size) {
	// Clear the send buffer
	ptr_sender->ResetBuffer();

	TransferMessage msg;
	msg.msg_type = MEMORY_TRANSFER;
	msg.data_len = size;
	ptr_sender->RawSend((char*)&msg, sizeof(msg), true);

	// Initialize the memory buffer
//	uint* mem_store = (uint*)malloc(size);
//  char* buffer = (char*)mem_store;
//	uint num_ints = size / sizeof(uint);
//	for (uint i = 0; i < num_ints; i++) {
//		mem_store[i] = i;
//	}


	char buffer[MVCTP_DATA_LEN];
	memset(buffer, 1, MVCTP_DATA_LEN);

	timeval last_time, cur_time;
	float time_diff;
	gettimeofday(&last_time, NULL);

	int period = size / MVCTP_DATA_LEN / 10;
	if (period == 0)
		period = 1;

	int send_count = 0;
	uint remained_size = size;
	uint period_sent_size = 0;
	uint total_sent_data = 0;
	while (remained_size > 0) {
		int packet_size = remained_size > MVCTP_ETH_FRAME_LEN ? MVCTP_ETH_FRAME_LEN
				: remained_size;
		ptr_sender->RawSend(buffer /*+ total_sent_data*/, packet_size - MVCTP_HLEN - ETH_HLEN, true);
		remained_size -= packet_size;
		total_sent_data += packet_size - MVCTP_HLEN - ETH_HLEN;

		// periodically calculate transfer speed
		period_sent_size += packet_size; //(packet_size + MVCTP_HLEN + ETH_HLEN);
		send_count++;
		if (send_count % period == 0) {
			gettimeofday(&cur_time, NULL);
			time_diff = (cur_time.tv_sec - last_time.tv_sec)
					+ (cur_time.tv_usec - last_time.tv_usec) / 1000000.0 + 0.001;

			last_time = cur_time;
			float rate = period_sent_size / time_diff / 1024.0 / 1024.0 * 8;
			period_sent_size = 0;
			char buf[100];
			sprintf(buf, "Data sending rate: %3.2f Mbps", rate);
			SendMessage(INFORMATIONAL, buf);
		}
	}

	//free(mem_store);
	SendMessage(COMMAND_RESPONSE, "Memory data transfer completed.");
	return 1;
}


int SenderStatusProxy::TransferString(string str, bool send_out_packets) {
	TransferMessage msg;
	msg.msg_type = STRING_TRANSFER;
	msg.data_len = str.length();
	ptr_sender->RawSend((char*)&msg, sizeof(msg), send_out_packets);
	ptr_sender->RawSend(str.c_str(), msg.data_len, send_out_packets);

	return 1;
}


