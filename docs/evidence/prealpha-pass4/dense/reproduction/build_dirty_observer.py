from pathlib import Path
import hashlib,json,shlex,shutil,subprocess,time,datetime
r=Path.cwd();o=r/'.cache/prealpha-pass4/dense';b=o/'dirty-observer';b.mkdir(exist_ok=True);production=r/'src/physics/WorldImpactEvents.cpp';s=production.read_text();s=s.replace('#include <unordered_map>','#include <unordered_map>\n#include <cstdlib>\n#include <fstream>')
s=s.replace('using namespace JudasPhysicsDetail;', '''using namespace JudasPhysicsDetail;
namespace {
struct DirtyWitnessCounters {
    std::uint64_t events = 0, originalRawKeys = 0, uniqueKeys = 0;
    std::uint64_t nonzeroPartKeys = 0, selectedWithoutChangedEndpoint = 0;
    std::uint64_t mismatches = 0;
    ~DirtyWitnessCounters() {
        if (const char *path = std::getenv("JUDAS_PASS4_DIRTY_REPORT")) {
            std::ofstream file(path);
            file << "{\\\"events\\\":" << events
                 << ",\\\"original_raw_keys\\\":" << originalRawKeys
                 << ",\\\"unique_keys\\\":" << uniqueKeys
                 << ",\\\"nonzero_part_keys\\\":" << nonzeroPartKeys
                 << ",\\\"selected_without_changed_endpoint\\\":" << selectedWithoutChangedEndpoint
                 << ",\\\"mismatches\\\":" << mismatches << "}\\n";
        }
    }
} dirtyWitness;
}
''')
old='''        std::sort(dirty.begin(), dirty.end());
        for (const auto &key : dirty)'''
new='''        std::sort(dirty.begin(), dirty.end());
        // Cache-only witness: reconstruct the accepted original union AFTER
        // all borrowed QuerySlots references are finished. No extra geometry.
        std::vector<std::array<unsigned, 4>> originalDirty{selected};
        for (const auto &event : scheduledImpacts)
            if (included[event.key[0]] == 2 || included[event.key[1]] == 2)
                originalDirty.push_back(event.key);
        for (unsigned slot : changed) {
            const auto candidates = QuerySlots(tree.FatAabb(bodies[slot].proxy));
            for (unsigned other : candidates)
                if (other != slot && CanRespond(slot, other)) {
                    const auto a = std::min(slot, other), b = std::max(slot, other);
                    for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                        for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb)
                            originalDirty.push_back({a, b, unsigned(pa), unsigned(pb)});
                }
        }
        ++dirtyWitness.events;
        dirtyWitness.originalRawKeys += originalDirty.size();
        dirtyWitness.uniqueKeys += dirty.size();
        for (const auto &key : dirty)
            dirtyWitness.nonzeroPartKeys += key[2] != 0 || key[3] != 0;
        dirtyWitness.selectedWithoutChangedEndpoint +=
            included[selected[0]] != 2 && included[selected[1]] != 2;
        std::sort(originalDirty.begin(), originalDirty.end());
        originalDirty.erase(std::unique(originalDirty.begin(), originalDirty.end()), originalDirty.end());
        if (std::getenv("JUDAS_PASS4_NEGATIVE_DIRTY") && !dirty.empty())
            dirty.erase(dirty.begin()); // deliberate missing key, never shipped
        if (originalDirty != dirty) {
            ++dirtyWitness.mismatches;
            throw std::runtime_error("Pass4 dirty scheduling witness diverged");
        }
        for (const auto &key : dirty)'''
assert old in s;s=s.replace(old,new);src=o/'WorldImpactEvents.observer.cpp';src.write_text(s);records=[]
def run(cmd,log):
 start=time.monotonic()
 with log.open('w')as f:subprocess.run(list(map(str,cmd)),cwd=r/'build',stdout=f,stderr=subprocess.STDOUT,check=True)
 records.append({'argv':list(map(str,cmd)),'seconds':time.monotonic()-start,'log':str(log.relative_to(r))})
lines=(r/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines();flags=[]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in lines if x.startswith(n+' =')))
obj=b/'WorldImpactEvents.cpp.o';run(['/usr/bin/c++',*flags,'-I'+str(r/'src/physics'),'-c',src,'-o',obj],b/'compile.log');archive=b/'libjudas_engine.a';shutil.copyfile(o/'libjudas_engine.integrated.a',archive);run(['ar','r',archive,obj],b/'archive.log');(b/'BUILD.json').write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'production_source_sha256':hashlib.sha256(production.read_bytes()).hexdigest(),'observer_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'commands':records,'scope':'Cache-only original-key-union witness, same actual world/queries. Negative env deliberately omits one key. Never production and never performance evidence.'},indent=2)+'\n');print('observer built',flush=True)
