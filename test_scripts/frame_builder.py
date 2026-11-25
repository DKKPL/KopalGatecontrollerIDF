#!/usr/bin/env python3
"""Builds Modbus RTU frames for the Kopal gate controller, to send by hand
(CoolTerm: Send String, Hex mode, no line ending).

You type the numbers from the mapping (register 120, coil 1, ...). The frame
carries the official wire address, number - 1, and a correct CRC.

Run: python frame_builder.py   (no packages needed)
"""
SLAVE_ID = 55


def crc16(data):
    """Modbus RTU CRC-16 (polynomial 0xA001)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def frame(slave, fc, number, value):
    """RTU request for mapping number `number`: the wire address is number - 1."""
    wire = number - 1
    data = bytes([slave, fc, (wire >> 8) & 0xFF, wire & 0xFF, (value >> 8) & 0xFF, value & 0xFF])
    c = crc16(data)
    return data + bytes([c & 0xFF, (c >> 8) & 0xFF])


def to_hex(data):
    return " ".join(f"{b:02X}" for b in data)


# FC06 writable registers: number -> (name, unit, min, max, note)
WRITE_REGISTERS = {
    100: ("Test register", "", 0, 65535, "FC04 100 reads it inverted, FC03 100 as written"),
    120: ("AutoCloseDelay", "s", 0, 65535, ""),
    121: ("PedestrianModeDistance", "cm", 0, 65535, ""),
    122: ("PassageLightDelay", "s", 0, 65535, ""),
    123: ("BeaconPreWarningTime", "s", 0, 65535, ""),
    124: ("AutoRedirectEnabled", "", 0, 65535, "future implementation, write ignored"),
    126: ("gatePassageMeters", "", 0, 65535, "future implementation, write ignored"),
    200: ("CLI command", "", 0, 9, "0 = acknowledge, 1 status, 2 gpio, 3 errors, 4 clear, "
                                   "5 counters, 6 cell*, 7 mqtt*, 8 version, 9 reset*  (*not available)"),
}
READ_ONLY = {125: "StopCount is read only: the board answers exception 02"}

COILS = {1: "Push Open", 2: "Push Close", 3: "Push Open Stop Close", 4: "Pedestrian",
         5: "(reserved)", 6: "(reserved)", 7: "(reserved)", 8: "(reserved)", 9: "Autoclose"}

# Read functions: fc -> (name, default first number, default count, valid numbers)
READ_FUNCTIONS = {
    0x01: ("Read Coils", 1, 9, "coils 1-9 (always 0)"),
    0x02: ("Read Discrete Inputs", 1, 22, "inputs 1-22"),
    0x03: ("Read Holding Registers", 120, 7, "100-111, 120-126, 200"),
    0x04: ("Read Input Registers", 1, 22, "1-22, 100-111, 120-126, 200"),
}


def ask_int(prompt, default=None):
    text = input(prompt).strip()
    if not text and default is not None:
        return default
    try:
        return int(text, 0)          # accepts 120 or 0x78
    except ValueError:
        print("  Not a valid number.")
        return None


def show(data, echo):
    print(f"\n  Frame  : {to_hex(data)}")
    print("  Paste into CoolTerm -> Send String -> Hex mode -> no line ending")
    if echo:
        print(f"  Answer : the same 8 bytes back (echo) when it is accepted")


def build_write_register():
    print("\n  Writable registers (FC06):")
    for n, (name, unit, lo, hi, note) in WRITE_REGISTERS.items():
        line = f"    [{n}] {name}  ({lo}-{hi} {unit})".rstrip()
        print(line + (f"  - {note}" if note else ""))
    n = ask_int("\n  Register number: ")
    if n is None:
        return
    value = ask_int("  Value (0-65535): ")
    if value is None or not 0 <= value <= 0xFFFF:
        print("  Value must be 0-65535.")
        return
    if n in READ_ONLY:
        print(f"  Warning: {READ_ONLY[n]}.")
    elif n not in WRITE_REGISTERS:
        print(f"  Warning: register {n} is not writable; the board answers exception 02.")
    else:
        name, _, lo, hi, _ = WRITE_REGISTERS[n]
        if not lo <= value <= hi:
            print(f"  Warning: {value} is outside {lo}-{hi} for {name}.")
    show(frame(SLAVE_ID, 0x06, n, value), echo=True)


def build_write_coil():
    print("\n  Coils (FC05):")
    for n, name in COILS.items():
        print(f"    [{n}] {name}")
    n = ask_int("\n  Coil number: ")
    if n is None:
        return
    state = input("  1 = press (0xFF00), 0 = no action (0x0000): ").strip()
    if state not in ("0", "1"):
        print("  Enter 0 or 1.")
        return
    if n not in COILS:
        print(f"  Warning: coil {n} does not exist; the board answers exception 02.")
    elif state == "1" and n in (1, 2, 3, 4):
        print("  Warning: the gate will move.")
    show(frame(SLAVE_ID, 0x05, n, 0xFF00 if state == "1" else 0x0000), echo=True)


def build_read():
    print("\n  Read functions:")
    for fc, (name, first, count, valid) in READ_FUNCTIONS.items():
        print(f"    [{fc}] FC{fc:02d} {name}: {valid}  (default {first}, count {count})")
    fc = ask_int("\n  Function code: ")
    if fc not in READ_FUNCTIONS:
        print("  Unknown function code.")
        return
    _, d_first, d_count, _ = READ_FUNCTIONS[fc]
    first = ask_int(f"  First number [{d_first}]: ", d_first)
    count = ask_int(f"  Count [{d_count}]: ", d_count)
    if first is None or count is None:
        return
    show(frame(SLAVE_ID, fc, first, count), echo=False)


def build_raw():
    print("\n  Raw frame: you give the WIRE address (no -1 is applied).")
    fc = ask_int("  Function code (decimal or 0x..): ")
    wire = ask_int("  Wire address: ")
    value = ask_int("  Value / count: ")
    if None in (fc, wire, value):
        return
    if not (0 <= fc <= 0xFF and 0 <= wire <= 0xFFFF and 0 <= value <= 0xFFFF):
        print("  Value out of range.")
        return
    show(frame(SLAVE_ID, fc, wire + 1, value), echo=False)


MENU = {
    "1": ("Write single register (FC06)", build_write_register),
    "2": ("Write single coil (FC05)", build_write_coil),
    "3": ("Read (FC01 / FC02 / FC03 / FC04)", build_read),
    "4": ("Raw frame (any FC, wire address)", build_raw),
}


def main():
    print("\n=== Kopal Modbus Frame Builder ===")
    print(f"  Slave {SLAVE_ID} (0x{SLAVE_ID:02X}), official numbering: wire address = number - 1")
    while True:
        print()
        for key, (title, _) in MENU.items():
            print(f"    [{key}] {title}")
        print("    [q] Quit")
        choice = input("\n  Choice: ").strip().lower()
        if choice in ("q", "quit", "exit", ""):
            break
        if choice not in MENU:
            print("  Unknown choice.")
            continue
        MENU[choice][1]()
        print("\n  " + "-" * 50)


if __name__ == "__main__":
    main()
