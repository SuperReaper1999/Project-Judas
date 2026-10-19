#!/usr/bin/env python3
"""Check an explicit current-reference set, leaving historical prose untouched.

Only local inline/reference links and GitHub-style Markdown heading anchors are
checked. This is not a Markdown renderer or a semantic/API/platform proof.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import unicodedata
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r'!?\[[^\]\n]*\]\((<[^>\n]+>|[^\s)]+)(?:\s+[\'\"][^\n)]*)?\)')
REFERENCE = re.compile(r'^\s{0,3}\[[^\]\n]+\]:\s*(<[^>\n]+>|\S+)')


def prose_lines(text: str):
    """Keep line numbers, excluding fenced examples and HTML comments."""
    fence = None
    comment = False
    for number, line in enumerate(text.splitlines(), 1):
        if comment:
            if '-->' not in line:
                continue
            line = line.split('-->', 1)[1]
            comment = False
        while '<!--' in line:
            before, after = line.split('<!--', 1)
            if '-->' in after:
                line = before + after.split('-->', 1)[1]
            else:
                line = before
                comment = True
                break
        marker = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if marker:
            token = marker.group(1)
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
            continue
        if fence is None:
            yield number, line


def heading_anchors(text: str) -> set[str]:
    anchors: set[str] = set()
    counts: dict[str, int] = {}
    for _, line in prose_lines(text):
        for identifier in re.findall(r'<(?:a|h[1-6])\b[^>]*\b(?:id|name)=[\'\"]([^\'\"]+)', line):
            anchors.add(identifier)
        heading = re.match(r'^\s{0,3}#{1,6}\s+(.+?)(?:\s+#+\s*)?$', line)
        if not heading:
            continue
        title = re.sub(r'<[^>]*>', '', heading.group(1)).lower()
        # GitHub removes punctuation, retains letters/numbers/marks, then
        # replaces spaces. Repeated headings receive an occurrence suffix.
        slug = ''.join(c for c in title if c in '_- ' or unicodedata.category(c)[0] in 'LNM')
        slug = slug.replace(' ', '-')
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        anchors.add(slug + (f'-{count}' if count else ''))
    return anchors


def local_links(text: str):
    for number, line in prose_lines(text):
        for match in LINK.finditer(line):
            yield number, match.group(1).strip('<>')
        reference = REFERENCE.match(line)
        if reference:
            yield number, reference.group(1).strip('<>')


def check(root: Path, documents: list[str], tracked: set[str] | None = None) -> dict:
    root = root.resolve()
    errors = []
    links = 0
    anchors_cache = {}
    if len(documents) != len(set(documents)):
        errors.append('duplicate document in current-reference inventory')
    for document in documents:
        source = root / document
        if not source.is_file():
            errors.append(f'{document}: missing current document')
            continue
        if tracked is not None and document not in tracked:
            errors.append(f'{document}: current document is not tracked')
        for number, target in local_links(source.read_text(encoding='utf-8')):
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc:
                continue
            links += 1
            path_text = unquote(parsed.path)
            # Repository-relative paths, not machine-specific filesystem links.
            if path_text.startswith('/'):
                errors.append(f'{document}:{number}: absolute local link {target}')
                continue
            destination = (source.parent / path_text).resolve() if path_text else source.resolve()
            try:
                relative = destination.relative_to(root).as_posix()
            except ValueError:
                errors.append(f'{document}:{number}: link escapes repository: {target}')
                continue
            if not destination.exists():
                errors.append(f'{document}:{number}: missing destination {target}')
                continue
            if tracked is not None:
                included = relative in tracked or (destination.is_dir() and any(
                    p.startswith(relative.rstrip('/') + '/') for p in tracked))
                if not included:
                    errors.append(f'{document}:{number}: untracked destination {target}')
            if parsed.fragment and destination.suffix.lower() == '.md':
                if destination not in anchors_cache:
                    anchors_cache[destination] = heading_anchors(destination.read_text(encoding='utf-8'))
                if unquote(parsed.fragment) not in anchors_cache[destination]:
                    errors.append(f'{document}:{number}: missing heading {target}')
    return {'documents': len(documents), 'local_links': links,
            'tracked_destinations_required': tracked is not None,
            'errors': errors, 'pass': not errors}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'tools/verification/current-docs.json')
    parser.add_argument('--require-tracked', action='store_true', help='require a complete Git checkout; used by CI')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    documents = manifest.get('documents')
    if manifest.get('schema') != 1 or not isinstance(documents, list) or not all(
            isinstance(p, str) and p.endswith('.md') and not Path(p).is_absolute() and '..' not in Path(p).parts
            for p in documents):
        parser.error('invalid current-reference inventory')
    tracked = None
    if args.require_tracked:
        tracked = set(subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0'))
    result = check(ROOT, documents, tracked)
    print(json.dumps(result, indent=2))
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
