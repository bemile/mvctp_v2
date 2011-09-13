/*
 * MVCTPBuffer.h
 *
 *  Created on: Jul 3, 2011
 *      Author: jie
 */

#ifndef MVCTPBUFFER_H_
#define MVCTPBUFFER_H_

#include "mvctp.h"
#include <pthread.h>


//	information for an MVCTP send/receive buffer
class MVCTPBuffer {
public:
	MVCTPBuffer(int buf_size);
	~MVCTPBuffer();

	size_t 	GetMaxBufferSize() {return max_buffer_size;};
	void 	SetMaxBufferSize(size_t buff_size) {max_buffer_size = buff_size;}
	size_t 	GetCurrentBufferSize() {return current_buffer_size;}
	size_t 	GetAvailableBufferSize();
	int 	GetNumEntries() {return num_entry;}
	int32_t	GetMinPacketId() {return min_packet_id;}
	int32_t GetMaxPacketId() {return max_packet_id;}

	PacketBuffer* 	Find(int32_t pid);
	//int 			PushBack(BufferEntry* entry);
	bool 			Insert(PacketBuffer* entry);
	int 			Delete(PacketBuffer*	entry);
	int 			DeleteUntil(int32_t start_id, int32_t end_id);
	bool 			IsEmpty();
	int 			ShrinkEntry(PacketBuffer* entry, size_t new_size);
	void			Clear();

	PacketBuffer* 				GetFreePacket();

protected:
	int 		num_entry;				// number of packet entries in the buffer
	size_t 		max_buffer_size;		// maximum data bytes assigned to the buffer
	size_t 		current_buffer_size;	// current occupied data bytes in the buffer
	int32_t		min_packet_id;
	int32_t		max_packet_id;

	map<int32_t, PacketBuffer*> 	buffer_pool;
	list<PacketBuffer*> 			free_packet_list;
	void 						AllocateFreePackets();
	void						AddFreePacket(PacketBuffer* entry);

	void DestroyEntry(PacketBuffer* entry);
};


#endif /* MVCTPBUFFER_H_ */
