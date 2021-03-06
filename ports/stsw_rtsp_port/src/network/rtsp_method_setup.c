/**
 * This file is part of stsw_rtsp_port, which belongs to StreamSwitch
 * project. And it's derived from Feng prject originally
 * 
 * Copyright (C) 2015  OpenSight team (www.opensight.cn)
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 **/

/** @file
 * @brief Contains SETUP method and reply handlers
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdbool.h>
#include <glib.h>

#include <liberis/headers.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include "feng.h"
#include "rtp.h"
#include "rtsp.h"
#include "ragel_parsers.h"
#include "fnc_log.h"
#include "media/demuxer.h"


/* get the current socket buffer size */
static unsigned get_sock_buffer_size(Sock *socket_obj, int bufOptName) {
  unsigned curSize;
  socklen_t sizeSize = sizeof curSize;
  if(socket_obj == NULL || socket_obj->fd == 0) {
      return 0;
  }

  if (getsockopt(socket_obj->fd, SOL_SOCKET, bufOptName,
		 (char*)&curSize, &sizeSize) < 0) {
    fnc_log(FNC_LOG_ERR,"getBufferSize() error");
    return 0;
  }

  return curSize;
}

/* increase the socket buffer */
static unsigned increase_sock_buffer_to(Sock *socket_obj, int bufOptName, unsigned requested_size) {
  // First, get the current buffer size.  If it's already at least
  // as big as what we're requesting, do nothing.
  unsigned curSize = get_sock_buffer_size(socket_obj, bufOptName);

  // Next, try to increase the buffer to the requested size,
  // or to some smaller size, if that's not possible:
  while (requested_size > curSize) {
    socklen_t sizeSize = sizeof requested_size;
    if (setsockopt(socket_obj->fd, SOL_SOCKET, bufOptName,
		   (char*)&requested_size, sizeSize) >= 0) {
      // success
      return requested_size;
    }
    requested_size = (requested_size+curSize)/2;
  }

  return get_sock_buffer_size(socket_obj, bufOptName);
}

#ifndef RTP_BUF_SIZE
#define RTP_BUF_SIZE (1024*1024)     /* 1 MB */
#endif

#ifndef RTPoverTCP_BUF_SIZE
#define RTPoverTCP_BUF_SIZE (512*1024) /* 512 KB */
#endif

#if 0
/**
 * @brief Parse the Host header and eventually set it at 
 *        transport structure
 *
 * @param req The request to check and parse
 * @param transport The transport structure to fill in host addr
 *
 * @retval RTSP_Ok Parsing completed correctly.
 *
 * @retval RTSP_NotImplemented The Host: header specifies a 
 *                             invalid ipv4 address.
 *
 */
static RTSP_ResponseCode parse_host_header(RTSP_Request *req, RTP_transport *transport)
{

    const char *host_hdr = g_hash_table_lookup(req->headers, "Host");
    struct in_addr inAddr;


    /* If we have no scale header, just return OK.
     */
    if ( host_hdr == NULL) {
        return RTSP_Ok;
    }
    if( host_hdr != NULL ) {
        fnc_log(FNC_LOG_VERBOSE, "Host header: %s\n", host_hdr);
    }


    /* check Host string value */
    if(!inet_aton(host_hdr, &inAddr)) {
        return RTSP_NotImplemented;
    }
    if(strlen(host_hdr) >= 64 ) {
        return RTSP_NotImplemented; // The header is too long
    }

    strncpy(transport->destination, host_hdr, sizeof(transport->destination));


    return RTSP_Ok;
}
#endif

void free_sock(gpointer data)
{
    Sock_close((Sock *)data);
}
static int get_sock_pair(Sock * sock_pair[2])
{
    Sock * rtp_sock = NULL, * rtcp_sock = NULL;
    char port_buffer[8];
    int ret = -1;
    int rtp_port_num = 0, rtcp_port_num = 0;
    GHashTable *socket_hash_table = NULL; 
    gboolean success = false;
    int i;
    
    socket_hash_table = g_hash_table_new_full(g_int_hash, g_int_equal,
                                                         g_free, free_sock);
    if(socket_hash_table == NULL){
        return -1;
    }
    for(i=0;i<8192;i++){  // try for 8192 times
        
        //create a new rtp socket with a random port
        snprintf(port_buffer, 8, "%d", 0);
        rtp_sock = Sock_bind(NULL, port_buffer, NULL, UDP, NULL);    
        if (rtp_sock == NULL) {
            ret = -1;
            break;
        }
        rtp_port_num = get_local_port(rtp_sock);

        
	  

        // To be usable for RTP, the client port number must be even:
        if ((rtp_port_num&1) != 0) { // it's odd
            // Record this socket in our table, and keep trying:
            int * key = g_new0(int, 1);
            *key = rtp_port_num;
            g_hash_table_insert(socket_hash_table, key, rtp_sock);
            rtp_sock = NULL;
            continue;
        }

        // Make sure we can use the next (i.e., odd) port number, for RTCP:
        rtcp_port_num = rtp_port_num|1;
        snprintf(port_buffer, 8, "%d", rtcp_port_num);
        rtcp_sock = Sock_bind(NULL, port_buffer, NULL, UDP, NULL);    
        if (rtcp_sock != NULL && get_local_port(rtcp_sock) >= 0) {
            success = true;
            ret = 0;
            break;
        }else{
            if(rtcp_sock != NULL){
                Sock_close(rtcp_sock);
                rtcp_sock = NULL;
            }
            int * key = g_new0(int, 1);
            *key = rtp_port_num;
            g_hash_table_insert(socket_hash_table, key, rtp_sock);
            rtp_sock = NULL;
            continue;
        }
    }
    g_hash_table_destroy(socket_hash_table);
    
    if(success == false){
        return ret;
    }
    
    sock_pair[0] = rtp_sock;
    sock_pair[1] = rtcp_sock;   
    
    return 0;
}


