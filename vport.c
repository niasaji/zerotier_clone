/* 
 VPort acts as a bridge between a TAP device (Linux network interface) and
 a VSwitch over UDP. It creates a virtual network port that:
 
 1. Creates a TAP device visible to the Linux kernel
 2. Establishes UDP communication with a VSwitch
 3. Forwards Ethernet frames bidirectionally between TAP and VSwitch
 4. Enables multiple VMs/containers to connect through a virtual switch

 */

#include "tap_utils.h"
#include "sys_utils.h"
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>      // Internet address manipulation
#include <net/ethernet.h>   // Ethernet protocol definitions
#include <pthread.h>        // POSIX threads


struct vport_t
{
  int tapfd;                       ///< TAP device file descriptor for kernel network stack communication
  int vport_sockfd;                ///< UDP socket file descriptor for VSwitch communication  
  struct sockaddr_in vswitch_addr; ///< VSwitch IP address and port for UDP communication
};

// Function declarations
void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port);
void *forward_ether_data_to_vswitch(void *raw_vport);
void *forward_ether_data_to_tap(void *raw_vport);

int main(int argc, char const *argv[])
{
  // Validate command line arguments
  if (argc != 3)
  {
    ERROR_PRINT_THEN_EXIT("Usage: vport {server_ip} {server_port}\n");
  }
  
  // Parse command line arguments
  const char *server_ip_str = argv[1];  // VSwitch IP address
  int server_port = atoi(argv[2]);      // VSwitch UDP port

  // Initialize VPort instance with VSwitch connection details
  struct vport_t vport;
  vport_init(&vport, server_ip_str, server_port);

  // Create uplink forwarder thread (TAP -> VSwitch)
  // This thread reads Ethernet frames from TAP device and sends them to VSwitch
  pthread_t up_forwarder;
  if (pthread_create(&up_forwarder, NULL, forward_ether_data_to_vswitch, &vport) != 0)
  {
    ERROR_PRINT_THEN_EXIT("fail to pthread_create: %s\n", strerror(errno));
  }

  // Create downlink forwarder thread (VSwitch -> TAP)  
  // This thread receives Ethernet frames from VSwitch and writes them to TAP device
  pthread_t down_forwarder;
  if (pthread_create(&down_forwarder, NULL, forward_ether_data_to_tap, &vport) != 0)
  {
    ERROR_PRINT_THEN_EXIT("fail to pthread_create: %s\n", strerror(errno));
  }

  // Wait for both forwarder threads to complete
  // In normal operation, these threads run indefinitely, so this blocks forever
  if (pthread_join(up_forwarder, NULL) != 0 || pthread_join(down_forwarder, NULL) != 0)
  {
    ERROR_PRINT_THEN_EXIT("fail to pthread_join: %s\n", strerror(errno));
  }

  return 0;
}

void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port)
{
  // Create TAP device with specific naming convention
  char ifname[IFNAMSIZ] = "tapyuan";  // Base name for TAP device
  int tapfd = tap_alloc(ifname);      // Create the TAP device
  if (tapfd < 0)
  {
    ERROR_PRINT_THEN_EXIT("fail to tap_alloc: %s\n", strerror(errno));
  }

  // Create UDP socket for VSwitch communication
  // AF_INET: IPv4, SOCK_DGRAM: UDP protocol
  int vport_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (vport_sockfd < 0)
  {
    ERROR_PRINT_THEN_EXIT("fail to socket: %s\n", strerror(errno));
  }

  // Configure VSwitch address structure for UDP communication
  struct sockaddr_in vswitch_addr;
  memset(&vswitch_addr, 0, sizeof(vswitch_addr));
  vswitch_addr.sin_family = AF_INET;                    // IPv4
  vswitch_addr.sin_port = htons(server_port);           // Port in network byte order
  
  // Convert IP address string to binary format
  if (inet_pton(AF_INET, server_ip_str, &vswitch_addr.sin_addr) != 1)
  {
    ERROR_PRINT_THEN_EXIT("fail to inet_pton: %s\n", strerror(errno));
  }

  // Populate VPort structure with initialized components
  vport->tapfd = tapfd;
  vport->vport_sockfd = vport_sockfd;
  vport->vswitch_addr = vswitch_addr;

  printf("[VPort] TAP device name: %s, VSwitch: %s:%d\n", ifname, server_ip_str, server_port);
}


