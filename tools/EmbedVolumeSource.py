#!/usr/bin/env python3
"""Package an author-owned volume source as an ordinary project ES module.
No engine runtime filesystem or undocumented native operation is required.
Usage: EmbedVolumeSource.py Imports/quarry.judasvolume Assets/scripts/source.js
Cook the same source with judas_volume_author first to update initial geometry.
"""
import argparse,json
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('module',type=Path);args=parser.parse_args()
data=json.loads(args.source.read_text());assert data['format']=='JudasVolumeTerrain' and data['version']==1
assert len(data['density'])<=32768,'M97 per-volume sample limit'
assert max(data['extent'])<=64,'M97 extent limit'
args.module.parent.mkdir(parents=True,exist_ok=True)
args.module.write_text('export const source='+json.dumps(json.dumps(data,separators=(',',':')))+';\n')
