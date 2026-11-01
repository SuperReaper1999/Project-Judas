#!/usr/bin/env python3
"""Watch a real, visible Willow Town process for a measured wall-clock interval.

This deliberately does not use Judas's frame-driven test harness. Only a copied
scene's normal startSpeed/observationRun properties are changed. The source
project, other windows and existing processes are never modified or closed.
Use a complete exported runtime package so engine fonts and other runtime data
remain available when the game is launched from its isolated project directory.
Leaving the child paused does not detach it from an outer task/process manager;
use an independent desktop/session when it must survive the observer exiting.
"""
from __future__ import annotations

import argparse
import ctypes as C
import ctypes.util
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
PREFIXES = ("WILLOW_WATCH_FINAL ", "WILLOW_WATCH_SAVE ", "WILLOW_TOWN ")


def utc() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def sha(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path: Path, value) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def configure_scene(text: str) -> str:
    """Change one owned script property record, preserving all other scene bytes."""
    lines = text.splitlines(keepends=True)
    inside = False
    count = 0
    for index, line in enumerate(lines):
        if line.startswith('object 1 "'):
            inside = True
        elif inside and line.strip() == "end":
            inside = False
        elif inside and line.lstrip().startswith("script.0.properties "):
            literal = line.split("script.0.properties ", 1)[1].strip()
            properties = json.loads(json.loads(literal))
            if not isinstance(properties, dict):
                raise ValueError("Coordinator properties must be an object")
            properties.update(startSpeed=3, observationRun=True)
            encoded = json.dumps(json.dumps(properties, separators=(",", ":")))
            lines[index] = "  script.0.properties " + encoded + "\n"
            count += 1
    if count != 1:
        raise ValueError("Expected exactly one object-1 coordinator property record")
    return "".join(lines)


def observation(line: str):
    for prefix in PREFIXES:
        start = line.find(prefix)
        if start >= 0:
            value = json.loads(line[start + len(prefix):])
            if not isinstance(value, dict):
                raise ValueError("Observation must be a JSON object")
            return prefix.strip(), value
    return None


