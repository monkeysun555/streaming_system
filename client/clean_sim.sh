sudo tc qdisc del dev wlan0 root handle 1: prio
sudo tc qdisc del dev wlan0 parent 1:3 handle 30: tbf rate 1000kbit buffer 1600 limit 3000
sudo tc filter del dev wlan0 protocol ip parent 1:0 prio 3 u32 match ip dst 10.18.56.175/32 flowid 1:3