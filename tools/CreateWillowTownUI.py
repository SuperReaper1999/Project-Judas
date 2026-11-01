#!/usr/bin/env python3
"""Rebuild Willow Town's ordinary Judas UI asset, without touching gameplay."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'projects/willow_town/Assets/hud.judasui'
INK=(.91,.93,.85,1); MUTED=(.63,.72,.67,1); PANEL=(.055,.10,.10,.98); GOLD=(.93,.74,.39,1)
elements=[]
def element(id,kind=2,text='',parent='canvas',pos=(0,0),size=(180,30),font=18,color=INK,bg=(0,0,0,0),wrap=False,align=(0,0)):
    fields=[id,parent,kind,0,1,1,0,int(wrap),1,0,0,0,0,*pos,*size,0,0,0,0,*align,
            0,0,0,0,0,0,0,0,*bg,*color,8,font,0,0,1,text,'','']
    elements.append(' '.join(json.dumps(v,ensure_ascii=False) if isinstance(v,str) else str(v) for v in fields))
element('canvas',0,parent='',size=(1280,720))
element('header',1,size=(1280,66),bg=PANEL)
element('brand',text='WILLOW TOWN',pos=(22,10),size=(290,28),font=25,color=GOLD)
element('subtitle',text='A little town with a mind of its own',pos=(23,39),size=(390,20),font=14,color=MUTED)
element('clock',pos=(430,16),size=(240,30),font=20)
for id,label in [('pause','Pause'),('speed','Speed: 1x'),('save','Save [K]'),('load','Load [L]')]:
    element(id,4,text=label,size=(95,34),font=16,bg=(.15,.24,.22,1),align=(.5,.5))
element('sidebar',1,pos=(940,66),size=(340,630),bg=PANEL)
element('person-name',text='Mara',font=27,color=GOLD)
element('person-job',text='Baker',font=16,color=MUTED)
element('person-action',text='Deciding what to do...',font=18,wrap=True)
element('person-reason',text='The decision comes from real needs and opportunities.',font=14,color=MUTED,wrap=True)
for need,col in [('hunger',(.89,.60,.36,1)),('fatigue',(.46,.70,.85,1)),('social',(.75,.57,.83,1))]:
    element('need-'+need,text=need.title(),font=16)
    element('track-'+need,1,size=(285,7),bg=(.17,.24,.23,1))
    element('bar-'+need,1,size=(200,7),bg=col)
element('person-money',text='Wallet: 12 coins',font=16,color=GOLD)
element('control',4,text='Take control [WASD]',font=17,bg=(.25,.40,.32,1),align=(.5,.5))
element('scores-title',text='WHAT MATTERS TO THEM',font=13,color=GOLD)
element('scores',text='Waiting for their first decision...',font=15,wrap=True)
element('experiment-label',text='TRY CHANGING THEIR NEEDS',font=12,color=MUTED)
for id,label in [('hungry','Hungry'),('tired','Tired'),('lonely','Lonely')]:
    element(id,4,text=label,font=14,bg=(.15,.24,.22,1),align=(.5,.5))
element('shop-status',text='Grocer: open',font=15,color=GOLD)
for id,label in [('shop-toggle','Close shop'),('restock','Restock +8')]:
    element(id,4,text=label,font=14,bg=(.15,.24,.22,1),align=(.5,.5))
element('footer',1,pos=(0,690),size=(1280,30),bg=PANEL)
element('message',text='Click a person in town, or choose their name below.',font=14,color=INK)
for id,name in [('301','Mara'),('302','Eli'),('303','Nia'),('304','Theo'),('305','Ada'),('306','Sam')]:
    element('resident-'+id,4,text=name,font=16,bg=(.09,.18,.16,.96),align=(.5,.5))
element('town-help',text='Click anyone to inspect  |  Space: pause  |  Esc: return to AI',font=13,color=INK)
element('selected-label',text='Mara',font=14,color=GOLD,bg=(.04,.08,.07,.85),align=(.5,.5))
DEST.parent.mkdir(parents=True,exist_ok=True)
DEST.write_text('JudasUI 1\n1280 720 1 1 0 '+str(len(elements))+'\n'+'\n'.join(elements)+'\n')
print('Wrote',DEST.relative_to(ROOT),len(elements),'elements')
if __name__=='__main__':pass
