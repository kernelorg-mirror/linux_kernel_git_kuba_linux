// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2019 Netronome Systems, Inc. */

#include <net/hstats.h>

#include "nfp_net.h"
#include "nfp_port.h"

/* MAC stats */
static const struct nfp_stat_pair {
	u32 attr;
	u32 offset;
} nfp_mac_stats_rx[] = {
	{
		IFLA_HSTATS_STAT_RFC2819_etherStatsOctets,
		NFP_MAC_STATS_RX_IN_OCTETS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_FrameTooLongErrors,
		NFP_MAC_STATS_RX_FRAME_TOO_LONG_ERRORS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_InRangeLengthErrors,
		NFP_MAC_STATS_RX_RANGE_LENGTH_ERRORS
	},
	/* missing NFP_MAC_STATS_RX_VLAN_RECEIVED_OK, no standard counter */
	{
		IFLA_HSTATS_STAT_RFC2863_Errors,
		NFP_MAC_STATS_RX_IN_ERRORS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsBroadcastPkts,
		NFP_MAC_STATS_RX_IN_BROADCAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsDropEvents,
		NFP_MAC_STATS_RX_DROP_EVENTS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_AlignmentErrors,
		NFP_MAC_STATS_RX_ALIGNMENT_ERRORS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_PAUSEMACCtrlFrames,
		NFP_MAC_STATS_RX_PAUSE_MAC_CTRL_FRAMES
	}, {
		IFLA_HSTATS_STAT_IEEE8023_FramesOK,
		NFP_MAC_STATS_RX_FRAMES_RECEIVED_OK
	}, {
		IFLA_HSTATS_STAT_IEEE8023_FrameCheckSequenceErrors,
		NFP_MAC_STATS_RX_FRAME_CHECK_SEQUENCE_ERRORS
	}, {
		IFLA_HSTATS_STAT_RFC2863_UcastPkts,
		NFP_MAC_STATS_RX_UNICAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsMulticastPkts,
		NFP_MAC_STATS_RX_MULTICAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts,
		NFP_MAC_STATS_RX_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsUndersizePkts,
		NFP_MAC_STATS_RX_UNDERSIZE_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts64Octets,
		NFP_MAC_STATS_RX_PKTS_64_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts65to127Octets,
		NFP_MAC_STATS_RX_PKTS_65_TO_127_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts512to1023Octets,
		NFP_MAC_STATS_RX_PKTS_512_TO_1023_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts1024to1518Octets,
		NFP_MAC_STATS_RX_PKTS_1024_TO_1518_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsJabbers,
		NFP_MAC_STATS_RX_JABBERS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsFragments,
		NFP_MAC_STATS_RX_FRAGMENTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts128to255Octets,
		NFP_MAC_STATS_RX_PKTS_128_TO_255_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts256to511Octets,
		NFP_MAC_STATS_RX_PKTS_256_TO_511_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts1519toMaxOctets,
		NFP_MAC_STATS_RX_PKTS_1519_TO_MAX_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsOversizePkts,
		NFP_MAC_STATS_RX_OVERSIZE_PKTS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_MACControlFrames,
		NFP_MAC_STATS_RX_MAC_CTRL_FRAMES_RECEIVED
	}
}, nfp_mac_stats_tx[] = {
	{
		IFLA_HSTATS_STAT_RFC2819_etherStatsOctets,
		NFP_MAC_STATS_TX_OUT_OCTETS
	},
	/* missing NFP_MAC_STATS_TX_VLAN_TRANSMITTED_OK, no standard counter */
	{
		IFLA_HSTATS_STAT_RFC2863_Errors,
		NFP_MAC_STATS_TX_OUT_ERRORS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsBroadcastPkts,
		NFP_MAC_STATS_TX_BROADCAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts64Octets,
		NFP_MAC_STATS_TX_PKTS_64_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts256to511Octets,
		NFP_MAC_STATS_TX_PKTS_256_TO_511_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts512to1023Octets,
		NFP_MAC_STATS_TX_PKTS_512_TO_1023_OCTETS
	}, {
		IFLA_HSTATS_STAT_IEEE8023_PAUSEMACCtrlFrames,
		NFP_MAC_STATS_TX_PAUSE_MAC_CTRL_FRAMES
	}, {
		IFLA_HSTATS_STAT_IEEE8023_FramesOK,
		NFP_MAC_STATS_TX_FRAMES_TRANSMITTED_OK
	}, {
		IFLA_HSTATS_STAT_RFC2863_UcastPkts,
		NFP_MAC_STATS_TX_UNICAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsMulticastPkts,
		NFP_MAC_STATS_TX_MULTICAST_PKTS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts65to127Octets,
		NFP_MAC_STATS_TX_PKTS_65_TO_127_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts128to255Octets,
		NFP_MAC_STATS_TX_PKTS_128_TO_255_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819_etherStatsPkts1024to1518Octets,
		NFP_MAC_STATS_TX_PKTS_1024_TO_1518_OCTETS
	}, {
		IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts1519toMaxOctets,
		NFP_MAC_STATS_TX_PKTS_1519_TO_MAX_OCTETS
	}
};

