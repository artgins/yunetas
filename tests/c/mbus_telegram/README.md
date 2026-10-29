# mbus_telegram test

Tests the M-Bus telegram decoder of `modules/c/mbus` (`mbus_telegram.h`)
without a yuno: frames in, frames or json out.

- **Frames**: `E5`, short, control and long frames found; bad checksum, bad
  stop byte, lengths that differ, a length under 3 and a bad fourth byte
  refused; an incomplete frame asks for more bytes.
- **Requests built**: `REQ_UD2` with and without FCB, `SND_NKE` to `FD`, a
  selection by secondary address (manufacturer and wildcards), and the
  refusals of a bad id, manufacturer, version or medium.
- **One case per rule of EN 13757-3**, with the answer worked out from the
  standard: each data coding (integers, real, BCD with its minus sign, LVAR
  text and binary), the primary VIF table, `FB` and `FD`, the units each
  quantity is given in, DIFEs (storage, tariff, subunit), the functions, the
  VIFEs interpreted (record error, positive and negative accumulation,
  correction factor, "date of") and the ones kept in the id, dates of type G,
  F, I and J (invalid, never set, and a field out of its range: an hour 31, a
  year 127), a real `+-Inf` (null, the rest decoded), a duration in months or
  years (`<quantity>_months`), repeated ids, fillers, manufacturer data and
  `DIF 1F`.
- **Headers and refusals**: `CI 7A`, `CI 72` with an encryption mode (refused)
  and with a signature that is not one (decoded), `CI 70`, `CI 73` (fixed
  data, refused), and records cut short in each part.
- **Real meters**: six telegrams of the libmbus collection (Kamstrup,
  Itron water and gas, Engelmann, Landis+Gyr, Sontex), whose decoding was
  checked record by record against libmbus's (`real_telegrams.inc`; licence
  in `LICENSE-libmbus-frames.txt`). One record differs on purpose since the
  date ranges are checked: storage 510 of the Landis+Gyr is a type F date with
  year 127 (all its bits set, a slot never written), `null` here and
  `2027-01-01T00:00` in libmbus.

The memory is checked free at the end.

The test is built only when `CONFIG_MODULE_MBUS` is set.

## Run

```bash
ctest -R '^mbus_telegram/' --output-on-failure --test-dir build
```
