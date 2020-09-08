sudo tc qdisc delete dev wlan0 root handle 1:0 netem delay 5ms
sudo tc qdisc delete dev wlan0 parent 1:1 handle 10: tbf rate 5mbit buffer 1600 limit 3000
