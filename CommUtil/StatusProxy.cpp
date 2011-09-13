/*
 * StatusProxy.cpp
 *
 *  Created on: Jun 25, 2011
 *      Author: jie
 */

#include "StatusProxy.h"

StatusProxy::StatusProxy(string addr, int port) {
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family 	 = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(addr.c_str());
	servaddr.sin_port 		 = htons(port);

	sockfd = -1;
	isConnected = false;
}

StatusProxy::~StatusProxy() {

}


int StatusProxy::ConnectServer() {
	if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		cout << "socket() error" << endl;
		return -1;
	}

	int res;
	while ((res = connect(sockfd, (SA *) &servaddr, sizeof(servaddr))) < 0) {
		cout << "connect() error" << endl;
		sleep(15);
		//return -1;
	}

	isConnected = true;
	return 1;
}


int StatusProxy::SendMessage(int msg_type, string msg) {
	int res;
	if ( (res = write(sockfd, &msg_type, sizeof(msg_type))) < 0) {
		cout << "Error sending message. " << endl;
		ReconnectServer();
		return -1;
	}

	int length = msg.length();
	if ( (res = write(sockfd, &length, sizeof(length))) < 0) {
		cout << "Error sending message. " << endl;
		ReconnectServer();
		return -1;
	}

	if ( (res = write(sockfd, msg.c_str(), length)) < 0) {
		cout << "Error sending message. " << endl;
		ReconnectServer();
		return -1;
	}
	else
		return 1;
}


void StatusProxy::StopService() {
	keep_alive = false;
	is_connected = false;
	close(sockfd);
}


void StatusProxy::StartService() {
	if (!isConnected)
		return;

	// Start the command execution thread
	keep_alive = true;
	pthread_create(&command_exec_thread, NULL, &StatusProxy::StartCommandExecThread, this);

	// Send node identity to the server
	SendNodeInfo();
}

int StatusProxy::SendNodeInfo() {
	struct utsname host_name;
	uname(&host_name);
	SendMessage(NODE_NAME, host_name.nodename);
	return 1;
}


void* StatusProxy::StartCommandExecThread(void* ptr) {
	((StatusProxy*)ptr)->RunCommandExecThread();
	return NULL;
}


void StatusProxy::RunCommandExecThread() {
	while (keep_alive) {
		int res;
		int msg_type;

		if ((res = read(sockfd, &msg_type, sizeof(msg_type))) < 0) {
			cout << "read() error." << endl;
			ReconnectServer();
			continue;
			//return;
		}

		int msg_length;
		if ((res = read(sockfd, &msg_length, sizeof(msg_length))) < 0) {
			cout << "read() error." << endl;
			ReconnectServer();
			continue;
		}

		char buffer[BUFFER_SIZE];
		if ((res = read(sockfd, buffer, msg_length)) < 0) {
			cout << "read() error." << endl;
			ReconnectServer();
			continue;
		} else {
			buffer[res] = '\0';
		}

		switch (msg_type) {
		case COMMAND:
		case PARAM_SETTING:
			HandleCommand(buffer);
			break;
		default:
			break;
		}
	}
}


void StatusProxy::ReconnectServer() {
	if (errno == EINTR)
		return;

	close(sockfd);
	is_connected = false;
	ConnectServer();
	SendNodeInfo();
	SendMessage(INFORMATIONAL, "Socket error. Service reconnected.");
}


int StatusProxy::HandleCommand(char* command) {
	string s = command;
	list<string> parts;
	Split(s, ' ', parts);
	if (parts.size() == 0)
		return 0;

	if (parts.front().compare("Restart") == 0) {
		HandleRestartCommand();
	} else {
		ExecSysCommand(command);
	}

	return 1;
}


//
void StatusProxy::HandleRestartCommand() {
	//TODO: Ugly way to close all opened file descriptors (sockets).
	//How to fix it?
	for (int i = 3; i < 1000; i++) {
		close(i);
	}

	// Restart the emulab test
	chdir("/users/jieli/bin");
	execl("/bin/sh", "sh", "/users/jieli/bin/run_starter.sh", (char *) 0);
	exit(0);
}



int StatusProxy::ExecSysCommand(char* command) {
	FILE* pFile = popen(command, "r");
	if (pFile != NULL) {
		char output[BUFFER_SIZE];
		int bytes = fread(output, 1, BUFFER_SIZE, pFile);
		output[bytes] = '\0';
		pclose(pFile);
		SendMessage(COMMAND_RESPONSE, output);
	} else {
		cout << "Cannot get output from execution." << endl;
	}
	return 1;
}


// Divide string s into sub strings separated by the character c
void StatusProxy::Split(string s, char c, list<string>& slist) {
	const char* ptr = s.c_str();
	int start = 0;
	int cur_pos = 0;
	for (; *ptr != '\0'; ptr++) {
		if (*ptr == c) {
			if (cur_pos != start) {
				string subs = s.substr(start, cur_pos - start);
				slist.push_back(subs);
			}
			start = cur_pos + 1;
		}

		cur_pos++;
	}

	if (cur_pos != start) {
		string subs = s.substr(start, cur_pos - start);
		slist.push_back(subs);
	}
}

