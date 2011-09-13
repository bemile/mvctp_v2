/*
 * MVCTPSender.h
 *
 *  Created on: Jul 21, 2011
 *      Author: jie
 */

#ifndef MVCTPSENDER_H_
#define MVCTPSENDER_H_

#include "mvctp.h"
#include "MVCTPComm.h"
#include "SendBufferMgr.h"

class MVCTPSender : public MVCTPComm {
public:
	MVCTPSender(int buf_size);
	~MVCTPSender();

	int RawSend(const char* data, size_t length, bool send_out);
	int IPSend(const char* data, size_t length, bool send_out);
	void SetSendRate(int num_mbps);
	void SetBufferSize(size_t buff_size);
	void ResetBuffer();

private:
	//MVCTPComm* ptr_mvctp_comm;
	SendBufferMgr *ptr_send_buf_mgr;
	int send_rate_in_mbps;

	// Should not be called publicly, may be deleted later
	int RawReceive(void* buff, size_t len);
};

#endif /* MVCTPSENDER_H_ */
