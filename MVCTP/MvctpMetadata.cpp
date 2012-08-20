/*
 * MvctpMetadata.cpp
 *
 *  Created on: Jul 1, 2012
 *      Author: jie
 */

#include "MvctpMetadata.h"

MvctpMetadata::MvctpMetadata() {
	pthread_rwlock_init(&map_lock, NULL);
}

MvctpMetadata::~MvctpMetadata() {
	for (map<int, MessageMetadata*>::iterator it = metadata_map.begin(); it != metadata_map.end(); it++) {
		delete (it->second);
	}

	pthread_rwlock_destroy(&map_lock);
}


void MvctpMetadata::AddMessageMetadata(MessageMetadata* ptr_meta) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map[ptr_meta->msg_id] = ptr_meta;
	pthread_rwlock_unlock(&map_lock);
}


void MvctpMetadata::RemoveMessageMetadata(uint msg_id) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map.erase(msg_id);
	pthread_rwlock_unlock(&map_lock);
}


MessageMetadata* MvctpMetadata::GetMetadata(uint msg_id) {
	MessageMetadata* temp = NULL;
	pthread_rwlock_rdlock(&map_lock);
	if (metadata_map.find(msg_id) != metadata_map.end())
		temp = metadata_map[msg_id];
	pthread_rwlock_unlock(&map_lock);
	return temp;
}


bool MvctpMetadata::IsTransferFinished(uint msg_id) {
	bool is_finished = false;
	pthread_rwlock_rdlock(&map_lock);
	if (metadata_map.find(msg_id) != metadata_map.end())
		is_finished = (metadata_map[msg_id]->unfinished_recvers.size() == 0);
	pthread_rwlock_unlock(&map_lock);
	return is_finished;
}


int MvctpMetadata::GetFileDescriptor(uint msg_id) {
	pthread_rwlock_rdlock(&map_lock);
	if (metadata_map.find(msg_id) == metadata_map.end())
		return -1;
	FileMessageMetadata* meta = (FileMessageMetadata*)metadata_map[msg_id];
	if (meta->file_descriptor < 0) {
		pthread_rwlock_unlock(&map_lock);
		pthread_rwlock_wrlock(&map_lock);
		meta->file_descriptor = open(meta->file_name.c_str(), O_RDONLY);
		pthread_rwlock_unlock(&map_lock);
	}
	else {
		pthread_rwlock_unlock(&map_lock);
	}
	return meta->file_descriptor;
}


void MvctpMetadata::RemoveFinishedReceiver(uint msg_id, int sock_fd) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map[msg_id]->unfinished_recvers.erase(sock_fd);
	pthread_rwlock_unlock(&map_lock);
}

