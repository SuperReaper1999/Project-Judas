#!/usr/bin/env python3
"""Capture installed Debian/Ubuntu Vulkan compiler source identities and notices."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess


def capture(output):
    output.mkdir()
    components = {}
    for component, package in (("glslang", "glslang-dev"), ("spirv-tools", "spirv-tools")):
        query = subprocess.check_output(
            ["dpkg-query", "-W", "-f=${db:Status-Status}\t${source:Package}\t${source:Version}\t${Version}", package],
            text=True,
        )
        status, source, version, binary_version = query.split("\t")
        if status != "installed":
            raise ValueError(f"Vulkan compiler package is not installed: {package}")
        copyright_file = Path("/usr/share/doc") / package / "copyright"
        licence = component + "-copyright.txt"
        shutil.copyfile(copyright_file, output / licence)
        for name in set(re.findall(r"/usr/share/common-licenses/([A-Za-z0-9.\-+]+)", copyright_file.read_text())):
            shutil.copyfile(Path("/usr/share/common-licenses") / name, output / name)
        components[component] = dict(version=version, source=source, licence=licence,
                                     binary_package=package, binary_version=binary_version)
    distro = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines()
                  if line and not line.startswith("#"))
    provenance = dict(schema=1, distribution=distro["PRETTY_NAME"].strip('"'), components=components)
    (output / "PROVENANCE.json").write_text(json.dumps(provenance, sort_keys=True, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    capture(parser.parse_args().output)