void *forward_ether_data_to_vswitch(void *raw_vport)
{
  struct vport_t *vport = (struct vport_t *)raw_vport;
  char ether_data[ETHER_MAX_LEN];  // Buffer for Ethernet frame (max 1518 bytes)
  
  while (true)
  {
    // Read Ethernet frame from TAP device (blocks until data available)
    // The TAP device provides complete Ethernet frames including headers
    int ether_datasz = read(vport->tapfd, ether_data, sizeof(ether_data));
    
    if (ether_datasz > 0)
    {
      // Validate minimum Ethernet frame size (14 bytes for header)
      assert(ether_datasz >= 14);
      
      // Parse Ethernet header for logging and debugging
      const struct ether_header *hdr = (const struct ether_header *)ether_data;

      // Forward complete Ethernet frame to VSwitch via UDP
      ssize_t sendsz = sendto(vport->vport_sockfd, ether_data, ether_datasz, 0, 
                             (struct sockaddr *)&vport->vswitch_addr, sizeof(vport->vswitch_addr));
      
      // Verify that the entire frame was sent
      if (sendsz != ether_datasz)
      {
        fprintf(stderr, "sendto size mismatch: ether_datasz=%d, sendsz=%d\n", ether_datasz, (int)sendsz);
      }

      // Log frame details for monitoring and debugging
      // Shows source/destination MAC addresses, EtherType, and frame size
      printf("[VPort] Sent to VSwitch:"
             " dhost<%02x:%02x:%02x:%02x:%02x:%02x>"      // Destination MAC
             " shost<%02x:%02x:%02x:%02x:%02x:%02x>"      // Source MAC  
             " type<%04x>"                                 // EtherType (IP, ARP, etc.)
             " datasz=<%d>\n",                            // Frame size
             hdr->ether_dhost[0], hdr->ether_dhost[1], hdr->ether_dhost[2], 
             hdr->ether_dhost[3], hdr->ether_dhost[4], hdr->ether_dhost[5],
             hdr->ether_shost[0], hdr->ether_shost[1], hdr->ether_shost[2], 
             hdr->ether_shost[3], hdr->ether_shost[4], hdr->ether_shost[5],
             ntohs(hdr->ether_type),  // Convert from network to host byte order
             ether_datasz);
    }
  }
}

void *forward_ether_data_to_tap(void *raw_vport)
{
  struct vport_t *vport = (struct vport_t *)raw_vport;
  char ether_data[ETHER_MAX_LEN];  // Buffer for Ethernet frame
  
  while (true)
  {
    // Receive Ethernet frame from VSwitch via UDP (blocks until data available)
    socklen_t vswitch_addrlen = sizeof(vport->vswitch_addr);
    int ether_datasz = recvfrom(vport->vport_sockfd, ether_data, sizeof(ether_data), 0,
                                (struct sockaddr *)&vport->vswitch_addr, &vswitch_addrlen);
    
    if (ether_datasz > 0)
    {
      // Validate minimum Ethernet frame size
      assert(ether_datasz >= 14);
      
      // Parse Ethernet header for logging
      const struct ether_header *hdr = (const struct ether_header *)ether_data;

      // Forward Ethernet frame to TAP device (inject into Linux network stack)
      ssize_t sendsz = write(vport->tapfd, ether_data, ether_datasz);
      
      // Verify that the entire frame was written
      if (sendsz != ether_datasz)
      {
        fprintf(stderr, "write size mismatch: ether_datasz=%d, sendsz=%d\n", ether_datasz, (int)sendsz);
      }

      // Log frame details for monitoring and debugging
      printf("[VPort] Forward to TAP device:"
             " dhost<%02x:%02x:%02x:%02x:%02x:%02x>"      // Destination MAC
             " shost<%02x:%02x:%02x:%02x:%02x:%02x>"      // Source MAC
             " type<%04x>"                                 // EtherType
             " datasz=<%d>\n",                            // Frame size
             hdr->ether_dhost[0], hdr->ether_dhost[1], hdr->ether_dhost[2], 
             hdr->ether_dhost[3], hdr->ether_dhost[4], hdr->ether_dhost[5],
             hdr->ether_shost[0], hdr->ether_shost[1], hdr->ether_shost[2], 
             hdr->ether_shost[3], hdr->ether_shost[4], hdr->ether_shost[5],
             ntohs(hdr->ether_type),
             ether_datasz);
    }
  }
}