static int
nfp_hstat_mac_get(struct net_device *netdev, struct rtnl_hstat_req *req,
		  const struct rtnl_hstat_group *grp)
{
	const struct nfp_stat_pair *pairs;
	struct nfp_port *port;
	unsigned int dir, i;

	port = nfp_port_from_netdev(netdev);
	if (!__nfp_port_get_eth_port(port) || !port->eth_stats)
		return -EINVAL;

	dir = rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_DIRECTION);
	pairs = dir == IFLA_HSTATS_QUAL_DIR_RX ?
		nfp_mac_stats_rx : nfp_mac_stats_tx;

	for (i = 0; i < grp->stats_cnt; i++)
		rtnl_hstat_dump(req, pairs[i].attr,
				readq(port->eth_stats + pairs[i].offset));
	return 0;
}

static struct rtnl_hstat_group nfp_hstat_mac_rx __ro_after_init = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DEV, RX),
	},

	.get_stats = nfp_hstat_mac_get,
};

static struct rtnl_hstat_group nfp_hstat_mac_tx __ro_after_init = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC(DEV, TX),
	},

	.get_stats = nfp_hstat_mac_get,
};

static const struct rtnl_hstat_group nfp_hstat_mac = {
	.has_children = true,
	.children = {
		&nfp_hstat_mac_rx,
		&nfp_hstat_mac_tx,
		NULL,
	},
};

static int
nfp_hstat_mac_head_drop(struct net_device *netdev, struct rtnl_hstat_req *req,
			const struct rtnl_hstat_group *grp)
{
	struct nfp_port *port;
	unsigned int off, dir;

	port = nfp_port_from_netdev(netdev);
	if (!__nfp_port_get_eth_port(port) || !port->eth_stats)
		return -EINVAL;

	dir = rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_DIRECTION);
	off = dir == IFLA_HSTATS_QUAL_DIR_RX ?
		NFP_MAC_STATS_RX_MAC_HEAD_DROP : NFP_MAC_STATS_TX_QUEUE_DROP;

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_RFC2819_etherStatsDropEvents,
			readq(port->eth_stats + off));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_tm = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC_BIDIR(DEV),
	},

	.get_stats = nfp_hstat_mac_head_drop,
	.stats	= {
		[1] =	RTNL_HSTATS_STAT_RFC2819_etherStatsDropEvents_BIT,
	},
	.stats_cnt = 1,
};

