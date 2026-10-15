set debuginfod enabled off
set language c++
add-symbol-file /tmp/judas-m87-PhysicsWorld-debug.o 0
set $w=('PhysicsWorld::Impl'*)0x5faf61340aa0
python
w=gdb.parse_and_eval('$w').dereference()
v=w['impactCounts']['_M_impl'];a=v['_M_start'];n=int(v['_M_finish']-a)
counts=[int(a[i]) for i in range(n)]
print('GROUPED_IMPACT_INCLUSIONS',[(label,sum(counts[lo:hi]),sum(x>0 for x in counts[lo:hi]),max(counts[lo:hi])) for label,lo,hi in [('floor',0,1),('cabinets',1,257),('pit walls',257,261),('loose',261,n)]])
end
