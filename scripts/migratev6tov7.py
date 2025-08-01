#!/usr/bin/env python3

import os
import re
import argparse

# List of default substitutions (original, new)
DEFAULT_SUBSTITUTIONS = [
    ("ASN_OCTET_STR,",          "DTP_STRING,   "),
    ("ASN_UNSIGNED,",           "DTP_INTEGER, "),
    ("ASN_BOOLEAN",             "DTP_BOOLEAN"),
    ("ASN_COUNTER64,",          "DTP_INTEGER,  "),
    ("ASN_INTEGER",             "DTP_INTEGER"),
    ("ASN_UNSIGNED64,",         "DTP_INTEGER,   "),
    ("ASN_JSON",                "DTP_JSON"),
    ("ASN_POINTER",             "DTP_POINTER"),
    ("ASN_SCHEMA",              "DTP_SCHEMA"),
    ("GCLASS_TIMER",            "C_TIMER"),
    ("GCLASS_TRANGER",          "C_TRANGER"),
    ("tranger_create_topic",    "tranger2_create_topic"),
    ("tranger_close_topic",     "tranger2_close_topic"),
    ("tranger_topic_name",      "tranger2_topic_name"),
    ("tranger_topic_size",      "tranger2_topic_size"),
    ("tranger_backup_topic",    "tranger2_backup_topic"),
    ("tranger_append_record",   "tranger2_append_record"),
    ("md_record_t",             "md2_record_ex_t"),

    ("\"EV_TIMEOUT\"",          "EV_TIMEOUT"),
    ("\"EV_TIMEOUT_PERIODIC\"", "EV_TIMEOUT_PERIODIC"),
    ("\"EV_STATE_CHANGED\"",    "EV_STATE_CHANGED"),
    ("\"EV_SEND_MESSAGE\"",     "EV_SEND_MESSAGE"),
    ("\"EV_SEND_IEV\"",         "EV_SEND_IEV"),
    ("\"EV_SEND_EMAIL\"",       "EV_SEND_EMAIL"),
    ("\"EV_DROP\"",             "EV_DROP"),
    ("\"EV_ON_OPEN\"",          "EV_ON_OPEN"),
    ("\"EV_ON_CLOSE\"",         "EV_ON_CLOSE"),
    ("\"EV_ON_MESSAGE\"",       "EV_ON_MESSAGE"),
    ("\"EV_ON_COMMAND\"",       "EV_ON_COMMAND"),
    ("\"EV_ON_IEV_MESSAGE\"",   "EV_ON_IEV_MESSAGE"),
    ("\"EV_ON_ID\"",            "EV_ON_ID"),
    ("\"EV_ON_ID_NAK\"",        "EV_ON_ID_NAK"),
    ("\"EV_ON_HEADER\"",        "EV_ON_HEADER"),
    ("\"EV_HTTP_MESSAGE\"",     "EV_HTTP_MESSAGE"),
    ("\"EV_CONNECT\"",          "EV_CONNECT"),
    ("\"EV_CONNECTED\"",        "EV_CONNECTED"),
    ("\"EV_DISCONNECT\"",       "EV_DISCONNECT"),
    ("\"EV_DISCONNECTED\"",     "EV_DISCONNECTED"),
    ("\"EV_RX_DATA\"",          "EV_RX_DATA"),
    ("\"EV_TX_DATA\"",          "EV_TX_DATA"),
    ("\"EV_TX_READY\"",         "EV_TX_READY"),
    ("\"EV_STOPPED\"",          "EV_STOPPED"),

    ("\"ST_CONNECTED\"",            "ST_CONNECTED"),
    ("\"ST_DISCONNECTED\"",         "ST_DISCONNECTED"),
    ("\"ST_WAIT_CONNECTED\"",       "ST_WAIT_CONNECTED"),
    ("\"ST_WAIT_TXED\"",            "ST_WAIT_TXED"),
    ("\"ST_WAIT_DISCONNECTED\"",    "ST_WAIT_DISCONNECTED"),
    ("\"ST_WAIT_STOPPED\"",         "ST_WAIT_STOPPED"),
    ("\"ST_WAIT_HANDSHAKE\"",       "ST_WAIT_HANDSHAKE"),
    ("\"ST_WAIT_IMEI\"",            "ST_WAIT_IMEI"),
    ("\"ST_WAIT_ID\"",              "ST_WAIT_ID"),
    ("\"ST_STOPPED\"",              "ST_STOPPED"),
    ("\"ST_SESSION\"",              "ST_SESSION"),
    ("\"ST_IDLE\"",                 "ST_IDLE"),
    ("\"ST_WAIT_RESPONSE\"",        "ST_WAIT_RESPONSE"),
    ("\"ST_OPENED\"",               "ST_OPENED"),
    ("\"ST_CLOSED\"",               "ST_CLOSED"),

    ("\"ST_WAITING_HANDSHAKE\"",    "ST_WAITING_HANDSHAKE"),
    ("\"ST_WAITING_FRAME_HEADER\"", "ST_WAITING_FRAME_HEADER"),
    ("\"ST_WAITING_PAYLOAD_DATA\"", "ST_WAITING_PAYLOAD_DATA"),


    ("uint64_t *ptxMsgs;",      "uint64_t txMsgs;"),
    ("uint64_t *prxMsgs;",      "uint64_t rxMsgs;"),

    ("gobj_read_int32_attr",    "gobj_read_integer_attr"),
    ("gobj_read_uint32_attr",   "gobj_read_integer_attr"),
    ("gobj_read_uint64_attr",   "gobj_read_integer_attr"),

    ("gobj_write_int32_attr",   "gobj_write_integer_attr"),
    ("gobj_write_uint32_attr",  "gobj_write_integer_attr"),
    ("gobj_write_uint64_attr",  "gobj_write_integer_attr"),
    ("gobj_start_childs",       "gobj_start_children"),
    ("gobj_stop_childs",        "gobj_stop_children"),
    ("gobj_node_owner",         "gobj_yuno_node_owner"),
    ("yuneta_work_dir",         "yuneta_root_dir"),

    ("gbuf_set_wr",             "gbuffer_set_wr"),
    ("gbuf_set_rd_offset",      "gbuffer_set_rd_offset"),
    ("gbuf_clear",              "gbuffer_clear"),
    ("gbuf_create",             "gbuffer_create"),
    ("gbuf_append_string",      "gbuffer_append_string"),
    ("gbuf_printf",             "gbuffer_printf"),
    ("gbuf_decref",             "gbuffer_decref"),
    ("gbuf_incref",             "gbuffer_incref"),
    ("gbuf_get",                "gbuffer_get"),
    ("gbuf_cur_rd_pointer",     "gbuffer_cur_rd_pointer"),
    ("gbuf_chunk",              "gbuffer_chunk"),
    ("gbuf_leftbytes",          "gbuffer_leftbytes"),
    ("gbuf_append",             "gbuffer_append"),
    ("GBUF_DECREF",             "GBUFFER_DECREF"),
    ("GBUF_INCREF",             "GBUFFER_INCREF"),
    ("gbuf_encodebase64",       "gbuffer_encode_base64"),
    ("get_host_name()",         "get_hostname()"),

    ("msg_iev_build_webix",     "msg_iev_build_response"),
    ("rc_free_iter",            "gobj_free_iter"),
    ("(*priv->prxMsgs)++;",     "priv->rxMsgs++;"),
    ("(*priv->ptxMsgs)++;",     "priv->txMsgs++;"),
    ("log_debug_json(LOG_DUMP_INPUT,", "gobj_trace_json(gobj,")
]

