/***********************************************************************
 *          login.js
 *
 *          Manage user login/logout
 *          v 1.0 with keycloak
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 *
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        username: null,
        oauth_conf: null,   // loaded from {hostname}.keycloak.json ("auth-server-url","realm","resource")
        access_token: null, // jwt
        refresh_token: null,
        full_oauth_response: null,

        _timeout_refresh: 0,    // refresh timeout in seconds
        _tokenParsed: null,
        _refreshParsed: null
    };




            /***************************
             *     Global functions
             ***************************/




            /***************************
             *      Local Methods
             ***************************/



    /********************************************
     *  {
     *      username: str
     *      password: str
     *      offline_access: bool
     *  }
     ********************************************/
    function do_login(self, kw)
    {
        let username = kw_get_str(kw, "username", "", false, true);
        let password = kw_get_str(kw, "password", "", false, true);
        let offline_access = kw_get_bool(kw, "offline_access", false, false, false);

        if(is_null(self.config.oauth_conf)) {
            log_error("oauth_conf NULL");
            return;
        }

        let oauth_endpoint = self.config.oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
        if(empty_string(oauth_endpoint)) {
            log_error(sprintf("%s: NO auth-server-url", self.gobj_short_name()));
            self.gobj_send_event(
                "EV_LOGIN_DENIED",
                {
                    error: "No auth server url"
                },
                self
            );
            return;
        }

        if(empty_string(username)) {
            log_error(sprintf("%s: No username", self.gobj_short_name()));
            self.gobj_send_event(
                "EV_LOGIN_DENIED",
                {
                    error: "No username"
                },
                self
            );
            return;
        }

        if(empty_string(password)) {
            log_error(sprintf("%s: No password", self.gobj_short_name()));
            self.gobj_send_event(
                "EV_LOGIN_DENIED",
                {
                    error: "No password"
                },
                self
            );
            return;
        }

        /*
         *  Save username
         */
        self.config.username = kw.username;

        let owner = self.config.oauth_conf["realm"];
        let service = self.config.oauth_conf["resource"];
        let url = sprintf("%srealms/%s/protocol/openid-connect/token", oauth_endpoint, owner);

        let xhr = new XMLHttpRequest();
        xhr.open("POST", url);
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");

        let data = "";
        let form_data = {
            "username": username,
            "password": password,
            "grant_type": "password",
            "client_id": service
        };
        if(offline_access) {
            form_data["scope"] = "openid offline_access";
        }

        /*
         *  Convert json to www-form-urlencoded
         */
        for(let k in form_data) {
            let v = form_data[k];
            if(empty_string(data)) {
                data += sprintf("%s=%s", k, v);
            } else {
                data += sprintf("&%s=%s", k, v);
            }
        }

        xhr.onreadystatechange = function () {
            if(xhr.readyState === 4) {
                let status = xhr.status;
                let response = xhr.responseText;
                if(status === 200) {
                    let r = JSON.parse(response);
                    self.gobj_send_event("EV_LOGIN_ACCEPTED", r, self);
                } else {
                    let error = "login denied";
                    if(status === 0) {
                        error += "\nOAuth server not available";
                    }
                    log_info(sprintf("status != 200, status %d, response '%s'", status, response)); // TODO TEST

                    try {
                        if(is_string(response) && !empty_string(response)) {
                            error = JSON.parse(response).error_description;
                        }
                    } catch(e) {
                    }
                    self.gobj_send_event(
                        "EV_LOGIN_DENIED",
                        {
                            error: error
                        },
                        self
                    );
                }
            }
        };
        xhr.send(data);
    }

    /********************************************
     *
     ********************************************/
    function do_logout(self)
    {
        if(is_null(self.config.oauth_conf)) {
            return;
        }

        let oauth_endpoint = self.config.oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
        let owner = self.config.oauth_conf["realm"];
        let service = self.config.oauth_conf["resource"];
        let url = sprintf("%srealms/%s/protocol/openid-connect/logout", oauth_endpoint, owner);

        let xhr = new XMLHttpRequest();
        xhr.open("POST", url);
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.setRequestHeader("Authorization", "Bearer " + self.config.jwt);

        let data = "";
        let form_data = {
            "refresh_token": self.config.refresh_token,
            "client_id": service
        };

        /*
         *  Convert json to www-form-urlencoded
         */
        for(let k in form_data) {
            let v = form_data[k];
            if(empty_string(data)) {
                data += sprintf("%s=%s", k, v);
            } else {
                data += sprintf("&%s=%s", k, v);
            }
        }

        xhr.onreadystatechange = function () {
            if (xhr.readyState === 4) {
                let status = xhr.status;
                let response = xhr.responseText;
                let error = null;
                if(status !== 204) {
                    log_info(sprintf("logout: response != 204, status %d, data %s", status, response));
                    try {
                        if(is_string(response)) {
                            error = JSON.parse(response);
                        }
                    } catch(e) {
                        error = response;
                    }
                }
                // do logout in any case
                self.gobj_send_event(
                    "EV_LOGOUT_DONE",
                    {
                        error: error
                    },
                    self
                );
            }
        };
        xhr.send(data);

        /*
         *  Clear username
         */
        self.config.username = "";
        kw_remove_local_storage_value("session");
    }

    /********************************************
     *
     ********************************************/
    function do_refresh(self)
    {
        let oauth_endpoint = self.config.oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
        let owner = self.config.oauth_conf["realm"];
        let service = self.config.oauth_conf["resource"];
        let url = sprintf("%srealms/%s/protocol/openid-connect/token", oauth_endpoint, owner);

        let xhr = new XMLHttpRequest();
        xhr.open("POST", url);
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.setRequestHeader("Authorization", "Bearer " + self.config.jwt);

        let data = "";
        let form_data = {
            "grant_type": "refresh_token",
            "refresh_token": self.config.refresh_token,
            "client_id": service
        };
        // if(offline_access) { // Funciona?, es necesario?
        //     form_data["scope"] = "openid offline_access"
        // }

        /*
         *  Convert json to www-form-urlencoded
         */
        for(let k in form_data) {
            let v = form_data[k];
            if(empty_string(data)) {
                data += sprintf("%s=%s", k, v);
            } else {
                data += sprintf("&%s=%s", k, v);
            }
        }

        xhr.onreadystatechange = function () {
            if (xhr.readyState === 4) {
                let status = xhr.status;
                let response = xhr.responseText;
                if(status === 200) {
                    let r = JSON.parse(response);
                    self.gobj_send_event("EV_LOGIN_REFRESHED", r, self);
                } else {
                    let error = "login denied";
                    log_error(sprintf("status != 200, status %d, data %s", status, response)); // TODO TEST

                    try {
                        if(is_string(response)) {
                            error = JSON.parse(response).error_description;
                        }
                    } catch(e) {
                    }

                    self.gobj_send_event(
                        "EV_LOGIN_DENIED",
                        {
                            error: error
                        },
                        self
                    );
                }
            }
        };
        xhr.send(data);
    }


            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  {
     *      username:
     *      password:
     *  }
     ********************************************/
    function ac_do_login(self, event, kw, src)
    {
        do_login(self, kw);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_do_logout(self, event, kw, src)
    {
        do_logout(self);
        return 0;
    }

    /********************************************
     *  kw: {
            access_token: ""
            expires_in: 36000                   // 10 hours
            "not-before-policy": 1662894464
            refresh_expires_in: 1800            // 30 minutes
            refresh_token: ""
            scope: "profile email"
            session_state: "d50cebba-c29b-48fc-a87f-0bfb1a09d7f4"
            token_type: "Bearer"
     *  }

    Ejemplo keycloak:  {
            "acr": "1",
            "allowed-origins": [],
            "aud": ["realm-management", "account"],
            "azp": "yunetacontrol",
            "email": "ginsmar@mulesol.es",
            "email_verified": true,
            "exp": 1666336696,
            "family_name": "Martínez",
            "given_name": "Ginés",
            "iat": 1666336576,
            "iss": "https://localhost:8641/auth/realms/mulesol",
            "jti": "96b60323-05c1-4cb1-87e8-8bd68e25a56c",
            "locale": "en",
            "name": "Ginés Martínez",
            "preferred_username": "ginsmar@mulesol.es",
            "realm_access": {},
            "resource_access": {},
            "scope": "profile email",
            "session_state": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sid": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
            "sub": "0a1e5c27-80f1-4225-943a-edfbc204972d",
            "typ": "Bearer"
        }
    Ejemplo de jwt dado por google  {
            "aud": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
            "azp": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
            "email": "ginsmar@gmail.com",
            "email_verified": true,
            "exp": 1666341427,
            "given_name": "Gins",
            "iat": 1666337827,
            "iss": "https://accounts.google.com",
            "jti": "b2a78ed2911514e30e51fb7b0da3c2032ba3a0aa",
            "name": "Gins",
            "nbf": 1666337527,
            "picture": "https://lh3.googleusercontent.com/a/ALm5wu0soemzAFPT0aSqz_-PyPBX_y9RXuSpRcwStQLRBg=s96-c",
            "sub": "109408784262322618770"
        }

     ********************************************/
    function ac_login_accepted(self, event, kw, src)
    {
        self.config.full_oauth_response = kw;
        self.config.access_token = kw["access_token"];
        self.config.refresh_token = kw["refresh_token"];

        /*
         *  Save the session
         */
        kw_set_local_storage_value(
            "session",
            {
                username: self.config.username,
                full_oauth_response: kw
            }
        );

        self.config._tokenParsed = jwt2json(self.config.access_token);
        self.config._refreshParsed = jwt2json(self.config.refresh_token);

// console.dir("kw", kw); // TODO TEST
// console.dir("_tokenParsed", self.config._tokenParsed); // TODO TEST
// console.dir("_refreshParsed", self.config._refreshParsed); // TODO TEST
// log_warning(JSON.stringify(self.config._tokenParsed));
// log_warning(JSON.stringify(self.config._refreshParsed));

        self.config._timeout_refresh = self.config._refreshParsed.exp - get_now() - 5;
        if(self.config._timeout_refresh <= 0) {
            self.config._timeout_refresh = 2; // Validity will be checked in refresh time
        }
        self.set_timeout(self.config._timeout_refresh*1000);

        log_warning("====> timeout refresh (seconds):" + self.config._timeout_refresh); // TODO TEST

        self.gobj_publish_event("EV_LOGIN_ACCEPTED", {
            username: self.config.username,
            jwt: self.config.access_token
        });
        return 0;
    }

    /********************************************
     *  kw: {
            access_token: "",
            expires_in: 65,
            refresh_expires_in: 60,
            refresh_token: "",
            token_type: "Bearer",
            "not-before-policy": 1662909765,
            session_state: "144c039d-6ffb-48f0-97ec-49535d596e5b",
            scope: "profile email"
        }
     *
     ********************************************/
    function ac_login_refreshed(self, event, kw, src)
    {
        self.config.full_oauth_response = kw;
        self.config.access_token = kw["access_token"];
        self.config.refresh_token = kw["refresh_token"];

        /*
         *  Save the session
         */
        kw_set_local_storage_value(
            "session",
            {
                username: self.config.username,
                full_oauth_response: kw
            }
        );

        self.config._tokenParsed = jwt2json(self.config.access_token);
        self.config._refreshParsed = jwt2json(self.config.refresh_token);

// console.dir("kw", kw); // TODO TEST
// console.dir("_tokenParsed", self.config._tokenParsed); // TODO TEST
// console.dir("_refreshParsed", self.config._refreshParsed); // TODO TEST
// log_warning(JSON.stringify(self.config._tokenParsed));
// log_warning(JSON.stringify(self.config._refreshParsed));

        self.config._timeout_refresh = self.config._refreshParsed.exp - get_now() - 5;
        if(self.config._timeout_refresh <= 0) {
            self.config._timeout_refresh = 2; // Validity will be checked in refresh time
        }
        self.set_timeout(self.config._timeout_refresh*1000);

        log_warning("====> timeout refresh (seconds):" + self.config._timeout_refresh); // TODO TEST

        self.gobj_publish_event("EV_LOGIN_REFRESHED", {
            username: self.config.username,
            jwt: self.config.access_token
        });
    }

    /********************************************
     *
     ********************************************/
    function ac_login_denied(self, event, kw, src)
    {
        kw_remove_local_storage_value("session");
        self.gobj_publish_event("EV_LOGIN_DENIED", kw);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_logout_done(self, event, kw, src)
    {
        kw_remove_local_storage_value("session");
        self.gobj_publish_event("EV_LOGOUT_DONE", kw);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_clear_session(self, event, kw, src)
    {
        kw_remove_local_storage_value("session");
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_timeout(self, event, kw, src)
    {
        log_warning("<==== timeout refresh"); // TODO TEST

        if(self.config._timeout_refresh > 0) {
            let now = get_now();
            if(now >= self.config._timeout_refresh) {
                // Refresh time has elapsed
                do_refresh(self);
            }
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_DO_LOGIN: input",
            "EV_DO_LOGOUT: input",
            "EV_LOGIN_ACCEPTED: output",
            "EV_LOGIN_DENIED: output",
            "EV_LOGIN_REFRESHED: output",
            "EV_LOGOUT_DONE: output",
            "EV_TIMEOUT"
        ],
        "state_list": [
            "ST_LOGOUT",
            "ST_WAIT_TOKEN",
            "ST_LOGIN"
        ],
        "machine": {
            "ST_LOGOUT":
            [
                ["EV_DO_LOGIN",             ac_do_login,            "ST_WAIT_TOKEN"],
                ["EV_LOGIN_DENIED",         ac_login_denied,        undefined],
                ["EV_DO_LOGOUT",            ac_clear_session,       undefined]
            ],
            "ST_WAIT_TOKEN":
            [
                ["EV_LOGIN_ACCEPTED",       ac_login_accepted,      "ST_LOGIN"],
                ["EV_LOGIN_DENIED",         ac_login_denied,        "ST_LOGOUT"]
            ],
            "ST_LOGIN":
            [
                ["EV_DO_LOGOUT",            ac_do_logout,           undefined],
                ["EV_LOGIN_REFRESHED",      ac_login_refreshed,     undefined],
                ["EV_LOGIN_DENIED",         ac_login_denied,        "ST_LOGOUT"],
                ["EV_LOGOUT_DONE",          ac_logout_done,         "ST_LOGOUT"],
                ["EV_TIMEOUT",              ac_timeout,             undefined]
            ]
        }
    };

    let Login = GObj.__makeSubclass__();
    let proto = Login.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Login",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Login, "Login");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        /*
         *  Child model
         */
        if(!self.config.subscriber) {
            self.config.subscriber = self.gobj_parent();  // Remove if not child model
        }
        if(self.config.subscriber) {
            self.gobj_subscribe_event(null, {}, self.config.subscriber);
        }

        /*
            Example of file:
            {
              "realm": "",                                          --> owner
              "auth-server-url": "https://localhost:9999/auth/",    --> endpoint
              "ssl-required": "external",
              "resource": "",                                       --> service
              "public-client": true,
              "confidential-port": 0
            }
         */
        // HACK punto gatillo: recupera el keycloak asociado a la url location.hostname
        let keycloak_config_file = sprintf("/static/app/conf/%s.keycloak.json", window.location.hostname);
        load_json_file(
            keycloak_config_file,
            function(json_content) {
                self.config.oauth_conf = json_content;
            },
            function(error_status) {
                log_error(sprintf("%s: can't load file, status error %d, url: '%s'",
                    self.gobj_short_name(), error_status, keycloak_config_file
                ));
            }
        );
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;

        /*
         *  Check if login is already done
         */
        let session = kw_get_local_storage_value("session", null, false);
        if(session) {
            if(!empty_string(session.username)) {
                self.config.username = session.username;
                self.gobj_change_state("ST_WAIT_TOKEN");
                self.gobj_send_event("EV_LOGIN_ACCEPTED", session.full_oauth_response, self);
            } else {
                kw_remove_local_storage_value("session");
            }
        }
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        //do_logout(this);
    };


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Login = Login;

})(this);
