/***********************************************************************
 *          deploy.js
 *
 *      Per-deployment identity DERIVED FROM THE SERVING HOSTNAME, so a
 *      SINGLE build serves every tenant/plane with no baked-in config:
 *
 *          https://<tenant>.yunetacontrol.com  → agents  plane, CC :1996
 *          https://<tenant>.yunetacontrol.ovh  → agent22 plane, CC :1997
 *
 *      Everything (tenant, control-center URL, BFF URL) is co-located on
 *      the SPA's own host:
 *          - cc_url  : wss://<host>:<cc_port>   (controlcenter __top_side__)
 *          - bff_url : https://<host>:1806      (auth BFF)
 *          - tenant  : first label of the host  ("artgins")
 *
 *      The cookie the BFF sets (Domain=<host>) therefore flows to both the
 *      BFF and the control center (same host), and the browser trusts the
 *      single letsencrypt cert for <host>.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

const CC_PORT_AGENTS   = 1996;  /* controlcenter for primary yuneta_agent */
const CC_PORT_AGENT22  = 1997;  /* controlcenter for yuneta_agent22       */
const BFF_PORT_AGENTS  = 1806;  /* auth BFF for the agents plane  (.com)  */
const BFF_PORT_AGENT22 = 1807;  /* auth BFF for the agent22 plane (.ovh)  */


/***************************************************************
 *  Resolve the deployment info from window.location.hostname.
 *  { host, tenant, plane, cc_url, bff_url }
 ***************************************************************/
function deploy_info()
{
    let host = (typeof window !== "undefined" && window.location && window.location.hostname)
        ? window.location.hostname : "localhost";

    let tenant = (host.split(".")[0]) || "local";
    let is_agent22 = host.endsWith(".ovh");
    let plane = is_agent22 ? "agent22" : "agents";
    let cc_port  = is_agent22 ? CC_PORT_AGENT22  : CC_PORT_AGENTS;
    let bff_port = is_agent22 ? BFF_PORT_AGENT22 : BFF_PORT_AGENTS;

    return {
        host:    host,
        tenant:  tenant,
        plane:   plane,
        cc_url:  `wss://${host}:${cc_port}`,
        bff_url: `https://${host}:${bff_port}`
    };
}

export {deploy_info};
