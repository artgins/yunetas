/****************************************************************************
 *          MSG_IEVENT.H
 *
    Inter-Events interchange gobj's events (event,kw) between two yunos.

    Inter-Events messages has the next basic structure:
        {
            "event": "",
            "kw": {
            }
        }

    Inside of "kw", all fields beginning with "__" are considered metadata and
    all fields beginning with one "_"  are considered private data
    (the will be **removed** before to send iev)

    There is a special field "__temp__" that can be used inside the yuno to transport internal information,
    but it will be delete when the iev go out the yuno

    They add "__md_iev__" metadata to provide data of the services interchanging the event:
    HACK: In a request/response pattern the "__md_iev__" metadata is returned in the response
          The sender can put data in the sent "__md_iev__" in order to match the response

        {
            "event": "",
            "kw": {
                "__md_iev__": { #^^ received md will be back in the response
                }
            }
        }

    They add "__md_yuno__" metadata to provide information of source yuno:

        {
            "event": "",
            "kw": {
                "__md_iev__": {
                }
               "__md_yuno__": { #^^ source yuno information
               }
            }
        }

    In "__md_iev__" metadata there is a special field "ievent_gate_stack"
    that contains the source/destination services information:
    The "ievent_gate_stack" field proceeds a LIFO queue,
    when a yuno wants to response they pop the first register, interchange dst/src and re-put it the queue

               "__md_iev__": {
                    "ievent_gate_stack": [
                        {
                            "dst_yuno": "",
                            "dst_role": "yuneta_agent",
                            "dst_service": "agent",
                            "src_yuno": "",
                            "src_role": "yuneta_cli",
                            "src_service": "cli",
                            "user": "gines",
                            "host": "gines-Nitro-AN517-53"
                        }
                    ]
                }


    In response messages of commands and statistics they provide the next fields:

        {
            "event": "EV_MT_COMMAND_ANSWER",
            "kw": {
                "result": 0,            #^^ negative value is an error
                "comment": "",          #^^ cause of error or some extra information in right cases
                "schema": [],           #^^ schema of "data"
                "data: {} or []         #^^ response data
            }
        }


 *
 *          Copyright (c) 2016,2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include "gobj.h"
#include "kwid.h"
#include "gbuffer.h"

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Constants
 **************************************************/
#define IEVENT_MESSAGE_AREA_ID "ievent_gate_stack"

#define COMMAND_RESULT(gobj, kw)     (kw_get_int((gobj), (kw), "result", -1, 0))
#define COMMAND_COMMENT(gobj, kw)    (kw_get_str((gobj), (kw), "comment", "", 0))
#define COMMAND_SCHEMA(gobj, kw)     (kw_get_dict_value((gobj), (kw), "schema", 0, 0))
#define COMMAND_DATA(gobj, kw)       (kw_get_dict_value((gobj), (kw), "data", 0, 0))

/***************************************************
 *              Structures
 **************************************************/

/***************************************************
 *              Prototypes
 **************************************************/
/*---------------------------------------------------------*
 *  Useful to send event's messages TO the outside world.
 *---------------------------------------------------------*/
PUBLIC gbuffer_t *iev_create_to_gbuffer( // old iev_create()
    hgobj gobj,
    gobj_event_t event,
    json_t *kw // owned
);

/*---------------------------------------------------------*
 *  Incorporate event's messages FROM the outside world.
 *---------------------------------------------------------*/
PUBLIC json_t *iev_create_from_gbuffer(
    hgobj gobj,
    const char **event,
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
);


/*-----------------------------------------------------*
 *  LIFO queue
    With this property you can do a chain (mono-hilo) of messages.
    The request/answer can be go through several yunos.
    '__your_stack__' is a LIFO queue, last input will be first output

        {
            '__md_iev__': {
                '__your_stack__': [         #^^ we use "ievent_gate_stack"
                    {request-msg_id-last},
                    ...
                    {request-msg_id-first}
                ],
                ...
            }
        }

    - Request/Response pattern:

        - client: push request.
        - server: get request, answer it interchanging src/dst.
        - client: pop request.

    - Order pattern
        - client: push request
        - server: pop request

 *-----------------------------------------------------*/
PUBLIC int msg_iev_push_stack( // Push a record in the stack
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    json_t *jn_data         // owned, data to be pushed in the stack
);

PUBLIC json_t *msg_iev_get_stack( // Get a record without popping from the stack. Return is NOT YOURS!
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    BOOL verbose
);

PUBLIC json_t * msg_iev_pop_stack( // Pop a record from stack. Return is YOURS, must be FREE!
    hgobj gobj,
    json_t *kw,
    const char *stack
);

/*---------------------------------------------------------*
 *  Helpers of request/response pattern
 *---------------------------------------------------------*/
PUBLIC json_t *build_command_response( // // old build_webix()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned, if null then not set
    json_t *jn_schema,  // owned, if null then not set
    json_t *jn_data     // owned, if null then not set
);

/*----------------------------------------------------------*
 *  Set iev metadata back in a response reversing src/dst
 *
 *  Implicitly:
 *      - Delete __temp__ key in kw_response
 *      - Set __md_yuno__ key
 *
 *  old msg_iev_answer() adding reverse_dst TRUE
 *  old msg_iev_answer_without_answer_filter() adding reverse_dst FALSE
 *-----------------------------------------------------------*/
PUBLIC json_t *msg_iev_set_back_metadata(
    hgobj gobj,
    json_t *kw_request,     // owned, kw request, used to extract ONLY __md_iev__
    json_t *kw_response,    // like owned, is returned!, created if null, the body of answer message
    BOOL no_reverse_dst
);

static inline json_t *msg_iev_build_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request // owned, used to extract ONLY __md_iev__.
) {
    json_t *jn_command = build_command_response(gobj, result, jn_comment, jn_schema, jn_data);
    json_t *jn_answer = msg_iev_set_back_metadata(gobj, kw_request, jn_command, TRUE);
    return jn_answer;
}

/*
 *  Trace inter-events with metadata of kw
 */
PUBLIC void trace_inter_event(hgobj gobj, const char *prefix, const char *event, json_t *kw);
/*
 *  Trace inter-events with full kw
 */
PUBLIC void trace_inter_event2(hgobj gobj, const char *prefix, const char *event, json_t *kw);


#ifdef __cplusplus
}
#endif
