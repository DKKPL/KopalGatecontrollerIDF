#!/usr/bin/env python3
"""Modbus RTU test for the Kopal gate controller (firmware 0.9 and later).

Checks what the Kopal Modbus mapping promises: every register and coil, the
read-backs, the writes and the exception answers. Official numbering: register
or coil N is sent as wire address N - 1 (see kopal_mb.py).

Run:
    python modbus_test.py COM3                       (Windows)
    python modbus_test.py /dev/cu.usbserial-XXXX     (macOS)

Options:
    --coils   also send the FC05 gate commands: THE GATE MOVES
    --poll    show the 22 board LEDs continuously at the end (Ctrl+C stops)
    --slave N slave address when it is not 55

Exit code 0 when every check passed, 1 otherwise.
"""
import argparse
import random
import sys
import time

import minimalmodbus

import kopal_mb
from kopal_mb import (EXC_ILLEGAL_ADDRESS, EXC_ILLEGAL_FUNCTION, EXC_ILLEGAL_VALUE,
                      exception_code)

PAUSE = 0.15          # seconds between transactions
CLI_DONE_TIMEOUT = 3.0

PERIPH_LABELS = {
    1: "Push Open", 2: "Push Close", 3: "Push Open Stop Close", 4: "Pedestrian",
    5: "Limit Switch Open", 6: "Limit Switch Close", 7: "Photocell", 8: "Safety strip",
    9: "Autoclose", 10: "Learn", 11: "Aux in",
    12: "Passage light out", 13: "Flash out", 14: "Lock out (1 = unlocked)", 15: "Aux out",
    16: "Pulses input", 17: "Direction input", 18: "Torque limit input", 19: "Torque",
    20: "Speed", 21: "Direction (1 = opening)", 22: "On/off",
}
ALWAYS_ZERO = (15, 16, 19, 20)   # "not implemented, always 0x0000"

# Writable public settings: number -> (name, unit, lowest and highest test value)
SETTINGS = {
    120: ("AutoCloseDelay", "s", 1, 600),
    121: ("PedestrianModeDistance", "cm", 10, 500),
    122: ("PassageLightDelay", "s", 1, 3600),
    123: ("BeaconPreWarningTime", "s", 0, 30),
}
SETTING_NAMES = {
    120: "AutoCloseDelay", 121: "PedestrianModeDistance", 122: "PassageLightDelay",
    123: "BeaconPreWarningTime", 124: "AutoRedirectEnabled", 125: "StopCount",
    126: "gatePassageMeters",
}

COIL_LABELS = {1: "Push Open", 2: "Push Close", 3: "Push Open Stop Close", 4: "Pedestrian",
               5: "(reserved)", 6: "(reserved)", 7: "(reserved)", 8: "(reserved)",
               9: "Autoclose"}

CLI_IDLE, CLI_BUSY, CLI_DONE = 0, 1, 2


# -- Output and bookkeeping ---------------------------------------------------

class Results:
    def __init__(self):
        self.passed = 0
        self.failed = []

    def ok(self, msg):
        self.passed += 1
        print(f"  PASS  {msg}")

    def fail(self, msg):
        self.failed.append(msg)
        print(f"  FAIL  {msg}")

    def check(self, condition, msg, detail=""):
        if condition:
            self.ok(msg)
        else:
            self.fail(f"{msg}  {detail}".rstrip())
        return condition


def info(msg):
    print(f"  info  {msg}")


def section(title):
    print(f"\n== {title} " + "=" * max(1, 60 - len(title)))


def pause():
    time.sleep(PAUSE)


def on_off(v):
    return "ON " if v else "off"


def call(res, what, fn):
    """Run one Modbus read; record a FAIL and return None when it raises."""
    try:
        value = fn()
        pause()
        return value
    except Exception as e:   # noqa: BLE001 - any transport or slave error is a failed check
        res.fail(f"{what}: {type(e).__name__}: {e}")
        pause()
        return None


def write(res, what, fn):
    """Run one Modbus write; record a FAIL and return False when it raises."""
    try:
        fn()
        pause()
        return True
    except Exception as e:   # noqa: BLE001
        res.fail(f"{what}: {type(e).__name__}: {e}")
        pause()
        return False


def expect_exception(res, what, code, fn):
    """The request must be answered with Modbus exception `code`."""
    try:
        fn()
        res.fail(f"{what}: accepted, expected exception {code:02d}")
    except minimalmodbus.SlaveReportedException as e:
        got = exception_code(e)
        res.check(got == code, f"{what} -> exception {code:02d}", f"(got: {e})")
    except Exception as e:   # noqa: BLE001
        res.fail(f"{what}: {type(e).__name__}: {e}")
    pause()


