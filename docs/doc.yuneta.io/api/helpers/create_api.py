#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
<!-- ============================================================== -->



<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**

```C
```

**Parameters**


---

**Return Value**


<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS Prototype                    -->
<!---------------------------------------------------->

**Prototype**

````JS
// Not applicable in JS
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python Prototype                -->
<!---------------------------------------------------->

**Prototype**

````Python
# Not applicable in Python
````

<!--====================================================-->
<!--                    End Tab Python                   -->
<!--====================================================-->

`````

``````

<!------------------------------------------------------------>
<!--                    Examples                            -->
<!------------------------------------------------------------>

```````{dropdown} Examples

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C examples                      -->
<!---------------------------------------------------->

````C
// TODO C examples
````

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS examples                     -->
<!---------------------------------------------------->

````JS
// TODO JS examples
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python examples                 -->
<!---------------------------------------------------->

````python
# TODO Python examples
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````

""")

# List of names
names = [
    "newdir",
    "newfile",
    "open_exclusive",
    "filesize",
    "filesize2",
    "lock_file",
    "unlock_file",
    "is_regular_file",
    "is_directory",
    "file_size",
    "file_permission",
    "file_exists",
    "subdir_exists",
    "file_remove",
    "mkrdir",
    "rmrdir",
    "rmrcontentdir",
    "delete_right_char",
    "delete_left_char",
    "build_path",
    "get_last_segment",
    "pop_last_segment",
    "helper_quote2doublequote",
    "helper_doublequote2quote",
    "all_numbers",
    "nice_size",
    "delete_right_blanks",
    "delete_left_blanks",
    "left_justify",
    "strntoupper",
    "strntolower",
    "translate_string",
    "change_char",
    "get_parameter",
    "get_key_value_parameter",
    "split2",
    "split_free2",
    "split3",
    "split_free3",
    "str_concat",
    "str_concat3",
    "str_concat_free",
    "idx_in_list",
    "str_in_list",
    "load_persistent_json",
    "load_json_from_file",
    "save_json_to_file",
    "create_json_record",
    "json_record_to_schema",
    "bits2jn_strlist",
    "bits2gbuffer",
    "strings2bits",
    "json_list_str_index",
    "jn2real",
    "jn2integer",
    "jn2string",
    "jn2bool",
    "cmp_two_simple_json",
    "json_is_identical",
    "anystring2json",
    "string2json",
    "set_real_precision",
    "get_real_precision",
    "json2str",
    "json2uglystr",
    "json_check_refcounts",
    "json_print_refcounts",
    "json_str_in_list",
    "walk_dir_tree",
    "get_number_of_files",
    "get_ordered_filename_array",
    "free_ordered_filename_array",
    "tdump",
    "tdump2json",
    "print_json2",
    "debug_json",
    "current_timestamp",
    "tm2timestamp",
    "t2timestamp",
    "start_sectimer",
    "test_sectimer",
    "start_msectimer",
    "test_msectimer",
    "time_in_miliseconds_monotonic",
    "time_in_miliseconds",
    "time_in_seconds",
    "htonll",
    "ntohll",
    "list_open_files",
    "formatdate",
    "count_char",
    "get_hostname",
    "create_uuid",
    "node_uuid",
    "comm_prot_register",
    "comm_prot_get_gclass",
    "comm_prot_free",
    "launch_daemon",
    "parse_url",
    "get_url_schema",
    "ghttp_parser_received",
    "ghttp_parser_destroy",
    "ghttp_parser_reset",
    "istream_create",
    "istream_destroy",
    "istream_read_until_num_bytes",
    "istream_read_until_delimiter",
    "istream_consume",
    "istream_cur_rd_pointer",
    "istream_length",
    "istream_get_gbuffer",
    "istream_pop_gbuffer",
    "istream_new_gbuffer",
    "istream_extract_matched_data",
    "istream_reset_wr",
    "istream_reset_rd",
    "istream_clear",
    "istream_is_completed"
]

# Loop through the list of names and create a file for each
for name in names:
    # Substitute the variable in the template
    formatted_text = template.substitute(_name_=name)

    # Create a unique file name for each name
    file_name = f"{name.lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
