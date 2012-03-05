/*
 * ExperimentManager.cpp
 *
 *  Created on: Jan 2, 2012
 *      Author: jie
 */

#include "SenderStatusProxy.h"
#include "ExperimentManager.h"


ExperimentManager::ExperimentManager() {
	file_size = 0;
	send_rate = 0;
	txqueue_len = 0;
	buff_size = 0;
	retrans_buff_size = 0;
	is_high_speed_exp = true;
}


ExperimentManager::~ExperimentManager() {

}

void ExperimentManager::DoSpeedTest(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	sender_proxy->SendMessageLocal(INFORMATIONAL, "Doing file transfer test to remove slow nodes...");
	sender_proxy->SetSendRate(600);
	sender_proxy->GenerateDataFile("/tmp/temp.dat", 100 * 1024 * 1024);

	sender_proxy->TransferFile("/tmp/temp.dat");
	sender->RemoveSlowNodes();
	sleep(3);
	sender_proxy->TransferFile("/tmp/temp.dat");

	system("sudo rm /tmp/temp.dat");

	// we want number of test nodes to be a multiple of 5
	num_test_nodes = sender->GetNumReceivers() / 5 * 5;
	if (num_test_nodes == 0)
		num_test_nodes = sender->GetNumReceivers();
	sender_proxy->SendMessageLocal(INFORMATIONAL, "File transfer test finished.");
}

void ExperimentManager::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	// Experiment parameters
	const int NUM_RUNS_PER_SETUP = 10; //30;
	const int NUM_FILE_SIZES = 2;
	const int NUM_SENDING_RATES = 2; //4;
	const int NUM_TXQUEUE_LENGTHS = 2;
	const int NUM_RETRANS_BUFF_SIZES = 2;
	const int NUM_UDP_BUFF_SIZES = 3;

	int file_sizes[NUM_FILE_SIZES] = {1024, 4095};
	int send_rates[NUM_SENDING_RATES] = {600, 650}; //{500, 600, 700, 800};
	//int txqueue_lengths[NUM_TXQUEUE_LENGTHS] = {10000, 1000};
	int retrans_buff_sizes[NUM_RETRANS_BUFF_SIZES] = {128, 512};
	int udp_buff_sizes[NUM_UDP_BUFF_SIZES] = {1024, 4096, 16384}; //{10, 50, 4096};
	string udp_buff_conf_commands[NUM_UDP_BUFF_SIZES] = {
													     "sudo sysctl -w net.ipv4.udp_mem=\"1024 2048 4096\"",
													     "sudo sysctl -w net.ipv4.udp_mem=\"4096 8192 16384\"",
													     "sudo sysctl -w net.ipv4.udp_mem=\"16384 32768 65536\""
													    };

	// First do the speed test to remove slow nodes
	DoSpeedTest(sender_proxy, sender);


	// Do the experiments
	char buf[256];
	sprintf(buf, "exp_results_%dnodes.csv", num_test_nodes);
	result_file.open(buf, ofstream::out | ofstream::trunc);
	result_file
			<< "File Size (MB),Send Rate (Mbps),Retrans.Buff. Size (MB),Buffer Size (MB),SessionID,NodeID,Total Transfer Time (Seconds),Multicast Time (Seconds),"
			<< "Retrans. Time (Seconds),Throughput (Mbps),Transmitted Packets,Retransmitted Packets,Retransmission Rate"
			<< endl;

	char msg[512];
	for (int i = 0; i < NUM_FILE_SIZES; i++) {
		//if (i != 1)
		//	continue;

		// Generate the data file with the given size
		file_size = file_sizes[i];
		int bytes = file_size * 1024 * 1024;
		sender_proxy->GenerateDataFile("/tmp/temp.dat", bytes);

		for (int j = 0; j < NUM_SENDING_RATES; j++) {
			//if (j <= 1)
			//	continue;

			send_rate = send_rates[j];
			sender_proxy->SetSendRate(send_rate);

			//for (int l = 0; l < NUM_TXQUEUE_LENGTHS; l++) {
				//txqueue_len = txqueue_lengths[l];
				//sender_proxy->SetTxQueueLength(txqueue_len);

			for (int l = 0; l < NUM_RETRANS_BUFF_SIZES; l++) {
				retrans_buff_size = retrans_buff_sizes[l];
				sender_proxy->SetRetransmissionBufferSize(retrans_buff_size);

				for (int s = 0; s < NUM_UDP_BUFF_SIZES; s++) {
					buff_size = udp_buff_sizes[s] * 4 / 1024; //* 4096;
					system(udp_buff_conf_commands[s].c_str());

					for (int n = 0; n < NUM_RUNS_PER_SETUP; n++) {
						sprintf(msg, "********** Run %d **********\nFile Size: %d MB\nSending Rate: %d Mbps\nRetrans.Buff. Size:%d MB\nBuffer Size: %d MB\n",
								n+1, file_size, send_rate, retrans_buff_size, buff_size);
						sender_proxy->SendMessageLocal(INFORMATIONAL, msg);

						finished_node_count = 0;
						sender_proxy->TransferFile("/tmp/temp.dat");
					}
				}
			}
		}

		// delete the data file
		system("sudo rm /tmp/temp.dat");
	}

	result_file.close();
}



