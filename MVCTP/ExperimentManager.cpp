/*
 * ExperimentManager.cpp
 *
 *  Created on: Jan 2, 2012
 *      Author: jie
 */

#include "SenderStatusProxy.h"
#include "ExperimentManager.h"


ExperimentManager::ExperimentManager() {

}


ExperimentManager::~ExperimentManager() {

}


void ExperimentManager::StartExperiment(SenderStatusProxy* sender_proxy) {
	// Experiment parameters
	const int NUM_RUNS_PER_SETUP = 5;
	const int NUM_FILE_SIZES = 4;
	const int NUM_SENDING_RATES = 5;
	const int NUM_TXQUEUE_LENGTHS = 2;
	const int NUM_UDP_BUFF_SIZES = 2;

	int file_sizes[NUM_FILE_SIZES] = {512, 1024, 2048, 4095};
	int sending_rates[NUM_SENDING_RATES] = {200, 400, 600, 700, 800};
	int txqueue_lengths[NUM_TXQUEUE_LENGTHS] = {1000, 10000};
	string udp_buff_sizes[NUM_UDP_BUFF_SIZES] = {"sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 16777216\"",
								"sudo sysctl -w net.ipv4.udp_mem=\"4096 8388608 16777216\""
							   };

	result_file.open("exp_results.csv", ofstream::out | ofstream::trunc);
	for (int i = 0; i < NUM_FILE_SIZES; i++) {
		// Generate the data file with the given size
		ulong file_size = file_sizes[i] * 1024 * 1024;
		sender_proxy->GenerateDataFile("/tmp/temp.dat", file_size);

		for (int j = 0; j < NUM_SENDING_RATES; j++) {
			sender_proxy->SetSendRate(sending_rates[j]);
			result_file << sending_rates[j] << " Mbps" << endl;
			for (int n = 0; n < NUM_RUNS_PER_SETUP; n++) {
				sender_proxy->TransferFile("/tmp/temp.dat");
			}
			result_file << endl << endl << endl;
		}

		// delete the data file
		system("sudo rm /tmp/temp.dat");
	}

	result_file.close();

}


void ExperimentManager::HandleExpResults(string msg) {
	if (result_file.is_open()) {
		result_file << msg;
	}
}




