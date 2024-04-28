/*
 * transport.c 
 *
 *	Project 3		
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file. 
 *
 */


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <time.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"

#define RECEIVER_WINDOW 3072
#define SENDER_WINDOW 3072

enum { SYN_SENT, SYN_RECEIVED, CSTATE_ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, TIME_WAIT, CLOSE_WAIT, LAST_ACK, CLOSED };    /* you should have more states */

/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;
    tcp_seq opp_sequence_num;
    tcp_seq ack_num;
    int opp_window_size;
    tcp_seq current_sequence_num;
    tcp_seq fin_ack_sequence_num;

    /* any other connection-wide global variables go here */
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);
void our_dprintf(const char *format,...);

/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx;
    tcphdr *tcp_hdr;
    int send_pkt_size, recv_pkt_size;

    ctx = (context_t *) calloc(1, sizeof(context_t));
    tcp_hdr = (tcphdr *) calloc(1, sizeof(tcphdr));
    
    assert(ctx);
    assert(tcp_hdr);

    generate_initial_seq_num(ctx);

    /* XXX: you should send a SYN packet here if is_active, or wait for one
     * to arrive if !is_active.  after the handshake completes, unblock the
     * application with stcp_unblock_application(sd).  you may also use
     * this to communicate an error condition back to the application, e.g.
     * if connection fails; to do so, just set errno appropriately (e.g. to
     * ECONNREFUSED, etc.) before calling the function.
     */

    if(is_active)
    {
        /*--- SYN Packet ---*/
        tcp_hdr->th_seq = htonl(ctx->initial_sequence_num);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_SYN;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);

        /*--- Receive SYN-ACK Packet ---*/
        bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(tcphdr));
        if ((tcp_hdr->th_flags & TH_ACK) && (tcp_hdr->th_flags & TH_SYN) && (ntohl(tcp_hdr->th_ack) == ctx->current_sequence_num))
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }
        else
        {
            errno = ECONNREFUSED;
        }
        ctx->connection_state = SYN_SENT;

        /*--- ACK Packet ---*/
        bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
        tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
        tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_ACK;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);

    }
    else
    {
        /*--- Receive SYN Packet ---*/
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(tcphdr));
        if (tcp_hdr->th_flags & TH_SYN)
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }
        else
        {
            errno = ECONNREFUSED;
        }
        ctx->connection_state = SYN_RECEIVED;

        /*--- SYN-ACK Packet ---*/
        bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
        tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
        tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_SYN;
        tcp_hdr->th_flags |= TH_ACK;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);

        /*--- Receive ACK Packet ---*/
        bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(tcphdr));
        if ((tcp_hdr->th_flags & TH_ACK) && (tcp_hdr->th_ack == ctx->current_sequence_num))
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }

    }

    ctx->connection_state = CSTATE_ESTABLISHED;
    stcp_unblock_application(sd);
    
    control_loop(sd, ctx);

    /* do any cleanup here */
    free(ctx);
}


/* generate random initial sequence number for an STCP connection */
static void generate_initial_seq_num(context_t *ctx)
{
    assert(ctx);

#ifdef FIXED_INITNUM
    /* please don't change this! */
    ctx->initial_sequence_num = 1;
#else
    /* you have to fill this up */
    srand(time(NULL));
    int r = rand() % 256;
    ctx->initial_sequence_num = r;
    ctx->current_sequence_num = ctx->initial_sequence_num;
#endif
}


/* control_loop() is the main STCP loop; it repeatedly waits for one of the
 * following to happen:
 *   - incoming data from the peer
 *   - new data from the application (via mywrite())
 *   - the socket to be closed (via myclose())
 *   - a timeout
 */
