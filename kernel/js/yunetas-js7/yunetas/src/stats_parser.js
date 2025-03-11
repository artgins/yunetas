/****************************************************************************
 *          stats_parser.js
 *
 *          Stats parser
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/

/***************************************************************
 *
 ***************************************************************/
function stats_parser(
    gobj,
    stats,
    kw,
    src
) {

}

/***************************************************************
 *
 ***************************************************************/
function build_stats_response(
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

export {
    stats_parser,
    build_stats_response,
};
