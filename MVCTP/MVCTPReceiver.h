/*
 * MVCTPReceiver.h
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#ifndef MVCTPRECEIVER_H_
#define MVCTPRECEIVER_H_

#include "mvctp.h"
#include "MVCTPComm.h"
#include "ReceiveBufferMgr.h"

class MVCTPReceiver : public MVCTPComm {
public:
	MVCTPReceiver(int buf_size);
	~MVCTPReceiver();

	int JoinGroup(string addr, ushort port);
	int RawReceive(void* buff, size_t len, int flags, SA* from, socklen_t* from_len);
	int IPReceive(void* buff, size_t len, int flags, SA* from, socklen_t* from_len);
	ReceiveBufferMgr* GetReceiveBufferManager();
	void SetBufferSize(size_t buff_size);
	size_t GetBufferSize();
	void SetPacketLossRate(int rate);
	int GetPacketLossRate();
	const struct ReceiveBufferStats GetBufferStats();
	void SetSocketBufferSize(size_t buff_size);
	void ResetBuffer();

private:
	ReceiveBufferMgr *ptr_recv_buf_mgr;

	// Should not be called publicly, may be deleted later
	int RawSend(char* data, size_t length, bool send_out);
};

#endif /* MVCTPRECEIVER_H_ */
