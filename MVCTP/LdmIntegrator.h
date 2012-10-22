/*
 * LdmIntegrator.h
 *
 *  Created on: Oct 22, 2012
 *      Author: jie
 */

#ifndef LDMINTEGRATOR_H_
#define LDMINTEGRATOR_H_

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "MVCTPSender.h"

using namespace std;

#define SERVER_PORT 	12350
#define	BUFFER_SIZE		4096

class StatusProxy;

class LdmIntegrator {
public:
	LdmIntegrator(MVCTPSender* s, string save_path, StatusProxy* p);
	void Start();
	void Stop();
	~LdmIntegrator();

private:
	MVCTPSender* sender;
	string save_dir;
	bool keep_alive;
	bool recv_thread_exited;
	bool send_thread_exited;
	StatusProxy* proxy;

	pthread_mutex_t send_mutex;
	pthread_t  recv_thread;
	static void* StartReceiveThread(void* ptr);
	void RunReceiveThread();
	int GetFilesInDirectory(vector<string> &files);

	pthread_t  send_thread;
	static void* StartSendThread(void* ptr);
	void RunSendThread();

	void SendMessage(char* msg);
};

#endif /* LDMINTEGRATOR_H_ */