def log_tail(path: Path, limit_bytes: int = 8192, lines: int = 12) -> list[str]:
    """Bound startup diagnostics even if an erroneous runtime produces huge logs."""
    try:
        with path.open("rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - limit_bytes))
            return stream.read(limit_bytes).decode("utf-8", errors="replace").splitlines()[-lines:]
    except OSError as error:
        return ["Unable to read runtime log: " + str(error)]


def memory_sample(pid: int) -> dict:
    result = {"pid": pid}
    try:
        stat = Path(f"/proc/{pid}/stat").read_text()
        fields = stat[stat.rfind(")") + 2:].split()
        result["cpu_seconds"] = (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith(("VmRSS:", "VmSize:", "Threads:")):
                key, value = line.split(":", 1)
                result[key] = int(value.split()[0])
        rollup = Path(f"/proc/{pid}/smaps_rollup")
        if rollup.exists():
            for line in rollup.read_text().splitlines():
                if line.startswith(("Rss:", "Pss:")):
                    key, value = line.split(":", 1)
                    result[key + "_KiB"] = int(value.split()[0])
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith(("MemAvailable:", "SwapFree:")):
                key, value = line.split(":", 1)
                result["host_" + key + "_KiB"] = int(value.split()[0])
    except (OSError, ValueError, IndexError) as error:
        result["read_error"] = str(error)
    return result


class X11:
    """Small window-specific X11 adapter; never sends global keyboard input."""
    class Data(C.Union):
        _fields_ = [("b", C.c_char * 20), ("s", C.c_short * 10), ("l", C.c_long * 5)]

    class Client(C.Structure):
        pass

    class Key(C.Structure):
        _fields_ = [("type", C.c_int), ("serial", C.c_ulong), ("send_event", C.c_int),
                    ("display", C.c_void_p), ("window", C.c_ulong), ("root", C.c_ulong),
                    ("subwindow", C.c_ulong), ("time", C.c_ulong), ("x", C.c_int),
                    ("y", C.c_int), ("x_root", C.c_int), ("y_root", C.c_int),
                    ("state", C.c_uint), ("keycode", C.c_uint), ("same_screen", C.c_int)]

    def __init__(self):
        library = ctypes.util.find_library("X11")
        if not library:
            raise RuntimeError("libX11 is required for the owned-window observation")
        self.lib = C.CDLL(library)
        self.lib.XOpenDisplay.argtypes = [C.c_char_p]
        self.lib.XOpenDisplay.restype = C.c_void_p
        self.lib.XCloseDisplay.argtypes = [C.c_void_p]
        self.lib.XDefaultRootWindow.argtypes = [C.c_void_p]
        self.lib.XDefaultRootWindow.restype = C.c_ulong
        self.lib.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        self.lib.XInternAtom.restype = C.c_ulong
        self.lib.XFree.argtypes = [C.c_void_p]
        self.lib.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
            C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_int),
            C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ubyte))]
        self.lib.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.c_void_p]
        self.lib.XFlush.argtypes = [C.c_void_p]
        self.lib.XSync.argtypes = [C.c_void_p, C.c_int]
        self.lib.XStringToKeysym.argtypes = [C.c_char_p]
        self.lib.XStringToKeysym.restype = C.c_ulong
        self.lib.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        self.lib.XKeysymToKeycode.restype = C.c_ubyte
        self.lib.XGetGeometry.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
            C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_uint),
            C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint)]
        self.display = self.lib.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("A usable DISPLAY is required; this is a visible desktop run")
        self.root = self.lib.XDefaultRootWindow(self.display)
        # A window can disappear between a query and an event. Keep that a reported
        # failure rather than letting Xlib terminate the observation process.
        self.handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)
        self.handler = self.handler_type(lambda _display, _error: 0)
        self.lib.XSetErrorHandler.argtypes = [self.handler_type]
        self.lib.XSetErrorHandler(self.handler)

    def atom(self, name: str) -> int:
        return self.lib.XInternAtom(self.display, name.encode(), 0)

    def property(self, window: int, name: str) -> list[int]:
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        format_bits = C.c_int()
        data = C.POINTER(C.c_ubyte)()
        status = self.lib.XGetWindowProperty(self.display, window, self.atom(name), 0, 16384,
            0, 0, C.byref(actual), C.byref(format_bits), C.byref(count), C.byref(remaining), C.byref(data))
        try:
            if status or not data or format_bits.value != 32:
                return []
            values = C.cast(data, C.POINTER(C.c_ulong))
            return [int(values[i]) for i in range(count.value)]
        finally:
            if data:
                self.lib.XFree(data)

    def owns(self, window: int, pid: int) -> bool:
        return self.property(window, "_NET_WM_PID") == [pid]

    def find(self, pid: int) -> int | None:
        for window in self.property(self.root, "_NET_CLIENT_LIST"):
            if self.owns(window, pid):
                return window
        return None

    def dimensions(self, window: int) -> tuple[int, int]:
        root, x, y = C.c_ulong(), C.c_int(), C.c_int()
        width, height, border, depth = C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
        ok = self.lib.XGetGeometry(self.display, window, C.byref(root), C.byref(x), C.byref(y),
            C.byref(width), C.byref(height), C.byref(border), C.byref(depth))
        if not ok or not width.value or not height.value:
            raise RuntimeError("Owned window is no longer drawable")
        return width.value, height.value

    def finish_key(self, window: int, pid: int) -> bool:
        if not self.owns(window, pid):
            return False
        code = self.lib.XKeysymToKeycode(self.display, self.lib.XStringToKeysym(b"F10"))
        if not code:
            return False
        for kind, mask in ((2, 1), (3, 2)):
            packet = self.Event()
            event = packet.key
            event.type, event.send_event = kind, 1
            event.display, event.window, event.root = self.display, window, self.root
            event.same_screen, event.keycode = 1, code
            self.lib.XSendEvent(self.display, window, 0, mask, C.byref(packet))
            self.lib.XFlush(self.display)
            if kind == 2:
                time.sleep(.08)
        return True

    def close_window(self, window: int, pid: int) -> bool:
        if not self.owns(window, pid):
            return False
        packet = self.Event()
        event = packet.client
        event.type, event.send_event = 33, 1
        event.display, event.window = self.display, window
        event.message_type, event.format = self.atom("WM_PROTOCOLS"), 32
        event.data.l[0], event.data.l[1] = self.atom("WM_DELETE_WINDOW"), 0
        self.lib.XSendEvent(self.display, window, 0, 0, C.byref(packet))
        self.lib.XFlush(self.display)
        return True

    def close(self):
        if self.display:
            self.lib.XSync(self.display, 0)
            self.lib.XCloseDisplay(self.display)
            self.display = None


