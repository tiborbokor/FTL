/* Pi-hole: A black hole for Internet advertisements
*  (c) 2019 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  API Implementation /api/dns
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

#include "../FTL.h"
#include "dns.h"
#include "../webserver/http-common.h"
#include "../webserver/json_macros.h"
#include "routes.h"
// {s,g}et_blockingstatus()
#include "../setupVars.h"
// set_blockingmode_timer()
#include "../timers.h"

static int get_blocking(struct ftl_conn *api)
{
	// Return current status
	cJSON *json = JSON_NEW_OBJ();
	const bool blocking = get_blockingstatus();
	JSON_OBJ_ADD_BOOL(json, "blocking", blocking);

	// Get timer information (if applicable)
	int delay;
	bool target_status;
	get_blockingmode_timer(&delay, &target_status);
	if(delay > -1)
	{
		cJSON *timer = JSON_NEW_OBJ();
		JSON_OBJ_ADD_NUMBER(timer, "delay", delay);
		JSON_OBJ_ADD_BOOL(timer, "blocking_target", target_status);
		JSON_OBJ_ADD_ITEM(json, "timer", timer);
	}
	else
	{
		JSON_OBJ_ADD_NULL(json, "timer");
	}

	// Send object (HTTP 200 OK)
	JSON_SEND_OBJECT(json);
}

static int set_blocking(struct ftl_conn *api)
{
	// Verify requesting client is allowed to access this ressource
	if(check_client_auth(api) == API_AUTH_UNAUTHORIZED)
	{
		return send_json_unauthorized(api);
	}

	if (api->payload.json == NULL) {
		return send_json_error(api, 400,
		                       "bad_request",
		                       "Invalid request body data",
		                       NULL);
	}

	cJSON *elem = cJSON_GetObjectItemCaseSensitive(api->payload.json, "blocking");
	if (!cJSON_IsBool(elem)) {
		return send_json_error(api, 400,
		                       "bad_request",
		                       "No \"blocking\" boolean in body data",
		                       NULL);
	}
	const bool target_status = cJSON_IsTrue(elem);

	// Get (optional) delay
	int delay = -1;
	elem = cJSON_GetObjectItemCaseSensitive(api->payload.json, "delay");
	if (cJSON_IsNumber(elem) && elem->valuedouble > 0.0)
		delay = elem->valueint;

	if(target_status == get_blockingstatus())
	{
		// The blocking status does not need to be changed

		// Delete a possibly running timer
		set_blockingmode_timer(-1, true);
	}
	else
	{
		// Activate requested status
		set_blockingstatus(target_status);

		// Start timer (-1 disables all running timers)
		set_blockingmode_timer(delay, !target_status);
	}

	// Return GET property as result of POST/PUT/PATCH action
	// if no error happened above
	return get_blocking(api);
}

int api_dns_blockingstatus(struct ftl_conn *api)
{
	if(api->method == HTTP_GET)
	{
		return get_blocking(api);
	}
	else if(api->method == HTTP_PUT)
	{
		return set_blocking(api);
	}
	else
	{
		// This results in error 404
		return 0;
	}
}

int api_dns_cacheinfo(struct ftl_conn *api)
{
	// Verify requesting client is allowed to access this ressource
	if(check_client_auth(api) == API_AUTH_UNAUTHORIZED)
	{
		return send_json_unauthorized(api);
	}

	cacheinforecord cacheinfo;
	getCacheInformation(&cacheinfo);
	cJSON *json = JSON_NEW_OBJ();
	JSON_OBJ_ADD_NUMBER(json, "cache_size", cacheinfo.cache_size);
	JSON_OBJ_ADD_NUMBER(json, "cache_inserted", cacheinfo.cache_inserted);
	JSON_OBJ_ADD_NUMBER(json, "cache_evicted", cacheinfo.cache_live_freed);
	JSON_SEND_OBJECT(json);
}
