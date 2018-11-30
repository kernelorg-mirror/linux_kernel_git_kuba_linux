.. SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)

=================================
Netlink device hierarchical stats
=================================

Overview
========

*hstats* are standardized driver statistics.  Both device and host driver
statistics can be exposed.

*hstats* are a partial replacement for `ethtool -S`, standard-based statistics
and statistics common across many drivers should be added to *hstats*.
Once statistic is exposed via *hstats* it should not be added to `ethtool -S`
(drivers which already expose it can keep doing so, but no new driver allowed).

Apart from common statistics identifiers *hstats* provide:
 - clear indication if statistic is maintained (unlike more compact
   fixed-structure stats, e.g. `struct rtnl_link_stats64`);
 - hierarchical structure (see :ref:`groups` and :ref:`hierarchies`);
 - exposes :ref:`qualifiers` - these are attributes of the statistics which
   help determine the source of the statistics;
 - provide flexible grouping mechanism which should reduce code duplication
   amongst drivers.

Netlink message structure
=========================

Basic structure of the `IFLA_STATS_LINK_HSTATS` attribute looks like this:

::

 [IFLA_STATS_LINK_HSTATS]
     [IFLA_HSTATS_GROUP]
         [IFLA_HSTATS_QUAL_xxx]
         [IFLA_HSTATS_QUAL_yyy]
         [IFLA_HSTATS_STATS]
             [IFLA_HSTATS_STAT_a]
             [IFLA_HSTATS_STAT_b]
         [IFLA_HSTATS_GROUP]
             [IFLA_HSTATS_QUAL_zzz]
             [IFLA_HSTATS_STATS]
                 [IFLA_HSTATS_STAT_c]
                 [IFLA_HSTATS_STAT_d]
         [IFLA_HSTATS_GROUP]
             [IFLA_HSTATS_QUAL_zzz]
             [IFLA_HSTATS_STATS]
                 [IFLA_HSTATS_STAT_c]
                 [IFLA_HSTATS_STAT_d]

.. _groups:

Groups
------

`IFLA_STATS_LINK_HSTATS` may contain zero or more `IFLA_HSTATS_GROUP`.

The statistics within a root level `IFLA_HSTATS_GROUP` must not count
events multiple times.

The statistics within a root level `IFLA_HSTATS_GROUP` must include all
of the events, excluding groups where `IFLA_HSTATS_PARTIAL` attribute is
present and set to non-zero value or children of such groups
(see :ref:`partials`).  This removes the need to sum up statistics in the
kernel, for example statistics maintained per queue by the driver can
be reported only per queue and user space can do the adding.

The purpose of multiple root level groups is to allow users tracking the flow
of packets through pipelines.  Let's consider the following RX NIC pipeline:

.. _example_flow:

.. kernel-figure::  hstats_flow_example.dot
   :alt:    statistics flow

   Example device statistics flow diagram

Each rectangle represents a root level `IFLA_STATS_LINK_HSTATS`.  Users should
be able to add all `IFLA_HSTATS_STAT_IEEE8023_FramesOK` within each of those
groups, and compare them to see at which stage of the processing pipeline
packets disappear.

.. _qualifiers:

Qualifiers
----------

Each `IFLA_HSTATS_GROUP` may have a set of qualifiers.  Qualifiers are
attributes which apply to the statistics like being a device statistic,
being an RX statistic, or counting events/packets of a specific queue.
See :ref:`qualifiers_list`.

.. _hierarchies:

Hierarchies
-----------

Each `IFLA_HSTATS_GROUP` can have further `IFLA_HSTATS_GROUP` s nested inside.
Qualifiers of a parent group apply to the child group.  The hierarchies have no
perscribed meaning, or set structure.  They are mostly present for the
convenience of the driver authors.

.. _partials:

Partial groups
--------------

`IFLA_HSTATS_PARTIAL` attribute declares groups contents as incomplete.
As explained in :ref:`groups` if a statistic is present it's sum within
a root group must add up to total number of events seen at a given stage
of processing.  This mostly relates to iterative qualifiers like
:ref:`qual_queue` or :ref:`qual_prio`.

Counting events multiple times towards different statistics, including
similar statistics from different families (e.g. IEEE stats vs IETF stats)
is perfectly normal and acceptable.

`IFLA_HSTATS_PARTIAL` is inherited to children the same as :ref:`qualifiers`.
It can carry one of the following flags:

IFLA_HSTATS_PARTIAL_CLASSIFIER
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Statistics from this group contain more detailed breakdown which may not
be complete.

