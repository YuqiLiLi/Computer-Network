/**********************************************************************
 * file:  sr_router.c
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;
  uint8_t *destination = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint8_t *source = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint16_t type = ntohs(ethernet_header->ether_type);
  memcpy(destination, ethernet_header->ether_dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(source, ethernet_header->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);

  
    if (type == ethertype_arp) {
      printf("ARP packet\n");
      sr_handle_arp_packet(sr, packet, len, interface, ethernet_header, source, destination);
    }
    if(type == ethertype_ip){
      printf("IP packet\n");
      //check minimum length
      int caculated_len = sizeof(sr_ethernet_hdr_t);
      sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
      caculated_len+=sizeof(sr_ip_hdr_t);
      if(len< caculated_len){
        printf("IP packet too short\n");
        return;
      }
      //check checksum
      if(cksum(ip_header, sizeof(sr_ip_hdr_t))!=ip_header->ip_sum){
        printf("IP checksum error\n");
        return;
      }
      if(ip_header->ip_p==ip_protocol_icmp){
        printf("ICMP packet\n");
        int icmp_start=caculated_len;
        caculated_len+=sizeof(sr_icmp_hdr_t);
        sr_icmp_hdr_t* icmp_header = (sr_icmp_hdr_t *) (packet + icmp_start);
        if(len<caculated_len){
          printf("ICMP packet too short\n");
          return;
        }
        if(cksum(icmp_header, sizeof(sr_icmp_hdr_t))!=icmp_header->icmp_sum){
          printf("ICMP checksum error\n");
          return;
        }      
      }

      sr_handle_ip_packet(sr, packet, len, interface, ethernet_header, source, destination);    
    }
    else{
      printf("Unknown packet\n");
    }
  

} /* end sr_handlepacket */


/* Add any additional helper methods here & don't forget to also declare
them in sr_router.h.

If you use any of these methods in sr_arpcache.c, you must also forward declare
them in sr_arpcache.h to avoid circular dependencies. Since sr_router
already imports sr_arpcache.h, sr_arpcache cannot import sr_router.h -KM */

void sr_handle_arp_packet(struct sr_instance *sr,
        uint8_t *packet,
        unsigned int len,
        char *interface,   
        sr_ethernet_hdr_t *ethernet_header,
        uint8_t *source,
        uint8_t *destination) {

  //get the arp header
  sr_arp_hdr_t *arp_header = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
  print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));
  //get mac&ip from arp header
  unsigned char send_MAC[ETHER_ADDR_LEN];
  unsigned char tar_MAC [ETHER_ADDR_LEN];
  uint32_t send_ip = arp_header->ar_sip;
  uint32_t tar_ip = arp_header->ar_tip;
  unsigned short op = ntohs(arp_header->ar_op);
  memcpy(send_MAC, arp_header->ar_sha, ETHER_ADDR_LEN);
  memcpy(tar_MAC, arp_header->ar_tha, ETHER_ADDR_LEN);

  /*In the case of an ARP request, you should only send an ARP reply 
  if the target IP address is one of your router’s IP addresses. 
  In the case of an ARP reply, 
  you should only cache the entry if the target IP address is one of your router’s IP addresses.
  */
  struct sr_if *router_interface = get_interface_from_ip(sr, tar_ip); 

  if (op == arp_op_request) {
    printf("arp_request\n");

    if (router_interface != NULL) {
      printf("find the interface\n");
      sr_arpcache_insert(&(sr->cache), send_MAC, send_ip);
      memcpy(ethernet_header->ether_shost, (uint8_t *) router_interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN); 
      memcpy(ethernet_header->ether_dhost, (uint8_t *) send_MAC, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(arp_header->ar_sha, router_interface->addr, ETHER_ADDR_LEN);
      memcpy(arp_header->ar_tha, send_MAC, ETHER_ADDR_LEN);
      arp_header->ar_sip = tar_ip;
      arp_header->ar_tip = send_ip; 
      arp_header->ar_op = htons(arp_op_reply);
      print_hdrs(packet, len);
      sr_send_packet(sr, packet, len, router_interface->name);
    }
    else {
      printf("no interface found\n");
    }
    
  }
  else if (op == arp_op_reply) {
    printf("arp_reply\n");
    if (router_interface != NULL) {
      printf("find the interface\n");
      sr_arpcache_insert(&(sr->cache), send_MAC, send_ip);
    }
    else {
      printf("no interface found\n");
    }

  }
}

