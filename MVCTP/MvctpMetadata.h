/*
 * MvctpMetadata.h
 *
 *  Created on: Jul 1, 2012
 *      Author: jie
 */

#ifndef MVCTPMETADATA_H_
#define MVCTPMETADATA_H_

#include <pthread.h>
#include "mvctp.h"

enum MsgTransferStatus {BOF_NOT_RECEIVED, IN_NORMAL_TRANSFER, FINISHED};

/*struct MessageMetadata {
	MsgTransferStatus 	status;
	MvctpMessageInfo 	msg_info;
};
*/

struct MessageMetadata {
	u_int32_t 	msg_id;
	bool		ignore_file;
    bool 		is_disk_file;          	// true for disk file transfer, false for memory transfer
    long long 	file_length;    		// the length of the file
    union {
        int 	file_descriptor;    	// file descriptor for the storage location if it's disk file transfer
        void* 	mem_buffer;   			// memory buffer location if it's memory transfer
    };
    void* 		data;                   // the pointer to any auxiliary data that the user wants to pass
};

class MvctpMetadata {
public:
	MvctpMetadata();
	virtual ~MvctpMetadata();

private:
	pthread_rwlock_t metadata_lock;
};

#endif /* MVCTPMETADATA_H_ */