/**
 * bind&connect the socket
 */
static RTSP_ResponseCode unicast_transport(RTSP_Client *rtsp,
                                           RTP_transport *transport,
                                           uint16_t rtp_port, uint16_t rtcp_port)
{
    char port_buffer[8];

    Sock * sock_pair[2];

    unsigned buffer_size = 0;
/*
    if (RTP_get_port_pair(rtsp->srv, &ser_ports) != ERR_NOERROR) {
        return RTSP_InternalServerError;
    }
*/
    /*Jmkn: Find a RTP&RTCP port pair available in system */
    sock_pair[0] = sock_pair[1] = NULL;
    if(get_sock_pair(sock_pair)){
        fnc_log(FNC_LOG_ERR,"Fail to get UDP port pair for RTP session");
        return RTSP_UnsupportedTransport;
    }

    

    //UDP bind for outgoing RTP packets
    transport->rtp_sock = sock_pair[0];
    /*increase the buffer to RTP_BUF_SIZE */
    buffer_size = increase_sock_buffer_to(transport->rtp_sock, SO_SNDBUF, RTP_BUF_SIZE);
    fnc_log(FNC_LOG_VERBOSE,"[rtsp] Set rtp data socket send buffer size to %u", buffer_size);


    //UDP bind for outgoing RTCP packets
    transport->rtcp_sock = sock_pair[1];
    
    
    //UDP connection for outgoing RTP packets
    snprintf(port_buffer, 8, "%d", rtp_port);
    if(transport->destination[0] != '\0') {
        Sock_connect (transport->destination, port_buffer,
                      transport->rtp_sock, UDP, NULL);
    }else{
        Sock_connect (get_remote_host(rtsp->sock), port_buffer,
                      transport->rtp_sock, UDP, NULL);
    }


    //UDP connection for outgoing RTCP packets
    snprintf(port_buffer, 8, "%d", rtcp_port);
    if(transport->destination[0] != '\0') {
        Sock_connect (transport->destination, port_buffer,
                      transport->rtcp_sock, UDP, NULL);
        
    }else{
        Sock_connect (get_remote_host(rtsp->sock), port_buffer,
                      transport->rtcp_sock, UDP, NULL);
    }


    if (!transport->rtp_sock)
        return RTSP_UnsupportedTransport;

    return RTSP_Ok;
}

/**
 * @brief Check the value parsed out of a transport specification.
 *
 * @param rtsp Client from which the request arrived
 * @param rtp_t The transport instance to set up with the parsed parameters
 * @param transport Structure containing the transport's parameters
 */
gboolean check_parsed_transport(RTSP_Client *rtsp, RTP_transport *rtp_t,
                                struct ParsedTransport *transport)
{

    unsigned buffer_size = 0;


    switch ( transport->protocol ) {
    case TransportUDP:
        if ( transport->mode == TransportUnicast ) {
            return ( unicast_transport(rtsp, rtp_t,
                                       transport->parameters.UDP.Unicast.port_rtp,
                                       transport->parameters.UDP.Unicast.port_rtcp)
                     == RTSP_Ok );
        } else { /* Multicast */
            return false;
        }
    case TransportTCP:
        if ( transport->parameters.TCP.ich_rtp &&
             !transport->parameters.TCP.ich_rtcp )
            transport->parameters.TCP.ich_rtcp = transport->parameters.TCP.ich_rtp + 1;

        if ( !transport->parameters.TCP.ich_rtp ) {
            /** @todo This part was surely broken before, so needs to be
             * written from scratch */
        }

        if ( transport->parameters.TCP.ich_rtp > 255 &&
             transport->parameters.TCP.ich_rtcp > 255 ) {
            fnc_log(FNC_LOG_ERR,
                    "Interleaved channel number already reached max\n");
            return false;
        }

        /*increase the buffer to RTP_BUF_SIZE */
        buffer_size = increase_sock_buffer_to(rtsp->sock, SO_SNDBUF, RTPoverTCP_BUF_SIZE);
        do {
            int nodelay = 1;
            setsockopt(rtsp->sock->fd, IPPROTO_TCP,TCP_NODELAY,&nodelay, sizeof(nodelay));/* disable Nagle for rtp over rtsp*/
        } while (0);
        fnc_log(FNC_LOG_VERBOSE,"[rtsp] Set RTPoverTCP data tcp socket send buffer size to %u", buffer_size);

        return interleaved_setup_transport(rtsp, rtp_t,
                                           transport->parameters.TCP.ich_rtp,
                                           transport->parameters.TCP.ich_rtcp);

    default:
        return false;
    }
}