X11.Client._fields_ = [("type", C.c_int), ("serial", C.c_ulong), ("send_event", C.c_int),
    ("display", C.c_void_p), ("window", C.c_ulong), ("message_type", C.c_ulong),
    ("format", C.c_int), ("data", X11.Data)]


class XEvent(C.Union):
    _fields_ = [("key", X11.Key), ("client", X11.Client), ("padding", C.c_long * 24)]


X11.Event = XEvent


def capture(x11: X11, window: int, pid: int, output: Path, name: str) -> dict:
    result = {"name": name, "utc": utc()}
    if not x11.owns(window, pid):
        return {**result, "error": "Owned window is unavailable"}
    executable = shutil.which("ffmpeg")
    if not executable:
        return {**result, "error": "Optional ffmpeg screenshot tool is unavailable"}
    try:
        width, height = x11.dimensions(window)
        command = [executable, "-nostdin", "-y", "-loglevel", "error", "-f", "x11grab",
            "-framerate", "1", "-draw_mouse", "0", "-video_size", f"{width}x{height}",
            "-window_id", str(window), "-i", os.environ["DISPLAY"], "-frames:v", "1",
            str(output / (name + ".png"))]
        with (output / (name + "-capture.log")).open("w") as log:
            done = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=8)
        result.update(exit_code=done.returncode, dimensions=[width, height])
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        result["error"] = str(error)
    return result


