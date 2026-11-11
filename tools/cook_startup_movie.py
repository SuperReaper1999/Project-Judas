#!/usr/bin/env python3
"""Cook a supplied MP4 into bounded independently decoded JPEG frames.
FFmpeg is an authoring dependency only; games use Judas's existing image decoder.
"""
import argparse, hashlib, json, struct, subprocess, tempfile
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='judas-splash-') as temp:
        subprocess.run(['ffmpeg','-v','error','-i',str(args.source),'-an','-vf',
                        'fps=30,scale=512:512:flags=lanczos','-frames:v','601','-q:v','3',str(Path(temp)/'%04d.jpg')],check=True)
        frames=sorted(Path(temp).glob('*.jpg'))
        if not 1<=len(frames)<=600:raise ValueError('startup movie must contain 1–600 frames')
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('wb')as target:
            target.write(struct.pack('<4sIIII',b'JSM1',512,512,30,len(frames)))
            for frame in frames:
                data=frame.read_bytes()
                if len(data)>2*1024*1024 or target.tell()+4+len(data)>32*1024*1024:raise ValueError('startup movie exceeds runtime bounds')
                target.write(struct.pack('<I',len(data)));target.write(data)
        provenance=dict(source_sha256=hashlib.sha256(args.source.read_bytes()).hexdigest(),
                        frames=len(frames),width=512,height=512,fps=30,
                        source='Operator supplied Judas vortex MP4; original remains unchanged.',
                        runtime='One JPEG frame decoded at a time with the existing Judas image decoder; no runtime FFmpeg dependency.')
        args.output.with_suffix('.json').write_text(json.dumps(provenance,indent=2)+'\n')
if __name__=='__main__':main()
