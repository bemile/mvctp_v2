DEV=$1
SPORT=$2
LOSS_RATE=$3

#tc qdisc del dev $DEV root
#tc qdisc add dev $DEV handle 1: root htb
#tc class add dev $DEV parent 1: classid 1:1 htb rate 50Kbps
#tc filter add dev $DEV parent 1: protocol ip prio 16 u32 match ip dport $DPORT 0xffff flowid 1:1

#tc qdisc del dev eth0 root


tc qdisc del dev $DEV root2
tc qdisc add dev $DEV handle 2: root2 htb 
tc class add dev $DEV parent 2: classid 2:1 htb rate 1000Mbps
tc qdisc add dev $DEV parent 2:1 handle 10:1 netem loss $LOSS_RATE #1%
tc filter add dev $DEV parent 2: protocol ip prio 16 u32 match ip dport $SPORT 0xffff flowid 2:1
