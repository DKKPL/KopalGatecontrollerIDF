"""Shared Modbus helpers for the Kopal gate controller test scripts.

The firmware uses the official Modbus numbering: register or coil N is sent as
wire address N - 1 (register 1 Push Open = wire 0, coil 9 Autoclose = wire 8).
`Instrument` takes the numbers from the mapping and subtracts 1 before sending,
so the scripts can use the mapping numbers everywhere.
"""
import minimalmodbus

SLAVE_ID = 55
BAUD = 9600
TIMEOUT = 3.0   # seconds; an FC06 settings write saves to flash before it answers

# Modbus exception codes the firmware sends
EXC_ILLEGAL_FUNCTION = 1   # function code not supported
EXC_ILLEGAL_ADDRESS = 2    # register or coil not in the mapping, or read only
EXC_ILLEGAL_VALUE = 3      # value not allowed (e.g. CLI code above 9)
EXC_DEVICE_FAILURE = 4     # CLI busy


class Instrument(minimalmodbus.Instrument):
    """minimalmodbus.Instrument that takes mapping numbers instead of wire addresses."""

    def read_registers(self, number, *args, **kwargs):
        return super().read_registers(number - 1, *args, **kwargs)

    def read_register(self, number, *args, **kwargs):
        return super().read_register(number - 1, *args, **kwargs)

    def write_register(self, number, *args, **kwargs):
        return super().write_register(number - 1, *args, **kwargs)

    def write_registers(self, number, *args, **kwargs):
        return super().write_registers(number - 1, *args, **kwargs)

    def read_bits(self, number, *args, **kwargs):
        return super().read_bits(number - 1, *args, **kwargs)

    def read_bit(self, number, *args, **kwargs):
        return super().read_bit(number - 1, *args, **kwargs)

    def write_bit(self, number, *args, **kwargs):
        return super().write_bit(number - 1, *args, **kwargs)


def open_instrument(port, slave=SLAVE_ID):
    """Open the serial port with the Kopal settings: 9600 8N1, RTU."""
    inst = Instrument(port, slave)
    inst.serial.baudrate = BAUD
    inst.serial.timeout = TIMEOUT
    inst.mode = minimalmodbus.MODE_RTU
    inst.clear_buffers_before_each_transaction = True
    return inst


def exception_code(error):
    """Modbus exception code from a minimalmodbus error, or None if it was not one."""
    text = str(error)
    for code, words in ((1, "illegal function"), (2, "illegal data address"),
                        (3, "illegal data value"), (4, "device failure")):
        if words in text:
            return code
    return None

