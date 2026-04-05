# ybatch

Batch command runner for Yuneta services. Connects to a running yuno and executes a script of commands — useful for automation, provisioning and smoke tests. Authentication supports JWT, user/password and config-file credentials.

## Usage

```bash
ybatch --url <ws://…> --script <file> [--jwt <token> | --user <u> --password <p>]
```

Run `ybatch --help` for all options.
