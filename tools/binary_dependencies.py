"""Small, bounded ELF64/PE import reader for distribution diagnostics.

Reads metadata, never executes an untrusted binary. This is dependency/ABI evidence,
not a proof that a driver, Android device or Windows API is compatible.
"""

import struct


class Reader:
    def __init__(self, data):
        self.data = data

    def block(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise ValueError("Truncated or out-of-range binary metadata")
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.block(offset, struct.calcsize(fmt)))

    def string(self, offset, end):
        if not 0 <= offset < end <= len(self.data):
            raise ValueError("Invalid binary string offset")
        stop = self.data.find(b"\0", offset, min(end, offset + 65536))
        if stop < 0:
            raise ValueError("Unterminated binary string")
        return self.data[offset:stop].decode("utf-8", errors="strict")


def elf(data):
    r = Reader(data)
    if r.block(0, 6) != b"\x7fELF\x02\x01":
        raise ValueError("Only little-endian ELF64 is currently supported")
    machine = r.unpack("<H", 18)[0]
    phoff, shoff = r.unpack("<QQ", 32)
    phsize, phcount, shsize, shcount = r.unpack("<HHHH", 54)
    if not shcount or shcount > 65535 or shsize < 64 or phsize < 56:
        raise ValueError("Invalid/unsupported ELF section or program table")
    sections = [r.unpack("<IIQQQQIIQQ", shoff + i * shsize) for i in range(shcount)]
    programs = [r.unpack("<IIQQQQQQ", phoff + i * phsize) for i in range(phcount)]

    def strings(section):
        link = section[6]
        if link >= len(sections):
            raise ValueError("Invalid ELF linked string section")
        table = sections[link]
        r.block(table[4], table[5])
        return lambda index: r.string(table[4] + index, table[4] + table[5])

    needed, paths, versions = set(), [], {}
    for section in sections:
        offset, size = section[4], section[5]
        if section[1] == 6:  # SHT_DYNAMIC
            r.block(offset, size)
            if size % 16:
                raise ValueError("Invalid ELF dynamic section size")
            name = strings(section)
            for entry in range(offset, offset + size, 16):
                tag, value = r.unpack("<qQ", entry)
                if tag == 0:
                    break
                if tag == 1:
                    needed.add(name(value))
                if tag in (15, 29):
                    paths.append({"kind": "rpath" if tag == 15 else "runpath", "value": name(value)})
        if section[1] == 0x6ffffffe:  # SHT_GNU_verneed, not exported definitions
            r.block(offset, size)
            name, current, seen = strings(section), offset, set()
            while True:
                if current in seen or not offset <= current <= offset + size - 16:
                    raise ValueError("Invalid ELF version requirement chain")
                seen.add(current)
                version, count, provider, auxiliary, following = r.unpack("<HHIII", current)
                if version != 1 or count > 65535:
                    raise ValueError("Unsupported ELF version requirement")
                required = versions.setdefault(name(provider), set())
                at, visited = current + auxiliary, set()
                for index in range(count):
                    if at in visited or not offset <= at <= offset + size - 16:
                        raise ValueError("Invalid ELF auxiliary version chain")
                    visited.add(at)
                    _, _, _, label, next_aux = r.unpack("<IHHII", at)
                    required.add(name(label))
                    if index + 1 < count and not next_aux:
                        raise ValueError("Truncated ELF auxiliary version chain")
                    at += next_aux
                if not following:
                    break
                current += following
    interpreter = None
    for program in programs:
        if program[0] == 3:  # PT_INTERP
            interpreter = r.string(program[2], program[2] + program[5])
    return {"format": "ELF64", "machine": machine, "needed": sorted(needed),
            "version_requirements": {name: sorted(labels) for name, labels in sorted(versions.items())},
            "search_paths": paths, "interpreter": interpreter,
            "load_alignments": [p[7] for p in programs if p[0] == 1]}


def pe(data):
    r = Reader(data)
    if r.block(0, 2) != b"MZ":
        raise ValueError("Not a PE executable")
    base = r.unpack("<I", 0x3c)[0]
    if r.block(base, 4) != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine, count, _, _, _, size, _ = r.unpack("<HHIIIHH", base + 4)
    optional = base + 24
    magic = r.unpack("<H", optional)[0]
    if magic not in (0x20b, 0x10b) or not 0 < count <= 96:
        raise ValueError("Unsupported PE optional header or section count")
    directories = optional + (112 if magic == 0x20b else 96)
    number = r.unpack("<I", directories - 4)[0]
    r.block(optional, size)
    if directories + min(number, 16) * 8 > optional + size:
        raise ValueError("Truncated PE data directories")
    sections = [r.unpack("<8sIIIIIIHHI", optional + size + i * 40) for i in range(count)]
    headers = r.unpack("<I", optional + 60)[0]

    def position(rva, length=1):
        if rva < headers:
            r.block(rva, length)
            return rva
        for section in sections:
            _, virtual_size, address, raw_size, raw, *_ = section
            if address <= rva and rva + length <= address + raw_size:
                result = raw + rva - address
                r.block(result, length)
                return result
        raise ValueError("PE RVA has no backed file section")

    imports = set()
    for index, stride, name_index in ((1, 20, 3), (13, 32, 1)):
        if index >= number:
            continue
        address, length = r.unpack("<II", directories + index * 8)
        if not address:
            continue
        if length < stride or length > 16 * 1024 * 1024:
            raise ValueError("Invalid PE import directory size")
        terminated = False
        for offset in range(0, length - stride + 1, stride):
            values = r.unpack("<" + "I" * (stride // 4), position(address + offset, stride))
            if not any(values):
                terminated = True
                break
            if index == 13 and not values[0] & 1:
                raise ValueError("Legacy VA delay imports are not supported")
            at = position(values[name_index])
            name = r.string(at, len(data))
            if "/" in name or "\\" in name or not name.lower().endswith(".dll"):
                raise ValueError("Unsafe or invalid PE import name")
            imports.add(name.lower())
        if not terminated:
            raise ValueError("Unterminated PE import directory")
    return {"format": "PE32+" if magic == 0x20b else "PE32", "machine": machine,
            "needed": sorted(imports)}


def inspect(data):
    if data.startswith(b"\x7fELF"):
        return elf(data)
    if data.startswith(b"MZ"):
        return pe(data)
    raise ValueError("Unrecognized executable format")