void sr_send_arp_request(struct sr_instance *sr, uint32_t ip){
  //send an ARP request 
  int arp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  uint8_t *packet = malloc(arp_len);
  sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;
  memcpy(ethernet_header->ether_dhost, (uint8_t *) "\xff\xff\xff\xff\xff\xff", ETHER_ADDR_LEN);
  
  struct sr_if *now_interface = sr->if_list;
  uint8_t *new_Packet;
  while(now_interface!=NULL){
    printf("interface name: %s\n", now_interface->name);
    memcpy(ethernet_header->ether_shost, (uint8_t *) now_interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
    ethernet_header->ether_type = htons(ethertype_arp);

    sr_arp_hdr_t *arp_header = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
    arp_header->ar_hrd = htons(arp_hrd_ethernet);
    arp_header->ar_pro = htons(0x0800);
    arp_header->ar_hln = 6;
    arp_header->ar_pln = 4;
    arp_header->ar_op = htons(arp_op_request);

    memcpy(arp_header->ar_sha, (uint8_t *) now_interface->addr, ETHER_ADDR_LEN);
    memcpy(arp_header->ar_tha, (uint8_t *) "\x00\x00\x00\x00\x00\x00", ETHER_ADDR_LEN);
    arp_header->ar_sip = now_interface->ip;
    arp_header->ar_tip = ip;

    new_Packet=malloc(arp_len);
    memcpy(new_Packet, ethernet_header, arp_len);
    print_hdrs(new_Packet, arp_len);
    sr_send_packet(sr, new_Packet, arp_len, now_interface->name);
    now_interface = now_interface->next;


    
  }
  

  
  
}

void sr_echo_icmp_packet(struct sr_instance *sr,
        uint8_t *packet,
        unsigned int len,
        uint8_t *source,
        uint8_t *destination,
        char *interface,
        sr_ethernet_hdr_t *ethernet_header,
        sr_ip_hdr_t *ip_header,
        sr_icmp_hdr_t *icmp_header
        ){
  //icmp echo request
  //!!!Echo reply (type 0)
  int icmp_start=sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t);
  icmp_header->icmp_type=0;
  icmp_header->icmp_code=0;
  //caculate checksum
  icmp_header->icmp_sum=0;
  icmp_header->icmp_sum=cksum(icmp_header,len-icmp_start);
  
  struct sr_if *now_interface = sr_get_interface(sr, interface);
  ip_header->ip_dst = ip_header->ip_src;
  ip_header->ip_src = now_interface->ip;
  ip_header->ip_sum=0;
  ip_header->ip_sum=cksum(ip_header,sizeof(sr_ip_hdr_t));

  memcpy(ethernet_header->ether_dhost, source, sizeof(uint8_t) * ETHER_ADDR_LEN); 
  memcpy(ethernet_header->ether_shost, destination, sizeof(uint8_t) * ETHER_ADDR_LEN);

  print_hdrs(packet, len);
  sr_send_packet(sr, packet, len, interface);

}