def outcomes(value: dict | None) -> dict:
    if not value:
        return {}
    people = value.get("residents", [])
    fields = ("id", "name", "alive", "ageDays", "job", "jobId", "health", "money",
              "work", "workDone", "meals", "rests", "chats", "jobsLost", "jobsFound",
              "deathCause", "deathTime", "selected")
    return {"time": value.get("time"), "census": value.get("census"),
        "alive": sum(person.get("alive", True) for person in people),
        "residents": [{key: person[key] for key in fields if key in person} for person in people]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", required=True, type=Path,
        help="judas executable inside a complete exported package (including engine fonts)")
    parser.add_argument("--project", type=Path, default=ROOT / "projects/willow_town/Willow_Town.judasproj")
    parser.add_argument("--output", required=True, type=Path, help="new output directory, never reused")
    parser.add_argument("--seconds", type=float, default=1200, help="actual observed wall seconds (default 1,200)")
    parser.add_argument("--sample-seconds", type=float, default=60)
    parser.add_argument("--window-size", default="1280x800")
    parser.add_argument("--leave-paused", action="store_true",
        help="request a saved, paused child at observer exit; an independent launch session is needed to survive caller cleanup")
    parser.add_argument("--internal-capture", type=Path,
        help="optional normal-runtime screenshot path after 600 actual fixed steps; does not enable the frame harness")
    parser.add_argument("--no-stdbuf", action="store_true",
        help="diagnostic: launch directly without the stdout line-buffering wrapper")
    parser.add_argument("--no-captures", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 86400 or not 1 <= args.sample_seconds <= 60:
        parser.error("seconds must be 1..86400; sample interval must be 1..60")
    try:
        width, height = map(int, args.window_size.split("x"))
        if not 640 <= width <= 8192 or not 480 <= height <= 8192:
            raise ValueError()
    except ValueError:
        parser.error("window size must be WIDTHxHEIGHT, at least 640x480")
    runtime, project, output = args.runtime.resolve(strict=True), args.project.resolve(strict=True), args.output.resolve()
    if output.exists():
        parser.error("output directory already exists; choose a fresh directory")
    internal_capture = args.internal_capture.expanduser().resolve() if args.internal_capture else None
    if internal_capture == output:
        parser.error("internal screenshot path must be a file, not the output directory")
    if internal_capture is not None and internal_capture.exists():
        parser.error("internal screenshot path already exists; choose a fresh file")
    source = project.parent
    if output.is_relative_to(source):
        parser.error("output must be outside the source project to avoid recursive copying")
    script = source / "Assets/game.js"
    if "startSpeed" not in script.read_text(encoding="utf-8") or "observationRun" not in script.read_text(encoding="utf-8"):
        parser.error("This game lacks the authored startSpeed/observationRun properties; use the expanded Willow Town")
    x11 = X11()
    output.mkdir(parents=True)
    copied = output / "project"
    shutil.copytree(source, copied, ignore=shutil.ignore_patterns("Saves", ".cache", "Builds", "__pycache__"))
    scene = copied / "Scenes/main.judas"
    old_scene = scene.read_text(encoding="utf-8")
    scene.write_text(configure_scene(old_scene), encoding="utf-8")
    state = {"format": 1, "state": "starting", "requested_wall_seconds": args.seconds,
        "requested_game_speed": 3, "created_utc": utc(), "runtime": str(runtime),
        "runtime_sha256": sha(runtime), "source_project": str(project), "isolated_project": str(copied / project.name),
        "source_scene_sha256": sha(source / "Scenes/main.judas"), "copied_scene_sha256": sha(scene),
        "configuration_change": {"startSpeed": 3, "observationRun": True},
        "test_harness": False, "frame_delta_override": False, "captures": [], "warnings": []}
    env = os.environ.copy()
    removed = [key for key in env if key.startswith("JUDAS_")]
    for key in removed:
        env.pop(key, None)
    state["removed_inherited_judas_environment_names"] = sorted(removed)
    env.update(XDG_DATA_HOME=str(output / "user"), JUDAS_WINDOW_SIZE=args.window_size, SDL_VIDEODRIVER="x11")
    if internal_capture is not None:
        internal_capture.parent.mkdir(parents=True, exist_ok=True)
        env["JUDAS_TERRAIN_SCREENSHOT"] = str(internal_capture)
        state["internal_capture_path"] = str(internal_capture)
    command = [str(runtime), str(copied / project.name)]
    stdbuf = shutil.which("stdbuf")
    if stdbuf and not args.no_stdbuf:
        command = [stdbuf, "-oL", "-eL", *command]
        state["stdout_wrapper"] = "stdbuf -oL -eL"
    else:
        state["stdout_wrapper"] = "none"
        state["warnings"].append("stdout wrapper disabled or unavailable; buffered logs can delay observations")
    state["command"], state["sdl_video_driver"] = command, "x11"
    stop_requested = False
    def signal_stop(_signum, _frame):
        nonlocal stop_requested
        stop_requested = True
    old_handlers = {sig: signal.signal(sig, signal_stop) for sig in (signal.SIGINT, signal.SIGTERM)}
    first = latest = final = final_save = None
    observations = 0
    log_offset, pending = 0, ""
    observed_speeds = set()
    observed_pauses = 0
    observed_controlled = 0
    missing_pause_fields = 0
    previous_metric = None
    started = None
    launched = time.monotonic()
    window = None
    process = None
    raw_path = output / "runtime.log"
    metrics = output / "metrics.jsonl"
    samples = output / "observations.jsonl"
    next_metric, next_capture = 0., args.seconds / 2
    duration_reached = False

    def read_log():
        nonlocal first, latest, final, final_save, observations, log_offset, pending
        nonlocal observed_pauses, observed_controlled, missing_pause_fields
        with raw_path.open("r", encoding="utf-8", errors="replace") as log:
            log.seek(log_offset); pending += log.read(); log_offset = log.tell()
        lines = pending.split("\n"); pending = lines.pop()
        if len(pending) > 1024 * 1024:
            state["warnings"].append("Oversized incomplete log line discarded by observer")
            pending = ""
        for line in lines:
            try:
                parsed = observation(line)
            except (ValueError, json.JSONDecodeError) as error:
                state["warnings"].append("Malformed game observation: " + str(error))
                continue
            if not parsed:
                continue
            kind, value = parsed
            if kind == "WILLOW_TOWN":
                if first is None:
                    first = value
                latest = value; observations += 1
                observed_speeds.add(value.get("speed"))
                observed_pauses += int(value.get("paused") is True)
                observed_controlled += int(value.get("controlled") is not None)
                missing_pause_fields += int("paused" not in value)
            elif kind == "WILLOW_WATCH_FINAL":
                final = value
            else:
                final_save = value
            with samples.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps({"utc": utc(), "wall_seconds": None if started is None else time.monotonic() - started,
                    "kind": kind, "value": value}, ensure_ascii=False) + "\n")

    try:
        with raw_path.open("wb") as raw:
            process = subprocess.Popen(command, cwd=copied, env=env, stdout=raw, stderr=subprocess.STDOUT)
        state["pid"], state["launched_utc"] = process.pid, utc()
        write_json(output / "status.json", state)
        while process.poll() is None and not stop_requested:
            read_log()
            now = time.monotonic()
            if window is None:
                window = x11.find(process.pid)
            if started is None:
                if window is not None and first is not None:
                    if first.get("speed") != 3:
                        raise RuntimeError("The actual game observation does not report speed 3")
                    started = now
                    state.update(state="observing", observation_started_utc=utc(), window_id=window,
                                 startup_seconds=now-launched, first_observation=outcomes(first))
                    if not args.no_captures:
                        state["captures"].append(capture(x11, window, process.pid, output, "start"))
                    print("Visible observation started: pid", process.pid, "for", args.seconds, "real seconds", flush=True)
                elif now - launched > 60:
                    raise RuntimeError("No owned visible window and valid game observation within 60 seconds")
            if started is not None:
                # A screenshot can take a moment; resource timestamps must use
                # the actual current clock, not the pre-capture loop timestamp.
                elapsed = time.monotonic() - started
                if elapsed >= next_metric:
                    metric = memory_sample(process.pid)
                    metric.update(utc=utc(), wall_seconds=elapsed)
                    if previous_metric and "cpu_seconds" in metric and "cpu_seconds" in previous_metric:
                        interval = elapsed - previous_metric["wall_seconds"]
                        metric["process_cpu_percent"] = 100 * (metric["cpu_seconds"] - previous_metric["cpu_seconds"]) / interval
                    with metrics.open("a") as stream:
                        stream.write(json.dumps(metric) + "\n")
                    previous_metric = metric
                    state.update(observed_wall_seconds=elapsed, observations=observations,
                                 last_observation=outcomes(latest), last_metric=metric)
                    write_json(output / "status.json", state)
                    print("Observed", round(elapsed, 1), "s; game time", latest.get("time") if latest else None, flush=True)
                    next_metric = elapsed + args.sample_seconds
                if not args.no_captures and elapsed >= next_capture and next_capture < args.seconds:
                    state["captures"].append(capture(x11, window, process.pid, output, "midpoint"))
                    next_capture = args.seconds
                if elapsed >= args.seconds:
                    duration_reached = True
                    break
            time.sleep(.2)

        read_log()
        if started is None and process.poll() is not None:
            state["startup_error"] = {
                "message": "Runtime exited before a visible, valid observation could start",
                "exit_code": process.poll(), "runtime_log_tail": log_tail(raw_path)}
        state["observation_finished_utc"] = utc()
        state["observed_wall_seconds"] = 0 if started is None else time.monotonic() - started
        state["duration_reached"] = duration_reached
        if process.poll() is None and window is not None:
            sent = x11.finish_key(window, process.pid)
            state["finish_key_sent_to_owned_window"] = sent
            deadline = time.monotonic() + 15
            while process.poll() is None and time.monotonic() < deadline:
                read_log()
                if final is not None and final_save and final_save.get("state") in ("completed", "failed"):
                    break
                time.sleep(.2)
            if not args.no_captures:
                state["captures"].append(capture(x11, window, process.pid, output, "finished"))
        read_log()
        saved = bool(final_save and final_save.get("state") == "completed")
        paused = bool(final and final.get("paused"))
        leave = bool(args.leave_paused and saved and paused and process.poll() is None)
        if process.poll() is None and not leave:
            if window:
                state["window_close_sent"] = x11.close_window(window, process.pid)
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                state["warnings"].append("Owned window did not close; sent SIGTERM to this child only")
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    state["warnings"].append("Owned child did not terminate; killed this child only")
                    process.kill(); process.wait(timeout=5)
        read_log()
        state.update(state=("startup_failed" if "startup_error" in state else
                   "completed" if duration_reached and saved and paused and observed_speeds == {3} and not observed_pauses else "incomplete"),
            runtime_left_running=leave and process.poll() is None,
            runtime_alive_at_summary=process.poll() is None,
            runtime_lifetime_after_observer_exit_verified=False,
            child_lifetime_note="Leaving a paused child does not detach it from an outer task/process manager. Use an independent desktop/session if it must survive caller cleanup. This summary only checks liveness before observer exit.",
            runtime_exit_code=process.poll(), observations=observations,
            final_observation=outcomes(final or latest), final_save=final_save,
            observed_regular_speeds=sorted(observed_speeds, key=lambda x: str(x)),
            regular_paused_observations=observed_pauses,
            regular_controlled_observations=observed_controlled,
            regular_observations_missing_paused_field=missing_pause_fields,
            continuous_state_observation=False,
            snapshot_limit="Periodic game-time snapshots cannot prove an absence of pauses between samples; compare actual game-clock progression with measured wall time.",
            stopped_by_signal=stop_requested)
        if started is not None and first and (final or latest):
            sim_delta = (final or latest).get("time", 0) - first.get("time", 0)
            state["observed_game_seconds"] = sim_delta
            state["game_seconds_per_wall_second"] = sim_delta / max(.001, state["observed_wall_seconds"])
            state["game_clock_difference_from_selected_3x_seconds"] = sim_delta - 3 * state["observed_wall_seconds"]
        if observations and observed_speeds != {3}:
            state["warnings"].append("Not every regular observation reported speed 3; no uninterrupted-3x claim")
        if internal_capture is not None:
            state["internal_capture_created"] = internal_capture.is_file()
            if internal_capture.is_file():
                state["internal_capture_sha256"] = sha(internal_capture)
        if observed_pauses:
            state["warnings"].append("The game reported paused regular observations; this was not an uninterrupted 3x run")
        write_json(output / "status.json", state)
        write_json(output / "summary.json", state)
        if "startup_error" in state:
            print("Runtime startup failed (exit", state["startup_error"]["exit_code"], "):",
                  "\n".join(state["startup_error"]["runtime_log_tail"]), flush=True)
        print("Observation", state["state"], "; final save", final_save, flush=True)
        return 0 if state["state"] == "completed" else 1
    except Exception as error:
        state.update(state="failed", error=str(error), failed_utc=utc())
        if started is None:
            state["startup_error"] = {"message": str(error),
                "exit_code": process.poll() if process is not None else None,
                "runtime_log_tail": log_tail(raw_path)}
        write_json(output / "status.json", state); write_json(output / "summary.json", state)
        if process is not None and process.poll() is None:
            if window:
                x11.close_window(window, process.pid)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=5)
        raise
    finally:
        x11.close()
        for sig, handler in old_handlers.items():
            signal.signal(sig, handler)


if __name__ == "__main__":
    raise SystemExit(main())
