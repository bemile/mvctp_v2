DEV=$1
SPORT=$2
LOSS_RATE=$3

#tc qdisc del dev $DEV root
#tc qdisc add dev $DEV handle 1: root htb
#tc class add dev $DEV parent 1: classid 1:1 htb rate 50Kbps
#tc filter add dev $DEV parent 1: protocol ip prio 16 u32 match ip dport $DPORT 0xffff flowid 1:1

#tc qdisc del dev eth0 root

tc qdisc del dev $DEV root
tc qdisc add dev $DEV handle 1: root htb
tc class add dev $DEV parent 1: classid 1:1 htb rate 1000Mbps
tc qdisc add dev $DEV parent 1:1 handle 10:1 netem loss $LOSS_RATE
tc filter add dev $DEV parent 1: protocol ip prio 1 u32 match ip sport $SPORT 0xffff flowid 1:1
