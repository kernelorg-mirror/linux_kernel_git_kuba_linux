// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2019 Netronome Systems, Inc. */

#include <net/hstats.h>

#include "nfp_net.h"

/* NFD per-vNIC stats */
static int
nfp_hstat_vnic_nfd_basic_get_rx(struct net_device *netdev,
				struct rtnl_hstat_req *req,
				const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(netdev);

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_FRAMES));
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_OCTETS));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_nfd_rx = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DEV, RX),
	},

	.get_stats = nfp_hstat_vnic_nfd_basic_get_rx,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT,
	},
	.stats_cnt = 2,
};

static int
nfp_hstat_vnic_nfd_basic_get_tx(struct net_device *netdev,
				struct rtnl_hstat_req *req,
				const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(netdev);

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS,
			nn_readq(nn, NFP_NET_CFG_STATS_TX_FRAMES));
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES,
			nn_readq(nn, NFP_NET_CFG_STATS_TX_OCTETS));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_nfd_tx = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DEV, TX),
	},

	.get_stats = nfp_hstat_vnic_nfd_basic_get_tx,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT,
	},
	.stats_cnt = 2,
};

int nfp_net_hstat_get_groups(const struct net_device *netdev,
			     struct rtnl_hstat_req *req)
{
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd_rx);
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd_tx);

	return 0;
}
