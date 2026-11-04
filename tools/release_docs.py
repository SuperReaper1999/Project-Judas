#!/usr/bin/env python3
"""Refresh or verify current documentation summaries from authoritative records.

Source identity comes from engine-version.json. Published identity comes from
docs/RELEASE_STATUS.json and may legitimately describe an earlier version.
Historical paragraphs and evidence are never regenerated.
"""
import argparse
import json
from pathlib import Path
import re
import sys

from release_manifest import compare, semver

ROOT = Path(__file__).resolve().parents[1]
BASE = 'https://github.com/SuperReaper1999/Project-Judas'
START = '<!-- judas-release-status:start -->'
END = '<!-- judas-release-status:end -->'
PAGES = (
    'README.md',
    'docs/README.md',
    'docs/VERSIONING.md',
    'docs/PREALPHA.md',
    'docs/ALPHA_SUPPORT_MATRIX.md',
    'docs/RELEASING.md',
    'docs/DISTRIBUTION.md',
    'docs/WINDOWS.md',
    'docs/ANDROID.md',
)


def read_records(root):
    source = json.loads((root / 'engine-version.json').read_text(encoding='utf-8'))
    record = json.loads((root / 'docs/RELEASE_STATUS.json').read_text(encoding='utf-8'))
    if source.get('schema') != 1 or record.get('schema') != 1:
        raise ValueError('unsupported source or publication record schema')
    semver(source['version'])
    if not isinstance(source.get('release_name'), str) or not source['release_name']:
        raise ValueError('missing source release name')
    published = record['publication']
    semver(published['version'])
    if published.get('channel') != 'Alpha':
        raise ValueError('this publication record describes the Alpha feed')
    if published.get('tag') != 'v' + published['version']:
        raise ValueError('publication tag does not match its version')
    if not re.fullmatch(r'[0-9a-f]{40}', published.get('source_revision', '')):
        raise ValueError('publication requires an exact source revision')
    if not re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', published.get('published_at', '')):
        raise ValueError('publication requires its UTC publication timestamp')
    if compare(published['version'], source['version']) > 0:
        raise ValueError('published version is newer than this source identity')
    for field in ('preparation_run', 'publication_run'):
        if type(published.get(field)) is not int or published[field] <= 0:
            raise ValueError('publication requires positive workflow run IDs')
    for version in {source['version'], published['version']}:
        notes = root / 'docs' / ('ALPHA_V' + version.split('-')[0].replace('.', '_') + '_NOTES.md')
        if not notes.is_file():
            raise ValueError('missing versioned release notes: ' + str(notes.relative_to(root)))
    return source, published


def summary(root, document):
    source, published = read_records(root)
    prefix = 'docs/' if document == 'README.md' else ''
    notes = prefix + 'ALPHA_V' + source['version'].split('-')[0].replace('.', '_') + '_NOTES.md'
    lines = [
        START,
        f"Source identity is **{source['release_name']}** (`{source['version']}`).",
        f"[Source notes]({notes}) · [Published packages]({BASE}/releases) ·",
        f"[Support boundaries]({prefix}ALPHA_SUPPORT_MATRIX.md).",
    ]
    if document == 'docs/RELEASING.md':
        tag = published['tag']
        lines += [
            '',
            f"Verified publication on **{published['published_at'][:10]}** is",
            f"[`{tag}`]({BASE}/releases/tag/{tag}) from",
            f"`{published['source_revision']}`.",
            f"[Linux/Windows qualification]({BASE}/actions/runs/{published['preparation_run']})",
            f"and [protected publication]({BASE}/actions/runs/{published['publication_run']}) passed.",
            'The published packages and Alpha feed were verified. Native Windows CI',
            'is functional evidence. Earlier Windows hardware and Android phone',
            'acceptance keep their recorded scopes.',
        ]
    lines += [
        '',
        'Source identity and publication are separate. The',
        f"[publication record]({prefix}RELEASE_STATUS.json) is a dated receipt.",
        'Check the release page for subsequently published packages.',
        END,
    ]
    return '\n'.join(lines)



def replace_block(text, start, end, value, document):
    if text.count(start) != 1 or text.count(end) != 1:
        raise ValueError(document + ': requires one complete managed block')
    begin, finish = text.index(start), text.index(end)
    if finish < begin:
        raise ValueError(document + ': reversed managed markers')
    return text[:begin] + value + text[finish + len(end):]


def findings_table(root):
    register = json.loads((root / 'docs/PREALPHA_FINDINGS.json').read_text(encoding='utf-8'))
    rows = register['findings']
    if not rows or len({row['id'] for row in rows}) != len(rows):
        raise ValueError('finding IDs must be nonempty and unique')
    lines = ['<!-- judas-findings:start -->',
             '| ID | Status | Current scope / disposition |',
             '| --- | --- | --- |']
    for row in rows:
        values = [row['id'], row['status'], row['disposition']]
        if any(not isinstance(value, str) or '\n' in value for value in values):
            raise ValueError('finding table requires single-line text')
        lines.append('| ' + ' | '.join(value.replace('|', r'\|') for value in values) + ' |')
    lines.append('<!-- judas-findings:end -->')
    return '\n'.join(lines)


def refresh(root, write=False):
    # Validate all pages before changing any.
    replacements = []
    for document in PAGES:
        path = root / document
        text = path.read_text(encoding='utf-8')
        expected = replace_block(text, START, END, summary(root, document), document)
        if expected != text:
            replacements.append((document, path, expected))
    document = 'docs/PREALPHA_FINDINGS.md'
    path = root / document
    text = path.read_text(encoding='utf-8')
    expected = replace_block(text, '<!-- judas-findings:start -->',
                             '<!-- judas-findings:end -->', findings_table(root), document)
    if expected != text:
        replacements.append((document, path, expected))
    if write:
        for _, path, expected in replacements:
            path.write_text(expected, encoding='utf-8')
    return [document for document, _, _ in replacements]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--write', action='store_true', help='refresh managed summaries')
    args = parser.parse_args()
    try:
        changed = refresh(args.root, args.write)
        result = {'pass': args.write or not changed, 'documents': len(PAGES) + 1,
                  'refreshed' if args.write else 'stale': changed}
        if changed and not args.write:
            result['repair'] = 'python3 tools/release_docs.py --write'
        print(json.dumps(result, indent=2))
        return 0 if result['pass'] else 1
    except (ValueError, KeyError, TypeError, OSError) as error:
        print('Release documentation: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