////
void ExperimentManager::StartExperimentLowSpeed(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	is_high_speed_exp = false;

	const int NUM_RUNS_PER_SETUP = 10; //30;
	const int NUM_FILE_SIZES = 2;
	const int NUM_UDP_BUFF_SIZES = 2;

	int file_sizes[NUM_FILE_SIZES] = {128, 256};
	int udp_buff_sizes[NUM_UDP_BUFF_SIZES] = {1024, 4096}; //{10, 50, 4096};
	string udp_buff_conf_commands[NUM_UDP_BUFF_SIZES] = {
														 "sudo sysctl -w net.ipv4.udp_mem=\"1024 2048 4096\"",
														 "sudo sysctl -w net.ipv4.udp_mem=\"4096 8192 16384\"",
														};

	sender->ExecuteCommandOnReceivers("sudo killall double", num_test_nodes);
	sender->ExecuteCommandOnReceivers("sudo killall fstime", num_test_nodes);

	// First do the speed test to remove slow nodes
	DoSpeedTest(sender_proxy, sender);

	sender->ExecuteCommandOnReceivers("/users/jieli/src/UnixBench/pgms/double 36000 &", num_test_nodes / 5);
	sender->ExecuteCommandOnReceivers("sh -c \"/users/jieli/src/UnixBench/pgms/fstime -t 3600 && /users/jieli/src/UnixBench/pgms/fstime -t 3600" \
			                          "&& /users/jieli/src/UnixBench/pgms/fstime -t 3600 && /users/jieli/src/UnixBench/pgms/fstime -t 3600" \
			                          "&& /users/jieli/src/UnixBench/pgms/fstime -t 3600\" &", num_test_nodes / 5);

	// Do the experiments
	char buf[256];
	sprintf(buf, "ls_exp_results_%dnodes.csv", num_test_nodes);
	result_file.open(buf, ofstream::out | ofstream::trunc);
	result_file
			<< "File Size (MB),UDP Buffer Size (MB),SessionID,NodeID,Total Transfer Time (Seconds),Multicast Time (Seconds),"
			<< "Retrans. Time (Seconds),Throughput (Mbps),Transmitted Packets,Retransmitted Packets,Retransmission Rate"
			<< endl;

	char msg[512];
	for (int i = 0; i < NUM_FILE_SIZES; i++) {
		// Generate the data file with the given size
		file_size = file_sizes[i];
		int bytes = file_size * 1024 * 1024;
		sender_proxy->GenerateDataFile("/tmp/temp.dat", bytes);

		for (int s = 0; s < NUM_UDP_BUFF_SIZES; s++) {
			buff_size = udp_buff_sizes[s] * 4 / 1024; //* 4096;
			system(udp_buff_conf_commands[s].c_str());

			for (int n = 0; n < NUM_RUNS_PER_SETUP; n++) {
				sprintf(
						msg,
						"********** Run %d **********\nFile Size: %d MB\nUDP Buffer Size: %d MB\n",
						n + 1, file_size, buff_size);
				sender_proxy->SendMessageLocal(INFORMATIONAL, msg);

				finished_node_count = 0;
				sender_proxy->TransferFile("/tmp/temp.dat");
			}
		}
	}
}



void ExperimentManager::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		if (is_high_speed_exp)
			result_file << file_size << "," << send_rate << "," << retrans_buff_size << "," << buff_size << "," << msg;
		else
			result_file << file_size << "," << buff_size << "," << msg;

		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}




