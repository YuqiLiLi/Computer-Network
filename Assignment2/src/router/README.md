# README for Assignment 2: Router

Name: Yuqi Li

JHED: 50813A

---

**DESCRIBE YOUR CODE AND DESIGN DECISIONS HERE**

This will be worth 10% of the assignment grade.

Some guiding questions:
- What files did you modify (and why)?
- What helper method did you write (and why)?
- What logic did you implement in each file/method?
- What problems or challenges did you encounter?

sr_router.c:
sr_handle_arp_packet
sr_send_icmp_packet
router.h:
void sr_handle_arp_packet(struct sr_instance*, uint8_t *, unsigned int, char *, sr_ethernet_hdr_t *, uint8_t *, uint8_t *);
sr_arpcache.c:
handle_arpreq
sr_arpcache_sweepreqs



Requirements Summary
Please checkout the tutorial slides for a complete checklist of what we test for this assignment. In summary: 
- The router must successfully route packets between the Internet and the application servers. 

- The router must respond correctly to ICMP echo requests (ping commands). 
- The router must correctly handle traceroutes through it (where it is not the end host) and to it (where it is the end host). 
- The router must handle TCP/UDP packets sent to one of its interfaces. In this case, the router should respond with an ICMP port unreachable (type 3, code 3). 

- The router must correctly handle ARP requests and replies.  ok
- The router must maintain an ARP cache whose entries are invalidated after a timeout period (15 seconds). ok 
- The router must queue all packets waiting for outstanding ARP replies. If a host does not respond to 5 ARP requests, the queued packet is dropped and an ICMP host unreachable message (type 3, code 1) is sent back to the source of the queued packet. ok
- The router must enforce guarantees on timeouts - that is, if an ARP request is not responded to within a fixed period of time, the ICMP host unreachable message (echo 3, code 1) is generated even if no more packets arrive at the router. (Note: You can guarantee this by implementing the sr_arpcache_sweepreqs() function in sr_arpcache.c correctly.)  ok
- The router must not needlessly drop packets (for example, when waiting for an ARP reply) ok

Testing Your Code