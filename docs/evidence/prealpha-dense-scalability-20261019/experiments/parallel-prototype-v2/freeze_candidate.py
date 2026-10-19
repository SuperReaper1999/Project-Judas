from pathlib import Path
import copy
import datetime
import gzip
import hashlib
import json
import shutil
import sys
import tarfile

proto = Path(__file__).resolve().parent
owned = proto.parent
base = owned / "baseline-e30"
dest = owned / (sys.argv[1] if len(sys.argv) > 1 else "candidate-parallel-toi-v2")
dest.mkdir()
(dest / "bin").mkdir()


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def desc(p):
    st = p.stat()
    return {"path": str(p.resolve()), "sha256": sha(p), "bytes": st.st_size,
            "mtime_ns": st.st_mtime_ns, "mode": oct(st.st_mode & 0o777)}


m = copy.deepcopy(json.loads((base / "FROZEN.json").read_text()))
m["utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
m["source_kind"] = "isolated one-object override of exact frozen baseline; NOT clean checkout build"
m["parent_snapshot"] = {"path": str(base.resolve()), "manifest_sha256": sha(base / "FROZEN.json")}
m["notes"] = [
    "Baseline source archive and embedded identity are inherited exactly from baseline-e30.",
    "Only WorldImpactEvents.cpp.o is replaced in the copied engine archive.",
    "Isolated generated query contexts duplicate unchanged CCD arithmetic for this experiment only.",
    "Actual override source bytes, build commands, binaries and query controls are hashed separately.",
    "No full-step parity or performance outcome is claimed before actual run/compare receipts."]
for name in ["CMakeCache.txt", "judas-build-info-Release.json", "source-inputs.tar.gz"]:
    shutil.copy2(base / name, dest / name)
for name in ["libjudas_engine.a", "judas_physics_stress_live_benchmark", "judas_physics_scaling_diagnostics"]:
    target = name if name.endswith(".a") else "bin/" + name
    src, dst = proto / name, dest / target
    shutil.copy2(src, dst)
    entry = desc(src)
    entry["copy_sha256"] = sha(dst)
    assert entry["sha256"] == entry["copy_sha256"]
    m["artifacts"][target] = entry
m["source_archive"] = desc(dest / "source-inputs.tar.gz")
m["source_archive"]["meaning"] = "Unchanged baseline; combine with explicit isolated overrides."
files = ["create_prototype.py", "build_prototype.py", "WorldInternal.h", "ParallelImpactQueries.h",
         "WorldImpactEvents.cpp", "PROTOTYPE.json", "BUILD.json", "QueryParity.cpp",
         "build_query_parity.py", "query-shared-parity.log", "freeze_candidate.py"]
archive = dest / "isolated-overrides.tar.gz"
with archive.open("wb") as raw:
    with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w") as tar:
            for name in files:
                p = proto / name
                info = tar.gettarinfo(str(p), arcname=name)
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = 0
                with p.open("rb") as stream:
                    tar.addfile(info, stream)
m["isolated_overrides"] = {name: desc(proto / name) for name in files}
m["isolated_override_archive"] = desc(archive)
m["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
(dest / "FROZEN.json").write_text(json.dumps(m, indent=2, sort_keys=True) + "\n")
print(json.dumps({"snapshot": str(dest), "manifestSha256": sha(dest / "FROZEN.json"),
                  "liveSha256": sha(dest / "bin/judas_physics_stress_live_benchmark")}, indent=2))
