"""Evidence-only token equivalence for the recorded, bounded formatting change.

This is not an engine API contract: future functional edits need not retain these
historical hashes. --current additionally verifies the checked-out source is the
recorded output, when validating this particular candidate.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re

TOKEN = re.compile(
    r'R"(?P<delim>[^\s()\\]{0,16})\(.*?\)(?P=delim)"|"(?:\\.|[^"\\])*"|'
    r"'(?:\\.|[^'\\])*'|//[^\n]*|/\*.*?\*/|[A-Za-z_]\w*|"
    r'(?:\d+(?:\.\d*)?|\.\d+)(?:[eEpP][+-]?\d+)?[A-Za-z_0-9]*|'
    r'::|->\*|->|<<=|>>=|==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|'
    r'%=|&=|\|=|\^=|<<|>>|\.\.\.|[^\s]', re.S)


def tokens(text):
    return [match.group() for match in TOKEN.finditer(text)
            if not match.group().startswith(('//', '/*'))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--current', type=Path)
    args = parser.parse_args()
    evidence = Path(__file__).resolve().parents[1]
    before = (evidence / 'formatting/PhysicsWorld.before.cpp').read_text()
    after = (evidence / 'formatting/PhysicsWorld.after.cpp').read_text()
    expected, actual = tokens(before), tokens(after)
    if expected != actual:
        raise SystemExit('Formatting changed C++ tokens')
    controls = {}
    for name, mutated in [
        ('CCD iteration budget', after.replace('iteration < advanceIterations',
                                             'iteration <= advanceIterations', 1)),
        ('response failure guard', after.replace('if (rows.empty())',
                                                'if (!rows.empty())', 1)),
    ]:
        if mutated == after or tokens(mutated) == expected:
            raise SystemExit('Negative control was not detected: ' + name)
        controls[name] = 'detected'
    if args.current and args.current.read_bytes() != (
            evidence / 'formatting/PhysicsWorld.after.cpp').read_bytes():
        raise SystemExit('Current source differs from recorded formatting output')
    print(json.dumps({
        'tokens': len(actual),
        'token_sha256': hashlib.sha256(json.dumps(
            actual, separators=(',', ':')).encode()).hexdigest(),
        'negative_controls': controls,
        'current_source_verified': bool(args.current),
    }, indent=2))


if __name__ == '__main__':
    main()
