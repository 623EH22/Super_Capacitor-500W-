# SEGGER RTT (official)

Official SEGGER RTT sources (SEGGER Microcontroller GmbH).
Taken from a local HPM SDK `middleware/segger_rtt` distribution.

| File | Purpose |
|------|---------|
| `SEGGER_RTT.c` / `SEGGER_RTT.h` | RTT core (control block, ring buffers) |
| `SEGGER_RTT_Conf.h` | Channel count / buffer sizes (UP enlarged to 4096 for ADC stream) |
| `SEGGER_RTT_printf.c` | `SEGGER_RTT_printf()` |
| `SEGGER_RTT_Syscalls_KEIL.c` | optional Keil microlib retarget (not linked by default) |
| `LICENSE.md` | SEGGER license |

Do not modify `SEGGER_RTT.c/h` to stay compatible with J-Link / RTT Viewer / J-Scope.

Project code (`drv_rtt_stream.c`) uses:
- UP channel 0 — Terminal (text `JSFMT:` banner)
- UP channel 1 — Data (binary frames), **explicitly configured via `SEGGER_RTT_ConfigUpBuffer`**
