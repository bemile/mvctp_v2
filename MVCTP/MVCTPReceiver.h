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
#include "TcpClient.h"
#include "../CommUtil/PerformanceCounter.h"
#include "../CommUtil/StatusProxy.h"

typedef	void (*VCMTP_BOF_Function)();
typedef void (*VCMTP_Recv_Complete_Function)();

struct MvctpReceiverStats {
	uint 	total_recv_packets;
	uint	total_recv_bytes;
	uint 	total_retrans_packets;
	uint	total_retrans_bytes;
	uint 	session_recv_packets;
	uint	session_recv_bytes;
	uint 	session_retrans_packets;
	uint	session_retrans_bytes;
	double	session_retrans_percentage;
	double	session_total_time;
	double	session_trans_time;
	double 	session_retrans_time;
};


struct MessageReceiveStatus {
	uint 		msg_id;
	long long 	msg_length;
	long long	received_bytes;
};


struct MvctpReceiverConfig {
	string 	multicast_addr;
	string  sender_ip_addr;
	int		sender_tcp_port;
	int		receive_mode;
	VCMTP_BOF_Function    			bof_function;
	VCMTP_Recv_Complete_Function	complete_function;
};


class MVCTPReceiver : public MVCTPComm {
public:
	MVCTPReceiver(int buf_size);
	~MVCTPReceiver();

	int 	JoinGroup(string addr, u_short port);
	int		ConnectSenderOnTCP();
	void 	Start();
	void	SetSchedRR(bool is_rr);

	void 	SetPacketLossRate(int rate);
	int 	GetPacketLossRate();
	void	SetBufferSize(size_t size);
	void 	SendSessionStatistics();
	void	ResetSessionStatistics();
	void	SendStatisticsToSender();
	void	ExecuteCommand(char* command);
	void 	SetStatusProxy(StatusProxy* proxy);
	const struct MvctpReceiverStats GetBufferStats();


private:
	TcpClient*		retrans_tcp_client;
	// used in the select() system call
	int		max_sock_fd;
	int 	multicast_sock;
	int		retrans_tcp_sock;
	fd_set	read_sock_set;
	ofstream retrans_info;

	int 				packet_loss_rate;
	uint				session_id;
	MvctpReceiverStats 	recv_stats;
	CpuCycleCounter		cpu_counter;
	StatusProxy*		status_proxy;

	PerformanceCounter 	cpu_info;
	map<uint, MessageReceiveStatus> recv_status_map;

	void 	ReconnectSender();

	// Memory-to-memory data tranfer
	void 	ReceiveMemoryData(const MvctpSenderMessage & msg, char* mem_data);
	void 	DoMemoryDataRetransmission(char* mem_data, const list<MvctpNackMessage>& nack_list);
	// Disk-to-disk data transfer
	void 	ReceiveFileBufferedIO(const MvctpSenderMessage & transfer_msg);
	void 	ReceiveFileMemoryMappedIO(const MvctpSenderMessage & transfer_msg);
	void 	DoFileRetransmission(int fd, const list<MvctpNackMessage>& nack_list);

	void 	DoAsynchronousWrite(int fd, size_t offset, char* data_buffer, size_t length);
	static void HandleAsyncWriteCompletion(sigval_t sigval);
	void	CheckReceivedFile(const char* file_name, size_t length);
	void	SendNackMessages(const list<MvctpNackMessage>& nack_list);
	void 	HandleMissingPackets(list<MvctpNackMessage>& nack_list, uint current_offset, uint received_seq);

	// Functions related to TCP data transfer
	void 	TcpReceiveMemoryData(const MvctpSenderMessage & msg, char* mem_data);
	void 	TcpReceiveFile(const MvctpSenderMessage & transfer_msg);

	MvctpSenderMessage ReceiveBOF();


	// Retransmission thread functions
	int		recv_fd;
	void 	StartRetransmissionThread();
	static void* StartRetransmissionThread(void* ptr);
	void	RunRetransmissionThread();
	pthread_t	retrans_thread;
	pthread_mutex_t retrans_list_mutex;
	bool	keep_retrans_alive;
	list<MvctpRetransRequest> retrans_list;
	int		mvctp_seq_num;
	size_t	total_missing_bytes;
	size_t	received_retrans_bytes;
	bool	is_multicast_finished;
	bool	retrans_switch;		// a switch that allows/disallows on-the-fly retransmission
};

#endif /* MVCTPRECEIVER_H_ */
