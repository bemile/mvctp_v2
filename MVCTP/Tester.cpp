/*
 * Tester.cpp
 *
 *  Created on: Jun 29, 2011
 *      Author: jie
 */

#include "Tester.h"

Tester::Tester() {
}

Tester::~Tester() {
	delete ptr_status_proxy;
}

void Tester::StartTest() {
	MVCTPInit();

	string serv_addr = ConfigInfo::GetInstance()->GetValue("Monitor_Server");
	string port = ConfigInfo::GetInstance()->GetValue("Monitor_Server_Port");

	int send_buf_size = atoi(ConfigInfo::GetInstance()->GetValue("Send_Buffer_Size").c_str());
	int recv_buf_size = atoi(ConfigInfo::GetInstance()->GetValue("Recv_Buffer_Size").c_str());

	if (IsSender()) {
		ptr_mvctp_sender = new MVCTPSender(send_buf_size);
		ptr_mvctp_sender->JoinGroup(group_id, mvctp_port);
		ptr_mvctp_sender->SetSendRate(50);

		if (serv_addr.length() > 0) {
			ptr_status_proxy = new SenderStatusProxy(serv_addr, atoi(port.c_str()), ptr_mvctp_sender);
			ptr_status_proxy->ConnectServer();
			ptr_status_proxy->StartService();
		}

		this->SendMessage(INFORMATIONAL, "I'm the sender. Just joined the multicast group.");

		while (true) {
			sleep(1);
		}
	}
	else {  //Is receiver
		// Set the receiver process to real-time scheduling mode
//		struct sched_param sp;
//		sp.__sched_priority = sched_get_priority_max(SCHED_RR);
//		sched_setscheduler(0, SCHED_RR, &sp);

		ptr_mvctp_receiver = new MVCTPReceiver(recv_buf_size);
		ptr_mvctp_receiver->JoinGroup(group_id, mvctp_port);

		if (serv_addr.length() > 0) {
			ptr_status_proxy = new ReceiverStatusProxy(serv_addr, atoi(port.c_str()), ptr_mvctp_receiver);
			ptr_status_proxy->ConnectServer();
			ptr_status_proxy->StartService();
		}

		this->SendMessage(INFORMATIONAL, "I'm a receiver. Just joined the multicast group.");

		TransferMessage msg;
		sockaddr_in from;
		socklen_t socklen = sizeof(from);
		int bytes;
		while (true) {
			if ( (bytes = ptr_mvctp_receiver->RawReceive(&msg, sizeof(msg), 0, (SA *)&from, &socklen)) < 0) {
				SysError("Tester::StartTester::RawReceive() error");
			}

			switch (msg.msg_type) {
			case STRING_TRANSFER:
				HandleStringTransfer(msg);
				break;
			case MEMORY_TRANSFER:
				HandleMemoryTransfer(msg, MVCTP_DATA_LEN);
				break;
			case FILE_TRANSFER:
				HandleFileTransfer(msg, 4096);
				break;
			default:
				break;
			}
		}
	}
}


void Tester::HandleStringTransfer(TransferMessage& msg) {
	char buff[msg.data_len + 1];
	sockaddr_in from;
	socklen_t socklen = sizeof(from);

	int bytes;
	if ((bytes = ptr_mvctp_receiver->RawReceive(buff, msg.data_len, 0, (SA *) &from,
			&socklen)) < 0) {
		SysError("Tester::HandleStringTransfer::RawReceive() error");
	}

	buff[bytes] = '\0';
	string s = "I received a message: ";
	s.append(buff);
	this->SendMessage(INFORMATIONAL, s);
}