For example, HW may maintain the number of packets received per priority,
but packets received without a priority tag may not be counted separately.
Since non-tagged packets are not counted the sum of statistics will not
be complete.

IFLA_HSTATS_PARTIAL_SLOWPATH
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Statistics from this group count slow path events.  This attribute is used
if hardware is capable of offload packet forwarding.  Some packets may still
reach the CPU for example if they were purposefully trapped for monitoring,
or no forwarding rule matched.

User space guidelines
=====================

User space `hstat` tool can be used to query statistics.  This tools is part
of the `iproute2` package.

Device driver guidelines
========================

Defining groups
---------------

The definition of a group of statistics has four sections:

.. code-block:: c

  static const struct rtnl_hstat_group my_group = {
	/* Qualifiers */
	.qualifiers = {
		[RTNL_HSTATS_QUAL_TYPE] = {
			.constant	= IFLA_HSTATS_QUAL_TYPE_DEV,
		},
		[RTNL_HSTATS_QUAL_DIRECTION] = {
			.constant	= IFLA_HSTATS_QUAL_DIR_RX,
		},
	},
	/* Partials */
	.partial_flags = IFLA_HSTATS_PARTIAL_CLASSIFIER,

	/* Statistics */
	.get_stats = nfp_hstat_vnic_nfd_basic_get,
	.stats	= {
		/* Word 0 stats */
		[0] =	RTNL_HSTATS_STAT_LINUX_CSUM_COMPLETE_BIT,
		/* Word 1 stats */
		[1] =	RTNL_HSTATS_STAT_RFC2819_etherStatsPkts_BIT |
			RTNL_HSTATS_STAT_RFC2819_etherStatsOctets_BIT,
	},
	.stats_cnt = 3,

	/* Child groups */
	.has_children	= true,
	.children	= {
		&my_child_group,
		NULL
	},
  };

Each section can be omitted, i.e. group may not specify any qualifiers,
statistics or have no children.  :ref:`qual_type` and :ref:`qual_direction`
qualifiers must be specified before any statistics is declared, that is to
say current group or one of parent groups must set them before any statistics
are reported.

If any qualifier is iterative (`max` or `get_max` set instead of `constant`)
the group will be dumped `max` times, the driver can find out about parameters
of current invocation with `rtnl_hstat_qual_get` helper.

Partial flags are optional, and mark group as incomplete (see :ref:`partials`).

Statistics are dumped by invoking `get_stats` callback.  Driver should report
the statistics with `rtnl_hstat_dump` helper.  The list of reported statistics
contains all the statistics driver will dump.  It is used for sizing netlink
replies and filtering.  Drivers must not dump more statistics or different
statistics than defined.  The definition of the bitmap requires a little bit
of care, as there is currently no automatic way to assign statistics to the
correct 64 bit word.

.. note::
   Qualifier constants and parameters to `rtnl_hstat_dump` take constants
   defined in include/uapi/linux/if_link.h, other constants come from
   include/net/hstats.h and have the `RTNL_` prefix.

If any child groups are attached the `has_children` member must be set to `true`
and `children` table must be NULL-terminated.  Child groups may be used in
different parent groups, but care has to be taken to never create loops.

Root groups should generally not have the direction set as a constant.
Most root groups should contain both RX and TX subgroups, rather than having
RX-only root groups and TX-only root groups.

Reporting groups
----------------

Drivers have to define one or more `rtnl_hstat_group`.

They should then implement `ndo_hstat_get_groups` callback.  Inside the callback
they can report every statistic group via `rtnl_hstat_add_grp` helper.

Counter resets
--------------

Counters should not reset their values while the driver is bound to the device,
e.g. when link is brought down.

Counters may reset on driver load/probe or when device is explicitly reset
via devlink.

For iterative qualifiers like queues or priorities drivers should maintain
statistics for all queues ever allocated (or simply always report for max).

.. _qualifiers_list:

List of qualifiers
==================

.. _qual_type:

Type
----

Indicates the source of the statistic.  Statistics can either be maintained
by the device or the driver.  Statistics may be calculated based on multiple
counters (e.g. two counters may need to be added), but such calculation must
not include statistics of different types.

IFLA_HSTATS_QUAL_TYPE_DEV
~~~~~~~~~~~~~~~~~~~~~~~~~
Device statistic, counter is maintained by the device and only reported by
the driver.  There is currently no distinction between hardware and
microcode/firmware statistics.

