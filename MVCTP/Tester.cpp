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

		ptr_mvctp_sender->SetStatusProxy(ptr_status_proxy);
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

		ptr_mvctp_receiver->SetStatusProxy(ptr_status_proxy);
		this->SendMessage(INFORMATIONAL, "I'm a receiver. Just joined the multicast group.");

		ptr_mvctp_receiver->Start();
	}
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

