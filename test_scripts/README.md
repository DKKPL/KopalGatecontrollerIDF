# Modbus test scripts

Scripts to test the Modbus RTU slave of the gate controller from a PC, through a
USB-RS485 adapter. They follow the Kopal Modbus mapping and need **firmware 0.9 or
later** (official numbering).

| Script | What it does |
|---|---|
| `modbus_test.py` | Checks everything the mapping promises and prints PASS/FAIL per check |
| `frame_builder.py` | Builds single frames (hex) to send by hand, e.g. with CoolTerm |
| `kopal_mb.py` | Shared helper for `modbus_test.py`: mapping numbers to wire addresses, exception codes |

## Addressing

Official Modbus numbering: register or coil **N** is sent as wire address **N − 1**.
Register 1 (Push Open) goes out as `00 00`, register 120 as `00 77`, coil 9 as `00 08`.
The scripts take the numbers from the mapping and do the − 1 themselves.

## Set up (once)

Needs Python 3.9 or later.

Windows (PowerShell, in this folder):
```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```
If PowerShell refuses to run the activate script, first run
`Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned`.

macOS / Linux:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Wiring

USB-RS485 adapter A/B to the board's RS485 DATA+/DATA−, and GND to GND. The board is
slave 55, 9600 baud, 8N1.

## Run the full test

```bash
python modbus_test.py COM3
```
Use your own port: `COM3`, `COM5`, … on Windows (Device Manager → Ports),
`/dev/cu.usbserial-…` on macOS.

What it checks, in this order:

1. Connection: register 101 is the slave address, and the firmware is 0.9 or later.
2. FC04 1–22, the board LEDs: every value is 0 or 1, and 15, 16, 19, 20 read 0.
3. FC02 1–22 gives the same values as FC04 1–22.
4. FC04 100–111: address, firmware version, and 104–111 reserved (0).
5. FC04 120–126: the public settings; 124 and 126 read 0.
6. FC03: 101–111 and 120–126 equal FC04, and 200 can be read.
7. FC06 test register 100: FC03 reads it as written, FC04 inverted.
8. FC06 120–123: write a test value, read it back with FC04 and FC03, then restore
   the original value. 124 and 126 ignore a write; 125 is read only (exception 02).
9. FC01 coils 1–9 read 0.
10. CLI register 200: commands 1 (status) and 8 (version), the done status, the last
    code on FC03 200, and the acknowledge. The command output appears on the serial
    console (ESP-IDF monitor), not over Modbus.
11. Exception answers: unknown registers and gaps (02), an unknown CLI code (03),
    and an unsupported function code, FC16 (01).

The settings test writes to flash and puts the original values back. It ends with a
summary; the exit code is 0 when everything passed.

Options:

| Option | Effect |
|---|---|
| `--coils` | Also sends the FC05 gate commands. **The gate moves**: it asks first. |
| `--poll` | Shows the 22 board LEDs continuously at the end (Ctrl+C stops). |
| `--slave N` | Slave address when it is not 55. |

Coil 9 (Autoclose) is not sent with `--coils`: it has no effect until the gate logic
fix is in (point 1 in `../fixes.md`).

## Build a single frame

```bash
python frame_builder.py
```
It needs no packages, so it also runs without the set-up above. Type the register or coil number from the mapping; it prints the frame with the right
wire address and CRC, e.g. FC04 register 1 → `37 04 00 00 00 01 34 5C`.
