/*
 * transport.c 
 *
 * EN.601.414/614: HW#3 (STCP)
 *
 * This file implements the STCP layer that sits between the
 * mysocket and network layers. You are required to fill in the STCP
 * functionality in this file. 
 *
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mysock.h"
#include "stcp_api.h"
#include "transport.h"
#include <time.h>

#define STCP_MSS 536
#define RECEIVER_WINDOW 3072
#define SENDER_WINDOW 3072

enum { 
    CSTATE_ESTABLISHED,
    CSTATE_CLOSED,
    CSTATE_LISTEN,
    CSTATE_SYN_SENT,
    CSTATE_SYN_RECEIVED,
    CSTATE_FIN_WAIT_1,
    CSTATE_FIN_WAIT_2,
    CSTATE_CLOSE_WAIT,
    CSTATE_LAST_ACK,
    CSTATE_TIME_WAIT

};    /* obviously you should have more states */


/* this structure is global to a mysocket descriptor */
typedef struct
{
    bool_t done;    /* TRUE once connection is closed */

    int connection_state;   /* state of the connection (established, etc.) */
    tcp_seq initial_sequence_num;

    /* any other connection-wide global variables go here */
    tcp_seq opp_sequence_num;
    tcp_seq ack_num;
    int opp_window_size;
    tcp_seq current_sequence_num;
    tcp_seq fin_ack_sequence_num;

    
} context_t;


static void generate_initial_seq_num(context_t *ctx);
static void control_loop(mysocket_t sd, context_t *ctx);




/* initialise the transport layer, and start the main loop, handling
 * any data from the peer or the application.  this function should not
 * return until the connection is closed.
 */
