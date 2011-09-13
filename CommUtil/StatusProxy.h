/*
 * StatusProxy.h
 *
 *  Created on: Jun 25, 2011
 *      Author: jie
 */

#ifndef STATUSPROXY_H_
#define STATUSPROXY_H_

#include "CommUtil.h"
#include <list>

using namespace std;

typedef struct sockaddr SA;


class StatusProxy {
public:
	StatusProxy(string addr, int port);
	~StatusProxy();

	int ConnectServer();
	void StartService();
	void StopService();
	int SendMessage(int msg_type, string msg);


protected:
	int sockfd;
	struct sockaddr_in servaddr;
	bool isConnected;

	bool keep_alive;
	bool is_connected;

	pthread_t command_exec_thread;
	static void* StartCommandExecThread(void* ptr);
	void RunCommandExecThread();
	void ReconnectServer();

	virtual int HandleCommand(char* command);
	void HandleRestartCommand();
	int ExecSysCommand(char* command);

	int SendNodeInfo();
	void Split(string s, char c, list<string>& slist);
};


#endif /* STATUSPROXY_H_ */
