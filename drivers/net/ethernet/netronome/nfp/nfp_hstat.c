// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2019 Netronome Systems, Inc. */

#include <net/hstats.h>

#include "nfp_net.h"

/* NFD per-vNIC stats */
static int
nfp_hstat_vnic_nfd_basic_get(struct net_device *netdev,
			     struct rtnl_hstat_req *req,
			     const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(netdev);
	u32 off;

	off = rtnl_hstat_is_rx(req) ?
		0 : NFP_NET_CFG_STATS_TX_OCTETS - NFP_NET_CFG_STATS_RX_OCTETS;

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_FRAMES + off));
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_OCTETS + off));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_nfd = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC_BIDIR(DEV),
	},

	.get_stats = nfp_hstat_vnic_nfd_basic_get,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT,
	},
	.stats_cnt = 2,
};

int nfp_net_hstat_get_groups(const struct net_device *netdev,
			     struct rtnl_hstat_req *req)
{
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd);

	return 0;
}
