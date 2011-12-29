/*
 * ReceiverStatusProxy.h
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#ifndef RECEIVERSTATUSPROXY_H_
#define RECEIVERSTATUSPROXY_H_

#include "../CommUtil/StatusProxy.h"
#include "MVCTPReceiver.h"
#include <sys/time.h>

class ReceiverStatusProxy : public StatusProxy {
public:
	ReceiverStatusProxy(string addr, int port, MVCTPReceiver* preceiver);
	ReceiverStatusProxy(string addr, int port, string group_addr, int mvctp_port, int buff_size);

protected:
	virtual int 	HandleCommand(char* command);
	virtual void 	InitializeExecutionProcess();

private:
	MVCTPReceiver* ptr_receiver;
	string 		mvctp_group_addr;
	int			mvctp_port_num;
	int			buffer_size;

	void	ConfigureEnvironment();
};

#endif /* RECEIVERSTATUSPROXY_H_ */
