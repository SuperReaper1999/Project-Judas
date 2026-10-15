set debuginfod enabled off
set pagination off
set $tls=$fs_base-96
frame 2
info registers rbp rsp
x/7ug $rbp-0x940
x/7ug $tls
python
import gdb,json
names=['predicates','intervalResolved','exactFallbacks','unresolved','invalidInputs','numericGapFallbacks','preparedOrientationMisses']
def words(address):
    return [int(gdb.parse_and_eval('((unsigned long long*)(%s))[%d]'%(address,i))) for i in range(7)]
before=words('$rbp-0x940');current=words('$tls');result={'scope':'Original stalled step188, unfinished at physical time0.003821390s. Pre-step snapshot located using original binary Step disassembly: GetContactGeometryDiagnostics output at rbp-0x940. TLS output located from original Get function fs:-96. No executable execution or new build.','before':dict(zip(names,before)),'current':dict(zip(names,current)),'unfinished_step_delta':dict(zip(names,[b-a for a,b in zip(before,current)]))}
print(json.dumps(result,indent=2))
with open('/tmp/m87-live-diagnostics/original-core-predicate-delta.json','w') as out:json.dump(result,out,indent=2)
end
quit