static int
nfp_hstat_mac_pp_pause(struct net_device *netdev, struct rtnl_hstat_req *req,
		       const struct rtnl_hstat_group *grp)
{
	static const u32 remap_rx[] = {
		0xe0, 0xe8, 0xb0, 0xb8, 0xf0, 0xf7, 0x100, 0x108
	};
	static const u32 remap_tx[] = {
		0x1c0, 0x1c8, 0x1e0, 0x1e8, 0x1d0, 0x1d8, 0x1f0, 0x1f8
	};
	struct nfp_port *port;
	const u32 *remap;
	u8 dir, prio;

	port = nfp_port_from_netdev(netdev);
	if (!__nfp_port_get_eth_port(port) || !port->eth_stats)
		return -EINVAL;

	prio = rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_PRIORITY);
	dir = rtnl_hstat_qual_get(req, RTNL_HSTATS_QUAL_DIRECTION);
	remap = dir == IFLA_HSTATS_QUAL_DIR_RX ? remap_rx : remap_tx;

	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_IEEE8023_PAUSEMACCtrlFrames,
			readq(port->eth_stats + remap[prio]));
	return 0;
}

static const struct rtnl_hstat_group nfp_hstat_pp_pause = {
	.qualifiers = {
		RTNL_HSTATS_QUALS_BASIC_BIDIR(DEV),
		[RTNL_HSTATS_QUAL_PRIORITY] = {
			.max	= 8,
		},
	},
	.partial_flags = IFLA_HSTATS_PARTIAL_CLASSIFIER,

	.get_stats = nfp_hstat_mac_pp_pause,
	.stats	= {
		[1] =	RTNL_HSTATS_STAT_IEEE8023_PAUSEMACCtrlFrames_BIT,
	},
	.stats_cnt = 1,
};

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
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_RFC2863_Errors,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_ERRORS + off));
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_RFC2863_Discards,
			nn_readq(nn, NFP_NET_CFG_STATS_RX_DISCARDS + off));
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
		[2] =	RTNL_HSTATS_STAT_RFC2863_Errors_BIT |
			RTNL_HSTATS_STAT_RFC2863_Discards_BIT,
	},
	.stats_cnt = 4,
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
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_RFC2819_etherStatsDropEvents,
			r_vec->rx_drops);
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
		[1] =	RTNL_HSTATS_STAT_RFC2819_etherStatsDropEvents_BIT,

	},
	.stats_cnt = 5,
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
	rtnl_hstat_dump(req, IFLA_HSTATS_STAT_RFC2863_Errors, r_vec->tx_errors);
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
		[1] =	RTNL_HSTATS_STAT_RFC2863_Errors_BIT,
	},
	.stats_cnt = 6,
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
	struct nfp_port *port;

	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_sw);
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd_pq);
	rtnl_hstat_add_grp(req, &nfp_hstat_vnic_nfd);

	port = nfp_port_from_netdev(netdev);
	if (__nfp_port_get_eth_port(port) && port->eth_stats) {
		rtnl_hstat_add_grp(req, &nfp_hstat_tm);
		rtnl_hstat_add_grp(req, &nfp_hstat_pp_pause);
		rtnl_hstat_add_grp(req, &nfp_hstat_mac);
	}

	return 0;
}

void __init nfp_net_hstat_init(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(nfp_mac_stats_rx); i++) {
		unsigned int attr;

		attr = nfp_mac_stats_rx[i].attr;
		nfp_hstat_mac_rx.stats[attr / 64] |= BIT_ULL(attr % 64);
	}
	nfp_hstat_mac_rx.stats_cnt = ARRAY_SIZE(nfp_mac_stats_rx);

	for (i = 0; i < ARRAY_SIZE(nfp_mac_stats_tx); i++) {
		unsigned int attr;

		attr = nfp_mac_stats_tx[i].attr;
		nfp_hstat_mac_tx.stats[attr / 64] |= BIT_ULL(attr % 64);
	}
	nfp_hstat_mac_tx.stats_cnt = ARRAY_SIZE(nfp_mac_stats_tx);
}
