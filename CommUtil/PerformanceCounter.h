/*
 * CpuUsageCounter.h
 *
 *  Created on: Jan 7, 2012
 *      Author: jie
 */

#ifndef CPUUSAGECOUNTER_H_
#define CPUUSAGECOUNTER_H_

#include "Timer.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;


class PerformanceCounter {
public:
	PerformanceCounter(int interval);
	~PerformanceCounter();

	void Start();
	void Stop();

	void SetCPUFlag(bool flag);
	void SetUDPRecvBuffFlag(bool flag);


private:
	int 	interval;   // measurement interval in milliseconds
	bool 	keep_running;

	bool    measure_cpu;
	bool    measure_udp_recv_buffer;


	void	MeasureCPUInfo(ofstream& output);
	void 	MeasureUDPRecvBufferInfo(ofstream& output);
	int		HexToDecimal(char* start, char* end);

	pthread_t count_thread;
	static void* StartCountThread(void* ptr);
	void RunCountThread();
};

#endif /* CPUUSAGECOUNTER_H_ */
