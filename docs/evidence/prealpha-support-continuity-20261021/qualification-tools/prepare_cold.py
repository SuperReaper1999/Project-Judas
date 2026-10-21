#!/usr/bin/env python3
"""Prepare a fresh-output copy of the existing M70/M77 cold harness; no runs."""
import argparse
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--character-binary', type=Path,
                        help='Fresh private M70 driver; normal build remains the source of flags/libraries')
    args = parser.parse_args()
    args.output, args.build = args.output.resolve(), args.build.resolve()
    if args.output.exists():
        raise RuntimeError('fresh output required')
    source = ROOT / '.cache/contact-redesign-20261021/cold/validate.py'
    text = source.read_text()
    marker = "HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2];BUILD=ROOT/'build'"
    if text.count(marker) != 1:
        raise RuntimeError('original harness wiring changed; inspect manually')
    replacement = f'HERE=Path(__file__).resolve().parent;ROOT=Path({str(ROOT)!r});BUILD=Path({str(args.build)!r})'
    copied_text = text.replace(marker, replacement)
    if args.character_binary:
        args.character_binary = args.character_binary.resolve()
        character_marker = "archive=BUILD/'libjudas_engine.a';character=BUILD/'judas_character_lab_tests'"
        if copied_text.count(character_marker) != 1:
            raise RuntimeError('original character binary wiring changed; inspect manually')
        copied_text = copied_text.replace(character_marker,
            f"archive=BUILD/'libjudas_engine.a';character=Path({str(args.character_binary)!r})")
    args.output.mkdir(parents=True)
    copied = args.output / 'validate.py'
    copied.write_text(copied_text)
    (args.output / 'HARNESS_PROVENANCE.json').write_text(json.dumps({
        'original': str(source), 'original_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'copied_sha256': hashlib.sha256(copied.read_bytes()).hexdigest(),
        'only_change': 'output-root/repository/build/optional explicit character binary wiring; observer, fixtures, controller, settings and acceptance unchanged',
        'explicit_character_binary': str(args.character_binary) if args.character_binary else None,
        'commands_not_run': [['python3', str(copied), 'prepare'], ['python3', str(copied), 'run']]}, indent=2) + '\n')
    print(f'Prepared only. Next: python3 {copied} prepare; then python3 {copied} run')


if __name__ == '__main__':
    main()
