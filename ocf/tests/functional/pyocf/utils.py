#
# Copyright(c) 2019-2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from ctypes import string_at


def print_buffer(
    buf, length, offset=0, width=16, ignore=0, stop_after_count_ignored=0, print_fcn=print,
):
    end = int(offset) + int(length)
    offset = int(offset)
    ignored_lines = 0
    buf = string_at(buf, length)
    whole_buffer_ignored = True
    stop_after_count_ignored = int(stop_after_count_ignored / width)

    for addr in range(offset, end, width):
        cur_line = buf[addr : min(end, addr + width)]
        byteline = ""
        asciiline = ""
        if not any(x != ignore for x in cur_line):
            if stop_after_count_ignored and ignored_lines > stop_after_count_ignored:
                print_fcn(
                    "<{} bytes of '0x{:02X}' encountered, stopping>".format(
                        stop_after_count_ignored * width, ignore
                    )
                )
                return
            ignored_lines += 1
            continue

        if ignored_lines:
            print_fcn("<{} of '0x{:02X}' bytes omitted>".format(ignored_lines * width, ignore))
            ignored_lines = 0

        for byte in cur_line:
            byte = int(byte)
            byteline += "{:02X} ".format(byte)
            if 31 < byte < 126:
                char = chr(byte)
            else:
                char = "."
            asciiline += char

        print_fcn("0x{:08X}\t{}\t{}".format(addr, byteline, asciiline))
        whole_buffer_ignored = False

    if whole_buffer_ignored:
        print_fcn("<whole buffer ignored>")
    elif ignored_lines:
        print_fcn("<'0x{:02X}' until end>".format(ignore))


class Size:
    _KiB = 1024
    _MiB = _KiB * 1024
    _GiB = _MiB * 1024
    _TiB = _GiB * 1024
    _SECTOR_SIZE = 512
    _PAGE_SIZE = 4096

    _unit_mapping = {
        "B": 1,
        "kiB": _KiB,
        "MiB": _MiB,
        "GiB": _GiB,
        "TiB": _TiB,
    }

    def __init__(self, b: int, sector_aligned: bool = False):
        if sector_aligned:
            self.bytes = int(((b + self._SECTOR_SIZE - 1) // self._SECTOR_SIZE) * self._SECTOR_SIZE)
        else:
            self.bytes = int(b)

    @classmethod
    def from_string(cls, string):
        string = string.strip()
        number, unit = string.split(" ")
        number = float(number)
        unit = cls._unit_mapping[unit]

        return cls(int(number * unit))

    def __lt__(self, other):
        return int(self) < int(other)

    def __le__(self, other):
        return int(self) <= int(other)

    def __eq__(self, other):
        return int(self) == int(other)

    def __ne__(self, other):
        return int(self) != int(other)

    def __gt__(self, other):
        return int(self) > int(other)

    def __ge__(self, other):
        return int(self) >= int(other)

    def __int__(self):
        return self.bytes

    def __index__(self):
        return self.bytes

    @classmethod
    def from_B(cls, value, sector_aligned=False):
        return cls(value, sector_aligned)

    @classmethod
    def from_KiB(cls, value, sector_aligned=False):
        return cls(value * cls._KiB, sector_aligned)

    @classmethod
    def from_MiB(cls, value, sector_aligned=False):
        return cls(value * cls._MiB, sector_aligned)

    @classmethod
    def from_GiB(cls, value, sector_aligned=False):
        return cls(value * cls._GiB, sector_aligned)

    @classmethod
    def from_TiB(cls, value, sector_aligned=False):
        return cls(value * cls._TiB, sector_aligned)

    @classmethod
    def from_sector(cls, value):
        return cls(value * cls._SECTOR_SIZE)

    @classmethod
    def from_page(cls, value):
        return cls(value * cls._PAGE_SIZE)

    @property
    def B(self):
        return self.bytes

    @property
    def KiB(self):
        return self.bytes / self._KiB

    @property
    def MiB(self):
        return self.bytes / self._MiB

    @property
    def GiB(self):
        return self.bytes / self._GiB

    @property
    def TiB(self):
        return self.bytes / self._TiB

    @property
    def sectors(self):
        return self.bytes // self._SECTOR_SIZE

    @property
    def blocks_4k(self):
        return self.bytes // 4096

    def __str__(self):
        if self.bytes < self._KiB:
            return "{} B".format(self.B)
        elif self.bytes < self._MiB:
            return "{} KiB".format(self.KiB)
        elif self.bytes < self._GiB:
            return "{} MiB".format(self.MiB)
        elif self.bytes < self._TiB:
            return "{} GiB".format(self.GiB)
        else:
            return "{} TiB".format(self.TiB)

    def __repr__(self):
        return f"Size({self.bytes})"

    def __eq__(self, other):
        return self.bytes == other.bytes

    def __add__(self, other):
        return Size(self.bytes + other.bytes)

    def __sub__(self, other):
        return Size(self.bytes - other.bytes)

    def __mul__(self, other):
        return Size(self.bytes * int(other))

    def __truediv__(self, other):
        return Size(self.bytes / int(other))

    def __floordiv__(self, other):
        return Size(self.bytes // int(other))

    def __rmul__(self, other):
        return Size(self.bytes * int(other))

    def __rtruediv__(self, other):
        return Size(int(other) / self.bytes)

    def __rfloordiv__(self, other):
        return Size(int(other) // self.bytes)

    def __iadd__(self, other):
        self.bytes += other.bytes

        return self

    def __isub__(self, other):
        self.bytes -= other.bytes

        return self

    def __imul__(self, other):
        self.bytes *= int(other)

        return self

    def __itruediv__(self, other):
        self.bytes /= int(other)

        return self

    def __ifloordir__(self, other):
        self.bytes //= int(other)

        return self


def print_structure(struct, indent=0):
    print(struct)
    for field, field_type in struct._fields_:
        value = getattr(struct, field)
        if hasattr(value, "_fields_"):
            print("{}{: <20} :".format("   " * indent, field))
            print_structure(value, indent=indent + 1)
            continue

        print("{}{: <20} : {}".format("   " * indent, field, value))


def struct_to_dict(struct):
    d = {}
    for field, field_type in struct._fields_:
        value = getattr(struct, field)
        if hasattr(value, "_fields_"):
            d[field] = struct_to_dict(value)
            continue
        d[field] = value

    return d