void sr_send_icmp_packet( struct sr_instance *sr,
                                uint32_t destination_ip,
                                uint8_t *ip_packet,
                                uint8_t type,
                                uint8_t code){

  int icmp_packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
  uint8_t *packet = malloc(icmp_packet_len);
  sr_ethernet_hdr_t *ethernet_header = (sr_ethernet_hdr_t *) packet;
  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
  sr_icmp_t3_hdr_t *icmp_header = (sr_icmp_t3_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  ethernet_header->ether_type = htons(ethertype_ip);

  ip_header->ip_tos = 0;
  ip_header->ip_len = htons(icmp_packet_len - sizeof(sr_ethernet_hdr_t));
  ip_header->ip_id = htons(0);
  ip_header->ip_off = htons(IP_DF);
  ip_header->ip_ttl = 64;
  ip_header->ip_p = ip_protocol_icmp;
  ip_header->ip_dst = destination_ip;
  //????
  ip_header->ip_hl = 5;
  ip_header->ip_v = 4;

  icmp_header->icmp_type = type;
  icmp_header->icmp_code = code;
  memcpy(icmp_header->data, ip_packet, ICMP_DATA_SIZE);

  // caculate cksum
  icmp_header->icmp_sum = 0;
  icmp_header->icmp_sum = cksum(icmp_header, sizeof(sr_icmp_t3_hdr_t));
  /*
  Find out which entry in the routing table has the longest prefix match with the destination IP address.
  Check the ARP cache for the next-hop MAC address corresponding to the next-hop IP.
  If ARP entry is there, forward the IP packet.
  otherwise, send an ARP request for the next-hop IP (if one hasn’t been sent within the last second), 
  and add the packet to the queue of packets waiting on this ARP request.
  */
 //longest prefix match
  unsigned long int l_match = 0;
  struct sr_rt *route_match = NULL;
  struct sr_rt *route_table = sr->routing_table;

  while (route_table != NULL) {
    unsigned long int mask_and= (unsigned long int) route_table->mask.s_addr & (unsigned long int) destination_ip;
    if (mask_and == (unsigned long int) route_table->dest.s_addr) {
      if ((route_table->mask.s_addr) > l_match) {
        l_match = route_table->mask.s_addr;
        route_match = route_table;
      }
    }
    route_table = route_table->next;
  }

  if(route_match!=NULL){

    //check the ARP cache
    uint32_t ip_nexthop = (uint32_t) route_match->gw.s_addr;
    struct sr_if *interface = get_interface_from_ip(sr, ip_nexthop);
    ip_header->ip_src = interface->ip;
    //caculate checksum
    ip_header->ip_sum=0;
    ip_header->ip_sum=cksum(ip_header,sizeof(sr_ip_hdr_t));
    memcpy(ethernet_header->ether_shost, (uint8_t *) interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
    //look up the ARP cache
    struct sr_arpentry *arp_entry = sr_arpcache_lookup(&(sr->cache), ip_nexthop);
    if(arp_entry!=NULL){
      printf("ARP entry found\n");
      memcpy(ethernet_header->ether_dhost, (uint8_t *) arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN);
      print_hdrs(packet, icmp_packet_len);
      sr_send_packet(sr, packet, icmp_packet_len, interface->name);
    }
    else{
      printf("ARP entry not found\n");
      struct sr_arpreq *arp_req = sr_arpcache_queuereq(
        &(sr->cache), 
        ip_nexthop, 
        packet, icmp_packet_len, 
        interface->name);
      handle_arpreq(sr, arp_req);
    }
  }

}

void sr_handle_ip_packet(struct sr_instance *sr,
        uint8_t *packet,
        unsigned int len,
        char *interface,
        sr_ethernet_hdr_t *ethernet_header,
        uint8_t *source,
        uint8_t *destination) {
  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));
  print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
  uint32_t destination_ip = ip_header->ip_dst;
  uint32_t source_ip = ip_header->ip_src;

  struct sr_if *router_interface = get_interface_from_ip(sr, destination_ip);
  //longest prefix match
  unsigned long int l_match = 0;
  struct sr_rt *route_match = NULL;
  struct sr_rt *route_table = sr->routing_table;
  while (route_table != NULL) {
    unsigned long int mask_and= (unsigned long int) route_table->mask.s_addr & (unsigned long int) destination_ip;
    if (mask_and == (unsigned long int) route_table->dest.s_addr) {
      if ((route_table->mask.s_addr) > l_match) {
        l_match = route_table->mask.s_addr;
        route_match = route_table;
      }
    }
    route_table = route_table->next;
  }
  // forward
  if(router_interface==NULL && route_match==NULL){
    //Destination net unreachable (type 3, code 0)
    printf("Destination unreachable\n");
    sr_send_icmp_packet(sr, source_ip, packet, 3, 0);
  }
  else{
    ip_forwarding(sr,packet,len,interface,ethernet_header,ip_header,source,destination,route_match);


  }



        
}

