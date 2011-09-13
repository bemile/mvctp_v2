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

protected:
	virtual int HandleCommand(char* command);

private:
	MVCTPReceiver* ptr_receiver;
};

#endif /* RECEIVERSTATUSPROXY_H_ */
