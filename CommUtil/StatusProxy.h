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
#include <signal.h>
#include <sys/types.h>

using namespace std;

typedef struct sockaddr SA;


class StatusProxy {
public:
	StatusProxy(string addr, int port);
	~StatusProxy();

	int ConnectServer();
	void StartService();
	void StopService();
	int SendMessageToManager(int msg_type, string msg);
	int ReadMessageFromManager(int& msg_type, string& msg);
	int SendMessageLocal(int msg_type, string msg);
	int ReadMessageLocal(int& msg_type, string& msg);

protected:
	int sockfd;
	struct sockaddr_in servaddr;
	bool isConnected;

	bool proxy_started;
	bool keep_alive;
	bool is_connected;

	// pipes used for message communication
	int		read_pipe_fds[2];
	int		write_pipe_fds[2];
	int		read_pipe_fd;
	int		write_pipe_fd;

	int 	execution_pid;

	pthread_t manager_send_thread;
	static void* StartManagerSendThread(void* ptr);
	void RunManagerSendThread();

	pthread_t manager_recv_thread;
	static void* StartManagerReceiveThread(void* ptr);
	void RunManagerReceiveThread();

	pthread_t proc_exec_thread;
	static void* StartProcessExecutionThread(void* ptr);
	void RunProcessExecutionThread();
	void StartExecutionProcess();
	virtual void InitializeExecutionProcess();

	void ReconnectServer();
	virtual int HandleCommand(const char* command);
	void HandleRestartCommand();
	int ExecSysCommand(const char* command);

	int SendNodeInfo();
	void Split(string s, char c, list<string>& slist);
	void SysError(string s);


private:

};


#endif /* STATUSPROXY_H_ */
