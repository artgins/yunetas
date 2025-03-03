/****************************************************************************
 *          command_parser.js
 *
 *          Command parser
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/

/***************************************************************
 *
 ***************************************************************/
function command_parser(
    gobj,
    command,
    kw,
    src
) {

}

/***************************************************************
 *
 ***************************************************************/
function build_command_response(
    gobj,
    result,
    comment,
    schema,
    data
) {
    return {
        result: result,
        comment: comment || {},
        schema: schema || {},
        data: data || []
    };
}
