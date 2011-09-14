/*
 * SenderStatusProxy.h
 *
 *  Created on: Jun 28, 2011
 *      Author: jie
 */

#ifndef SENDERSTATUSPROXY_H_
#define SENDERSTATUSPROXY_H_

#include "../CommUtil/StatusProxy.h"
#include "MVCTPSender.h"
#include <sys/time.h>


enum MsgTag {
	MSG_START = 1010101010,
	MSG_END = 1111111111
};

enum TransferMsgType {
	STRING_TRANSFER,
	MEMORY_TRANSFER,
	FILE_TRANSFER
};

struct TransferMessage {
	 TransferMsgType	msg_type;
	 size_t 			data_len;
	 char       		file_name[30];
};


class SenderStatusProxy : public StatusProxy {
public:
	SenderStatusProxy(string addr, int port, MVCTPSender* psender);

protected:
	virtual int HandleCommand(char* command);
	int HandleSendCommand(list<string>& slist);

	int TransferString(string str, bool send_out_packets);
	int TransferMemoryData(int size);
	void TransferFile(string file_name);
	void SendMemoryData(void* buffer, size_t length);

private:
	MVCTPSender* ptr_sender;

	int GenerateDataFile(string file_name, int size_mb);
};


#endif /* SENDERSTATUSPROXY_H_ */
