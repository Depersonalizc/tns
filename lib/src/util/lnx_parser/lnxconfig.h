/*
 * lnxconfig.h - C lnx parser
 *
 * This file constains the public API and structs representing an lnx
 * file.  For an overview of how to use this parser, see demo.c.
 *
 * NOTE: This parser uses the "list.h" linked-list implementation
 * provided in the c-utils repo.  If you are using different list.h,
 * you may need to rename the list.h here to avoid conflicts.
 */
#ifndef __LNXCONFIG_H__
#define __LNXCONFIG_H__

#include <stddef.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include "src/util/lnx_parser/list.h"

#define LNX_IFNAME_MAX 64

typedef enum {
    ROUTING_MODE_NONE   = 0,   // Unspecified
    ROUTING_MODE_STATIC = 1,   // Static routes only (no RIP, used for hosts)
    ROUTING_MODE_RIP    = 2,   // Use RIP (default for routers)
} routing_mode_t;


// interface <ifname> <assigned ip>/<prefix> <udp_addr>:<udp_port>
typedef struct {
    char name[LNX_IFNAME_MAX];

    struct in_addr assigned_ip;
    int prefix_len;

    struct in_addr udp_addr;
    uint16_t udp_port;

    list_link_t link;
} lnx_interface_t;

// neighbor <dest_addr> at <udp-addr>:<udp port> via <ifname>
typedef struct {
    struct in_addr dest_addr;

    struct in_addr udp_addr;
    uint16_t udp_port;

    char ifname[LNX_IFNAME_MAX];

    list_link_t link;
} lnx_neighbor_t;

// rip advertise-to <dest>
typedef struct {
    struct in_addr dest;

    list_link_t link;
} lnx_rip_neighbor_t;

// route <network_addr>/<prefix> via <next_hop_addr>
typedef struct {
    struct in_addr network_addr;
    int prefix_len;

    struct in_addr next_hop;

    list_link_t link;
} lnx_static_route_t;


// Top-level struct that represents the lnx file
typedef struct {
    list_t interfaces;    // list of type lnx_interface_t
    list_t neighbors;     // list of type lnx_neighbor_t
    list_t rip_neighbors; // list of type lnx_advertise_neighbor_t
    list_t static_routes; // list of type lnx_static_route_t

    routing_mode_t routing_mode;
} lnxconfig_t;

// Parse the config
lnxconfig_t *lnxconfig_parse(const char *config_file);

// Free the config
void lnxconfig_destroy(lnxconfig_t *config);

#endif