# List of regex-based substitutions (regex_pattern, replacement)
DEFAULT_REGEX_SUBSTITUTIONS = [
    (r'\blog_debug\(', 'gobj_log_debug(gobj, '),  # Replace log_debug( with gobj_log_debug(gobj,
    (r'\blog_error\(', 'gobj_log_error(gobj, '),  # Replace log_error( as whole word
    (r'\blog_info\(', 'gobj_log_info(gobj, '),    # Replace log_info( as whole word
    (r'\blog_warning\(', 'gobj_log_warning(gobj, '),  # Replace log_warning( as whole word
    (r'\blog_critical\(', 'gobj_log_critical(gobj, '),  # Replace log_warning( as whole word
    (r'\blog_debug_json\(0,', 'gobj_trace_json(gobj,'),
    (r'\btrace_msg\(', 'gobj_trace_msg(gobj, '),

    (r'\blog_debug_dump\(0,', 'gobj_trace_dump(gobj,'),
    (r'\blog_debug_gbuf\(LOG_DUMP_OUTPUT,', 'gobj_trace_dump_gbuf(gobj,'),
    (r'\blog_debug_gbuf\(LOG_DUMP_INPUT,', 'gobj_trace_dump_gbuf(gobj,'),
    (r'\blog_debug_gbuf\(0,', 'gobj_trace_dump_gbuf(gobj,'),
    (r'\blog_debug_full_gbuf\(LOG_DUMP_INPUT, ', 'gobj_trace_dump_full_gbuf(gobj,'),

    (r'\bGBUFFER ', 'gbuffer_t '),


    (r'log_debug_dump\((?!gobj)', 'gobj_trace_dump(gobj, '),
    (r'kw_get_dict\((?!gobj)', 'kw_get_dict(gobj, '),
    (r'kw_get_list\((?!gobj)', 'kw_get_list(gobj, '),
    (r'kw_set_dict_value\((?!gobj)', 'kw_set_dict_value(gobj, '),
    (r'kw_get_dict_value\((?!gobj)', 'kw_get_dict_value(gobj, '),
    (r'kw_select\((?!gobj)', 'kw_select(gobj, '),
    (r'kw_get_str\((?!gobj)', 'kw_get_str(gobj, '),
    (r'kw_get_int\((?!gobj)', 'kw_get_int(gobj, '),
    (r'kw_get_bool\((?!gobj)', 'kw_get_bool(gobj, '),
    (r'kw_get_real\((?!gobj)', 'kw_get_real(gobj, '),
    (r'kw_get_list_value\((?!gobj)', 'kw_get_list_value(gobj, '),
    (r'get_ordered_filename_array\((?!gobj)', 'get_ordered_filename_array(gobj, '),
]

