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
	if (parts.front().compare("SetRecvBuffSize") == 0) {
		if (parts.size() == 2) {
			size_t size = atoi(parts.back().c_str());
			ptr_receiver->SetBufferSize(size);
			sprintf(msg, "Receive buffer size has been set to %d bytes.", size);
			SendMessage(COMMAND_RESPONSE, msg);
		}
	}
	else if (parts.front().compare("SetSocketBuffSize") == 0) {
		if (parts.size() == 2) {
			size_t size = atoi(parts.back().c_str());
			ptr_receiver->SetSocketBufferSize(size);
			sprintf(msg, "Socket buffer size has been set to %d bytes.", size);
			SendMessage(COMMAND_RESPONSE, msg);
		}
	}
	else if (parts.front().compare("ResetBuffer") == 0) {
		ptr_receiver->ResetBuffer();
		SendMessage(COMMAND_RESPONSE, "Receive buffer has been reset.");
	}
	else if (parts.front().compare("SetLossRate") == 0) {
		int rate = atoi(parts.back().c_str());
		ptr_receiver->SetPacketLossRate(rate);
		sprintf(msg, "Packet loss rate has been set to %d per thousand.", rate);
		SendMessage(COMMAND_RESPONSE, msg);
	}
	else if (parts.front().compare("GetLossRate") == 0) {
		int rate = ptr_receiver->GetPacketLossRate();
		sprintf(msg, "Packet loss rate: %d per thousand.", rate);
		SendMessage(COMMAND_RESPONSE, msg);
	}
	else if (parts.front().compare("CreateLogFile") == 0) {
		if (parts.size() == 2) {
			CreateNewLogFile(parts.back().c_str());
			SendMessage(COMMAND_RESPONSE, "New log file created.");
		}
	} else if (parts.front().compare("SetLogSwitch") == 0) {
		if (parts.size() == 2) {
			if (parts.back().compare("On") == 0) {
				MVCTP::is_log_enabled = true;
			} else if (parts.back().compare("Off") == 0) {
				MVCTP::is_log_enabled = false;
			}
			SendMessage(COMMAND_RESPONSE, "Log switch set.");
		}
	} else {
		StatusProxy::HandleCommand(command);
	}

	return 1;
}
