DEV=$1
DPORT=$2
RATE=$3

#tc qdisc del dev $DEV root
#tc qdisc add dev $DEV handle 1: root htb
#tc class add dev $DEV parent 1: classid 1:1 htb rate 50Kbps
#tc filter add dev $DEV parent 1: protocol ip prio 16 u32 match ip dport $DPORT 0xffff flowid 1:1

#tc qdisc del dev eth0 root


tc qdisc del dev $DEV root
tc qdisc add dev $DEV handle 1: root pfifo_fast 
tc class add dev $DEV parent 1: classid 1:1 pfifo_fast rate $RATE #50Kbps
#tc qdisc add dev $DEV parent 1:1 handle 10:1 netem loss 1%
tc filter add dev $DEV parent 1: protocol ip prio 16 u32 match ip dport $DPORT 0xffff flowid 1:1
