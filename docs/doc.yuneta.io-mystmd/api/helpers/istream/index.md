# `istream` — Input stream helper

The `istream` helper provides a growable input buffer with delimiter- and
length-based parsing primitives. It is used by protocol GClasses
(`c_prot_*`) to assemble incoming bytes into framed messages without
blocking the event loop.

## Lifecycle

| Function | Purpose |
|---|---|
| [`istream_create()`](./istream_create.md) | Allocate a new stream |
| [`istream_destroy()`](./istream_destroy.md) | Free a stream |
| [`ISTREAM_CREATE`](./ISTREAM_CREATE.md) | Convenience macro |
| [`ISTREAM_DESTROY`](./ISTREAM_DESTROY.md) | Convenience macro |

## Reading

| Function | Purpose |
|---|---|
| [`istream_read_until_delimiter()`](./istream_read_until_delimiter.md) | Read until a delimiter byte sequence |
| [`istream_read_until_num_bytes()`](./istream_read_until_num_bytes.md) | Read a fixed number of bytes |
| [`istream_extract_matched_data()`](./istream_extract_matched_data.md) | Consume matched data |
| [`istream_is_completed()`](./istream_is_completed.md) | Check whether the current read has finished |

## Inspection & state

| Function | Purpose |
|---|---|
| [`istream_length()`](./istream_length.md) | Unread byte count |
| [`istream_cur_rd_pointer()`](./istream_cur_rd_pointer.md) | Current read pointer |
| [`istream_get_gbuffer()`](./istream_get_gbuffer.md) | Access the backing gbuffer |
| [`istream_pop_gbuffer()`](./istream_pop_gbuffer.md) | Detach the backing gbuffer |
| [`istream_new_gbuffer()`](./istream_new_gbuffer.md) | Replace the backing gbuffer |
| [`istream_clear()`](./istream_clear.md) | Clear stream contents |
| [`istream_consume()`](./istream_consume.md) | Advance the read pointer |
| [`istream_reset_rd()`](./istream_reset_rd.md) | Reset read pointer |
| [`istream_reset_wr()`](./istream_reset_wr.md) | Reset write pointer |
