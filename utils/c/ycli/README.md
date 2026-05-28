# ycli

Interactive ncurses-based terminal UI for Yuneta v7. Provides a layout for browsing yunos, inspecting state, and sending commands — a friendlier alternative to raw `ycommand` for live debugging. (The source directory was `yunos/c/yuno_cli` before 7.4.x; the binary has always been `ycli`.)

## Usage

```bash
ycli [--url ws://host:port]
```

Navigate with arrow keys; press `?` inside the UI for help.
