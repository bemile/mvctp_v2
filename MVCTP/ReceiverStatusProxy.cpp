/*
 * ReceiverStatusProxy.cpp
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#include "ReceiverStatusProxy.h"

ReceiverStatusProxy::ReceiverStatusProxy(string addr, int port, MVCTPReceiver* preceiver)
		: StatusProxy(addr, port) {
	ptr_receiver = preceiver;


}


ReceiverStatusProxy::ReceiverStatusProxy(string addr, int port, string group_addr, int mvctp_port, int buff_size)
		: StatusProxy(addr, port) {
	mvctp_group_addr = group_addr;
	mvctp_port_num = mvctp_port;
	buffer_size = buff_size;
	ptr_receiver = NULL;

	ConfigureEnvironment();
}


void ReceiverStatusProxy::ConfigureEnvironment() {
	// adjust udp (for multicasting) and tcp (for retransmission) receive buffer sizes
	system("sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 36777216\""); // 16777216
	system("sudo sysctl -w net.ipv4.tcp_rmem=\"4096 8388608 36777216\"");
	system("sudo sysctl -w net.ipv4.tcp_wmem=\"4096 8388608 36777216\"");
	system("sudo sysctl -w net.core.rmem_default=\"16777216\""); //8388608
	system("sudo sysctl -w net.core.rmem_max=\"36777216\""); // 16777216
	system("sudo sysctl -w net.core.wmem_default=\"16777216\""); //8388608
	system("sudo sysctl -w net.core.wmem_max=\"36777216\""); //

	char command[256];
	sprintf(command, "sudo ifconfig %s txqueuelen 10000",
			ptr_receiver->GetInterfaceName().c_str());
	system(command);
}


//
void ReceiverStatusProxy::InitializeExecutionProcess() {
	if (ptr_receiver != NULL)
			delete ptr_receiver;

	ptr_receiver = new MVCTPReceiver(buffer_size);
	ptr_receiver->SetStatusProxy(this);
	ptr_receiver->JoinGroup(mvctp_group_addr, mvctp_port_num);
	ptr_receiver->Start();
}


int ReceiverStatusProxy::HandleCommand(char* command) {
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
	if (parts.front().compare("SetLossRate") == 0) {
		if (parts.size() == 2) {
			int rate = atoi(parts.back().c_str());
			ptr_receiver->SetPacketLossRate(rate);
			sprintf(msg, "Packet loss rate has been set to %d per thousand.", rate);
			SendMessageLocal(COMMAND_RESPONSE, msg);
		}
		else {
			SendMessageLocal(COMMAND_RESPONSE, "Usage: SetLossRate lost_packets_per_thousand)");
		}
	}
	else if (parts.front().compare("GetLossRate") == 0) {
		int rate = ptr_receiver->GetPacketLossRate();
		sprintf(msg, "Packet loss rate: %d per thousand.", rate);
		SendMessageLocal(COMMAND_RESPONSE, msg);
	}
	else if (parts.front().compare("SetBufferSize") == 0) {
		if (parts.size() == 2) {
			int buf_size = atoi(parts.back().c_str());
			ptr_receiver->SetBufferSize(buf_size);
			sprintf(msg, "Receive buffer size has been set to %d.", buf_size);
			SendMessageLocal(COMMAND_RESPONSE, msg);
		}
		else {
			SendMessageLocal(COMMAND_RESPONSE, "Usage: SetBufferSize size_in_bytes");
		}
	}
	else if (parts.front().compare("CreateLogFile") == 0) {
		if (parts.size() == 2) {
			CreateNewLogFile(parts.back().c_str());
			SendMessageLocal(COMMAND_RESPONSE, "New log file created.");
		}
	} else if (parts.front().compare("SetLogSwitch") == 0) {
		if (parts.size() == 2) {
			if (parts.back().compare("On") == 0) {
				MVCTP::is_log_enabled = true;
			} else if (parts.back().compare("Off") == 0) {
				MVCTP::is_log_enabled = false;
			}
			SendMessageLocal(COMMAND_RESPONSE, "Log switch set.");
		}
	} else {
		StatusProxy::HandleCommand(command);
	}

	return 1;
}
