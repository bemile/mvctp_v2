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

	// adjust udp (for multicasting) and tcp (for retransmission) receive buffer sizes
	system("sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 16777216\"");
	system("sudo sysctl -w net.ipv4.tcp_rmem=\"4096 8388608 16777216\"");
	system("sudo sysctl -w net.ipv4.tcp_wmem=\"4096 8388608 16777216\"");
	system("sudo sysctl -w net.core.rmem_default=\"8388608\"");
	system("sudo sysctl -w net.core.rmem_max=\"16777216\"");
	system("sudo sysctl -w net.core.wmem_default=\"8388608\"");
	system("sudo sysctl -w net.core.wmem_max=\"16777216\"");

	char command[256];
	sprintf(command, "sudo ifconfig %s txqueuelen 10000", ptr_sender->GetInterfaceName().c_str());
	system(command);
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
	else if (parts.front().compare("TcpSend") == 0) {
			parts.erase(parts.begin());
			HandleTcpSendCommand(parts);
	}
	else if (parts.front().compare("SetRate") == 0) {
		if (parts.size() == 2) {
			int rate = atoi(parts.back().c_str());
			SetSendRate(rate); //ptr_sender->SetSendRate(rate);
			sprintf(msg, "Data sending rate has been set to %d Mbps.", rate);
			SendMessage(COMMAND_RESPONSE, msg);
		}
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
	else if (parts.front().compare("CreateDataFile") == 0) {
		if (parts.size() == 3) {
			parts.pop_front();
			string file_name = parts.front();
			parts.pop_front();
			unsigned long size = strtoul(parts.front().c_str(), NULL, 0);
			GenerateDataFile(file_name, size);
			SendMessage(COMMAND_RESPONSE, "Data file generated.");
		}
	}
	else {
		StatusProxy::HandleCommand(command);
	}

	return 1;
}


void SenderStatusProxy::SetSendRate(int rate_mbps) {
	double MBps = rate_mbps / 8.0;
	char rate[25];
	sprintf(rate, "%.2fMbps", MBps);

	char command[256];
	sprintf(command, "sudo ./rate-limit.sh %s %d %d %s", ptr_sender->GetInterfaceName().c_str(),
					PORT_NUM, BUFFER_TCP_SEND_PORT, rate);
	ExecSysCommand(command);
}


//
int SenderStatusProxy::HandleSendCommand(list<string>& slist) {
	bool memory_transfer = false;
	bool file_transfer = false;
	bool send_out_packets = true;

	int 	mem_transfer_size = 0;
	string	file_name;

	string arg = "";
	list<string>::iterator it;
	for (it = slist.begin(); it != slist.end(); it++) {
		if ((*it)[0] == '-') {
			switch ((*it)[1]) {
			case 'm':
				memory_transfer = true;
				it++;
				mem_transfer_size = atoi((*it).c_str());	// in Megabytes
				break;
			case 'f':
				file_transfer = true;
				it++;
				file_name = *it;
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
		TransferFile(file_name);
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
	SendMessage(INFORMATIONAL, "Transferring memory data...");

	char* buffer = (char*)malloc(size);
	ptr_sender->SendMemoryData(buffer, size);
	free(buffer);

	SendMessage(COMMAND_RESPONSE, "Memory data transfer completed.");
	return 1;
}


// Transfer a disk file to all receivers
void SenderStatusProxy::TransferFile(string file_name) {
	system("sudo sync && sudo echo 3 > /proc/sys/vm/drop_caches");

	SendMessage(INFORMATIONAL, "Transferring file...");
	ptr_sender->SendFile(file_name.c_str());
	SendMessage(COMMAND_RESPONSE, "File transfer completed.");

}



// Multicast a string message to receivers
int SenderStatusProxy::TransferString(string str, bool send_out_packets) {
	SendMessage(COMMAND_RESPONSE, "Specified string successfully sent.");
	return 1;
}


// Send data using TCP connections (for performance comparison)
int SenderStatusProxy::HandleTcpSendCommand(list<string>& slist) {
	bool memory_transfer = false;
	bool file_transfer = false;
	bool send_out_packets = true;

	int mem_transfer_size = 0;
	string file_name;

	string arg = "";
	list<string>::iterator it;
	for (it = slist.begin(); it != slist.end(); it++) {
		if ((*it)[0] == '-') {
			switch ((*it)[1]) {
			case 'm':
				memory_transfer = true;
				it++;
				mem_transfer_size = atoi((*it).c_str()); // in Megabytes
				break;
			case 'f':
				file_transfer = true;
				it++;
				file_name = *it;
				break;
			case 'n':
				send_out_packets = false;
				break;
			default:
				break;
			}
		} else {
			arg.append(*it);
			arg.append(" ");
		}
	}

	//ptr_sender->IPSend(&command[index + 1], args.length(), true);
	if (memory_transfer) {
		TcpTransferMemoryData(mem_transfer_size);
	} else if (file_transfer) {
		TcpTransferFile(file_name);
	} else {
	}
}


int SenderStatusProxy::TcpTransferMemoryData(int size) {
	SendMessage(INFORMATIONAL, "Transferring memory data...\n");

	char* buffer = (char*)malloc(size);
	ptr_sender->TcpSendMemoryData(buffer, size);
	free(buffer);

	SendMessage(COMMAND_RESPONSE, "Memory data transfer completed.\n\n");
	return 1;
}


void SenderStatusProxy::TcpTransferFile(string file_name) {
	SendMessage(INFORMATIONAL, "Transferring file...\n");
	ptr_sender->TcpSendFile(file_name.c_str());
	SendMessage(COMMAND_RESPONSE, "File transfer completed.\n\n");
}



// Generate a local data file for disk-to-disk transfer experiments
int SenderStatusProxy::GenerateDataFile(string file_name, ulong bytes) {
	int buf_size = 256;
	char buffer[buf_size];
	for (int i = 0; i < buf_size; i++) {
		buffer[i] = i;
	}

	ulong remained_size = bytes;
	ofstream myfile(file_name.c_str(), ios::out | ios::trunc);
	if (myfile.is_open()) {
		while (remained_size > 0) {
			ulong len = remained_size < buf_size ? remained_size : buf_size;
			myfile.write(buffer, len);
			remained_size -= len;
		}

		myfile.close();
		return 1;
	}
	else {
		return -1;
	}
}




