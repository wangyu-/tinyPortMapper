# tinyPortMapper (or tinyPortForwarder)
A Lightweight High-Performance Port Mapping/Forwarding Utility using epoll, Supports both TCP and UDP 

# Supported Platforms
Linux host (including desktop Linux,Android phone/tablet, OpenWRT router, or Raspberry PI). Binaries of `amd64` `x86` `mips_be` `mips_le` `arm` are provided.

# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyPortMapper/releases

### Running
Assume you want to map/forward local port 1234 to 10.222.2.1:443
```
# for both TCP and UDP
./tinymapper_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -t -u

# for TCP only
./tinymapper_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -t

# for UDP only
./tinymapper_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -u
```

##### NOTE
```
#local port and remote port can be the same
./tinymapper_amd64 -l0.0.0.0:443 -r10.222.2.1:443 -u
```
# Options
```
tinyPortMapper
git version:25ea4ec047    build date:Nov  4 2017 22:55:23
repository: https://github.com/wangyu-/tinyPortMapper

usage:
    ./this_program  -l <listen_ip>:<listen_port> -r <remote_ip>:<remote_port>  [options]

main options:
    -t                                    enable TCP forwarding/mapping
    -u                                    enable UDP forwarding/mapping

other options:
    --sock-buf            <number>        buf size for socket, >=10 and <=10240, unit: kbyte, default: 1024
    --log-level           <number>        0: never    1: fatal   2: error   3: warn
                                          4: info (default)      5: debug   6: trace
    --log-position                        enable file name, function name, line number in log
    --disable-color                       disable log color
    -h,--help                             print this help message
```

# Peformance Test
### Environment
A linux virtual machine with 2 cores @2.4ghz

iperf3 server and tinyPortMapper command:
```
./iperf3 -s    # start iperf3 server      
./tinymapper -l 0.0.0.0:5202 -r127.0.0.1:5201 -t -u
```
iperf3 version is `3.2-stable`
### TCP
```
root@debian9:~# iperf3 -c 127.0.0.1 -p5202
Connecting to host 127.0.0.1, port 5202
[  4] local 127.0.0.1 port 37604 connected to 127.0.0.1 port 5202
[ ID] Interval           Transfer     Bandwidth       Retr  Cwnd
[  4]   0.00-1.00   sec   696 MBytes  5.84 Gbits/sec    0    639 KBytes
[  4]   1.00-2.00   sec   854 MBytes  7.17 Gbits/sec    0    639 KBytes
[  4]   2.00-3.00   sec   727 MBytes  6.10 Gbits/sec    0    639 KBytes
[  4]   3.00-4.00   sec   670 MBytes  5.62 Gbits/sec    0    639 KBytes
[  4]   4.00-5.00   sec   644 MBytes  5.40 Gbits/sec    0    639 KBytes
[  4]   5.00-6.00   sec   957 MBytes  8.03 Gbits/sec    0    639 KBytes
[  4]   6.00-7.00   sec   738 MBytes  6.19 Gbits/sec    0    639 KBytes
[  4]   7.00-8.00   sec   714 MBytes  5.99 Gbits/sec    0    639 KBytes
[  4]   8.00-9.00   sec   817 MBytes  6.85 Gbits/sec    0    639 KBytes
[  4]   9.00-10.00  sec   619 MBytes  5.19 Gbits/sec    0    639 KBytes
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bandwidth       Retr
[  4]   0.00-10.00  sec  7.26 GBytes  6.24 Gbits/sec    0             sender
[  4]   0.00-10.00  sec  7.26 GBytes  6.24 Gbits/sec                  receiver
```

```
root@debian9:~/Desktop/iperf/src# ./iperf3 -c 127.0.0.1 -p 5202  -M1400
Connecting to host 127.0.0.1, port 5202
[  5] local 127.0.0.1 port 57228 connected to 127.0.0.1 port 5202
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   621 MBytes  5.21 Gbits/sec    0    247 KBytes
[  5]   1.00-2.00   sec   556 MBytes  4.66 Gbits/sec    0    247 KBytes
[  5]   2.00-3.00   sec   615 MBytes  5.16 Gbits/sec    0    247 KBytes
[  5]   3.00-4.00   sec   614 MBytes  5.15 Gbits/sec    0    247 KBytes
[  5]   4.00-5.00   sec   621 MBytes  5.21 Gbits/sec    0    247 KBytes
[  5]   5.00-6.00   sec   658 MBytes  5.52 Gbits/sec    0    247 KBytes
[  5]   6.00-7.00   sec   569 MBytes  4.78 Gbits/sec    0    247 KBytes
[  5]   7.00-8.00   sec   563 MBytes  4.72 Gbits/sec    0    247 KBytes
[  5]   8.00-9.00   sec   607 MBytes  5.09 Gbits/sec    0    247 KBytes
[  5]   9.00-10.00  sec   759 MBytes  6.37 Gbits/sec    0    247 KBytes
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  6.04 GBytes  5.19 Gbits/sec    0             sender
[  5]   0.00-10.05  sec  6.04 GBytes  5.16 Gbits/sec                  receiver

```
### UDP

