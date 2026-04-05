# json_diff

Semantic diff of two JSON files. Arrays can be compared as unordered sets, and metadata / private fields can be filtered out — convenient when comparing test fixtures or yuno configurations.

## Usage

```bash
json_diff <file1.json> <file2.json> [options]
```

Run `json_diff --help` for the full list of options (unordered arrays, skip private keys, etc.).
