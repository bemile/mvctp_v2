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
#include "MvctpSenderMetadata.h"
#include "../CommUtil/PerformanceCounter.h"
#include "../CommUtil/StatusProxy.h"

typedef	void (*VCMTP_BOF_Function)();
typedef void (*VCMTP_Recv_Complete_Function)();

struct MvctpReceiverStats {
	uint	current_msg_id;
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

	// new stat fields
	PerformanceCounter	cpu_monitor;
	CpuCycleCounter		reset_cpu_timer;
	int					num_recved_files;
	int					num_failed_files;
	double				last_file_recv_time;
	vector<string>		session_stats_vec;
};


struct MessageReceiveStatus {
	uint 		msg_id;
	string		msg_name;
	union {
			void* 	mem_buffer;
			int		file_descriptor;
		};
	int			retx_file_descriptor;
	bool		is_multicast_done;
	long long 	msg_length;
	uint		current_offset;
	long long	multicast_packets;
	long long	multicast_bytes;
	long long 	retx_packets;
	long long 	retx_bytes;
	bool		recv_failed;
	CpuCycleCounter 	start_time_counter;
	double		multicast_time;
};


struct MvctpReceiverConfig {
	string 	multicast_addr;
	string  sender_ip_addr;
	int		sender_tcp_port;
	int		receive_mode;
	VCMTP_BOF_Function    			bof_function;
	VCMTP_Recv_Complete_Function	complete_function;
};


// MVCTPReceiver is the main class that communicates with the MVCTP Sender
class MVCTPReceiver : public MVCTPComm {
public:
	MVCTPReceiver(int buf_size);
	virtual ~MVCTPReceiver();

	int 	JoinGroup(string addr, u_short port);
	int		ConnectSenderOnTCP();
	void 	Start();
	void	SetSchedRR(bool is_rr);

	void 	SetPacketLossRate(int rate);
	int 	GetPacketLossRate();
	void	SetBufferSize(size_t size);
	void 	SendHistoryStats();
	void	ResetHistoryStats();
	void	SendHistoryStatsToSender();
	void 	SendSessionStatistics();
	void	ResetSessionStatistics();
	void 	AddSessionStatistics(uint msg_id);
	void	SendSessionStatisticsToSender();
	void	ExecuteCommand(char* command);
	void 	SetStatusProxy(StatusProxy* proxy);
	const struct MvctpReceiverStats GetBufferStats();


private:
	TcpClient*		retrans_tcp_client;
	// used in the select() system call
	int			max_sock_fd;
	int 		multicast_sock;
	int			retrans_tcp_sock;
	fd_set		read_sock_set;
	ofstream 	retrans_info;

	int 				packet_loss_rate;
	uint				session_id;
	MvctpReceiverStats 	recv_stats;
	CpuCycleCounter		cpu_counter;
	StatusProxy*		status_proxy;

	PerformanceCounter 	cpu_info;


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


	// Receive status map for all active files
	map<uint, MessageReceiveStatus> recv_status_map;

	// File descriptor map for the main RECEIVING thread. Format: <msg_id, file_descriptor>
	map<uint, int> 	recv_file_map;

	char read_ahead_buffer[MVCTP_PACKET_LEN];
	MvctpHeader* read_ahead_header;
	char* read_ahead_data;



	//*********************** Main receiving thread functions ***********************
	pthread_t	recv_thread;
	void 	StartReceivingThread();
	static void* StartReceivingThread(void* ptr);
	void	RunReceivingThread();
	void	HandleMulticastPacket();
	void	HandleUnicastPacket();
	void	HandleBofMessage(MvctpSenderMessage& sender_msg);
	void  	HandleEofMessage(uint msg_id);
	void	PrepareForFileTransfer(MvctpSenderMessage& sender_msg);
	void	HandleSenderMessage(MvctpSenderMessage& sender_msg);
	void	AddRetxRequest(uint msg_id, uint current_offset, uint received_seq);

	//*********************** Retransmission thread functions ***********************
	void 	StartRetransmissionThread();
	static void* StartRetransmissionThread(void* ptr);
	void	RunRetransmissionThread();
	pthread_t			retrans_thread;
	pthread_mutex_t 	retrans_list_mutex;
	bool				keep_retrans_alive;
	list<MvctpRetransRequest> 	retrans_list;

	int		mvctp_seq_num;
	size_t	total_missing_bytes;
	size_t	received_retrans_bytes;
	bool	is_multicast_finished;
	bool	retrans_switch;		// a switch that allows/disallows on-the-fly retransmission

};

#endif /* MVCTPRECEIVER_H_ */