IFLA_HSTATS_QUAL_TYPE_DRV
~~~~~~~~~~~~~~~~~~~~~~~~~
Driver statistic, fully maintained by software.

.. _qual_direction:

Direction
---------

Direction allows drivers to indicate whether statistics is counting incoming
or outgoing frames.  This is useful as it limmits the number of statistic IDs
which have to be defined, but also often allows drivers to simplify the
reporting if HW maintains the statitics for both directions in the same format,
just in a different location.

Direction and PCIe ports
~~~~~~~~~~~~~~~~~~~~~~~~
For PCIe port netdevs (representors) all statistics are expressed from
device's prespective.  If frame is submitted for transmission on a PCIe
interface it will be seen as "received" by the port (representor).

This means it is possible to see transmission side-only statistics in receive
direction and vice versa (e.g. `IFLA_HSTATS_STAT_LINUX_CSUM_PARTIAL`).

Device statistics reported on port (representor) interfaces, count the total
number of events on given device port.  This includes both frames forwarded
via the "fast path" and explicitly directed to the device port from the
port (representor) interface.

Driver statistics necessarily count only the "fallback path" frames, and
should always be reported with `IFLA_HSTATS_PARTIAL` set to
`IFLA_HSTATS_PARTIAL_SLOWPATH`.

IFLA_HSTATS_QUAL_DIR_RX
~~~~~~~~~~~~~~~~~~~~~~~
received

IFLA_HSTATS_QUAL_DIR_TX
~~~~~~~~~~~~~~~~~~~~~~~
transmitted

.. _qual_queue:

Queue
-----

Queue (or channel) is a Receive Side Scaling construct allowing multiple
consumers and producers to communicate with the device.  It is usually
applicable to the PCIe interface "side" of the device.

Number of reported queues is in no way indicative of how many queues are
currently enabled on the system.

.. _qual_prio:

Priority
--------

Priority is defined as Layer 2 IEEE 802.1Q frame priority.

Traffic class
-------------

Traffic class is any non-Layer 2 queuing and priritization.

List of statistics
==================

All statistics are 64 bits.

Common statistics
-----------------

Statistics from this section are not based on any standard but rather ones
which commonly appear in Linux drivers.

The fact that statistic ID is assigned for some event does not constitute
a recommendation that given statistic is implemented by drivers.  Maintaining
statistics has a performance cost associated and should be considered carefully.

.. _stat_linux_pkts:

IFLA_HSTATS_STAT_LINUX_PKTS
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of packets which were successfully handed over to the next layer.
This means packets passed up during reception and passed down during
transmission.  This differs from both IETF and IEEE definition of basic
packet counters, which generally count at the same layer boundary in both
directions.

No error or discard counters are included in this counter.

IFLA_HSTATS_STAT_LINUX_BYTES
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Same as :ref:`stat_linux_pkts` but counts bytes instead of packets.

IFLA_HSTATS_STAT_LINUX_BUSY
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of times the driver returned `NETDEV_TX_BUSY` to the stack from
in TX handler.

This statistic is defined in to-device direction only.

IFLA_HSTATS_STAT_LINUX_CSUM_PARTIAL
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of successfully handled descriptors with CHECKSUM_PARTIAL offload to
the hardware.

This statistic is defined in to-device direction only.

IFLA_HSTATS_STAT_LINUX_CSUM_COMPLETE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of successfully handled descriptors indicating CHECKSUM_COMPLETE.

This statistic is defined in from-device direction only.

IFLA_HSTATS_STAT_LINUX_CSUM_UNNECESSARY
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of successfully handled descriptors indicating CHECKSUM_UNNECESSARY.

This statistic is defined in from-device direction only.

IFLA_HSTATS_STAT_LINUX_SEGMENTATION_OFFLOAD_PKTS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Number of successfully handled segmentation- and reassembly-offload descriptors,
regardless of transport protocol (TSO, USO, etc.)

For transmission it means the number of frames requiring segmentation submitted
successfully to the device's transmission function (driver) or number of
correctly parsed descriptors for such packets (device).

For reception it can be used to count Generic Receive Offload frames, but *not*
Large Receive Offload frames.

This statistic is incremented once per each frame pre-segmentation as seen
by the Linux stack, not once per each frame on the wire.

IETF RFC2819 statistics
-----------------------
IETF RFC2819 ("Remote Network Monitoring MIB")-compatible statistics.

IFLA_HSTATS_STAT_RFC2819_etherStatsDropEvents
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsDropEvents counter

