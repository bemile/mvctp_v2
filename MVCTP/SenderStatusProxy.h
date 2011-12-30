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
	SenderStatusProxy(string addr, int port, string group_addr, int mvctp_port, int buff_size);

protected:
	virtual int HandleCommand(const char* command);
	int 	HandleSendCommand(list<string>& slist);
	int 	HandleTcpSendCommand(list<string>& slist);
	void 	SetSendRate(int rate_mbps);

	int 	TransferString(string str, bool send_out_packets);
	int 	TransferMemoryData(int size);
	void 	TransferFile(string file_name);
	int 	TcpTransferMemoryData(int size);
	void 	TcpTransferFile(string file_name);
	void 	SendMemoryData(void* buffer, size_t length);

	virtual void InitializeExecutionProcess();

private:
	MVCTPSender* ptr_sender;
	string 		mvctp_group_addr;
	int			mvctp_port_num;
	int			buffer_size;

	void	ConfigureEnvironment();
	int 	GenerateDataFile(string file_name, ulong bytes);
};


#endif /* SENDERSTATUSPROXY_H_ */
