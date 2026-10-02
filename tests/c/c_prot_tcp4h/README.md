# c_prot_tcp4h test

Tests `C_PROT_TCP4H`, the frame protocol with a 4-byte length header.

`test1` (`main_test1.c` + `c_test1.c`) is about the size of a frame. A raw
server (`C_PROT_RAW`) writes tcp4h by hand to a `C_PROT_TCP4H` client:

- a frame of 60000 bytes in three parts: the client must deliver it whole
  (its payload buffer starts at 4 KB and grows with what arrives);
- then only the header of a frame of 10 MB, no payload: 300 ms later the
  yuno's memory must not have grown by its length, and the client drops at
  its `timeout_payload` (*"Timeout waiting PAYLOAD data"*).

Up to 7.25.21 the length written in the header was reserved at once (red:
*"The header of a huge frame reserved its length"*, 10 MB grown).

## Run

```bash
ctest -R '^c_prot_tcp4h/' --output-on-failure --test-dir build
```
