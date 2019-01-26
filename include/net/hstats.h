// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/* Copyright (C) 2019 Netronome Systems, Inc. */

#ifndef _NET_HSTATS_H
#define _NET_HSTATS_H

#include <linux/if_link.h>
#include <linux/kernel.h>
#include <net/netlink.h>

struct net_device;
struct sk_buff;

/* Internal driver/core qualifiers used as indexes in qualifier tables
 * and translated into IFLA_HSTATS_QUAL_* in dumps.
 */
enum {
	RTNL_HSTATS_QUAL_TYPE,
	RTNL_HSTATS_QUAL_DIRECTION,
	RTNL_HSTATS_QUAL_QUEUE,
	RTNL_HSTATS_QUAL_PRIORITY,
	RTNL_HSTATS_QUAL_TC,

	RTNL_HSTATS_QUAL_CNT
};

struct hstat_dumper;
struct rtnl_hstat_group;

struct rtnl_hstat_req {
	int err;
	struct sk_buff *skb;
	struct hstat_dumper *dumper;
};

struct rtnl_hstat_qualifier {
	unsigned int constant;
	unsigned int min;
	unsigned int max;
	int (*get_max)(const struct net_device *dev,
		       const struct rtnl_hstat_group *grp);
};

/**
 * struct rtnl_hstat_group - node in the hstat hierarchy
 * @qualifiers:	attributes describing this group
 * @has_children: @children array is present and NULL-terminated
 * @stats_cnt:	number of stats in the bitmask
 * @stats:	bitmask of stats present
 * @get_stats:	driver callback for dumping the stats
 * @children:	NULL-terminated array of groups inheriting the qualifiers
 *		@has_children has to be set for core to parse the array
 */
struct rtnl_hstat_group {
	/* Note: this is *not* indexed with IFLA_* attributes! */
	struct rtnl_hstat_qualifier qualifiers[RTNL_HSTATS_QUAL_CNT];
	bool has_children;
	/* Can't use bitmaps - words are variable length */
	unsigned int stats_cnt;
	u64 stats[DIV_ROUND_UP(IFLA_HSTATS_STAT_MAX + 1, 64)];
	int (*get_stats)(struct net_device *dev, struct rtnl_hstat_req *req,
			 const struct rtnl_hstat_group *grp);

	const struct rtnl_hstat_group *children[];
};

void rtnl_hstat_add_grp(struct rtnl_hstat_req *req,
			const struct rtnl_hstat_group *grp);
bool rtnl_hstat_qual_is_set(struct rtnl_hstat_req *req, int qual);
int rtnl_hstat_qual_get(struct rtnl_hstat_req *req, int qual);

static inline void
rtnl_hstat_dump(struct rtnl_hstat_req *req, const int id, const u64 val)
{
	if (req->err)
		return;
	if (nla_put_u64_64bit(req->skb, id, val, IFLA_HSTATS_STAT_UNSPEC))
		req->err = -EMSGSIZE;
}

size_t rtnl_get_link_hstats_size(const struct net_device *dev);
size_t rtnl_get_link_hstats(struct sk_buff *skb, struct net_device *dev,
			    int *prividx);

enum {
#define RTNL_HSTAT_BIT(_name, _word) \
	RTNL_HSTATS_STAT_ ## _name ## _BIT = \
		BIT_ULL(IFLA_HSTATS_STAT_ ## _name - 1 - ((_word) * 64))

	/* Common Linux stats */
	RTNL_HSTAT_BIT(LINUX_PKTS, 0),
	RTNL_HSTAT_BIT(LINUX_BYTES, 0),
	RTNL_HSTAT_BIT(LINUX_BUSY, 0),
	RTNL_HSTAT_BIT(LINUX_CSUM_PARTIAL, 0),
	RTNL_HSTAT_BIT(LINUX_CSUM_COMPLETE, 0),
	RTNL_HSTAT_BIT(LINUX_CSUM_UNNECESSARY, 0),
	RTNL_HSTAT_BIT(LINUX_SEGMENTATION_OFFLOAD_PKTS, 0),
#undef RTNL_HSTAT_BIT
};

/* Helper defines for common qualifier sets */
#define RTNL_HSTATS_QUALS_BASIC(type, dir)				\
	[RTNL_HSTATS_QUAL_TYPE] = {					\
		.constant	= IFLA_HSTATS_QUAL_TYPE_ ##type,	\
	},								\
	[RTNL_HSTATS_QUAL_DIRECTION] = {				\
		.constant	= IFLA_HSTATS_QUAL_DIR_ ##dir,		\
	}
#endif
