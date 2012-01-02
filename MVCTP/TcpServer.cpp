/*
 * TcpServer.cpp
 *
 *  Created on: Sep 15, 2011
 *      Author: jie
 */

#include "TcpServer.h"

TcpServer::TcpServer(int port) {
	port_num = port;
	if ( (server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		SysError("TcpServer::TcpServer()::socket() error");
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port_num);

	while (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		//SysError("TcpServer::TcpServer()::bind() error");
		cout << "TcpServer::TcpServer()::bind() error" << endl;
		sleep(10);
	}

	// Used in the select() system call
	max_conn_sock = -1;
}

TcpServer::~TcpServer() {
	list<int>::iterator it;
	for (it = conn_sock_list.begin(); it != conn_sock_list.end(); it++) {
		close(*it);
	}
	close(server_sock);
}


list<int> TcpServer::GetSocketList() {
	return conn_sock_list;
}

void TcpServer::Listen()	 {
	if (listen(server_sock, 200) < 0) {
		SysError("TcpServer::Listen()::listen() error");
	}
}


int TcpServer::Accept() {
	int conn_sock;
	if ( (conn_sock = accept(server_sock, (struct sockaddr*)NULL, NULL))< 0) {
		SysError("TcpServer::Accept()::accept() error");
	}

	pthread_mutex_lock(&sock_list_mutex);
	FD_SET(conn_sock, &master_read_fds);
	if (conn_sock > max_conn_sock) {
		max_conn_sock = conn_sock;
	}
	conn_sock_list.push_back(conn_sock);
	pthread_mutex_unlock(&sock_list_mutex);

	return conn_sock;
}


void TcpServer::SendToAll(const void* data, size_t length) {
	list<int> bad_sock_list;

	list<int>::iterator it;

	cout << "Current sockets: ";
	for (it = conn_sock_list.begin(); it != conn_sock_list.end(); it++) {
		cout << *it << "    ";
	}
	cout << endl;

	pthread_mutex_lock(&sock_list_mutex);
	for (it = conn_sock_list.begin(); it != conn_sock_list.end(); it++) {
		if (send(*it, data, length, 0) <= 0) {
			bad_sock_list.push_back(*it);
		}
		cout << "Message sent to socket " << *it << endl;
	}

	for (it = bad_sock_list.begin(); it != bad_sock_list.end(); it++) {
		close(*it);
		conn_sock_list.remove(*it);
		FD_CLR(*it, &master_read_fds);
		cout << "TcpServer::SendToAll()::One socket deleted: " << *it << endl;
	}
	pthread_mutex_unlock(&sock_list_mutex);
}


int TcpServer::SelectSend(int conn_sock, const void* data, size_t length) {
	int res = send(conn_sock, data, length, 0);
	if (res <= 0 && length > 0) {
		pthread_mutex_lock(&sock_list_mutex);
		close(conn_sock);
		conn_sock_list.remove(conn_sock);
		FD_CLR(conn_sock, &master_read_fds);
		cout << "TcpServer::SelectSend()::One socket deleted: " << conn_sock << endl;
		pthread_mutex_unlock(&sock_list_mutex);
	}
	return res;
}


int TcpServer::SelectReceive(int* conn_sock, void* buffer, size_t length) {
	fd_set read_fds = master_read_fds;
	if (select(max_conn_sock + 1, &read_fds, NULL, NULL, NULL) == -1) {
		SysError("TcpServer::SelectReceive::select() error");
	}

	list<int>::iterator it;
	int res = 0;
	list<int> bad_socks;
	pthread_mutex_lock(&sock_list_mutex);
	for (it = conn_sock_list.begin(); it != conn_sock_list.end(); it++) {
		if (FD_ISSET(*it, &read_fds)) {
			res = recv(*it, buffer, length, MSG_WAITALL);
			if (res <= 0) {
				bad_socks.push_back(*it);
			}

			*conn_sock = *it;
			break;
		}
	}

	// remove all broken sockets
	for (it = bad_socks.begin(); it != bad_socks.end(); it++) {
		conn_sock_list.remove(*it);
		close(*it);
		FD_CLR(*it, &master_read_fds);
		cout << "SelectReceive()::One broken socket deleted: " << *it << endl;
	}
	pthread_mutex_unlock(&sock_list_mutex);

	return res;
}

void TcpServer::SysError(char* info) {
	perror(info);
	exit(-1);
}


//====================== A separate thread that accepts new client requests =======================
void TcpServer::Start() {
	pthread_mutex_init(&sock_list_mutex, 0);

	int res;
	if ( (res= pthread_create(&server_thread, NULL, &TcpServer::StartServerThread, this)) != 0) {
		SysError("TcpServer::Start()::pthread_create() error");
	}
}

void* TcpServer::StartServerThread(void* ptr) {
	((TcpServer*)ptr)->AcceptClients();
	pthread_exit(NULL);
}


void TcpServer::AcceptClients() {
	Listen();

	while (true) {
		Accept();
	}
}