/**
 * Gets the track requested for the object
 *
 * @param rtsp_s the session where to save the addressed resource
 *
 * @return The pointer to the requested track
 *
 * @retval NULL Unable to find the requested resource or track, or
 *              other errors. The client already received a response.
 */
static Track *select_requested_track(RTSP_Request *req, RTSP_session *rtsp_s)
{
    feng *srv = rtsp_s->srv;
    char *trackname = NULL;
    Track *selected_track = NULL;

    /* Just an extra safety to abort if we have URI and not resource
     * or vice-versa */
    g_assert( (rtsp_s->resource && rtsp_s->resource_uri) ||
              (!rtsp_s->resource && !rtsp_s->resource_uri) );

    /* Check if the requested URL is valid. If we already have a
     * session open, the resource URL has to be the same; otherwise,
     * we have to check if we're given a propr presentation URL
     * (having the SDP_TRACK_SEPARATOR string in it).
     */
    if ( !rtsp_s->resource ) {
        /* Here we don't know the URL and we have to find it out, we
         * check for the presence of the SDP_TRACK_URI_SEPARATOR */
        Url url;
        char *path;

        char *separator = strstr(req->object, SDP_TRACK_URI_SEPARATOR);

        /* If we found no separator it's a resource URI */
        if ( separator == NULL ) {
            rtsp_quick_response(req, RTSP_AggregateOnly);
            return NULL;
        }

        trackname = separator + strlen(SDP_TRACK_URI_SEPARATOR);

        /* Here we set the base resource URI, which is different from
         * the path; since the object is going to be used and freed we
         * have to dupe it here. */
        rtsp_s->resource_uri = g_strndup(req->object, separator - req->object);

        Url_init(&url, rtsp_s->resource_uri);
        path = g_uri_unescape_string(url.path, "/");
        Url_destroy(&url);
        if(req->client->cached_resource != NULL){
            rtsp_s->resource = req->client->cached_resource;
            req->client->cached_resource = NULL;
        }else{
            if (!(rtsp_s->resource = r_open(srv, path))) {
                fnc_log(FNC_LOG_DEBUG, "Resource for %s not found\n", path);

                g_free(path);
                g_free(rtsp_s->resource_uri);
                rtsp_s->resource_uri = NULL;

                rtsp_quick_response(req, RTSP_NotFound);
                return NULL;
            } 
        }



        /*set rtsp session into the resource*/
        rtsp_s->resource->rtsp_sess = rtsp_s;


        g_free(path);
    } else {
        /* We know the URL already */
        const size_t resource_uri_length = strlen(rtsp_s->resource_uri);

        /* Check that it starts with the correct resource URI */
        if ( strncmp(req->object, rtsp_s->resource_uri, resource_uri_length) != 0 ) {
            rtsp_quick_response(req, RTSP_AggregateNotAllowed);
            return NULL;
        }

        /* Now make sure that we got a presentation URL, rather than a
         * resource URL
         */
        if ( strncmp(req->object + resource_uri_length,
                     SDP_TRACK_URI_SEPARATOR,
                     strlen(SDP_TRACK_URI_SEPARATOR)) != 0 ) {
            rtsp_quick_response(req, RTSP_AggregateOnly);
            return NULL;
        }

        trackname = req->object
            + resource_uri_length
            + strlen(SDP_TRACK_URI_SEPARATOR);
    }

    if ( (selected_track = r_find_track(rtsp_s->resource, trackname))
         == NULL )
        rtsp_quick_response(req, RTSP_NotFound);

    return selected_track;
}

/**
 * Sends the reply for the setup method
 * @param rtsp the buffer where to write the reply
 * @param req The client request for the method
 * @param session the new RTSP session allocated for the client
 * @param rtp_s the new RTP session allocated for the client
 */