void Tester::HandleMemoryTransfer(TransferMessage& msg, size_t buff_size) {
	//char* mem_store = (char*)malloc(msg.data_len);

	char buff[buff_size];
	sockaddr_in from;
	socklen_t socklen = sizeof(from);

	timeval start_time, end_time;
	char s[512];
	sprintf(s, "Start memory transfer... Total size to transfer: %d", msg.data_len);
	this->SendMessage(INFORMATIONAL, s);
	gettimeofday(&start_time, NULL);
	int bytes = 0;
	size_t remained_size = msg.data_len;
	size_t data_bytes = 0;
	while (remained_size > 0) {
		int recv_size = remained_size > buff_size ? buff_size : remained_size;
		if ((bytes = ptr_mvctp_receiver->RawReceive(buff/*mem_store + data_bytes*/, recv_size, 0,
				(SA *) &from, &socklen)) < 0) {
			SysError("Tester::HandleMemoryTransfer::RawReceive() error");
		}

		int actual_size = bytes > 0 ? bytes + MVCTP_HLEN + ETH_HLEN : 0;
		remained_size -= actual_size; //bytes;
		data_bytes += bytes;
	}

//	uint num_ints = data_bytes / sizeof(uint);
//	uint* ptr = (uint*)mem_store;
//	for (uint i = 0; i < num_ints; i++) {
//		if (ptr[i] != i) {
//			this->SendMessage(INFORMATIONAL, "Invalide data detected!");
//			break;
//		}
//	}
//	this->SendMessage(INFORMATIONAL, "Memory data test passed!");

	gettimeofday(&end_time, NULL);
	//free(mem_store);

	float trans_time = end_time.tv_sec - start_time.tv_sec +
			(end_time.tv_usec - start_time.tv_usec) / 1000000.0 + 0.001;
	sprintf(s, "Memory transfer finished. Total transfer time: %.2f", trans_time);
	this->SendMessage(INFORMATIONAL, s);

	// Data transfer statistics
	struct ReceiveBufferStats stats = ptr_mvctp_receiver->GetReceiveBufferManager()->GetBufferStats();
	sprintf(s, "Statistics:\n# Total Packets: %d\n# Retrans. Packets: %d\n# Dup. Retrans. Packets: %d",
			stats.num_received_packets, stats.num_retrans_packets, stats.num_dup_retrans_packets);
	this->SendMessage(INFORMATIONAL, s);

	ptr_mvctp_receiver->ResetBuffer();

	float retrans_rate = stats.num_retrans_packets * 1.0 / stats.num_received_packets;
	float throughput = msg.data_len * 8.0 / 1024.0 / 1024.0 / trans_time;
	// Format:TransferBytes, Transfer Time (Sec), Throughput (Mbps), #Packets, #Retransmitted Packets, #Retransmission Rate
	sprintf(s, "%d,%.2f,%.2f,%d,%d,%.4f\n", msg.data_len, trans_time, throughput, stats.num_received_packets,
					stats.num_retrans_packets, retrans_rate);
	this->SendMessage(EXP_RESULT_REPORT, s);



//	char file_name[30];
//	string host_name = ExecSysCommand("hostname -f");
//	int index = host_name.find('-');
//	host_name = host_name.substr(0, index);
//	sprintf(file_name, "/users/jieli/results/mem_transfer_size%d_buff%d_%s.csv",
//			msg.data_len, ptr_mvctp_receiver->GetBufferSize(), host_name.c_str());
//
//	ofstream fout(file_name);
//	if (!fout.fail()) {
//		struct ReceiveBufferStats stats = ptr_mvctp_receiver->GetBufferStats();
//		float retrans_rate = stats.num_retransmitted_packets * 1.0 / stats.num_received_packets;
//		fout << "Transfer Time (Sec), #Packets, #Retransmitted Packets, #Retransmission Rate" << endl;
//		sprintf(s, "%.2f,%d,%d,%f\n", trans_time, stats.num_received_packets,
//				stats.num_retransmitted_packets, retrans_rate);
//		fout << s;
//		fout.close();
//	}
}


void Tester::HandleFileTransfer(TransferMessage& msg, size_t buff_size) {

}


void Tester::SendMessage(int level, string msg) {
	ptr_status_proxy->SendMessage(level, msg);
}

bool Tester::IsSender() {
	struct utsname host_name;
	uname(&host_name);
	string nodename = host_name.nodename;
	if (nodename.find("node0") != string::npos ||
			nodename.find("ubuntu") != string::npos) {
		return true;
	}
	else
		return false;
}

string Tester::ExecSysCommand(char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char output[4096];
    string result = "";
    while(!feof(pipe)) {
    	int bytes = fread(output, 1, 4095, pipe);
    	output[bytes] = '\0';
    	result += output;
    }
    pclose(pipe);
    return result;
}

