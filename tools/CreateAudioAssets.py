#!/usr/bin/env python3
"""Reproduce original M60 sound content with a local FFmpeg tool (no runtime dependency)."""
import subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
audio=root/'projects/audio_lab/Assets/audio'
signals={
'music.flac':('aevalsrc=0.07*sin(2*PI*220*t)*(0.6+0.4*sin(2*PI*t/12))+0.045*sin(2*PI*330*t)+0.025*sin(2*PI*440*t):s=48000:d=180',2),
'ambience.flac':('anoisesrc=color=pink:amplitude=0.2:sample_rate=48000:duration=20:seed=60',2),
 'tone.wav':('sine=frequency=650:sample_rate=48000:duration=2',1),
 'pulse.wav':('aevalsrc=if(between(t\\,0.02\\,0.03)\\,0.25*sin(2*PI*2200*t)\\,0):s=48000:d=1',1)}
for name,(source,channels) in signals.items():
 subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i',source,'-ac',str(channels),'-y',str(audio/name)],check=True)
