/*
 This header declares functions for creating and managing TAP (network tap)
 devices on Linux systems. TAP devices create virtual Ethernet interfaces
 that can be used to inject/capture network traffic at the data link layer.
 */

#ifndef __TUNTAP_H
#define __TUNTAP_H

#include <linux/if.h>       // Network interface structures and constants
#include <linux/if_tun.h>   // TUN/TAP device specific structures and constants
#include <fcntl.h>          // File control operations
#include <string.h>         // String manipulation functions
#include <sys/ioctl.h>      // I/O control operations
#include <unistd.h>         // POSIX system calls

/*
 This function creates a new TAP device and returns its file descriptor.
 The device name can be specified in the 'dev' parameter, or if empty,
 the kernel will assign a name automatically.
 */
int tap_alloc(char *dev);

#endif