# List of default regex patterns to delete matching lines
DEFAULT_REGEX_DELETIONS = [
    r'.*"gobj",\s+"%s",.*',  # Matches lines with "gobj", "%s" with spaces allowed in between
    r'priv->ptxMsgs = gobj_danger_attr_ptr\(gobj, "txMsgs"\);',
    r'priv->prxMsgs = gobj_danger_attr_ptr\(gobj, "rxMsgs"\);',
    r'gobj_set_qs.*',
    r'gobj_incr_qs.*',
    r'gobj_decr_qs.*'
]

def substitute_strings_in_file(filename, substitutions):
    """
    Substitutes strings in a file based on the substitutions list.

    Args:
        filename (str): Path to the C file to modify.
        substitutions (list of tuples): List of (original_string, new_string) pairs.
    """
    if not os.path.isfile(filename):
        print(f"Error: File '{filename}' does not exist.")
        return

    if not filename.endswith(".c") and not filename.endswith(".h"):
        print(f"Error: File '{filename}' is not a C source or header file.")
        return

    try:
        # Read the content of the file
        with open(filename, 'r', encoding='utf-8') as file:
            content = file.read()

        # Perform the substitutions
        for original, new in substitutions:
            content = content.replace(original, new)

        # Write the updated content back to the file
        with open(filename, 'w', encoding='utf-8') as file:
            file.write(content)

        print(f"Substitutions completed successfully for file: {filename}")

    except FileNotFoundError:
        print("Error: The file could not be found.")
    except PermissionError:
        print("Error: Permission denied. Cannot modify the file.")
    except Exception as e:
        print(f"Error: An unexpected error occurred while processing the file: {e}")

def substitute_regex_in_file(filename, regex_substitutions):
    """
    Substitutes strings in a file based on regex patterns.

    Args:
        filename (str): Path to the C file to modify.
        regex_substitutions (list of tuples): List of (regex_pattern, replacement) pairs.
    """
    if not os.path.isfile(filename):
        print(f"Error: File '{filename}' does not exist.")
        return

    if not filename.endswith(".c") and not filename.endswith(".h"):
        print(f"Error: File '{filename}' is not a C source or header file.")
        return

    try:
        # Read the content of the file
        with open(filename, 'r', encoding='utf-8') as file:
            lines = file.readlines()

        # Perform regex substitutions
        updated_lines = []
        for line in lines:
            for pattern, replacement in regex_substitutions:
                line = re.sub(pattern, replacement, line)
            updated_lines.append(line)

        # Write the updated content back to the file
        with open(filename, 'w', encoding='utf-8') as file:
            file.writelines(updated_lines)

        print(f"Regex substitutions completed successfully for file: {filename}")

    except FileNotFoundError:
        print("Error: The file could not be found.")
    except PermissionError:
        print("Error: Permission denied. Cannot modify the file.")
    except Exception as e:
        print(f"Error: An unexpected error occurred while processing the file: {e}")

def delete_lines_matching_regex(filename, regex_patterns):
    """
    Deletes lines in a file that match any of the regex patterns.

    Args:
        filename (str): Path to the C file to modify.
        regex_patterns (list of str): List of regex patterns to match lines for deletion.
    """
    if not os.path.isfile(filename):
        print(f"Error: File '{filename}' does not exist.")
        return

    if not filename.endswith(".c") and not filename.endswith(".h"):
        print(f"Error: File '{filename}' is not a C source or header file.")
        return

    try:
        # Read the content of the file
        with open(filename, 'r', encoding='utf-8') as file:
            lines = file.readlines()

        # Filter out lines matching any of the regex patterns
        filtered_lines = []
        for line in lines:
            if not any(re.search(pattern, line) for pattern in regex_patterns):
                filtered_lines.append(line)

        # Write the filtered content back to the file
        with open(filename, 'w', encoding='utf-8') as file:
            file.writelines(filtered_lines)

        print(f"Lines matching regex patterns deleted successfully for file: {filename}")

    except FileNotFoundError:
        print("Error: The file could not be found.")
    except PermissionError:
        print("Error: Permission denied. Cannot modify the file.")
    except Exception as e:
        print(f"Error: An unexpected error occurred while processing the file: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Substitute strings and delete lines in a C source or header file based on predefined rules.")
    parser.add_argument("filename", type=str, help="The path to the C source or header file to modify.")

    args = parser.parse_args()

    # Perform the substitutions using the default list
    substitute_strings_in_file(args.filename, DEFAULT_SUBSTITUTIONS)

    # Perform the regex substitutions using the default list
    substitute_regex_in_file(args.filename, DEFAULT_REGEX_SUBSTITUTIONS)

    # Delete lines matching the default regex patterns
    delete_lines_matching_regex(args.filename, DEFAULT_REGEX_DELETIONS)
