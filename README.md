# VSwitch - Virtual Ethernet Switch
A lightweight virtual Ethernet switch implementation that connects multiple virtual network interfaces over UDP.

## Architecture
[VM/Container] <-> [TAP Device] <-> [VPort] <-> [UDP] <-> [VSwitch] <-> [UDP] <-> [VPort] <-> [TAP Device] <-> [VM/Container]

## Components

VSwitch `(vswitch.py)` - Learning Ethernet switch with MAC address table  
VPort `(vport.c)` - Virtual port that bridges TAP devices to VSwitch via UDP  
TAP Utils `(tap_utils.c/h)` - TAP device creation and management  
Setup Script `(setup.sh)` - Network interface configuration  

## Quick Start
1. Compile VPort
```bash
gcc -o vport vport.c tap_utils.c -lpthread
```
2. Start VSwitch
```bash
python3 vswitch.py 8080
```
3. Create VPort (requires sudo)
```bash
sudo ./vport 127.0.0.1 8080
```
4. Configure TAP Interface
```bash
# Modify setup.sh to use correct interface name
sudo ip addr add 10.1.1.101/24 dev tapy
sudo ip link set tapy up
```
5. Test Network
```bash
# Create second VPort and configure with different IP
sudo ./vport 127.0.0.1 8080  # In another terminal
sudo ip addr add 10.1.1.102/24 dev tapy1
sudo ip link set tapy1 up
```

### Features

MAC Learning - Automatically learns and forwards based on MAC addresses  
Broadcast Support - Handles broadcast frames (ARP, DHCP, etc.)  
Multiple VPorts - Supports multiple virtual ports per switch  
Real-time Logging - Frame-level visibility for debugging  

### Requirements

Linux with TUN/TAP support  
Root privileges (for TAP device creation)  
GCC compiler  
Python 3  
pthread library  

Credits to : https://github.com/peiyuanix/build-your-own-zerotier/tree/master

#### License
MIT License