void transport_init(mysocket_t sd, bool_t is_active)
{
    context_t *ctx;


    ctx = (context_t *) calloc(1, sizeof(context_t));
    assert(ctx);

    generate_initial_seq_num(ctx);

    /* XXX: you should send a SYN packet here if is_active, or wait for one
     * to arrive if !is_active.  after the handshake completes, unblock the
     * application with stcp_unblock_application(sd).  you may also use
     * this to communicate an error condition back to the application, e.g.
     * if connection fails; to do so, just set errno appropriately (e.g. to
     * ECONNREFUSED, etc.) before calling the function.
     */
    int send_pkt_size, recv_pkt_size;


    STCPHeader *tcp_hdr;
    tcp_hdr = (STCPHeader *)calloc(1, sizeof(STCPHeader));
    assert(tcp_hdr);

    if (is_active) {

       
        printf("Sending SYN packet.\n");
        tcp_hdr->th_seq = htonl(ctx->initial_sequence_num);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_SYN;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);

        printf("Waiting for SYN-ACK packet.\n");

        bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(STCPHeader));
        if ((tcp_hdr->th_flags & TH_ACK) && (tcp_hdr->th_flags & TH_SYN) && (ntohl(tcp_hdr->th_ack) == ctx->current_sequence_num))
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }
        else
        {
            errno = ECONNREFUSED;
        }

        ctx->connection_state= CSTATE_SYN_SENT;
        bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
        tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
        tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_ACK;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);



       
    
   
    } else {
        // wait for syn
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(STCPHeader));
        if (tcp_hdr->th_flags & TH_SYN)
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }
        else
        {
            errno = ECONNREFUSED;
        }
        ctx->connection_state = CSTATE_SYN_RECEIVED;

        /*--- SYN-ACK Packet ---*/
        bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
        tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
        tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
        tcp_hdr->th_off = 5;
        tcp_hdr->th_flags |= TH_SYN;
        tcp_hdr->th_flags |= TH_ACK;
        tcp_hdr->th_win = htons(RECEIVER_WINDOW);
        ctx->current_sequence_num++;
        send_pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);

        /*--- Receive ACK Packet ---*/
        bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
        recv_pkt_size = stcp_network_recv(sd, tcp_hdr, sizeof(STCPHeader));
        if ((tcp_hdr->th_flags & TH_ACK) && (tcp_hdr->th_ack == ctx->current_sequence_num))
        {
            ctx->opp_sequence_num = ntohl(tcp_hdr->th_seq);
            ctx->opp_window_size = ntohs(tcp_hdr->th_win);
        }

        // send syn ack

        // wait for ack

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
    /*ctx->initial_sequence_num =;*/
    srand(time(NULL)); // 使用当前时间作为随机数生成的种子
    int r=rand()%256;
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
    STCPHeader *tcp_hdr;
    char* payload;
    char* payload1;
    int payload_size, pkt_size;
    int current_sender_window;

    tcp_hdr = (STCPHeader *)calloc(1, sizeof(STCPHeader));

    while (!ctx->done)
    {
        unsigned int event;

        /* see stcp_api.h or stcp_api.c for details of this function */
        /* XXX: you will need to change some of these arguments! */
        printf("Waiting for data from application.\n");
        event = stcp_wait_for_event(sd, ANY_EVENT, NULL);
    
        /* check whether it was the network, app, or a close request */
        if (event & APP_DATA && ctx->connection_state == CSTATE_ESTABLISHED) {
    
            payload = (char *) calloc(1, SENDER_WINDOW);
            bzero((char *)payload, SENDER_WINDOW);
            
            payload_size = stcp_app_recv(sd, payload, current_sender_window);
            
            while (payload_size > 0)
            {
                bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
                tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                if(payload_size > STCP_MSS)
                {
                    pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), payload, STCP_MSS, NULL);
                    pkt_size = STCP_MSS;
                }
                else
                {
                    pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), payload, payload_size, NULL);
                    pkt_size = pkt_size - sizeof(STCPHeader);
                }
                ctx->current_sequence_num = ctx->current_sequence_num + pkt_size;
                payload = payload + pkt_size;
                payload_size = payload_size - pkt_size;
            }
        }
        printf("Waiting for data from network.\n");
        if (event & NETWORK_DATA && ctx->connection_state == CSTATE_ESTABLISHED) {
    
            
           payload1 = (char *) calloc(1, STCP_MSS+20);
            bzero((char *)payload1, STCP_MSS+20);

            pkt_size = stcp_network_recv(sd, payload1, STCP_MSS+20);

            bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
            tcp_hdr = (STCPHeader *)payload1;
            
            if (tcp_hdr->th_flags & TH_ACK)
            {
                /*--- Setting Context ---*/
                ctx->ack_num = ntohl(tcp_hdr->th_ack);
                ctx->opp_window_size = ntohs(tcp_hdr->th_win);

                if (ntohl(tcp_hdr->th_ack) == ctx->fin_ack_sequence_num)
                {
                    if (ctx->connection_state == CSTATE_FIN_WAIT_1)
                    {
                        ctx->connection_state = CSTATE_FIN_WAIT_2;
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
                bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
                tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + 1);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_flags |= TH_ACK;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);
                if (ctx->connection_state == CSTATE_ESTABLISHED)
                {
                    ctx->connection_state = CSTATE_CLOSE_WAIT;
                }
                else if (ctx->connection_state == CSTATE_FIN_WAIT_2)
                {
                    ctx->connection_state = CSTATE_CLOSED;
                    ctx->done = TRUE;
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
                bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
                tcp_hdr->th_ack = htonl(ctx->opp_sequence_num + payload_size);
                tcp_hdr->th_off = 5;
                tcp_hdr->th_flags |= TH_ACK;
                tcp_hdr->th_win = htons(RECEIVER_WINDOW);
                pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);
            }
        }
        if (event & APP_CLOSE_REQUESTED) {
            bzero((STCPHeader *)tcp_hdr, sizeof(STCPHeader));
            tcp_hdr->th_seq = htonl(ctx->current_sequence_num);
            tcp_hdr->th_off = 5;
            tcp_hdr->th_flags |= TH_FIN;
            tcp_hdr->th_win = htons(RECEIVER_WINDOW);
            ctx->current_sequence_num++;
            pkt_size = stcp_network_send(sd, tcp_hdr, sizeof(STCPHeader), NULL);
            if (ctx->connection_state == CSTATE_ESTABLISHED)
            {
                ctx->connection_state = CSTATE_FIN_WAIT_1;
            }
            else
            {
                ctx->connection_state = CSTATE_LAST_ACK;
                ctx->done = TRUE;
                ctx->connection_state = CSTATE_CLOSED;
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



