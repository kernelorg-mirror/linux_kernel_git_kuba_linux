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

/* NFD per-Q stats */
static int
nfp_hstat_vnic_nfd_get_queues(const struct net_device *dev,
			      const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(dev);

	return nn->max_r_vecs;
}

static int
nfp_hstat_vnic_nfd_pq_get(struct net_device *dev,
			  struct rtnl_hstat_req *req,
			  const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(dev);
	u32 queue, off;

	queue = rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_QUEUE);

	off = NFP_NET_CFG_TXR_STATS(queue);
	off += rtnl_hstat_is_rx(req) ?
		0 : NFP_NET_CFG_RXR_STATS_BASE - NFP_NET_CFG_TXR_STATS_BASE;

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS, nn_readq(nn, off));
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES,
			nn_readq(nn, off + 8));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_nfd_pq = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC_BIDIR(DEV),
		[RTNL_HSTATS_QUAL_QUEUE] = {
			.get_max	= nfp_hstat_vnic_nfd_get_queues,
		},
	},

	.get_stats = nfp_hstat_vnic_nfd_pq_get,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT,
	},
	.stats_cnt = 2,
};

/* vNIC software stats */
static int
nfp_hstat_vnic_sw_rx_get(struct net_device *dev, struct rtnl_hstat_req *req,
			 const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(dev);
	struct nfp_net_r_vector *r_vec;

	r_vec = &nn->r_vecs[rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_QUEUE)];

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS, r_vec->rx_pkts);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES, r_vec->rx_bytes);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_CSUM_PARTIAL,
			r_vec->hw_csum_rx_complete);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_CSUM_UNNECESSARY,
			r_vec->hw_csum_rx_ok + r_vec->hw_csum_rx_inner_ok);
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_sw_rx = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DRV, RX),
		[RTNL_HSTATS_QUAL_QUEUE] = {
			.get_max	= nfp_hstat_vnic_nfd_get_queues,
		},
	},

	.get_stats = nfp_hstat_vnic_sw_rx_get,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT |
			RTNL_HSTATS_STAT_LINUX_CSUM_PARTIAL_BIT |
			RTNL_HSTATS_STAT_LINUX_CSUM_UNNECESSARY_BIT,

	},
	.stats_cnt = 4,
};

static int
nfp_hstat_vnic_sw_tx_get(struct net_device *dev, struct rtnl_hstat_req *req,
			 const struct rtnl_hstat_group *grp)
{
	struct nfp_net *nn = netdev_priv(dev);
	struct nfp_net_r_vector *r_vec;

	r_vec = &nn->r_vecs[rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_QUEUE)];

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_PKTS, r_vec->tx_pkts);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BYTES, r_vec->tx_bytes);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_BUSY, r_vec->tx_busy);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_CSUM_PARTIAL,
			r_vec->hw_csum_tx + r_vec->hw_csum_tx_inner);
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_LINUX_SEGMENTATION_OFFLOAD_PKTS,
			r_vec->tx_lso);
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_vnic_sw_tx = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DRV, TX),
		[RTNL_HSTATS_QUAL_QUEUE] = {
			.get_max	= nfp_hstat_vnic_nfd_get_queues,
		},
	},

	.get_stats = nfp_hstat_vnic_sw_tx_get,
	.stats	= {
		[0] =	RTNL_HSTATS_STAT_LINUX_PKTS_BIT |
			RTNL_HSTATS_STAT_LINUX_BYTES_BIT |
			RTNL_HSTATS_STAT_LINUX_BUSY_BIT |
			RTNL_HSTATS_STAT_LINUX_CSUM_PARTIAL_BIT |
			RTNL_HSTATS_STAT_LINUX_SEGMENTATION_OFFLOAD_PKTS_BIT,
	},
	.stats_cnt = 5,
};

static const struct rtnl_hstat_group nfp_hstat_vnic_sw = {
	.has_children = true,
	.children = {
		&nfp_hstat_vnic_sw_rx,
		&nfp_hstat_vnic_sw_tx,
		NULL,
	},
};

int nfp_net_hstat_get_groups(const struct net_device *netdev,
			     struct rtnl_hstat_req *req)
{
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_sw);
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd_pq);
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd);

	return 0;
}
