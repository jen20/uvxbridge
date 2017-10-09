/*
 * Copyright (C) 2017 Joyent Inc.
 * All rights reserved.
 *
 * Written by: Matthew Macy <matt.macy@joyent.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>


#include "uvxbridge.h"

static void
usage(char *name)
{
	printf("usage %s -i <ingress> -e <egress> -c <config> -m <config mac address> -p <provisioning agent mac address> [-d]\n", name);
	exit(1);
}

uint64_t
mac_parse(char *input)
{
	char *idx, *mac = strdup(input);
	const char *del = ":";
	uint8_t mac_num[8];
	int i;

	for (i = 0; ((idx = strsep(&mac, del)) != NULL) && i < ETHER_ADDR_LEN; i++)
		mac_num[i] = strtol(idx, NULL, 16);
	free(mac);
	if (i < ETHER_ADDR_LEN)
		return 0;
	return  *(uint64_t *)&mac_num;
}


static int
netmap_setup(vxstate_t &state, char *ingress, char *egress, char *config)
{
	struct nm_desc *ni, *ne, *nc;

	ni = ne = nc = NULL;

	nc = nm_open(config, NULL, 0, NULL);
	if (nc == NULL) {
		D("cannot open %s", config);
		return (1);
	}
	state.vs_nm_config = nc;
	if (egress == NULL || config == NULL)
		return (0);
	ni = nm_open(ingress, NULL, NM_OPEN_NO_MMAP, nc);
	if (ni == NULL) {
		D("cannot open %s", ingress);
		return (1);
	}
	ne = nm_open(egress, NULL, NM_OPEN_NO_MMAP, nc);
	if (ne == NULL) {
		D("cannot open %s", egress);
		return (1);
	}
	state.vs_nm_egress = ne;
	state.vs_nm_ingress = ni;

	return (0);
}

int
main(int argc, char *const argv[])
{
	int ch;
	char *ingress, *egress, *config, *log;
	uint64_t pmac, cmac;
	vxstate_t state;
	bool debug;

	ingress = egress = config = NULL;
	pmac = cmac = 0;
	while ((ch = getopt(argc, argv, "i:e:c:m:p:l:d")) != -1) {
		switch (ch) {
			case 'i':
				ingress = optarg;
				break;
			case 'e':
				egress = optarg;
				break;
			case 'c':
				config = optarg;
				break;
			case 'p':
				pmac = mac_parse(optarg);
				break;
			case 'm':
				cmac = mac_parse(optarg);
				break;
			case 'l':
				log = optarg;
				break;
			case 'd':
				debug = true;
				break;
			case '?':
			default:
				usage(argv[0]);
		}
	}
	if (pmac == 0) {
		printf("missing provisioning agent mac address\n");
		usage(argv[0]);
	}
	if (cmac == 0) {
		printf("missing bridge configuration mac address\n");
		usage(argv[0]);
	}
	if (config == NULL) {
		printf("missing config netmap interface\n");
		usage(argv[0]);
	}
	if (ingress == NULL && !debug) {
		printf("missing ingress netmap interface\n");
		usage(argv[0]);
	}
	if (egress == NULL && !debug) {
		printf("missing egress netmap interface\n");
		usage(argv[0]);
	}
	state.vs_prov_mac = pmac;
	state.vs_ctrl_mac = cmac;
	if (netmap_setup(state, ingress, egress, config)) {
		printf("netmap setup failed - cannot continue\n");
		exit(1);
	}
	/* start datapath thread */
	/* .... */

	run_controlpath(state);
	return 0;
}
