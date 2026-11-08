/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// PlayStation 2 platform bring-up for Betrock++.
// Uses the ps2dev "ps2_drivers" helper library, which loads the needed IRX
// modules (DEV9, SMAP, NETMAN, USB mass storage, ...) and starts lwIP.

#if defined(PS2)

#include <cstdio>

extern "C" {
#include <kernel.h>
#include <sifrpc.h>
}

#include <ps2_eeip_driver.h>
#include <ps2_filesystem_driver.h>
#include <ps2_network_driver.h>

namespace {
void OnProgress(EEIP_PROGRESS_EVENT ev, void*) {
	switch (ev) {
	case EEIP_PROGRESS_SETTING_LINK_MODE:
		std::printf("[net] setting link mode\n");
		break;
	case EEIP_PROGRESS_TCPIP_INIT:
		std::printf("[net] starting TCP/IP stack\n");
		break;
	case EEIP_PROGRESS_APPLYING_IP_CONFIG:
		std::printf("[net] applying IP configuration\n");
		break;
	case EEIP_PROGRESS_WAITING_LINK_UP:
		std::printf("[net] waiting for link (is the Ethernet cable plugged in?)\n");
		break;
	case EEIP_PROGRESS_LINK_UP:
		std::printf("[net] link up\n");
		break;
	case EEIP_PROGRESS_WAITING_DHCP:
		std::printf("[net] waiting for DHCP lease\n");
		break;
	case EEIP_PROGRESS_DHCP_BOUND:
		std::printf("[net] DHCP lease acquired\n");
		break;
	case EEIP_PROGRESS_READY:
		std::printf("[net] network ready\n");
		break;
	}
}
} // namespace

bool Platform::Init() {
	SifInitRpc(0);

	// Filesystem first so relative paths (config, world/) resolve on the boot device
	init_ps2_filesystem_driver();

	if (init_network_driver(true) != EEIP_INIT_STATUS_OK) {
		std::printf("[net] failed to initialise the network drivers (no network adapter?)\n");
		return false;
	}

	eeip_network_config_t cfg;
	eeip_network_config_default_dhcp(&cfg);
	cfg.timeout_seconds = 20;
	cfg.on_progress = OnProgress;

	if (configure_eeip_network(&cfg) != EEIP_NET_STATUS_OK) {
		std::printf("[net] network configuration failed\n");
		return false;
	}

	struct ip4_addr ip, nm, gw;
	if (eeip_get_current_config(&ip, &nm, &gw) == 0) {
		const unsigned char* b = reinterpret_cast<const unsigned char*>(&ip.addr); // network byte order
		std::printf("[net] IP address: %u.%u.%u.%u\n", b[0], b[1], b[2], b[3]);
	}
	return true;
}
#endif