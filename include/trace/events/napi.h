/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM napi

#if !defined(_TRACE_NAPI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NAPI_H

#include <linux/netdevice.h>
#include <linux/tracepoint.h>
#include <linux/ftrace.h>

#define NO_DEV "(no_device)"

TRACE_EVENT(napi_poll,

	TP_PROTO(struct napi_struct *napi, int work, int budget),

	TP_ARGS(napi, work, budget),

	TP_STRUCT__entry(
		__field(	struct napi_struct *,	napi)
		__string(	dev_name, napi->dev ? napi->dev->name : NO_DEV)
		__field(	int,			work)
		__field(	int,			budget)
	),

	TP_fast_assign(
		__entry->napi = napi;
		__assign_str(dev_name, napi->dev ? napi->dev->name : NO_DEV);
		__entry->work = work;
		__entry->budget = budget;
	),

	TP_printk("napi poll on napi struct %p for device %s work %d budget %d",
		  __entry->napi, __get_str(dev_name),
		  __entry->work, __entry->budget)
);

TRACE_EVENT(napi_poller_enter,

	TP_PROTO(int idle),

	TP_ARGS(idle),

	TP_STRUCT__entry(
		__field(	int,			idle)
	),

	TP_fast_assign(
		__entry->idle = idle;
	),

	TP_printk("idle %d", __entry->idle)
);

TRACE_EVENT(napi_poller_select,

	    TP_PROTO(struct napi_struct *napi, u64 now, int from_idle),

	    TP_ARGS(napi, now, from_idle),

	TP_STRUCT__entry(
		__field(	struct napi_struct *,	napi)
		__field(	int,			since_poll)
		__field(	int,			local)
		__field(	int,			from_idle)
	),

	TP_fast_assign(
		__entry->napi = napi;
		__entry->since_poll = now - napi->last_poll;
		__entry->local = napi->last_poll_thread == current;
		__entry->from_idle = from_idle;
	),

	TP_printk("napi struct %p (age %d local %d from_idle %d)",
		  __entry->napi, __entry->since_poll, __entry->local,
		  __entry->from_idle)
);

TRACE_EVENT(napi_poller_avg_lat,

	TP_PROTO(int avg_lat),

	TP_ARGS(avg_lat),

	TP_STRUCT__entry(
		__field(	int,			avg_lat)
	),

	TP_fast_assign(
		__entry->avg_lat = avg_lat;
	),

	TP_printk("avg_lat %d", __entry->avg_lat)
);

TRACE_EVENT(napi_poller_exit,

	    TP_PROTO(int idle, s64 time_to_sleep, char c),

	    TP_ARGS(idle, time_to_sleep, c),

	TP_STRUCT__entry(
		__field(	int,			idle)
		__field(	char,			wait_type)
		__field(	s64,			to)
	),

	TP_fast_assign(
		__entry->idle = idle;
		__entry->to = time_to_sleep;
		__entry->wait_type = c;
	),

	TP_printk("idle %d, next in %lld (wait_type %c)",
		  __entry->idle, __entry->to, __entry->wait_type)
);

#undef NO_DEV

#endif /* _TRACE_NAPI_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