# -- Checks -------------------------------------------------------------------

def check_connection(inst, res, slave):
    section("Connection (FC04 100-103)")
    regs = call(res, "FC04 100-103", lambda: inst.read_registers(100, 4, functioncode=4))
    if regs is None:
        print("\n  No answer. Check the port, the RS485 wiring (A/B), the slave address and")
        print("  that the board runs firmware 0.9 or later (official numbering).")
        return False
    res.check(regs[1] == slave, f"[101] Modbus address = {regs[1]}", f"(expected {slave})")
    res.check(regs[2] > 0 or regs[3] >= 9, f"[102/103] firmware {regs[2]}.{regs[3]}",
              "(official numbering needs 0.9 or later)")
    return True


def check_peripherals(inst, res):
    section("FC04 board LEDs (1-22)")
    regs = call(res, "FC04 1-22", lambda: inst.read_registers(1, 22, functioncode=4))
    if regs is None:
        return None
    for n, v in enumerate(regs, start=1):
        print(f"        [{n:>2}] {PERIPH_LABELS[n]:<26} = {on_off(v)}")
    res.check(all(v in (0, 1) for v in regs), "every value is 0 or 1", str(regs))
    zeros = [n for n in ALWAYS_ZERO if regs[n - 1] != 0]
    res.check(not zeros, "15, 16, 19, 20 read 0 (not implemented)", f"(not 0: {zeros})")
    return regs


def check_discrete_inputs(inst, res):
    section("FC02 discrete inputs (1-22) = FC04 1-22")
    before = call(res, "FC04 1-22", lambda: inst.read_registers(1, 22, functioncode=4))
    bits = call(res, "FC02 1-22", lambda: inst.read_bits(1, 22, functioncode=2))
    after = call(res, "FC04 1-22", lambda: inst.read_registers(1, 22, functioncode=4))
    if None in (before, bits, after):
        return
    if before != after:
        info("an input changed during the check; run it again with the inputs at rest")
        return
    diff = [n for n in range(1, 23) if bits[n - 1] != before[n - 1]]
    res.check(not diff, "FC02 1-22 equals FC04 1-22", f"(differs at {diff})")


def check_identification(inst, res, slave):
    section("FC04 identification (100-111)")
    regs = call(res, "FC04 100-111", lambda: inst.read_registers(100, 12, functioncode=4))
    if regs is None:
        return
    info(f"[100] test register (inverted) = 0x{regs[0]:04X}")
    res.check(regs[1] == slave, f"[101] Modbus address = {regs[1]}")
    info(f"[102/103] firmware {regs[2]}.{regs[3]}")
    res.check(all(v == 0 for v in regs[4:]), "[104-111] reserved, read 0", str(regs[4:]))


def read_settings(inst, res, fc):
    return call(res, f"FC{fc:02d} 120-126", lambda: inst.read_registers(120, 7, functioncode=fc))


def check_settings_read(inst, res):
    section("FC04 public settings (120-126)")
    regs = read_settings(inst, res, 4)
    if regs is None:
        return None
    units = {120: "s", 121: "cm", 122: "s", 123: "s", 125: "cycles"}
    for i, v in enumerate(regs):
        n = 120 + i
        extra = f"  ({v / 100:.2f} m)" if n == 121 else ""
        print(f"        [{n}] {SETTING_NAMES[n]:<24} = {v} {units.get(n, '')}{extra}")
    res.check(regs[4] == 0 and regs[6] == 0, "[124] and [126] read 0 (future implementation)")
    return regs


def check_holding(inst, res):
    section("FC03 holding registers: read back what FC06 writes")
    ident4 = call(res, "FC04 101-103", lambda: inst.read_registers(101, 3, functioncode=4))
    ident3 = call(res, "FC03 100-111", lambda: inst.read_registers(100, 12, functioncode=3))
    if ident3 is not None and ident4 is not None:
        res.check(ident3[1:4] == ident4, "FC03 101-103 equals FC04 101-103", f"({ident3[1:4]} vs {ident4})")
        res.check(all(v == 0 for v in ident3[4:]), "FC03 104-111 read 0")
    s4 = read_settings(inst, res, 4)
    s3 = read_settings(inst, res, 3)
    if s3 is not None and s4 is not None:
        res.check(s3 == s4, "FC03 120-126 equals FC04 120-126", f"({s3} vs {s4})")
    call(res, "FC03 200 (last CLI code)", lambda: inst.read_registers(200, 1, functioncode=3))


