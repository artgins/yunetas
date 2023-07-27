/****************************************************************************
 *          MSG_IEVENT.H
 *
 *          Common of Inter-Events messages
 *
 *          (Message Identifier Area):
 *
 *          __md_iev__
 *
 *
 *  Type of messages:
        "__order__",
        "__request__",
        "__answer__",
        "__publishing__",
        "__subscribing__",
        "__unsubscribing__",
 *
 *          Copyright (c) 2016,2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <gobj.h>
#include <kwid.h>
#include <gbuffer.h>
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Constants
 **************************************************/
#define IEVENT_MESSAGE_AREA_ID "ievent_gate_stack"

/***************************************************
 *              Structures
 **************************************************/
typedef struct {
    char event[80];
    json_t *kw;
} iev_msg_t;

/***************************************************
 *              Prototypes
 **************************************************/
/*-----------------------------------------------------*
 *  Useful to send event's messages TO outside world.
 *-----------------------------------------------------*/
PUBLIC gbuffer_t *iev_create_to_gbuffer( // TODO old iev_create()
    hgobj gobj,
    gobj_event_t event,
    json_t *kw // owned
);

/*-----------------------------------------------------*
 *  Incorporate event's messages FROM outside world.
 *-----------------------------------------------------*/
PUBLIC int iev_create_from_gbuffer(
    hgobj gobj,
    iev_msg_t *iev_msg,
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
);


/*
 *  build_webix()
 *  Must return a json dict with "result", "schema" and "data" keys.
 *  On error "result" will be a negative value, and "data" a string with the error description
 *  On successful "data" must return an any type of json data.
 *  "schema" will describe the format of data in "data".

    {
        "result": Integer,
        "comment": String,
        "schema": Array or Object,
        "data": Array or Object
    }

 *
 */
#define WEBIX_RESULT(gobj, webix)     (kw_get_int((gobj), (webix), "result", 0, KW_REQUIRED))
#define WEBIX_COMMENT(gobj, webix)    (kw_get_str((gobj), (webix), "comment", "", KW_REQUIRED))
#define WEBIX_SCHEMA(gobj, webix)     (kw_get_dict_value((gobj), (webix), "comment", 0, KW_REQUIRED))
#define WEBIX_DATA(gobj, webix)       (kw_get_dict_value((gobj), (webix), "data", 0, KW_REQUIRED))

PUBLIC json_t *build_webix(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data     // owned
);

/*
 *  Return a new kw with all minus this keys:
        "__md_iev__"
        "__temp__"
        "__md_tranger__"
 */
PUBLIC json_t *msg_iev_pure_clone(
    json_t *kw  // NO owned
);


PUBLIC json_t *msg_iev_build_webix(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request, // owned, used to extract ONLY __md_iev__.
    const char *msg_type
);
PUBLIC json_t *msg_iev_build_webix2_without_answer_filter(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data,    // owned
    json_t *kw_request, // owned, used to extract ONLY __md_iev__.
    const char *msg_type
);

/*-----------------------------------------------------*
 *  server: build the answer of kw, filters applied.
 *-----------------------------------------------------*/
PUBLIC json_t *msg_iev_answer( // USING c_agent.c, c_ievent_srv.c, c_ievent_cli.c
    hgobj gobj,
    json_t *kw_request,     // owned, kw request, used to extract ONLY __md_iev__.
    json_t *kw_answer,      // like owned, is returned!, created if null, body of answer message.
    const char *msg_type
);

PUBLIC json_t *msg_iev_answer_without_answer_filter(
    hgobj gobj,
    json_t *kw_request,         // kw request, owned
    json_t *kw_answer,          // like owned, is returned!, created if null, body of answer message.
    const char *msg_type
);
/*-----------------------------------------------------*
 *
 *  LIFO queue
 *  With this property you can do a chain (mono-hilo) of messages.
 *  The request/answer can be propagate through several yunos.
 *
 *  __stack__ is a LIFO queue, ultimo en entrar, primero en salir
 *
 {
    '__md_iev__': {
        '__your_stack__': [
            request-msg_id-last,
            ...
            request-msg_id-first
        ],
        ...
    }
 }

__your_stack__ es una cola LIFO. Se puede usar de varias maneras:

    - Un cliente lanza una petición a un servicio, y el client obtiene una respuesta:

        - client: push request.
        - server: get request, answer it.
        - client: pop request.

    - Un cliente lanza una petición a un servicio, pero no hay respuesta:
        El server tiene que borrar el request de la cola FIFO:

        - client: push request
        - server: pop request

        Este caso lo uso en las clases IEvent_cli, IEvent_srv:
            el que envía añade una "cabecera" al mensaje original,
            y el que recibe quita la "cabecera" porque es solo para el,
            y deja intacto el mensaje, como el mensaje original.
            Se usa para enroutar el evento entre servicios de distintos yunos,
            los inter-eventos. Deja la droga Niyamaka.

            El mensaje se enruta conforme a la ruta establecida en las clases IEvent_cli.
            La cola FIFO implementada en la zona de identicación del mensaje,
            nos ayuda a implementar un protocolo clásico de unión en nodos,
            con las cabeceras insertadas por un nodo y eliminadas por el receptor.
            O sea, un adaptador de protocolos, que pone y quita cabeceras
            o también una pasarela de mensajes entre un sistema y otro.
            IEvent_cli, IEvent_srv se comportarían como pasarela de mensajes,
            o como filtro, o como conector.

 *-----------------------------------------------------*/
PUBLIC int msg_iev_push_stack( // USING c_ievent_cli.c c_ievent_srv.c, c_agent.c
    hgobj gobj,
    json_t *kw,             // not owned
    const char *stack,
    json_t *jn_user_info    // owned
);

PUBLIC json_t *msg_iev_get_stack( // return is not yours! USING c_ievent_cli.c c_ievent_srv.c
    json_t *kw,          // not owned
    const char *stack,
    BOOL print_not_found
);
PUBLIC json_t * msg_iev_pop_stack( // USING c_agent.c
    json_t *kw,
    const char *stack
);  // WARNING free the return.

/*
    var msg_type_list = [
        "__order__",
        "__request__",
        "__answer__",
        "__publishing__",
        "__subscribing__",
        "__unsubscribing__",
        "__first_shot__"
    ];
*/
PUBLIC int msg_set_msg_type(
    json_t *kw,
    const char *msg_type // empty string to delete the key
);
PUBLIC const char *msg_get_msg_type(
    json_t *kw
);

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
