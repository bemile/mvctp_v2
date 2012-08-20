/*
 * MvctpSenderMetadata.cpp
 *
 *  Created on: Jul 1, 2012
 *      Author: jie
 */

#include "MvctpSenderMetadata.h"

MvctpSenderMetadata::MvctpSenderMetadata() {
	pthread_rwlock_init(&map_lock, NULL);
}

MvctpSenderMetadata::~MvctpSenderMetadata() {
	for (map<uint, MessageMetadata*>::iterator it = metadata_map.begin(); it != metadata_map.end(); it++) {
		delete (it->second);
	}

	pthread_rwlock_destroy(&map_lock);
}


void MvctpSenderMetadata::AddMessageMetadata(MessageMetadata* ptr_meta) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map[ptr_meta->msg_id] = ptr_meta;
	pthread_rwlock_unlock(&map_lock);
}


void MvctpSenderMetadata::RemoveMessageMetadata(uint msg_id) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map.erase(msg_id);
	pthread_rwlock_unlock(&map_lock);
}


MessageMetadata* MvctpSenderMetadata::GetMetadata(uint msg_id) {
	MessageMetadata* temp = NULL;
	map<uint, MessageMetadata*>::iterator it;
	pthread_rwlock_rdlock(&map_lock);
	if ((it = metadata_map.find(msg_id)) != metadata_map.end())
		temp = it->second;
	pthread_rwlock_unlock(&map_lock);
	return temp;
}


bool MvctpSenderMetadata::IsTransferFinished(uint msg_id) {
	bool is_finished = false;
	map<uint, MessageMetadata*>::iterator it;
	pthread_rwlock_rdlock(&map_lock);
	if ( (it = metadata_map.find(msg_id)) != metadata_map.end())
		is_finished = (it->second->unfinished_recvers.size() == 0);
	pthread_rwlock_unlock(&map_lock);
	return is_finished;
}


int MvctpSenderMetadata::GetFileDescriptor(uint msg_id) {
	map<uint, MessageMetadata*>::iterator it;
	pthread_rwlock_rdlock(&map_lock);
	if ( (it = metadata_map.find(msg_id)) == metadata_map.end())
		return -1;

	FileMessageMetadata* meta = (FileMessageMetadata*)it->second;
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


void MvctpSenderMetadata::RemoveFinishedReceiver(uint msg_id, int sock_fd) {
	pthread_rwlock_wrlock(&map_lock);
	metadata_map[msg_id]->unfinished_recvers.erase(sock_fd);
	pthread_rwlock_unlock(&map_lock);
}

