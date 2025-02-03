# Helpers

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)


## File System Functions
- [`file_exists()`](#file_exists)
- [`file_permission()`](#file_permission)
- [`file_remove()`](#file_remove)
- [`filesize()`](#filesize)
- [`filesize2()`](#filesize2)
- [`is_directory()`](#is_directory)
- [`is_regular_file()`](#is_regular_file)
- [`lock_file()`](#lock_file)
- [`mkrdir()`](#mkrdir)
- [`newdir()`](#newdir)
- [`newfile()`](#newfile)
- [`open_exclusive()`](#open_exclusive)
- [`rmrcontentdir()`](#rmrcontentdir)
- [`rmrdir()`](#rmrdir)
- [`subdir_exists()`](#subdir_exists)
- [`unlock_file()`](#unlock_file)

## String Functions
- [`all_numbers()`](#all_numbers)
- [`build_path()`](#build_path)
- [`change_char()`](#change_char)
- [`delete_left_blanks()`](#delete_left_blanks)
- [`delete_left_char()`](#delete_left_char)
- [`delete_right_blanks()`](#delete_right_blanks)
- [`delete_right_char()`](#delete_right_char)
- [`get_key_value_parameter()`](#get_key_value_parameter)
- [`get_last_segment()`](#get_last_segment)
- [`get_parameter()`](#get_parameter)
- [`helper_doublequote2quote()`](#helper_doublequote2quote)
- [`helper_quote2doublequote()`](#helper_quote2doublequote)
- [`idx_in_list()`](#idx_in_list)
- [`left_justify()`](#left_justify)
- [`nice_size()`](#nice_size)
- [`pop_last_segment()`](#pop_last_segment)
- [`replace_string()`](#replace_string)
- [`split2()`](#split2)
- [`split3()`](#split3)
- [`split_free2()`](#split_free2)
- [`split_free3()`](#split_free3)
- [`str_concat()`](#str_concat)
- [`str_concat3()`](#str_concat3)
- [`str_concat_free()`](#str_concat_free)
- [`str_in_list()`](#str_in_list)
- [`strntolower()`](#strntolower)
- [`strntoupper()`](#strntoupper)
- [`translate_string()`](#translate_string)

## JSON Functions
- [`anystring2json()`](#anystring2json)
- [`bits2gbuffer()`](#bits2gbuffer)
- [`bits2jn_strlist()`](#bits2jn_strlist)
- [`create_json_record()`](#create_json_record)
- [`json2str()`](#json2str)
- [`json2uglystr()`](#json2uglystr)
- [`json_is_identical()`](#json_is_identical)
- [`json_list_str_index()`](#json_list_str_index)
- [`json_record_to_schema()`](#json_record_to_schema)
- [`json_str_in_list()`](#json_str_in_list)
- [`load_json_from_file()`](#load_json_from_file)
- [`load_persistent_json()`](#load_persistent_json)
- [`save_json_to_file()`](#save_json_to_file)
- [`string2json()`](#string2json)
- [`strings2bits()`](#strings2bits)

## Time Functions
- [`current_timestamp()`](#current_timestamp)
- [`formatdate()`](#formatdate)
- [`start_msectimer()`](#start_msectimer)
- [`start_sectimer()`](#start_sectimer)
- [`t2timestamp()`](#t2timestamp)
- [`test_msectimer()`](#test_msectimer)
- [`test_sectimer()`](#test_sectimer)
- [`time_in_miliseconds()`](#time_in_miliseconds)
- [`time_in_miliseconds_monotonic()`](#time_in_miliseconds_monotonic)
- [`time_in_seconds()`](#time_in_seconds)
- [`tm2timestamp()`](#tm2timestamp)

## Utility Functions
- [`bin2hex()`](#bin2hex)
- [`count_char()`](#count_char)
- [`create_uuid()`](#create_uuid)
- [`get_hostname()`](#get_hostname)
- [`hex2bin()`](#hex2bin)
- [`htonll()`](#htonll)
- [`list_open_files()`](#list_open_files)
- [`node_uuid()`](#node_uuid)
- [`ntohll()`](#ntohll)
- [`print_json2()`](#print_json2)
- [`set_real_precision()`](#set_real_precision)
- [`tdump()`](#tdump)
- [`tdump2json()`](#tdump2json)

## Protocol/Daemon Functions
- [`comm_prot_free()`](#comm_prot_free)
- [`comm_prot_get_gclass()`](#comm_prot_get_gclass)
- [`comm_prot_register()`](#comm_prot_register)
- [`launch_daemon()`](#launch_daemon)

## URL Parsing Functions
- [`get_url_schema()`](#get_url_schema)
- [`parse_url()`](#parse_url)

## Debugging Functions
- [`init_backtrace_with_backtrace()`](#init_backtrace_with_backtrace)
- [`show_backtrace_with_backtrace()`](#show_backtrace_with_backtrace)

## HTTP Parser Functions
- [`ghttp_parser_create()`](#ghttp_parser_create)
- [`ghttp_parser_destroy()`](#ghttp_parser_destroy)
- [`ghttp_parser_received()`](#ghttp_parser_received)
- [`ghttp_parser_reset()`](#ghttp_parser_reset)

## IStream Functions
- [`istream_clear()`](#istream_clear)
- [`istream_consume()`](#istream_consume)
- [`istream_create()`](#istream_create)
- [`istream_cur_rd_pointer()`](#istream_cur_rd_pointer)
- [`istream_destroy()`](#istream_destroy)
- [`istream_extract_matched_data()`](#istream_extract_matched_data)
- [`istream_get_gbuffer()`](#istream_get_gbuffer)
- [`istream_is_completed()`](#istream_is_completed)
- [`istream_length()`](#istream_length)
- [`istream_new_gbuffer()`](#istream_new_gbuffer)
- [`istream_pop_gbuffer()`](#istream_pop_gbuffer)
- [`istream_read_until_delimiter()`](#istream_read_until_delimiter)
- [`istream_read_until_num_bytes()`](#istream_read_until_num_bytes)
- [`istream_reset_rd()`](#istream_reset_rd)
- [`istream_reset_wr()`](#istream_reset_wr)


```{toctree}
:caption: Helpers functions
:maxdepth: 0

newdir.md
newfile.md
open_exclusive.md
filesize.md
filesize2.md
lock_file.md
unlock_file.md
is_regular_file.md
is_directory.md
file_size.md
file_permission.md
file_exists.md
subdir_exists.md
file_remove.md
mkrdir.md
rmrdir.md
rmrcontentdir.md
delete_right_char.md
delete_left_char.md
build_path.md
get_last_segment.md
pop_last_segment.md
helper_quote2doublequote.md
helper_doublequote2quote.md
all_numbers.md
init_backtrace_with_backtrace.md
show_backtrace_with_backtrace.md
replace_string.md
nice_size.md
delete_right_blanks.md
delete_left_blanks.md
left_justify.md
strntoupper.md
strntolower.md
translate_string.md
change_char.md
get_parameter.md
get_key_value_parameter.md
split2.md
split_free2.md
split3.md
split_free3.md
str_concat.md
str_concat3.md
str_concat_free.md
idx_in_list.md
str_in_list.md
json_config.md
load_persistent_json.md
load_json_from_file.md
save_json_to_file.md
create_json_record.md
json_record_to_schema.md
bits2jn_strlist.md
bits2gbuffer.md
strings2bits.md
json_list_str_index.md
jn2real.md
jn2integer.md
jn2string.md
jn2bool.md
cmp_two_simple_json.md
json_is_identical.md
anystring2json.md
string2json.md
set_real_precision.md
get_real_precision.md
json2str.md
json2uglystr.md
json_check_refcounts.md
json_print_refcounts.md
json_str_in_list.md
walk_dir_tree.md
get_number_of_files.md
get_ordered_filename_array.md
free_ordered_filename_array.md
hex2bin.md
bin2hex.md
tdump.md
tdump2json.md
print_json2.md
debug_json.md
current_timestamp.md
tm2timestamp.md
t2timestamp.md
start_sectimer.md
test_sectimer.md
start_msectimer.md
test_msectimer.md
time_in_miliseconds_monotonic.md
time_in_miliseconds.md
time_in_seconds.md
htonll.md
ntohll.md
list_open_files.md
formatdate.md
count_char.md
get_hostname.md
create_uuid.md
node_uuid.md
comm_prot_register.md
comm_prot_get_gclass.md
comm_prot_free.md
launch_daemon.md
parse_url.md
get_url_schema.md
ghttp_parser_create.md
ghttp_parser_received.md
ghttp_parser_destroy.md
ghttp_parser_reset.md
istream_create.md
istream_destroy.md
istream_read_until_num_bytes.md
istream_read_until_delimiter.md
istream_consume.md
istream_cur_rd_pointer.md
istream_length.md
istream_get_gbuffer.md
istream_pop_gbuffer.md
istream_new_gbuffer.md
istream_extract_matched_data.md
istream_reset_wr.md
istream_reset_rd.md
istream_clear.md
istream_is_completed


```
