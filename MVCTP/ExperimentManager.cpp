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
}


ExperimentManager::~ExperimentManager() {

}

void ExperimentManager::DoSpeedTest(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	sender_proxy->SendMessageLocal(INFORMATIONAL, "Doing file transfer test to remove slow nodes...");
	sender_proxy->SetSendRate(600);
	sender_proxy->GenerateDataFile("/tmp/temp.dat", 256 * 1024 * 1024);

	sender_proxy->TransferFile("/tmp/temp.dat");
	sender->RemoveSlowNodes();
	sleep(3);
	sender_proxy->TransferFile("/tmp/temp.dat");

	// we want number of test nodes to be a multiple of 5
	num_test_nodes = sender->GetNumReceivers() / 5 * 5;
	sender_proxy->SendMessageLocal(INFORMATIONAL, "File transfer test finished.");
}

void ExperimentManager::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	// Experiment parameters
	const int NUM_RUNS_PER_SETUP = 5;
	const int NUM_FILE_SIZES = 4;
	const int NUM_SENDING_RATES = 4;
	const int NUM_TXQUEUE_LENGTHS = 2;
	const int NUM_UDP_BUFF_SIZES = 2;

	int file_sizes[NUM_FILE_SIZES] = {512, 1024, 2048, 4095};
	int send_rates[NUM_SENDING_RATES] = {500, 600, 700, 800};
	int txqueue_lengths[NUM_TXQUEUE_LENGTHS] = {1000, 10000};
	string udp_buff_sizes[NUM_UDP_BUFF_SIZES] = {"sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 16777216\"",
								"sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 16777216\""
							   };

	// First do the speed test to remove slow nodes
	DoSpeedTest(sender_proxy, sender);


	// Do the experiments
	result_file.open("exp_results.csv", ofstream::out | ofstream::trunc);
	result_file
			<< "#Transfer Size (Bytes),Send Rate (Mbps),SessionID,NodeID,Total Transfer Time (Seconds),Multicast Time (Seconds),"
			<< "Retrans. Time (Seconds),Throughput (Mbps),Transmitted Packets,Retransmitted Packets,Retransmission Rate"
			<< endl;

	char msg[512];
	for (int i = 0; i < NUM_FILE_SIZES; i++) {
		// Generate the data file with the given size
		file_size = file_sizes[i] * 1024 * 1024;
		sender_proxy->GenerateDataFile("/tmp/temp.dat", file_size);

		for (int j = 0; j < NUM_SENDING_RATES; j++) {
			send_rate = send_rates[j];
			sender_proxy->SetSendRate(send_rate);
			for (int n = 0; n < NUM_RUNS_PER_SETUP; n++) {
				sprintf(msg, "******** Run %d ********\nFile Size: %d MB\nSending Rate: %d Mbps\n",
						n+1, file_sizes[i], send_rates[j]);
				sender_proxy->SendMessageLocal(INFORMATIONAL, msg);

				finished_node_count = 0;
				sender_proxy->TransferFile("/tmp/temp.dat");
			}
		}

		// delete the data file
		system("sudo rm /tmp/temp.dat");
	}

	result_file.close();
}


void ExperimentManager::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		result_file << file_size << "," << send_rate << "," << msg;
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}




