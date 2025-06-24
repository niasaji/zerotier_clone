/* 
 This file implements TAP device creation and management functions.
 TAP devices are virtual network interfaces that operate at the Ethernet
 frame level, allowing userspace programs to send and receive raw Ethernet frames.
*/

#include "tap_utils.h"

/*
 This function performs the following operations:
 1. Opens the TUN/TAP clone device (/dev/net/tun)
 2. Configures it as a TAP device (Ethernet frames, no packet info)
 3. Assigns a name to the device
 4. Returns the file descriptor for reading/writing Ethernet frames
 
 @note This function requires root privileges to create network interfaces
 */

int tap_alloc(char *dev)
{
  struct ifreq ifr;  // Interface request structure for ioctl operations
  int fd, err;

  // Open the TUN/TAP clone device - this is the entry point for creating virtual interfaces
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
  {
    return fd;  // Return the error code from open()
  }

  /* Configure interface request structure:
    
   Flags explanation:
   - IFF_TUN: Creates a TUN device (Layer 3 - IP packets, no Ethernet headers)
   - IFF_TAP: Creates a TAP device (Layer 2 - Ethernet frames with headers)
   - IFF_NO_PI: Do not prepend packet information to frames
   
   We use IFF_TAP because we want to work with complete Ethernet frames
   including MAC addresses for our virtual switch implementation.
   */
  memset(&ifr, 0, sizeof(ifr));           // Clear the structure
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;    // Set TAP mode without packet info

  // If a specific device name is requested, copy it to the interface request
  if (*dev)
  {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  // Create the TAP interface using ioctl system call
  // TUNSETIFF: Set the interface type and configuration
  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
  {
    close(fd);  // Clean up file descriptor on error
    return err; // Return the error code
  }

  // Copy the actual assigned device name back to the caller
  // (In case the kernel assigned a different name than requested)
  strcpy(dev, ifr.ifr_name);
  
  return fd;  // Return the file descriptor for the TAP device
}