CC=g++
CFLAGS=-lpthread -lrt
VPATH=starter:CommUtil:MVCTP
OBJ=EmulabStarter.o ConfigInfo.o mvctp.o Tester.o MVCTPComm.o \
SendBufferMgr.o ReceiveBufferMgr.o MVCTPBuffer.o MVCTPSender.o MVCTPReceiver.o \
MulticastComm.o RawSocketComm.o InetComm.o UdpComm.o NetInterfaceManager.o \
ReceiverStatusProxy.o SenderStatusProxy.o StatusProxy.o



all: emustarter

emustarter: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS)

EmulabStarter.o: ConfigInfo.o Tester.o

ConfigInfo.o: 

Tester.o: mvctp.o MulticastComm.o RawSocketComm.o InetComm.o MVCTPComm.o SendBufferMgr.o ReceiveBufferMgr.o MVCTPBuffer.o MVCTPSender.o MVCTPReceiver.o UdpComm.o ReceiverStatusProxy.o SenderStatusProxy.o StatusProxy.o NetInterfaceManager.o

SenderStatusProxy.o : StatusProxy.o MVCTPSender.o MVCTPComm.o

ReceiverStatusProxy.o : StatusProxy.o MVCTPReceiver.o MVCTPComm.o

MVCTPSender.o: mvctp.o MVCTPComm.o SendBufferMgr.o 

MVCTPReceiver.o: mvctp.o MVCTPComm.o ReceiveBufferMgr.o 

MVCTPComm.o: mvctp.o MulticastComm.o RawSocketComm.o NetInterfaceManager.o

SendBufferMgr.o ReceiveBufferMgr.o: mvctp.o MVCTPBuffer.o UdpComm.o InetComm.o

MVCTPBuffer.o: mvctp.o

MulticastComm.o RawSocketComm.o: mvctp.o InetComm.o 

InetComm.o : mvctp.o

UdpComm.o NetInterfaceManager.o: mvctp.o

mvctp.o :

StatusProxy.o : CommUtil.h



# Clean Action
clean:
	rm *.o


