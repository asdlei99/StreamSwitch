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
/**
 * @file
 *
 * @brief Shared definitions between Ragel parsers and their users
 */

#ifndef FENG_RAGEL_PARSERS_H
#define FENG_RAGEL_PARSERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>


struct RTSP_Client;
struct RTP_transport;

/**
 * @defgroup ragel Ragel parsing
 * @ingroup RTSP
 *
 * @brief Functions and data structure for parsing of RTSP protocol.
 *
 * This group enlists all the shared data structure between the
 * parsers written using Ragel (http://www.complang.org/ragel/) and
 * the users of those parsers, usually the method handlers (see @ref
 * rtsp_methods).
 *
 * @{
 */

/**
 * @defgroup ragel_transport Transport: header parsing
 *
 * @{
 */

/**
 * @brief Structure filled by the ragel parser of the transport header.
 *
 * @internal
 */
struct ParsedTransport {
    enum { TransportUDP, TransportTCP, TransportSCTP } protocol;
    //! Mode for UDP transmission, here is easier to access
    enum { TransportUnicast, TransportMulticast } mode;
    union {
        union {
            struct {
                uint16_t port_rtp;
                uint16_t port_rtcp;
            } Unicast;
            struct {
            } Multicast;
        } UDP;
        struct {
            uint16_t ich_rtp;  //!< Interleaved channel for RTP
            uint16_t ich_rtcp; //!< Interleaved channel for RTCP
        } TCP;
        struct {
            uint16_t ch_rtp;  //!< SCTP channel for RTP
            uint16_t ch_rtcp; //!< SCTP channel for RTCP
        } SCTP;
    } parameters;
};

gboolean check_parsed_transport(struct RTSP_Client *rtsp,
                                struct RTP_transport *rtp_t,
                                struct ParsedTransport *transport);


gboolean ragel_parse_transport_header(struct RTSP_Client *rtsp,
                                      struct RTP_transport *rtp_t,
                                      const char *header);
/**
 *@}
 */

/**
 * @defgroup ragel_range Range: header parsing
 *
 * @{
 */

struct RTSP_Range;

gboolean ragel_parse_range_header(const char *header,
                                  RTSP_Range *range);

/**
 *@}
 */


/**
 *@}
 */
#ifdef __cplusplus
}
#endif

#endif
