/*
 * CpuUsageCounter.h
 *
 *  Created on: Jan 7, 2012
 *      Author: jie
 */

#ifndef CPUUSAGECOUNTER_H_
#define CPUUSAGECOUNTER_H_

#include "Timer.h"
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;


class CpuUsageCounter {
public:
	CpuUsageCounter(int interval);
	~CpuUsageCounter();

	void Start();
	void Stop();


private:
	int 	interval;   // measurement interval in milliseconds
	bool 	keep_running;

	pthread_t count_thread;
	static void* StartCountThread(void* ptr);
	void RunCountThread();
};

#endif /* CPUUSAGECOUNTER_H_ */