IFLA_HSTATS_STAT_RFC2819_etherStatsOctets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsOctets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts counter

IFLA_HSTATS_STAT_RFC2819_etherStatsBroadcastPkts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsBroadcastPkts counter

IFLA_HSTATS_STAT_RFC2819_etherStatsMulticastPkts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsMulticastPkts counter

IFLA_HSTATS_STAT_RFC2819_etherStatsCRCAlignErrors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsCRCAlignErrors counter

IFLA_HSTATS_STAT_RFC2819_etherStatsUndersizePkts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsUndersizePkts counter

IFLA_HSTATS_STAT_RFC2819_etherStatsOversizePkts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsOversizePkts counter

IFLA_HSTATS_STAT_RFC2819_etherStatsFragments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsFragments counter

IFLA_HSTATS_STAT_RFC2819_etherStatsJabbers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsJabbers counter

IFLA_HSTATS_STAT_RFC2819_etherStatsCollisions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsCollisions counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts64Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts64Octets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts65to127Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts65to127Octets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts128to255Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts128to255Octets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts256to511Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts256to511Octets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts512to1023Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts512to1023Octets counter

IFLA_HSTATS_STAT_RFC2819_etherStatsPkts1024to1518Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
etherStatsPkts1024to1518Octets counter

Extended size statistics
------------------------
IETF RFC2819 defines simple packet-size based statistics for packets between
64 and 1518 octets.  However, larger frames are commonly supported.  This
sroup defines additional packet-size based statistics.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts1024toMaxOctets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 1024,
and no higher bound.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts1519toMaxOctets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 1519,
and no higher bound.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts1024to2047Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 1024,
and higher bound of 2047.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts2048to4095Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 2048,
and higher bound of 4095.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts4096to8191Octets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 4096,
and higher bound of 8191.

IFLA_HSTATS_STAT_RFC2819EXT_etherStatsPkts8192toMaxOctets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Definition follows etherStatsPkts65to127Octets with lower range bound of 8192,
and no higher bound.

IETF RFC2863 statistics
-----------------------
IETF RFC2863 ("The Interfaces Group MIB")-compatible statistics.

IFLA_HSTATS_STAT_RFC2863_errors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`ifInErrors` or `ifOutErrors` depending on :ref:`qual_direction`.

IFLA_HSTATS_STAT_RFC2863_discards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`ifInDiscards` or `ifOutDiscards` depending on :ref:`qual_direction`.

IEEE 802.3 statistics
---------------------
IEEE 802.3 standard statistics.  Statistics which indicate two corresponding
IEEE 802.3 attributes can be used in both directions, those which only
mention a single attribute are undefined in the other direction.

IFLA_HSTATS_STAT_IEEE8023_FramesOK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aFramesReceivedOK` or `aFramesTransmittedOK` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_OctetsOK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aOctetsReceivedOK` or `aOctetsTransmittedOK` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_MulticastFramesOK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aMulticastFramesReceivedOK` or `aMulticastFramesXmittedOK` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_BroadcastFramesOK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aBroadcastFramesReceivedOK` or `aBroadcastFramesXmittedOK` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_FrameCheckSequenceErrors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aFrameCheckSequenceErrors`

IFLA_HSTATS_STAT_IEEE8023_AlignmentErrors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aAlignmentErrors`

IFLA_HSTATS_STAT_IEEE8023_InRangeLengthErrors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aInRangeLengthErrors`

IFLA_HSTATS_STAT_IEEE8023_OutOfRangeLengthField
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aOutOfRangeLengthField`

IFLA_HSTATS_STAT_IEEE8023_FrameTooLongErrors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aFrameTooLongErrors`

IFLA_HSTATS_STAT_IEEE8023_SymbolErrorDuringCarrier
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aSymbolErrorDuringCarrier`

IFLA_HSTATS_STAT_IEEE8023_MACControlFrames
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aMACControlFramesReceived` or `aMACControlFramesTransmitted` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_UnsupportedOpcodes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aUnsupportedOpcodesReceived`

IFLA_HSTATS_STAT_IEEE8023_PAUSEMACCtrlFrames
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aPAUSEMACCtrlFramesReceived` or `aPAUSEMACCtrlFramesTransmitted` depending
on :ref:`qual_direction`.

IFLA_HSTATS_STAT_IEEE8023_FECCorrectedBlocks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aFECCorrectedBlocks`

IFLA_HSTATS_STAT_IEEE8023_FECUncorrectableBlocks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
`aFECUncorrectableBlocks`
