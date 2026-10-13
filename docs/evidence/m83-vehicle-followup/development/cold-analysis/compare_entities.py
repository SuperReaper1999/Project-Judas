#!/usr/bin/env python3
"""Read-only comparison of the preserved M83 entities participant, version 1.

Run with the original .save and restored-entities.bin; JSON goes to stdout.
This independently decodes the existing SaveArchive layout. It does not load
Judas, advance physics, normalize stored state or modify either input file.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shlex
import struct


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def number(self, kind):
        size = struct.calcsize('<' + kind)
        if self.offset + size > len(self.data):
            raise ValueError('truncated field')
        result = struct.unpack_from('<' + kind, self.data, self.offset)
        self.offset += size
        return result[0] if len(result) == 1 else list(result)

    def string(self):
        size = self.number('I')
        if size > 8 * 1024 * 1024 or self.offset + size > len(self.data):
            raise ValueError('invalid/truncated string')
        result = self.data[self.offset:self.offset + size]
        self.offset += size
        return result

    def finish(self):
        if self.offset != len(self.data):
            raise ValueError('trailing data')


def participant(save):
    outer = Reader(save)
    if outer.string() != b'JudasGameSave':
        raise ValueError('not a Judas game save')
    outer.string()  # Immutable header is fingerprinted with the whole input.
    payload = Reader(outer.string())
    outer.finish()
    found = None
    count = payload.number('I')
    if count > 65536:
        raise ValueError('invalid participant count')
    for _ in range(count):
        name = payload.string().decode('utf-8')
        version, data = payload.number('I'), payload.string()
        if name == 'entities':
            if found is not None or version != 1:
                raise ValueError('duplicate/newer entities participant')
            found = data
    payload.finish()
    if found is None:
        raise ValueError('entities participant absent')
    return found


def entities(data):
    reader = Reader(data)
    text = reader.string().decode('utf-8')
    authority_start = reader.offset
    next_id, time = reader.number('Q'), reader.number('d')
    count = reader.number('I')
    if count > 8192:
        raise ValueError('invalid entity count')
    records, body_ids = {}, []
    for _ in range(count):
        start = reader.offset
        entity = reader.number('Q')
        reader.number('3B')  # hasState, authored, transient
        reader.number('2i')  # fidelity, coarse motion
        reader.number('d')   # dormant time
        reader.number('B')   # forced fidelity exists
        reader.number('i')   # forced fidelity value
        reader.number('13f') # position, quaternion, linear/angular velocity
        body = reader.number('B')
        if body:
            reader.number('6f')  # ordinary body force/torque accumulators
            body_ids.append(entity)
        if entity in records:
            raise ValueError('duplicate entity')
        records[entity] = data[start:reader.offset]
    authority_end = reader.offset
    view = None
    if reader.number('B'):
        view = dict(position=reader.number('3f'), rotation=reader.number('4f'),
                    scale=reader.number('3f'), fov=reader.number('f'))
    reader.finish()
    return dict(text=text, next_id=next_id, time=time, records=records,
                body_ids=body_ids, view=view,
                authority=data[authority_start:authority_end],
                authority_offset=authority_start, view_offset=authority_end)


def transforms(text):
    objects, entity = {}, None
    other = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith('object '):
            entity = int(shlex.split(stripped)[1])
            objects[entity] = {}
        if entity is not None and stripped.split(' ', 1)[0] in ('position', 'rotation', 'scale'):
            words = stripped.split()
            objects[entity][words[0]] = [float(value) for value in words[1:]]
        else:
            other.append(line)
    return objects, other


def distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def rotation_distance(a, b):
    # q and -q represent the same orientation. Normalize only in this semantic
    # comparison, never in the archive or authoritative record comparison.
    a = [v / math.sqrt(sum(x*x for x in a)) for v in a]
    b = [v / math.sqrt(sum(x*x for x in b)) for v in b]
    chord = min(distance(a, b), distance(a, [-v for v in b]))
    return 4 * math.asin(min(1.0, chord / 2))


def compare(expected, restored):
    e, r = entities(expected), entities(restored)
    eo, en = transforms(e['text'])
    ro, rn = transforms(r['text'])
    differences = []
    for entity in sorted(set(eo) | set(ro)):
        a, b = eo.get(entity), ro.get(entity)
        if a == b:
            continue
        if a is None or b is None or set(a) != set(b):
            differences.append(dict(entity=entity, incompatible_transform=True))
            continue
        differences.append(dict(entity=entity,
                                position_delta_m=distance(a['position'], b['position']),
                                rotation_delta_rad=rotation_distance(a['rotation'], b['rotation']),
                                scale_delta=distance(a['scale'], b['scale']),
                                quaternion_sign_reversed=sum(x*y for x, y in zip(a['rotation'], b['rotation'])) < 0))
    raw_differences = [dict(offset=i, expected=a, restored=b)
                       for i, (a, b) in enumerate(zip(e['authority'], r['authority'])) if a != b]
    view_position_delta = None
    if e['view'] and r['view']:
        view_position_delta = distance(e['view']['position'], r['view']['position'])
    return dict(
        expected_entities_sha256=sha(expected), restored_entities_sha256=sha(restored),
        expected_total_bytes=len(expected), restored_total_bytes=len(restored),
        entity_count=len(e['records']), body_entities=e['body_ids'],
        allocation_counter_equal=e['next_id'] == r['next_id'],
        simulation_time_equal=e['time'] == r['time'],
        expected_authority_offset=e['authority_offset'], restored_authority_offset=r['authority_offset'],
        expected_view_offset=e['view_offset'], restored_view_offset=r['view_offset'],
        authoritative_section_bytes=len(e['authority']),
        authoritative_section_sha256=sha(e['authority']),
        restored_authoritative_section_sha256=sha(r['authority']),
        authoritative_section_exact=e['authority'] == r['authority'],
        differing_authoritative_entities=[entity for entity in sorted(set(e['records']) | set(r['records']))
                                          if e['records'].get(entity) != r['records'].get(entity)],
        first_authoritative_byte_differences=raw_differences[:16],
        first_aligned_view_byte_differences=[dict(relative_offset=i, expected_absolute_offset=e['view_offset']+i,
                                                 restored_absolute_offset=r['view_offset']+i, expected=a, restored=b)
                                             for i, (a, b) in enumerate(zip(expected[e['view_offset']:], restored[r['view_offset']:])) if a != b][:16],
        scene_text_exact=e['text'] == r['text'],
        scene_non_transform_lines_exact=en == rn,
        scene_transform_differences=differences,
        max_scene_position_delta_m=max((d.get('position_delta_m', math.inf) for d in differences), default=0),
        max_scene_rotation_delta_rad=max((d.get('rotation_delta_rad', math.inf) for d in differences), default=0),
        expected_view=e['view'], restored_view=r['view'],
        view_position_delta_m=view_position_delta,
        view_rotation_scale_fov_exact=all(e['view'][k] == r['view'][k] for k in ('rotation', 'scale', 'fov')) if e['view'] and r['view'] else e['view'] == r['view'],
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('restored_entities', type=Path)
    args = parser.parse_args()
    save, restored = args.snapshot.read_bytes(), args.restored_entities.read_bytes()
    result = compare(participant(save), restored)
    result.update(snapshot_path=str(args.snapshot.resolve()), snapshot_sha256=sha(save),
                  restored_artifact_path=str(args.restored_entities.resolve()),
                  script_sha256=sha(Path(__file__).read_bytes()),
                  scope='Read-only archive analysis; no engine execution, no physics step, no input mutation.')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
