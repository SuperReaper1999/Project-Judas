#!/usr/bin/env python3
"""Offline authoring of original CC0 demo/test tones, not runtime synthesis.
Recreate WAV assets with Python; MP3 conversion additionally uses ffmpeg.
"""
import math,struct,wave,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent
RATE=48000
def clip(name,duration,frequencies):
    values=[]
    for i in range(int(RATE*duration)):
        t=i/RATE;edge=min(1,t/.02,(duration-t)/.02)
        sample=sum(math.sin(2*math.pi*f*t) for f in frequencies)/len(frequencies)
        values.append(struct.pack('<h',round(5000*edge*sample)))
    with wave.open(str(ROOT/name),'wb') as stream:
        stream.setnchannels(1);stream.setsampwidth(2);stream.setframerate(RATE);stream.writeframes(b''.join(values))
clip('loop.wav',2,[220,330])
clip('one_shot.wav',.45,[660,880])
clip('centered.wav',.75,[440,554.365,659.255])
subprocess.run(['ffmpeg','-y','-loglevel','error','-i',str(ROOT/'centered.wav'),'-codec:a','libmp3lame','-b:a','96k',str(ROOT/'centered.mp3')],check=True)
(ROOT/'centered.wav').unlink()
