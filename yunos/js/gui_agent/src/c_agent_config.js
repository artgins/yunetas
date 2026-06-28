/***********************************************************************
 *          c_agent_config.js
 *
 *      C_AGENT_CONFIG — app-level config service (named service "agent_config").
 *
 *      Single source of truth for the user-entered agent endpoints. Lives
 *      for the whole app lifetime (a named service of the yuno), so it
 *      outlives the Settings/Console views that are created and destroyed
 *      as the user navigates.
 *
 *      State is held in two SDF_PERSIST attrs, auto-loaded from the browser
 *      localStorage on create and saved with gobj_save_persistent_attrs():
 *          - agents        : array of {label, url, yuno_role, yuno_name, service}
 *          - active_agent  : label of the agent the Console connects to
 *
 *      NOTHING private is committed: this file only defines the shape; the
 *      values are entered by the user in C_SETTINGS.
 *
 *      When Phase 2 adds login, this service is the natural seed of a
 *      wattyzer-style C_WZ_APP root service (it would also own the shell).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, sdata_flag_t, event_flag_t,
    gclass_create, log_error,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_save_persistent_attrs,
    gobj_subscribe_event,
    gobj_publish_event,
} from "@yuneta/gobj-js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_CONFIG";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,                      null, "Subscriber of output events"),

SDATA(data_type_t.DTP_JSON,     "agents",       sdata_flag_t.SDF_PERSIST, "[]", "Configured agent endpoints"),
SDATA(data_type_t.DTP_STRING,   "active_agent", sdata_flag_t.SDF_PERSIST, "",   "Label of the active agent"),

SDATA(data_type_t.DTP_STRING,   "auth_url",       sdata_flag_t.SDF_PERSIST, "", "OIDC/Keycloak base URL (e.g. https://auth.artgins.com)"),
SDATA(data_type_t.DTP_STRING,   "auth_realm",     sdata_flag_t.SDF_PERSIST, "", "Keycloak realm"),
SDATA(data_type_t.DTP_STRING,   "auth_client_id", sdata_flag_t.SDF_PERSIST, "", "Keycloak public client id (Direct Access Grants enabled)"),
SDATA_END()
];

let PRIVATE_DATA = {};
let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Public functions
                     ***************************/




/***************************************************************
 *  Read the current config: {agents, active_agent}.
 ***************************************************************/
function agent_config_get(gobj)
{
    let agents = gobj_read_attr(gobj, "agents");
    if(!Array.isArray(agents)) {
        agents = [];
    }
    return {
        agents:       agents,
        active_agent: gobj_read_attr(gobj, "active_agent") || ""
    };
}

/***************************************************************
 *  Replace the config, persist it, and notify subscribers.
 *  Single write path so every mutation persists + publishes.
 ***************************************************************/
function agent_config_set(gobj, agents, active_agent)
{
    if(!Array.isArray(agents)) {
        agents = [];
    }
    gobj_write_attr(gobj, "agents", agents);
    gobj_write_attr(gobj, "active_agent", active_agent || "");
    gobj_save_persistent_attrs(gobj);

    gobj_publish_event(gobj, "EV_AGENTS_CHANGED", {
        agents:       agents,
        active_agent: active_agent || ""
    });
}

/***************************************************************
 *  Read the OIDC/Keycloak auth config: {auth_url, realm, client_id}.
 ***************************************************************/
function agent_config_get_auth(gobj)
{
    return {
        auth_url:  gobj_read_attr(gobj, "auth_url") || "",
        realm:     gobj_read_attr(gobj, "auth_realm") || "",
        client_id: gobj_read_attr(gobj, "auth_client_id") || ""
    };
}

/***************************************************************
 *  Replace the auth config and persist it.
 ***************************************************************/
function agent_config_set_auth(gobj, auth)
{
    auth = auth || {};
    gobj_write_attr(gobj, "auth_url", auth.auth_url || "");
    gobj_write_attr(gobj, "auth_realm", auth.realm || "");
    gobj_write_attr(gobj, "auth_client_id", auth.client_id || "");
    gobj_save_persistent_attrs(gobj);
}

/***************************************************************
 *  Return the active agent object, or null if none/invalid.
 ***************************************************************/
function agent_config_get_active(gobj)
{
    let {agents, active_agent} = agent_config_get(gobj);
    if(!active_agent) {
        return null;
    }
    for(let a of agents) {
        if(a && a.label === active_agent) {
            return a;
        }
    }
    return null;
}




                    /***************************
                     *              FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", []]
    ];

    /*---------------------------------------------*
     *          Events
     *  Subscribers (the Console) are optional in Phase 1, so the
     *  change event is tagged EVF_NO_WARN_SUBS.
     *---------------------------------------------*/
    const event_types = [
        ["EV_AGENTS_CHANGED", event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table
        0,  // command_table
        0,  // s_user_trace_level
        0   // gclass_flag
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_agent_config()
{
    return create_gclass(GCLASS_NAME);
}

export {
    register_c_agent_config,
    agent_config_get,
    agent_config_set,
    agent_config_get_active,
    agent_config_get_auth,
    agent_config_set_auth,
};