def check_test_register(inst, res):
    section("FC06 test register 100 (FC03 raw, FC04 inverted)")
    for val in (0x0000, 0xFFFF, 0x1234, 0xAAAA, 0x5555, 0x0001, 0x8000):
        if not write(res, f"FC06 100 = 0x{val:04X}", lambda: inst.write_register(100, val, functioncode=6)):
            continue
        raw = call(res, "FC03 100", lambda: inst.read_registers(100, 1, functioncode=3))
        inv = call(res, "FC04 100", lambda: inst.read_registers(100, 1, functioncode=4))
        if raw is None or inv is None:
            continue
        res.check(raw[0] == val and inv[0] == (~val & 0xFFFF),
                  f"write 0x{val:04X}: FC03 0x{raw[0]:04X}, FC04 0x{inv[0]:04X}")


def check_settings_write(inst, res):
    section("FC06 public settings: write, read back with FC04 and FC03, restore")
    initial = read_settings(inst, res, 4)
    if initial is None:
        return
    for n, (name, unit, lo, hi) in SETTINGS.items():
        orig = initial[n - 120]
        test = random.randint(lo, hi)
        if test == orig:
            test = lo if orig != lo else hi
        print(f"\n        [{n}] {name}: {orig} {unit} -> {test} {unit}")
        if not write(res, f"FC06 {n} = {test}", lambda: inst.write_register(n, test, functioncode=6)):
            continue
        r4 = call(res, f"FC04 {n}", lambda: inst.read_registers(n, 1, functioncode=4))
        r3 = call(res, f"FC03 {n}", lambda: inst.read_registers(n, 1, functioncode=3))
        if r4 is not None and r3 is not None:
            res.check(r4[0] == test and r3[0] == test, f"[{n}] read back {test}",
                      f"(FC04 {r4[0]}, FC03 {r3[0]})")
        write(res, f"restore FC06 {n} = {orig}", lambda: inst.write_register(n, orig, functioncode=6))
        back = call(res, f"FC04 {n}", lambda: inst.read_registers(n, 1, functioncode=4))
        if back is not None:
            res.check(back[0] == orig, f"[{n}] restored {orig}", f"(reads {back[0]})")

    print()
    for n in (124, 126):
        write(res, f"FC06 {n} = 1 (write ignored)", lambda: inst.write_register(n, 1, functioncode=6))
        r = call(res, f"FC04 {n}", lambda: inst.read_registers(n, 1, functioncode=4))
        if r is not None:
            res.check(r[0] == 0, f"[{n}] write ignored, still reads 0")
    expect_exception(res, "FC06 125 StopCount (read only)", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.write_register(125, 1, functioncode=6))


def check_coils_read(inst, res):
    section("FC01 coils (1-9): momentary, always 0")
    bits = call(res, "FC01 1-9", lambda: inst.read_bits(1, 9, functioncode=1))
    if bits is not None:
        res.check(bits == [0] * 9, "coils 1-9 read 0", str(bits))


def check_cli(inst, res):
    section("CLI register 200 (output on the serial console)")
    write(res, "FC06 200 = 0 (start idle)", lambda: inst.write_register(200, 0, functioncode=6))
    st = call(res, "FC04 200", lambda: inst.read_registers(200, 1, functioncode=4))
    if st is not None:
        res.check(st[0] == CLI_IDLE, "status idle (0)", f"(reads {st[0]})")

    for code, name in ((1, "status"), (8, "version")):
        if not write(res, f"FC06 200 = {code} ({name})", lambda: inst.write_register(200, code, functioncode=6)):
            continue
        status, deadline = None, time.time() + CLI_DONE_TIMEOUT
        while time.time() < deadline:
            r = call(res, "FC04 200", lambda: inst.read_registers(200, 1, functioncode=4))
            if r is not None and r[0] == CLI_DONE:
                status = r[0]
                break
        res.check(status == CLI_DONE, f"CLI {code} ({name}) done (2)")
        last = call(res, "FC03 200", lambda: inst.read_registers(200, 1, functioncode=3))
        if last is not None:
            res.check(last[0] == code, f"FC03 200 reads the last code {code}", f"(reads {last[0]})")
        write(res, "FC06 200 = 0 (acknowledge)", lambda: inst.write_register(200, 0, functioncode=6))
        st = call(res, "FC04 200", lambda: inst.read_registers(200, 1, functioncode=4))
        if st is not None:
            res.check(st[0] == CLI_IDLE, "back to idle (0) after acknowledge", f"(reads {st[0]})")