static void control_loop(mysocket_t sd, context_t *ctx)
{
    assert(ctx);
    assert(!ctx->done);
    tcphdr *tcp_hdr;
    char* payload;
    char* payload1;
    int payload_size, pkt_size;
    int current_sender_window;
    
    tcp_hdr = (tcphdr *) calloc(1, sizeof(tcphdr));
    
    while (!ctx->done)
    {
        unsigned int event;
        /* see stcp_api.h or stcp_api.c for details of this function */
        /* XXX: you will need to change some of these arguments! */
        event = stcp_wait_for_event(sd, ANY_EVENT, NULL);
        // our_dprintf("event occured %d\n", event);
        current_sender_window = SENDER_WINDOW - (ctx->current_sequence_num - ctx->ack_num);
        // our_dprintf("event: %d", event);
        /* check whether it was the network, app, or a close request */
        
        if (event & NETWORK_DATA)
        {
            payload1 = (char *) calloc(1, STCP_MSS+20);
            bzero((char *)payload1, STCP_MSS+20);

            pkt_size = stcp_network_recv(sd, payload1, STCP_MSS+20);

            bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
            tcp_hdr = (tcphdr *)payload1;
            
            if (tcp_hdr->th_flags & TH_ACK)
            {
                /*--- Setting Context ---*/
                ctx->ack_num = ntohl(tcp_hdr->th_ack);
                ctx->opp_window_size = ntohs(tcp_hdr->th_win);

                if (ntohl(tcp_hdr->th_ack) == ctx->fin_ack_sequence_num)
                {
                    if (ctx->connection_state == FIN_WAIT_1)
                    {
                        ctx->connection_state = FIN_WAIT_2;
                    }
                }
            }
            else if (tcp_hdr->th_flags & TH_FIN)
            {
                stcp_fin_received(sd);
                /*--- Setting Context ---*/
                ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
                ctx->opp_window_size = ntohs(tcp_hdr->th_win);

                /*--- Sending Payload to app layer if there is data ---*/
                if (pkt_size > 20)
                {
                    payload1 = payload1+20;
                    payload_size = pkt_size-20;
                    stcp_app_send(sd, payload1, payload_size);
                }

                /*--- Sending Ack Packet ---*/
                bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
                tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_flags |= TH_ACK;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);
                if (ctx->connection_state == CSTATE_ESTABLISHED)
                {
                    ctx->connection_state = CLOSE_WAIT;
                }
                else if (ctx->connection_state == FIN_WAIT_2)
                {
                    ctx->connection_state = CLOSED;
                    ctx->done = true;
                }
                
            }
            else
            {
                /*--- Setting Context ---*/
                ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
                ctx->opp_window_size = ntohs(tcp_hdr->th_win);
                /*--- Sending Payload to app layer ---*/
                payload1 = payload1+20;
                payload_size = pkt_size-20;
                stcp_app_send(sd, payload1, payload_size);

                /*--- Sending Ack Packet ---*/
                bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
                tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + payload_size);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_flags |= TH_ACK;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);
            }
        }
        if ((event & APP_DATA) && (current_sender_window > 0))
        {
            payload = (char *) calloc(1, SENDER_WINDOW);
            bzero((char *)payload, SENDER_WINDOW);
            
            payload_size = stcp_app_recv(sd, payload, current_sender_window);
            
            while (payload_size > 0)
            {
                bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
                tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                if(payload_size > STCP_MSS)
                {
                    pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), payload, STCP_MSS, NULL);
                    pkt_size = STCP_MSS;
                }
                else
                {
                    pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), payload, payload_size, NULL);
                    pkt_size = pkt_size - sizeof(tcphdr);
                }
                ctx->current_sequence_num = ctx->current_sequence_num + pkt_size;
                payload = payload + pkt_size;
                payload_size = payload_size - pkt_size;
            }
        } 

        if (event & APP_CLOSE_REQUESTED)
        {   
            bzero((tcphdr *)tcp_hdr, sizeof(tcphdr));
            tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
            tcp_hdr->th_off = 5;
            tcp_hdr->th_flags |= TH_FIN;
            tcp_hdr->th_win = htons(RECEIVER_WINDOW);
            ctx->current_sequence_num++;
            pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(tcphdr), NULL);
            if (ctx->connection_state == CSTATE_ESTABLISHED)
            {
                ctx->connection_state = FIN_WAIT_1;
            }
            else
            {
                ctx->connection_state = LAST_ACK;
                ctx->done = true;
                ctx->connection_state = CLOSED;
            }
            ctx->fin_ack_sequence_num = ctx->current_sequence_num;
        }

        /* etc. */
    }
}


/**********************************************************************/
/* our_dprintf
 *
 * Send a formatted message to stdout.
 * 
 * format               A printf-style format string.
 *
 * This function is equivalent to a printf, but may be
 * changed to log errors to a file if desired.
 *
 * Calls to this function are generated by the dprintf amd
 * dperror macros in transport.h
 */
void our_dprintf(const char *format,...)
{
    va_list argptr;
    char buffer[1024];

    assert(format);
    va_start(argptr, format);
    vsnprintf(buffer, sizeof(buffer), format, argptr);
    va_end(argptr);
    fputs(buffer, stdout);
    fflush(stdout);
}