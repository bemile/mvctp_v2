/*
 * TcpClient.cpp
 *
 *  Created on: Sep 15, 2011
 *      Author: jie
 */

#include "TcpClient.h"

TcpClient::TcpClient(string serv_addr, int port) {
	server_port = port;
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		SysError("TcpClient::TcpClient()::socket() error");
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);

	in_addr_t temp_addr = inet_addr(serv_addr.c_str());
	if (temp_addr == -1) {
		struct hostent* ptrhost = gethostbyname(serv_addr.c_str());
		if (ptrhost != NULL) {
			temp_addr = ((in_addr*)ptrhost->h_addr_list[0])->s_addr;
		}
	}
	server_addr.sin_addr.s_addr = temp_addr;

}

TcpClient::~TcpClient() {
	// TODO Auto-generated destructor stub
}


int TcpClient::Connect() {
	int res;
	while ((res = connect(sock_fd, (sockaddr *) &server_addr,
			sizeof(server_addr))) < 0) {
		cout << "TcpClient::Connect()::connect() error" << endl;
		sleep(15);
		//return -1;
	}
	return res;
}


int TcpClient::Send(const void* data, size_t length) {
	return send(sock_fd, data, length, 0);
}


int TcpClient::Receive(void* buffer, size_t length) {
	return recv(sock_fd, buffer, length, 0);
}

void TcpClient::SysError(char* info) {
	perror(info);
	exit(-1);
}