static void send_setup_reply(RTSP_Client * rtsp, RTSP_Request *req, RTSP_session * session, RTP_session * rtp_s)
{
    RTSP_Response *response = rtsp_response_new(req, RTSP_Ok);
    GString *transport = g_string_new("");

    if (!rtp_s->transport.rtp_sock)
        return;
    switch (Sock_type(rtp_s->transport.rtp_sock)) {
    case UDP:
        /*
          if (Sock_flags(rtp_s->transport.rtp_sock)== IS_MULTICAST) {
	  g_string_append_printf(reply,
				 "RTP/AVP;multicast;ttl=%d;destination=%s;port=",
				 // session->resource->info->ttl,
				 DEFAULT_TTL,
				 session->resource->info->multicast);
                 } else */
        { // XXX handle TLS here


            if(rtp_s->transport.destination[0] != '\0') {
                g_string_append_printf(transport,
                                       "RTP/AVP;unicast;source=%s;destination=%s;"
                                       "client_port=%d-%d;server_port=",
                                       get_local_host(rtsp->sock),
                                       rtp_s->transport.destination,
                                       get_remote_port(rtp_s->transport.rtp_sock),
                                       get_remote_port(rtp_s->transport.rtcp_sock));

            }else{
                g_string_append_printf(transport,
                                       "RTP/AVP;unicast;source=%s;"
                                       "client_port=%d-%d;server_port=",
                                       get_local_host(rtsp->sock),
                                       get_remote_port(rtp_s->transport.rtp_sock),
                                       get_remote_port(rtp_s->transport.rtcp_sock));
            }

        }

        g_string_append_printf(transport, "%d-%d",
                               get_local_port(rtp_s->transport.rtp_sock),
                               get_local_port(rtp_s->transport.rtcp_sock));

        break;
    case LOCAL:
        if (Sock_type(rtsp->sock) == TCP) {
            g_string_append_printf(transport,
                                   "RTP/AVP/TCP;interleaved=%d-%d",
                                   rtp_s->transport.rtp_ch,
                                   rtp_s->transport.rtcp_ch);
        }

        break;
    default:
        break;
    }
    g_string_append_printf(transport, ";ssrc=%08X", rtp_s->ssrc);

    g_hash_table_insert(response->headers,
                        g_strdup(eris_hdr_transport),
                        g_string_free(transport, false));

    /* We add the Session here since it was not added by rtsp_response_new (the
     * incoming request had no session).
     */
    g_hash_table_insert(response->headers,
                        g_strdup(eris_hdr_session),
                        g_strdup(session->session_id));

    rtsp_response_send(response);
}

/**
 * RTSP SETUP method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 */
void RTSP_setup(RTSP_Client * rtsp, RTSP_Request *req)
{
    const char *transport_header = NULL;
    RTP_transport transport;

    Track *req_track = NULL;

    //mediathread pointers
    RTP_session *rtp_s = NULL;
    RTSP_session *rtsp_s;

    // init
    memset(&transport, 0, sizeof(transport));

    if ( !rtsp_request_check_url(req) )
        return;



    do{
        RTSP_ResponseCode error;
        if ( (error = parse_require_header(req)) != RTSP_Ok ){
            rtsp_quick_response(req, error);
            return;
        }

    }while (0);



    /* Parse the transport header through Ragel-generated state machine.
     *
     * The full documentation of the Transport header syntax is available in
     * RFC2326, Section 12.39.
     *
     * If the parsing returns false, we should respond to the client with a
     * status 461 "Unsupported Transport"
     */
    transport_header = g_hash_table_lookup(req->headers, "Transport");
    if ( transport_header == NULL ||
         !ragel_parse_transport_header(rtsp, &transport, transport_header) ) {

        rtsp_quick_response(req, RTSP_UnsupportedTransport);
        return;
    }

    /* Check if we still have space for new connections, if not, respond with a
     * 453 status (Not Enough Bandwidth), so that client knows what happened. */
    if (rtsp->srv->connection_count > rtsp->srv->srvconf.max_conns) {
        /* @todo should redirect, but we haven't the code to do that just
         * yet. */
        rtsp_quick_response(req, RTSP_NotEnoughBandwidth);
        return;
    }

    /* Here we'd be adding a new session if we supported more than
     * one, and the user didn't provide one. */
    if ( (rtsp_s = rtsp->session) == NULL )
        rtsp_s = rtsp_session_new(rtsp);

    /* Get the selected track; if the URI was invalid or the track
     * couldn't be found, the function will take care of sending out
     * the error response, so we don't need to do anything else.
     */
    if ( (req_track = select_requested_track(req, rtsp_s)) == NULL )
        return;

    rtp_s = rtp_session_new(rtsp, rtsp_s, &transport, req->object, req_track);

    send_setup_reply(rtsp, req, rtsp_s, rtp_s);

    if ( rtsp_s->cur_state == RTSP_SERVER_INIT )
        rtsp_s->cur_state = RTSP_SERVER_READY;
}
