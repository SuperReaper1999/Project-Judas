import shlex,json,pathlib
p=pathlib.Path('docs/evidence/prealpha-pass2/integration/final/known-failures/bake-knots/Assets/2.judasbasin')
t=iter(shlex.split(p.read_text())); take=lambda n:[next(t) for _ in range(n)]
h=take(16);take(12*int(h[14])+2*int(h[15])); sh=take(24); grid=take(int(sh[1])*int(sh[2]));cells=[];violations=[]
for i in range(int(sh[22])):
    ch=take(12);nt,nk=map(int,ch[-2:]); geo=[list(map(float,take(12))) for j in range(nt)]; knots=[list(map(float,take(2))) for j in range(nk)]
    vv=[{'index':j,'previous':knots[j-1],'current':knots[j],'drop':knots[j][1]-knots[j-1][1]} for j in range(1,nk) if knots[j][0]<=knots[j-1][0] or knots[j][1]<knots[j-1][1]]
    cells.append({'header':ch,'geometry':geo,'knots':knots}); violations.extend({'cell':i,**v} for v in vv)
    print('cell',i,'nt/nk',nt,nk,'capacity',ch[7], 'violations',vv)
out={'source':str(p),'header':h,'surface_header':sh,'cells':cells,'violations':violations};pathlib.Path('.cache/prealpha-liquid-rotation/capacity-investigation/original-knots.json').write_text(json.dumps(out,indent=2)+'\n')
