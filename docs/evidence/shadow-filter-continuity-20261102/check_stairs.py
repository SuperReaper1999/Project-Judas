from pathlib import Path
from fractions import Fraction as F
import hashlib, re, subprocess

project=Path('projects/fluid_comparison')
scene=project/'Scenes/rectangular_pool.judas'
old=Path(__file__).resolve().with_name('pool-before.judas').read_text()
new=Path(__file__).resolve().with_name('pool-after-stairs.judas').read_text()

def objects(text):
    return re.findall(r'object \d+ "([^"\n]+)"\n(.*?)\nend',text,re.S)

def property(block,name):
    return re.search(r'^  '+re.escape(name)+r' (.+)$',block,re.M)[1]

def vector(block,name):
    return tuple(F(x) for x in property(block,name).split())

def stairs(text):
    out=[]
    for name,block in objects(text):
        if not name.startswith('Exit stair'): continue
        p=vector(block,'position');h=vector(block,'body.half-extents')
        assert h==vector(block,'render.half-extents')
        out.append(tuple((c-r,c+r) for c,r in zip(p,h)))
    return out

def overlaps(boxes):
    return [(i,j) for i,a in enumerate(boxes) for j,b in enumerate(boxes[:i])
            if all(min(a[k][1],b[k][1])>max(a[k][0],b[k][0]) for k in range(3))]

def contains(boxes,p):
    return any(all(lo<=v<=hi for v,(lo,hi) in zip(p,b)) for b in boxes)

a,b=stairs(old),stairs(new)
assert len(a)==len(b)==14
assert len(overlaps(a))==13, 'negative control must detect the old overlap'
assert not overlaps(b), 'new slabs must have disjoint interiors'
# Exact rational arrangement: occupancy is constant on every open cell; checking
# all vertices and interval midpoints also includes every boundary ownership case.
axes=[]
for axis in range(3):
    bounds=sorted({v for box in a+b for v in box[axis]})
    axes.append(sorted(set(bounds+[(x+y)/2 for x,y in zip(bounds,bounds[1:])]+[bounds[0]-1,bounds[-1]+1])))
count=0
for x in axes[0]:
    for y in axes[1]:
        for z in axes[2]:
            assert contains(a,(x,y,z))==contains(b,(x,y,z)),(x,y,z)
            count+=1
for i,(current,next_) in enumerate(zip(b,b[1:]),1):
    assert current[2][1]==next_[2][0], 'no stair gap'
    assert current[1][1]==F(i,5) and current[1][0]==0
assert b[0][2][0]==F(42,100) and b[-1][2][1]==F(403,100)
assert b[-1][1][1]==F(28,10)

def stock(text):
    out=[]
    for name,block in objects(text):
        if '\n  fluid-volume\n' not in '\n'+block+'\n': continue
        px,py,pz=vector(block,'position')
        nx,ny,nz=map(int,property(block,'fluid-volume.count').split())
        s=F(property(block,'fluid-volume.spacing'))
        out += [(px+(F(x)-F(nx-1,2))*s,py+y*s,pz+(F(z)-F(nz-1,2))*s)
                for x in range(nx) for y in range(ny) for z in range(nz)]
    return sorted(out)
assert stock(old)==stock(new) and len(stock(new))==1440
print('PASS original overlap negative control:',len(overlaps(a)),'pairs')
print('PASS partition has no interior overlaps')
print('PASS exact union arrangement samples:',count)
print('PASS 1440 particle positions are identical')