def check_exceptions(inst, res):
    section("Exception answers")
    expect_exception(res, "FC04 register 999", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_registers(999, 1, functioncode=4))
    expect_exception(res, "FC04 1-23 (23 does not exist)", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_registers(1, 23, functioncode=4))
    expect_exception(res, "FC04 100-126 (gap 112-119)", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_registers(100, 27, functioncode=4))
    expect_exception(res, "FC03 register 1 (input only)", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_registers(1, 1, functioncode=3))
    expect_exception(res, "FC06 register 1 (not writable)", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.write_register(1, 1, functioncode=6))
    expect_exception(res, "FC06 200 = 99 (unknown CLI code)", EXC_ILLEGAL_VALUE,
                     lambda: inst.write_register(200, 99, functioncode=6))
    expect_exception(res, "FC01 coil 10", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_bits(10, 1, functioncode=1))
    expect_exception(res, "FC02 1-23", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.read_bits(1, 23, functioncode=2))
    expect_exception(res, "FC05 coil 10", EXC_ILLEGAL_ADDRESS,
                     lambda: inst.write_bit(10, 1, functioncode=5))
    expect_exception(res, "FC16 write multiple (not supported)", EXC_ILLEGAL_FUNCTION,
                     lambda: inst.write_registers(120, [1]))


def check_coil_commands(inst, res):
    section("FC05 gate commands (THE GATE MOVES)")
    answer = input("  The gate will open, stop, close and stop. Clear the gate area. Proceed? (y/N): ")
    if answer.strip().lower() != "y":
        info("skipped")
        return
    steps = [(3, "stop (OSC)"), (1, "Push Open"), (3, "stop (OSC)"), (2, "Push Close"),
             (3, "stop (OSC)"), (4, "Pedestrian"), (3, "stop (OSC)"), (5, "reserved coil 5: no action")]
    for coil, what in steps:
        if not write(res, f"FC05 coil {coil} {what}", lambda: inst.write_bit(coil, 1, functioncode=5)):
            continue
        time.sleep(2.0)
        r = call(res, "FC04 22", lambda: inst.read_registers(22, 1, functioncode=4))
        if r is not None:
            info(f"coil {coil} {what:<28} -> On/off (22) = {on_off(r[0])}")
    write(res, "FC05 coil 1 = 0x0000 (no action)", lambda: inst.write_bit(1, 0, functioncode=5))
    info("coil 9 (Autoclose) is not sent: it has no effect until the gate fix is in (point 1 in fixes.md)")


def continuous_poll(inst):
    section("Continuous poll, board LEDs 1-22 (Ctrl+C stops)")
    hdrs = ["OPN", "CLS", "OSC", "PED", "LSO", "LSC", "PHO", "SFT", "AUT", "LRN", "AXI",
            "PLT", "FLH", "LCK", "AXO", "PLS", "DIR", "TQL", "TRQ", "SPD", "DRQ", "ONF"]
    print("  " + " ".join(f"{h:>3}" for h in hdrs))
    try:
        while True:
            try:
                regs = inst.read_registers(1, 22, functioncode=4)
                print("  " + " ".join(f"{on_off(v)}" for v in regs))
            except Exception as e:   # noqa: BLE001
                print(f"  error: {e}")
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n  stopped")


def main():
    ap = argparse.ArgumentParser(description="Kopal gate controller Modbus test")
    ap.add_argument("port", help="serial port, e.g. COM3 or /dev/cu.usbserial-XXXX")
    ap.add_argument("--slave", type=int, default=kopal_mb.SLAVE_ID)
    ap.add_argument("--coils", action="store_true", help="send FC05 gate commands (the gate moves)")
    ap.add_argument("--poll", action="store_true", help="continuous LED poll at the end")
    args = ap.parse_args()

    print(f"Kopal Modbus test  port {args.port}  slave {args.slave}  9600 8N1  official numbering")
    try:
        inst = kopal_mb.open_instrument(args.port, args.slave)
    except Exception as e:   # noqa: BLE001
        print(f"Cannot open {args.port}: {e}")
        return 1

    res = Results()
    if not check_connection(inst, res, args.slave):
        return 1
    check_peripherals(inst, res)
    check_discrete_inputs(inst, res)
    check_identification(inst, res, args.slave)
    check_settings_read(inst, res)
    check_holding(inst, res)
    check_test_register(inst, res)
    check_settings_write(inst, res)
    check_coils_read(inst, res)
    check_cli(inst, res)
    check_exceptions(inst, res)
    if args.coils:
        check_coil_commands(inst, res)

    section("Summary")
    print(f"  {res.passed} passed, {len(res.failed)} failed")
    for msg in res.failed:
        print(f"    FAIL  {msg}")

    if args.poll:
        continuous_poll(inst)
    inst.serial.close()
    return 0 if not res.failed else 1


if __name__ == "__main__":
    sys.exit(main())
