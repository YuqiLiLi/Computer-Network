# README for Assignment 2: Router



## Handling Address Resolution (ARP)
For this part, I fill some code in sr_arpcache.c and also created a function "sr_send_arp_request" in router.c which used in sr_arpcache.c.


### sr_arpcache_sweepreqs
Iterate all pending ARP requests in the ARP cache of sr instance, and for each request, hanled it by function handle_arpreq.


I use a while loop to realize it.

### handle_arpreq

Time Check: compare the current time with the time the request was last sent to check if already one second passed since the last request was sent.

Exceeded Attempts: If ARP request has been sent 5 times without receiving a reply, it will judege the host is unreachable and sends an ICMP type 3, code 1 message (Destination Host Unreachable) to to the source of the packet  waiting for this ARP request. 

Resend ARP Request: Retrieve the interface (next_interface) associated with the packet waiting for this ARP reply, calls sr_send_arp_request to send out the ARP request again. 

#### sr_send_arp_request
For function, sr_send_arp_request, it create and send an ARP request packet through a pointed interface. It let the ARP requests broadcast across the network to find the MAC address associated with a specific IP address.


### sr_handle_arp_packet
For this function it judge the op type of packet and decided what to do next step.

If the ARP packet is a request (arp_op_request), it constructs an ARP reply packet. 
The ARP header is filled out to reply to the request, swapping the sender and target IP addresses and MAC addresses. 
The operation code is set to indicate an ARP reply (arp_op_reply).
and the reply packet is then sent out via the same interface.

If the ARP packet is a reply (arp_op_reply), it updates the ARP cache with the sender's MAC and IP addresses. If there are packets waiting on this ARP reply, it proceeds to send these pending packets using the sender's MAC address obtained from the ARP reply.

### sr_send_pending_packets

send packets that were pending an ARP reply in queue.


## IP Forwarding
This part is all done in sr_router.c. To realize the function of forwarding ip packet and send icmp error packet. I created functions in sr_router.c and add definition in sr_router.h.
### sr_handlepacket
This function is used for judged the type of packet and decided what to do next.

If arp: Iterates over the router's interface list to find an interface that matches the target IP address in the ARP request or reply. If a matching interface is found, the sr_handle_arp_packet function is called to handle the ARP packet.
If ip: do Sanity-check the packet--check that it meets minimum length and has the correct checksum. If there's a checksum error, it prints an error message and returns. If the checksum is correct and the packet is of type ICMP, it further checks if the packet length meets the requirements.

if the packet is vaild, it iterates through the interface list, looking for an interface that matches the destination IP address of the packet. If a matching interface is found, it call the sr_handle_ip_packet function directly.
If no matching interface is found, it calls the ip_forwarding function to forward packet in next hop.

### sr_handle_ip_packet
ICMP Packet Handling: The function first checks if the IP packet is an ICMP packet by examining the ip_p (protocol) field of the IP header. If the packet is ICMP (ip_protocol_icmp), it proceeds to check the ICMP type.

ICMP Echo Request (Type 8): If the ICMP packet is identified as an echo request (type 8), the function then prepares to send an echo reply (type 0).It calls sr_icmp_echo_reply to send an ICMP echo reply back to the sender. This involves modifying the received echo request packet to make it a valid echo reply and then sending back.
If the IP packet is not an ICMP packet, it sends an ICMP "Port Unreachable" message (type 3, code 3) by call function sr_send_icmp_packet.



### ip_forwarding
I think this function and sr_send_icmp_packet is the two most important function in my code.It involves TTL management, routing decisions, ARP resolution for next-hop MAC addresses, and handling cases where the destination is unreachable or the next-hop MAC address is unknown. 

1. TTL Decrement and Check: The function begins by decrementing the Time To Live (TTL) field of the IP header by 1 to prevent infinite looping of packets within the network. If the TTL drops to 1 or less before decrementing , the router sends an ICMP Time Exceeded message (type 11) back to the sender.

Route Lookup: The function performs a longest prefix match against the router's routing table to find the best route to the packet's destination IP address. If no suitable route is found, it sends an ICMP Network Unreachable message (type 3, code 0) back to the sender.

Next-Hop Address Resolution: If a matching route is found, the function extracts the next-hop IP address and the interface name from the routing entry.

ARP Cache Check: Before forwarding, the router checks its ARP cache to resolve the next-hop IP address to a MAC address. If an ARP cache entry exists for the next-hop IP: it updates the destination MAC address and source MAC address in the Ethernet header with the MAC address of the outgoing interface. Then send packet through interface.

### sr_icmp_echo_reply & sr_send_icmp_packet
These two function are same generally, but for echo_reply it has different length.

## Problem Encouter
1. when realize the function of handle_ip_packet, at first I tried to put the sr_handle_ip_packet into ip_forwarding too because i think it is also a part of ip_forwarding, but it was difficult for me to debug and i find that it can't send port unreachable by that method. I think it perhaps because when get into ip_forwarding, it will try to find next hop. so I put the check before it get in to it.
2. At first my traceroute couldn't work and it shows have arrived but not send any message back. i print a lot of info in output and find that ICMP "Port Unreachable" message (type 3, code 3) is send to the interface and not send back to client, it beacuse i use the function of ip_forwarding in the send_icmp_packet and it has a loop through sending(which means it send back to the server again)
3. when do checksum, at first i only know before recaculate i need to set 0, but don't know when caculated to compare it need too, so i just encouter a lot of checksum error at first.


