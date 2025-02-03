# Helpers

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)


:::{toctree}
:caption: File System Functions
:maxdepth: 1

file_exists
file_permission
file_remove
file_size
filesize
filesize2
is_directory
is_regular_file
lock_file
mkrdir
newdir
newfile
open_exclusive
rmrcontentdir
rmrdir
subdir_exists
unlock_file
:::

:::{toctree}
:caption: String Helper Functions
:maxdepth: 1

all_numbers
bin2hex
build_path
change_char
count_char
delete_left_blanks
delete_left_char
delete_right_blanks
delete_right_char
get_key_value_parameter
get_last_segment
get_parameter
helper_doublequote2quote
helper_quote2doublequote
hex2bin
idx_in_list
left_justify
nice_size
pop_last_segment
replace_string
split2
split3
split_free2
split_free3
str_concat
str_concat3
str_concat_free
str_in_list
strntolower
strntoupper
translate_string
:::

:::{toctree}
:caption: JSON Helper Functions
:maxdepth: 1

anystring2json
bits2gbuffer
bits2jn_strlist
cmp_two_simple_json
create_json_record
debug_json
get_real_precision
jn2bool
jn2integer
jn2real
jn2string
json2str
json2uglystr
json_check_refcounts
json_config
json_is_identical
json_list_str_index
json_print_refcounts
json_record_to_schema
json_str_in_list
load_json_from_file
load_persistent_json
print_json2
save_json_to_file
set_real_precision
string2json
strings2bits
:::

:::{toctree}
:caption: Directory Walk Functions
:maxdepth: 1

free_ordered_filename_array
get_number_of_files
get_ordered_filename_array
walk_dir_tree
:::

:::{toctree}
:caption: Time and Date Functions
:maxdepth: 1

approxidate_careful
approxidate_relative
current_timestamp
datestamp
formatdate
htonll
list_open_files
ntohll
parse_date
parse_date_basic
parse_date_format
parse_expiry_date
start_msectimer
start_sectimer
t2timestamp
test_msectimer
test_sectimer
time_in_miliseconds
time_in_miliseconds_monotonic
time_in_seconds
tm2timestamp
:::

:::{toctree}
:caption: Misc Utilities
:maxdepth: 1

create_uuid
get_hostname
node_uuid
:::

:::{toctree}
:caption: Common Protocol Functions
:maxdepth: 1

comm_prot_free
comm_prot_get_gclass
comm_prot_register
:::

:::{toctree}
:caption: Daemon Launcher
:maxdepth: 1

launch_daemon
:::

:::{toctree}
:caption: URL Parsing
:maxdepth: 1

get_url_schema
parse_url
:::

:::{toctree}
:caption: Debug / Backtrace
:maxdepth: 1

init_backtrace_with_backtrace
show_backtrace_with_backtrace
tdump
tdump2json
:::

:::{toctree}
:caption: HTTP Parser
:maxdepth: 1

ghttp_parser_create
ghttp_parser_destroy
ghttp_parser_received
ghttp_parser_reset
:::

:::{toctree}
:caption: IStream Parser
:maxdepth: 1

istream_clear
istream_consume
istream_create
istream_cur_rd_pointer
istream_destroy
istream_extract_matched_data
istream_get_gbuffer
istream_is_completed
istream_length
istream_new_gbuffer
istream_pop_gbuffer
istream_read_until_delimiter
istream_read_until_num_bytes
istream_reset_rd
istream_reset_wr
:::
