#!/usr/bin/env python3
"""Summarize preserved frame samples and explicit foreground image differences."""
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
import csv,json,re,math,statistics
R=Path(__file__).resolve().parents[1];root=R/'.cache/m93/measure';result={'timing':'completed render wall ms, excludes swap/pacing; fresh process, warm OS cache','modes':{},'quality':[],'storage':{}}
def stats(v):
 v=sorted(v);return {'samples':len(v),'median':statistics.median(v),'p95':v[math.ceil(len(v)*.95)-1],'max':max(v)}
for mode in ['source-baseline','source-candidate','cooked-full','cooked-lod']:
 loads=[];payload=[];rss=[];frames={1:[],2:[]};tris={1:set(),2:set()};gpu={1:[],2:[]};cpu={1:[],2:[]}
 for folder in sorted(root.glob('*-'+mode)):
  log=(folder/'stdout.log').read_text();loads.append(float(re.search('load_ms=([0-9.]+)',log)[1]));payload.append(int(re.search(r'gpu_bytes=(\d+)',log)[1]));rss.append(int(re.search(r'Maximum resident set size \(kbytes\): (\d+)',(folder/'memory.txt').read_text())[1]))
  for row in csv.DictReader((folder/'frames.csv').open()):v=int(row['views']);frames[v].append(float(row['ms']));tris[v].add(int(row['triangles']))
  p=json.loads((folder/'profile.json').read_text());g={}
  for f in p['frames']:
   fid=f['id'];view=1 if 13<=fid<=72 else 2 if 85<=fid<=144 else 0
   if view:cpu[view].append(sum(x['inclusive_ns'] for x in f['scopes']if x['name']=='Opaque instance submission')/1e6)
   for x in f['gpu']['passes']:
    if x['pass']=='Opaque instances' and not x['pending']:g[x['source_frame']]=g.get(x['source_frame'],0)+x['milliseconds']
  for fid,value in g.items():
   view=1 if 13<=fid<=72 else 2 if 85<=fid<=144 else 0
   if view:gpu[view].append(value)
 result['modes'][mode]={'load_ms':loads,'gpu_payload_bytes':payload,'peak_process_rss_kib':rss,'views':{v:{'wall_ms':stats(frames[v]),'triangles':sorted(tris[v]),'gpu_opaque_ms':stats(gpu[v]),'cpu_opaque_submission_ms':stats(cpu[v])}for v in [1,2]}}
for a,b in [('source-baseline','source-candidate'),('source-candidate','cooked-full'),('cooked-full','cooked-lod')]:
 for view in [1,2]:
  left=Image.open(root/f'0-{a}/view{view}.png').convert('RGB');right=Image.open(root/f'0-{b}/view{view}.png').convert('RGB')
  def mask(im):return ImageChops.difference(im,Image.new('RGB',im.size,im.getpixel((0,0)))).convert('L').point(lambda x:255 if x>6 else 0)
  lm,rm=mask(left),mask(right);union=ImageChops.lighter(lm,rm);diff=ImageChops.difference(left,right);rms=ImageStat.Stat(diff,union).rms;count=union.histogram()[255];xor=ImageChops.difference(lm,rm).histogram()[255]
  result['quality'].append({'left':a,'right':b,'view':view,'foreground_union_pixels':count,'foreground_rgb_rms':math.sqrt(sum(x*x for x in rms)/3),'background_threshold_mask_difference_pixels':xor,'mask_difference_percent':100*xor/max(1,count),'mask_definition':'luma difference >6 from each image corner background; not a depth silhouette or certified geometric error'})
for source,cooked in [('column.obj','column.judasmesh'),('tile0.png','tile0.judastex')]:result['storage'][source]={'source_bytes':(R/'.cache/m93/fixtures/Assets'/source).stat().st_size,'cooked_bytes':(R/'.cache/m93/fixtures/Assets'/cooked).stat().st_size}
(root/'SUMMARY.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