void ip_forwarding(struct sr_instance *sr,
                  uint8_t *packet,
                  unsigned int len,
                  char *interface,
                  sr_ethernet_hdr_t *ethernet_header,
                  sr_ip_hdr_t *ip_header,
                  uint8_t *source,
                  uint8_t *destination,
                  struct sr_rt *route_match){
                    // get_protocol& destination & source
  uint8_t ip_protocol = ip_header->ip_p;
  uint32_t ip_desnation = ip_header->ip_dst;
  uint32_t ip_source=ip_header->ip_src;
  // get the interface
  struct sr_if *now_interface = get_interface_from_ip(sr, ip_desnation);

  //if the frame contains an IP packet that is not destined for one of our interfaces:
  if(now_interface==NULL){
    /*
    Decrement the TTL by 1 and recompute the packet checksum over the modified header.
  If TTL is expired, send an ICMP time exceeded (type 11) message.
  Find out which entry in the routing table has the longest prefix match with the destination IP address.
  Check the ARP cache for the next-hop MAC address corresponding to the next-hop IP.
  If ARP entry is there, forward the IP packet.
  Otherwise, send an ARP request for the next-hop IP (if one hasn’t been sent within the last second), 
  and add the packet to the queue of packets waiting on this ARP request.
    */
    ip_header->ip_ttl--;
    if(ip_header->ip_ttl==0){
      printf("TTL expired\n");
      sr_send_icmp_packet(sr, ip_source, packet, 11, 0);
    }
    else{
      //caculate checksum
      ip_header->ip_sum=0;
      ip_header->ip_sum=cksum(ip_header,sizeof(sr_ip_hdr_t));
      //longest prefix match
      uint32_t ip_nexthop = (uint32_t) route_match->gw.s_addr;
      struct sr_arpentry *arp_entry = sr_arpcache_lookup(&(sr->cache), ip_nexthop);

      if(arp_entry!=NULL){
        printf("ARP entry found\n");
        struct sr_if *next_interface = sr_get_interface(sr, (const char*)(route_match->interface));
        
        memcpy(ethernet_header->ether_shost, (uint8_t *) next_interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
        memcpy(ethernet_header->ether_dhost, (uint8_t *) arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN);
        print_hdrs(packet, len);
        sr_send_packet(sr, packet, len, next_interface->name);
      }
      else{
        printf("ARP entry not found\n");
        struct sr_arpreq *arp_req = sr_arpcache_queuereq(
          &(sr->cache), 
          ip_nexthop, 
          packet, len, 
          now_interface->name);
        handle_arpreq(sr, arp_req);
       
      }
      
    }
  }
  else{
    //ip packet is destined for one of our interfaces
    if(ip_protocol==ip_protocol_icmp){
      //icmp packet
      int icmp_start=sizeof(sr_ethernet_hdr_t)+sizeof(sr_ip_hdr_t);
      sr_icmp_hdr_t* icmp_header = (sr_icmp_hdr_t *) (packet + icmp_start);
      print_hdr_icmp(packet + icmp_start);
      if(icmp_header->icmp_type==8&&icmp_header->icmp_code==0){
        //icmp echo request
        //!!!Echo reply (type 0)
        sr_echo_icmp_packet(sr, packet, len, source, destination, interface, ethernet_header, ip_header, icmp_header);
      }
    }
    else{
      /*
      Port unreachable (type 3, code 3)
      Sent if an IP packet containing a UDP or TCP payload is sent to one of the router’s interfaces. 
      This is needed for traceroute to work.
      */
      printf("Not ICMP packet\n");
      sr_send_icmp_packet(sr, ip_source, packet, 3, 3);
    }
  }

}

