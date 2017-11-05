# tinyForwarder( or tinyPortMapper)
A Lightweight High-Performance Port Forwarder, Supports both TCP and UDP 

# Supported Platforms
Linux host (including desktop Linux,Android phone/tablet, OpenWRT router, or Raspberry PI).Binaries of amd64 x86 mips_be mips_le arm are provided.

# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyForwarder/releases

### Running
Assume you want to forward local port 1234 to 10.222.2.1:443 (map 10.222.2.1:443 to local 1234)
```
# for both TCP and UDP
./forwarder_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -t -u

# for both TCP only
./forwarder_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -t

# for both UDP only
./forwarder_amd64 -l0.0.0.0:1234 -r10.222.2.1:443 -u
```

##### NOTE
```
#local port and remote port can be the same
./forwarder_amd64 -l0.0.0.0:443 -r10.222.2.1:443 -u
```
# Options
```
tinyForwarder
git version:25ea4ec047    build date:Nov  4 2017 22:55:23
repository: https://github.com/wangyu-/tinyForwarder

usage:
    ./this_program  -l <listen_ip>:<listen_port> -r <remote_ip>:<remote_port>  [options]

main options:
    -t                                    enable TCP forward
    -u                                    enable UDP forward

other options:
    --sock-buf            <number>        buf size for socket, >=10 and <=10240, unit: kbyte, default: 1024
    --log-level           <number>        0: never    1: fatal   2: error   3: warn
                                          4: info (default)      5: debug   6: trace
    --log-position                        enable file name, function name, line number in log
    --disable-color                       disable log color
    -h,--help                             print this help message
```