```

root@debian9:~/Desktop/iperf/src# ./iperf3 -c 127.0.0.1 -u -b10000M -p 5202 --pacing-timer 1 --get-server-output
Connecting to host 127.0.0.1, port 5202
[  5] local 127.0.0.1 port 38435 connected to 127.0.0.1 port 5202
[ ID] Interval           Transfer     Bitrate         Total Datagrams
[  5]   0.00-1.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   1.00-2.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   2.00-3.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   3.00-4.00   sec  1.16 GBytes  9.99 Gbits/sec  19078
[  5]   4.00-5.00   sec  1.16 GBytes  10.0 Gbits/sec  19100
[  5]   5.00-6.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   6.00-7.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   7.00-8.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   8.00-9.00   sec  1.16 GBytes  10.0 Gbits/sec  19089
[  5]   9.00-10.00  sec  1.16 GBytes  10.0 Gbits/sec  19088
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.00  sec  11.6 GBytes  10.0 Gbits/sec  0.000 ms  0/190889 (0%)  sender
[  5]   0.00-10.04  sec  9.21 GBytes  7.88 Gbits/sec  0.027 ms  39874/190876 (21%)  receiver

Server output:
Accepted connection from 127.0.0.1, port 46680
[  5] local 127.0.0.1 port 5201 connected to 127.0.0.1 port 43075
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-1.00   sec   892 MBytes  7.49 Gbits/sec  0.019 ms  3974/18264 (22%)
[  5]   1.00-2.00   sec   996 MBytes  8.36 Gbits/sec  0.018 ms  3138/19090 (16%)
[  5]   2.00-3.00   sec   944 MBytes  7.90 Gbits/sec  0.213 ms  3964/19079 (21%)
[  5]   3.00-4.00   sec   897 MBytes  7.54 Gbits/sec  0.017 ms  4731/19094 (25%)
[  5]   4.00-5.00   sec   964 MBytes  8.08 Gbits/sec  0.016 ms  3663/19093 (19%)
[  5]   5.00-6.00   sec   961 MBytes  8.06 Gbits/sec  0.017 ms  3694/19085 (19%)
[  5]   6.00-7.00   sec   920 MBytes  7.72 Gbits/sec  0.020 ms  4359/19090 (23%)
[  5]   7.00-8.00   sec   940 MBytes  7.89 Gbits/sec  0.019 ms  4034/19092 (21%)
[  5]   8.00-9.00   sec   925 MBytes  7.76 Gbits/sec  0.014 ms  4277/19089 (22%)
[  5]   9.00-10.00  sec   944 MBytes  7.92 Gbits/sec  0.017 ms  3970/19089 (21%)
[  5]  10.00-10.04  sec  46.3 MBytes  8.95 Gbits/sec  0.027 ms  70/811 (8.6%)
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.04  sec  9.21 GBytes  7.88 Gbits/sec  0.027 ms  39874/190876 (21%)  receiver


root@debian9:~/Desktop/iperf/src# ./iperf3 -c 127.0.0.1 -u -b500M -p 5202 -l1400 --pacing-timer 1 --get-server-output 
Connecting to host 127.0.0.1, port 5202
[  5] local 127.0.0.1 port 60822 connected to 127.0.0.1 port 5202
[ ID] Interval           Transfer     Bitrate         Total Datagrams
[  5]   0.00-1.00   sec  59.6 MBytes   500 Mbits/sec  44643
[  5]   1.00-2.00   sec  59.6 MBytes   499 Mbits/sec  44605
[  5]   2.00-3.00   sec  59.7 MBytes   501 Mbits/sec  44681
[  5]   3.00-4.00   sec  59.6 MBytes   500 Mbits/sec  44641
[  5]   4.00-5.00   sec  59.6 MBytes   500 Mbits/sec  44645
[  5]   5.00-6.00   sec  59.6 MBytes   500 Mbits/sec  44641
[  5]   6.00-7.00   sec  59.6 MBytes   500 Mbits/sec  44644
[  5]   7.00-8.00   sec  59.6 MBytes   500 Mbits/sec  44643
[  5]   8.00-9.00   sec  59.6 MBytes   500 Mbits/sec  44641
[  5]   9.00-10.00  sec  59.6 MBytes   500 Mbits/sec  44645
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.00  sec   596 MBytes   500 Mbits/sec  0.000 ms  0/446429 (0%)  sender
[  5]   0.00-10.04  sec   536 MBytes   448 Mbits/sec  0.009 ms  45043/446429 (10%)  receiver

Server output:
-----------------------------------------------------------
Server listening on 5201
-----------------------------------------------------------
Accepted connection from 127.0.0.1, port 46700
[  5] local 127.0.0.1 port 5201 connected to 127.0.0.1 port 50621
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-1.00   sec  51.7 MBytes   434 Mbits/sec  0.041 ms  3897/42649 (9.1%)
[  5]   1.00-2.00   sec  55.0 MBytes   461 Mbits/sec  0.012 ms  3440/44628 (7.7%)
[  5]   2.00-3.00   sec  51.0 MBytes   428 Mbits/sec  0.007 ms  6427/44659 (14%)
[  5]   3.00-4.00   sec  50.2 MBytes   421 Mbits/sec  0.029 ms  7104/44679 (16%)
[  5]   4.00-5.00   sec  55.7 MBytes   467 Mbits/sec  0.019 ms  2859/44590 (6.4%)
[  5]   5.00-6.00   sec  52.3 MBytes   439 Mbits/sec  0.013 ms  5291/44448 (12%)
[  5]   6.00-7.00   sec  56.1 MBytes   471 Mbits/sec  0.011 ms  2774/44816 (6.2%)
[  5]   7.00-8.00   sec  52.3 MBytes   439 Mbits/sec  0.012 ms  5512/44709 (12%)
[  5]   8.00-9.00   sec  55.1 MBytes   462 Mbits/sec  0.021 ms  3349/44616 (7.5%)
[  5]   9.00-10.00  sec  53.8 MBytes   451 Mbits/sec  0.005 ms  4390/44659 (9.8%)
[  5]  10.00-10.04  sec  2.64 MBytes   506 Mbits/sec  0.009 ms  0/1976 (0%)
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Jitter    Lost/Total Datagrams
[  5]   0.00-10.04  sec   536 MBytes   448 Mbits/sec  0.009 ms  45043/446429 (10%)  receiver

```
