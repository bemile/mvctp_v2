/*
 * ExperimentManager2.cpp
 *
 *  Created on: Aug 31, 2012
 *      Author: jie
 */

#include "SenderStatusProxy.h"
#include "ExperimentManager2.h"


ExperimentManager2::ExperimentManager2() {
}


ExperimentManager2::~ExperimentManager2() {

}


void ExperimentManager2::StartExperiment(SenderStatusProxy* sender_proxy, MVCTPSender* sender) {
	system("mkdir /tmp/temp");
	system("cp /users/jieli/src/file_sizes.txt /tmp/temp");
	system("cp /users/jieli/src/inter_arrival_times.txt /tmp/temp");

	cout << "Genearating files..." << endl;
	GenerateFiles();

	const int NUM_FILES = 10;

	char file_name[256];
	for (int i = 0; i < NUM_FILES; i++) {
		sprintf(file_name, "/tmp/temp/temp%d.dat", i + 1);
		sender->SendFile(file_name);
	}
}


void ExperimentManager2::GenerateFiles() {
	static const int FILE_COUNT = 1000;
	static const int BUF_SIZE = 4096;

	ifstream infile ("/tmp/temp/file_sizes.txt");
	int size = 0;
	char file_name[50];
	int file_index = 1;
	char buf[BUF_SIZE];
	int count = 0;
	while (infile >> size) {
		int remained_size = size;
		sprintf(file_name, "temp%d.dat", file_index++);
		ofstream outfile (file_name, ofstream::binary | ofstream::trunc);
		while (remained_size > 0) {
			int data_len = (remained_size > BUF_SIZE) ? BUF_SIZE : remained_size;
			outfile.write(buf, data_len);
			remained_size -= data_len;
		}
		outfile.close();

		if (file_index > FILE_COUNT)
			break;
	}
}


void ExperimentManager2::HandleExpResults(string msg) {
	if (result_file.is_open() && finished_node_count < num_test_nodes) {
		finished_node_count++;
		if (finished_node_count == num_test_nodes)
			result_file.flush();
	}
}

