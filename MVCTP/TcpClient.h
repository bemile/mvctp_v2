/*
 * TcpClient.h
 *
 *  Created on: Sep 15, 2011
 *      Author: jie
 */

#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include <iostream>
#include <list>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

class TcpClient {
public:
	TcpClient(string serv_addr, int port);
	virtual ~TcpClient();

	int Connect();
	int GetSocket();

	int Send(const void* data, size_t length);
	int Receive(void* buffer, size_t length);

private:
	sockaddr_in server_addr;
	int server_port;
	int sock_fd;


	void SysError(char* info);
};

#endif /* TCPCLIENT_H_ */
