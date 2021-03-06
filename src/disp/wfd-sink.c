/*
 * MiracleCast - Wifi-Display/Miracast Implementation
 *
 * Copyright (c) 2013-2014 David Herrmann <dh.herrmann@gmail.com>
 *
 * MiracleCast is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * MiracleCast is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MiracleCast; If not, see <http://www.gnu.org/licenses/>.
 */
#include <time.h>
#include <systemd/sd-event.h>
#include "ctl.h"
#include "disp.h"
#include "wfd-dbus.h"

static int wfd_sink_set_session(struct wfd_sink *sink,
				struct wfd_session *session)
{
	int r;

	assert_ret(sink);

	if(sink->session == session) {
		return 0;
	}

	if(session) {
		r = ctl_wfd_add_session(ctl_wfd_get(), session);
		if(0 > r) {
			return r;
		}
	}
	
	if(sink->session) {
		ctl_wfd_remove_session_by_id(ctl_wfd_get(),
						wfd_session_get_id(sink->session),
						NULL);
		wfd_session_unref(sink->session);
	}

	sink->session = session ? wfd_session_ref(session) : NULL;
	wfd_fn_sink_properties_changed(sink, "Session");

	return 0;
}

int wfd_sink_new(struct wfd_sink **out,
				struct ctl_peer *peer,
				union wfd_sube *sube)
{
	struct wfd_sink *sink;

	assert_ret(out);
	assert_ret(peer);
	assert_ret(sube);
	assert_ret(wfd_sube_device_is_sink(sube));
	
	sink = calloc(1, sizeof(struct wfd_sink));
	if(!sink) {
		return -ENOMEM;
	}

	sink->label = strdup(peer->label);
	if(!sink->label) {
		wfd_sink_free(sink);
		return -ENOMEM;
	}

	sink->peer = peer;
	sink->dev_info = *sube;

	*out = sink;

	return 0;
}

void wfd_sink_free(struct wfd_sink *sink)
{
	if(!sink) {
		return;
	}

	wfd_sink_set_session(sink, NULL);

	if(sink->label) {
		free(sink->label);
	}

	free(sink);
}

const char * wfd_sink_get_label(struct wfd_sink *sink)
{
	assert_retv(sink, NULL);

	return sink->label;
}

const union wfd_sube * wfd_sink_get_dev_info(struct wfd_sink *sink)
{
	assert_retv(sink, NULL);

	return &sink->dev_info;
}

struct ctl_peer * wfd_sink_get_peer(struct wfd_sink *sink)
{
	assert_retv(sink, NULL);

	return sink->peer;
}

int wfd_sink_create_session(struct wfd_sink *sink, struct wfd_session **out)
{
	int r;
	_wfd_session_unref_ struct wfd_session *sess = NULL;

	assert_ret(sink);
	assert_ret(out);

	if(wfd_sink_is_session_started(sink)) {
		return -EALREADY;
	}

	r = wfd_out_session_new(&sess,
					ctl_wfd_alloc_session_id(ctl_wfd_get()),
					sink);
	if(0 > r) {
		return r;
	}

	r = wfd_sink_set_session(sink, sess);
	if(0 > r) {
		return r;
	}

	*out = wfd_session_ref(sess);

	wfd_fn_sink_properties_changed(sink, "Session");

	return 0;
}

int wfd_fn_out_session_ended(struct wfd_session *s)
{
	assert_ret(wfd_is_out_session(s));

	wfd_sink_set_session(wfd_out_session_get_sink(s), NULL);

	return 0;
}

bool wfd_sink_is_session_started(struct wfd_sink *sink)
{
	assert_retv(sink, false);

	return NULL != sink->session;
}
