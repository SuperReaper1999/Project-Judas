#!/usr/bin/env python3
"""Dependency-light provenance tests: no SDL, graphics, ICU, VM or network needed."""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import tarfile
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    source = pathlib.Path(__file__).resolve().parents[1]
    records = []

    def publish(complete=False):
        report = {"schema": 1, "checks": len(records), "complete": complete,
                  "passed": complete and all(r["passed"] for r in records),
                  "failures": sum(not r["passed"] for r in records), "results": records}
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            temporary = args.output.with_suffix(args.output.suffix + ".tmp")
            temporary.write_text(json.dumps(report, indent=2) + "\n")
            temporary.replace(args.output)

    publish()

    def run(label, command, succeeds=True):
        result = subprocess.run(command, text=True, capture_output=True, timeout=60)
        ok = (result.returncode == 0) == succeeds
        records.append({"name": label, "passed": ok, "command": list(map(str, command)),
                        "returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
        publish()
        if not ok:
            raise RuntimeError(label + ": " + result.stdout + result.stderr)

    with tempfile.TemporaryDirectory(prefix="judas-identity-") as temporary:
        root = pathlib.Path(temporary)
        fixture = root / "source"
        fixture.mkdir()
        shutil.copy2(source / "engine-version.json", fixture)
        (fixture / "cmake").mkdir()
        for name in ("JudasBuildIdentity.cmake", "JudasBuildIdentity.h.in", "JudasBuildIdentity.json.in", "JudasSourceInfo.json.in"):
            shutil.copy2(source / "cmake" / name, fixture / "cmake" / name)
        (fixture / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\nproject(IdentityFixture LANGUAGES NONE)\n"
            'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/JudasBuildIdentity.cmake")\n'
            'judas_build_identity("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")\n'
            'file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/identity.json" INPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/judas-build-info.json.in")\n')

        def configure(label, directory, *options, succeeds=True):
            build = root / label
            run(label, ["cmake", "-S", str(directory), "-B", str(build), *options], succeeds)
            return json.loads((build / "identity.json").read_text()) if succeeds else None

        unknown = configure("unknown-source", fixture)
        assert unknown["source_origin"] == "unknown" and unknown["source_revision"] == "unknown"
        assert unknown["channel"] == "Local" and unknown["source_dirty"] is True
        configure("invalid-channel", fixture, "-DJUDAS_BUILD_CHANNEL=Alpha", succeeds=False)
        configure("invalid-version", fixture, "-DJUDAS_ENGINE_VERSION=misleading-version", succeeds=False)
        archive = {"schema": 1, "revision": "a" * 40, "tree": "b" * 40,
                   "source_fingerprint": "c" * 64, "source_dirty": False, "channel": "Experimental"}
        manifest_entries = [{"path": p.relative_to(fixture).as_posix(), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                            for p in sorted(fixture.rglob("*")) if p.is_file()]
        manifest_bytes = ('{\n"schema":1,\n"files":[\n' + ',\n'.join(
            json.dumps(p, ensure_ascii=False, separators=(",", ":")) for p in manifest_entries) + '\n]}\n').encode()
        (fixture / "JUDAS_SOURCE_FILES.json").write_bytes(manifest_bytes)
        archive["source_files_sha256"] = hashlib.sha256(manifest_bytes).hexdigest()
        (fixture / "JUDAS_SOURCE_INFO.json").write_text(json.dumps(archive))
        restored = configure("source-archive", fixture)
        assert restored["source_origin"] == "archive" and restored["source_revision"] == archive["revision"]
        assert restored["source_dirty"] is False and restored["channel"] == "Experimental"
        absent_git = configure("archive-without-git-tool", fixture, "-Djudas_git=/not-an-installed-git")
        assert absent_git["source_origin"] == "archive" and absent_git["source_dirty"] is False
        main_channel = configure("independent-main", fixture, "-DJUDAS_BUILD_CHANNEL=main")
        assert main_channel["channel"] == "main" and main_channel["source_revision"] == restored["source_revision"]
        assert main_channel["build_id"] != restored["build_id"]
        repeated = configure("archive-repeat", fixture)
        assert repeated == restored, "same archive has reproducible metadata without timestamps/build paths"
        version_bytes = (fixture / "engine-version.json").read_bytes()
        (fixture / "engine-version.json").write_bytes(version_bytes + b"\n")
        changed = configure("archive-edited", fixture)
        assert changed["source_dirty"] and changed["source_fingerprint"] != restored["source_fingerprint"]
        (fixture / "engine-version.json").write_bytes(version_bytes)
        (fixture / "third_party").mkdir()
        (fixture / "third_party/new.cpp").write_text("// newly compiled input\n")
        added = configure("archive-added-dependency", fixture)
        assert added["source_dirty"] and added["source_fingerprint"] != restored["source_fingerprint"]
        (fixture / "third_party/new.cpp").unlink()
        (fixture / "JUDAS_SOURCE_FILES.json").write_bytes(manifest_bytes + b"\n")
        configure("archive-manifest-tampered", fixture, succeeds=False)
        (fixture / "JUDAS_SOURCE_FILES.json").write_bytes(manifest_bytes)
        archive["revision"] = "invented"
        (fixture / "JUDAS_SOURCE_INFO.json").write_text(json.dumps(archive))
        configure("malformed-archive", fixture, succeeds=False)
        (fixture / "JUDAS_SOURCE_INFO.json").unlink()
        if shutil.which("git"):
            run("git-init", ["git", "-C", str(fixture), "init", "-b", "Experimental"])
            run("git-add", ["git", "-C", str(fixture), "add", "."])
            run("git-commit", ["git", "-C", str(fixture), "-c", "user.name=Identity Fixture",
                               "-c", "user.email=fixture@invalid.example", "commit", "-m", "fixture"])
            clean = configure("git-clean", fixture)
            assert clean["source_origin"] == "git" and clean["source_dirty"] is False
            assert clean["channel"] == "Experimental" and len(clean["source_revision"]) == 40
            archive_path = root / "tracked-source.tar.gz"
            run("tracked-source-archive", ["python3", str(source / "tools/package_source.py"),
                "--source", str(fixture), "--output", str(archive_path), "--channel", "Experimental"])
            extracted = root / "extracted"
            extracted.mkdir()
            with tarfile.open(archive_path) as stream:
                stream.extractall(extracted, filter="data")
            round_trip = configure("portable-source-round-trip", extracted, "-Djudas_git=/not-an-installed-git")
            assert round_trip["source_origin"] == "archive" and not round_trip["source_dirty"]
            assert round_trip["source_revision"] == clean["source_revision"] and round_trip["source_fingerprint"] == clean["source_fingerprint"]
            (extracted / "src").mkdir()
            (extracted / "src/new.cpp").write_text("// added build input\n")
            additional = configure("archive-extra-input", extracted)
            assert additional["source_dirty"] and additional["source_fingerprint"] != round_trip["source_fingerprint"]
            clean_extra = fixture / "third_party/untracked.cpp"
            clean_extra.write_text("// globbed dependency input\n")
            observed = configure("git-clean-untracked-input", fixture)
            assert not observed["source_dirty"] and observed["source_fingerprint"] == clean["source_fingerprint"]
            assert observed["source_untracked_count"] == clean["source_untracked_count"] + 1
            assert observed["build_id"] != clean["build_id"], "clean tracked metadata must still identify observed untracked inputs"
            clean_extra.unlink()
            (fixture / "engine-version.json").write_text(json.dumps({"schema": 1, "version": "0.1.1-prealpha", "maturity": "pre-alpha"}))
            dirty = configure("git-dirty", fixture)
            assert dirty["source_dirty"] and dirty["source_fingerprint"] != clean["source_fingerprint"]
            run("version-refresh-same-build", ["cmake", "-S", str(fixture), "-B", str(root / "git-clean")])
            refreshed = json.loads((root / "git-clean/identity.json").read_text())
            assert refreshed["version"] == "0.1.1-prealpha", "default version refreshes in an existing build cache"
            new_input = fixture / "src/new.cpp"
            new_input.parent.mkdir()
            new_input.write_text("// new input\n")
            new_source = configure("git-new-input", fixture)
            assert new_source["source_fingerprint"] == dirty["source_fingerprint"]
            assert new_source["source_untracked_fingerprint"] != dirty["source_untracked_fingerprint"]
            assert new_source["source_untracked_count"] == dirty["source_untracked_count"] + 1
            assert new_source["build_id"] != dirty["build_id"], "new dirty development inputs alter the build identity"
            run("git-main", ["git", "-C", str(fixture), "switch", "-c", "main"])
            main_source = configure("git-main-channel", fixture)
            assert main_source["channel"] == "main" and main_source["source_revision"] == clean["source_revision"]
    publish(complete=True)
    print(f"Build provenance configuration: {len(records)} controls passed")


if __name__ == "__main__":
    main()
