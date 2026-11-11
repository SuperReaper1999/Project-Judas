#!/usr/bin/env python3
"""Measure an unmodified Linux/X11 editor using the optional SDL present observer.

Use a disposable project copy. Each run owns a fresh preferences/output directory
and closes only the window belonging to the process it launched.
"""
import argparse
import csv
import ctypes as C
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from performance.metrics import summarize


class ClientMessage(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('message_type', C.c_ulong),
                ('format', C.c_int), ('data', C.c_long * 5)]


class KeyEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('root', C.c_ulong),
                ('subwindow', C.c_ulong), ('time', C.c_ulong), ('x', C.c_int), ('y', C.c_int),
                ('x_root', C.c_int), ('y_root', C.c_int), ('state', C.c_uint),
                ('keycode', C.c_uint), ('same_screen', C.c_int)]


class Event(C.Union):
    _fields_ = [('type', C.c_int), ('client', ClientMessage), ('key', KeyEvent), ('pad', C.c_long * 24)]


class X11:
    def __init__(self):
        self.x = C.CDLL(ctypes.util.find_library('X11'))
        signatures = {
            'XOpenDisplay': ([C.c_char_p], C.c_void_p),
            'XDefaultRootWindow': ([C.c_void_p], C.c_ulong),
            'XInternAtom': ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
            'XQueryTree': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                            C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)], C.c_int),
            'XGetWindowProperty': ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_long, C.c_long,
                                    C.c_int, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                                    C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                                    C.POINTER(C.POINTER(C.c_ubyte))], C.c_int),
            'XGetGeometry': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                              C.POINTER(C.c_int), C.POINTER(C.c_uint), C.POINTER(C.c_uint),
                              C.POINTER(C.c_uint), C.POINTER(C.c_uint)], C.c_int),
            'XTranslateCoordinates': ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int,
                                       C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)], C.c_int),
            'XFree': ([C.c_void_p], C.c_int),
            'XStringToKeysym': ([C.c_char_p], C.c_ulong),
            'XKeysymToKeycode': ([C.c_void_p, C.c_ulong], C.c_uint),
            'XSetInputFocus': ([C.c_void_p, C.c_ulong, C.c_int, C.c_ulong], C.c_int),
            'XSendEvent': ([C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)], C.c_int),
            'XFlush': ([C.c_void_p], C.c_int),
            'XCloseDisplay': ([C.c_void_p], C.c_int),
        }
        for name, (args, result) in signatures.items():
            getattr(self.x, name).argtypes = args
            getattr(self.x, name).restype = result
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError('X11 display is unavailable')
        self.root = self.x.XDefaultRootWindow(self.display)
        self.pid_atom = self.atom('_NET_WM_PID')
        # A window may disappear between querying its parent and its property.
        self.error_handler = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)(lambda *_: 0)
        self.x.XSetErrorHandler.argtypes = [C.c_void_p]
        self.x.XSetErrorHandler(C.cast(self.error_handler, C.c_void_p))

    def atom(self, name):
        return self.x.XInternAtom(self.display, name.encode(), 0)

    def find(self, pid):
        pending = [self.root]
        for _ in range(2048):
            if not pending:
                return None
            window = pending.pop()
            actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
            fmt, data = C.c_int(), C.POINTER(C.c_ubyte)()
            status = self.x.XGetWindowProperty(self.display, window, self.pid_atom, 0, 1, 0, 0,
                                               C.byref(actual), C.byref(fmt), C.byref(count),
                                               C.byref(remaining), C.byref(data))
            matches = status == 0 and fmt.value == 32 and count.value == 1 and bool(data)
            matches = matches and C.cast(data, C.POINTER(C.c_ulong))[0] == pid
            if data:
                self.x.XFree(data)
            if matches:
                return window
            root, parent, children, size = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
            if self.x.XQueryTree(self.display, window, C.byref(root), C.byref(parent),
                                 C.byref(children), C.byref(size)):
                pending.extend(children[i] for i in range(size.value))
                if children:
                    self.x.XFree(children)
        raise RuntimeError('X11 window inventory exceeds diagnostic bound')

    def key(self, window, name):
        self.x.XSetInputFocus(self.display, window, 2, 0)
        code = self.x.XKeysymToKeycode(self.display, self.x.XStringToKeysym(name.encode()))
        if not code:
            raise ValueError('unknown desktop key: ' + name)
        for kind, mask in [(2, 1), (3, 2)]:
            event = Event()
            event.key.type, event.key.display = kind, self.display
            event.key.window, event.key.root = window, self.root
            event.key.keycode, event.key.same_screen = code, 1
            self.x.XSendEvent(self.display, window, 0, mask, C.byref(event))
            self.x.XFlush(self.display)
            time.sleep(.08)

    def screenshot(self, window, destination):
        from PIL import ImageGrab
        root, child = C.c_ulong(), C.c_ulong()
        x, y, width, height, border, depth = C.c_int(), C.c_int(), C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
        if not self.x.XGetGeometry(self.display, window, C.byref(root), C.byref(x), C.byref(y),
                                  C.byref(width), C.byref(height), C.byref(border), C.byref(depth)):
            raise RuntimeError('cannot inspect the launched window geometry')
        if not self.x.XTranslateCoordinates(self.display, window, self.root, 0, 0,
                                            C.byref(x), C.byref(y), C.byref(child)):
            raise RuntimeError('cannot locate the launched window on the desktop')
        self.window_pixels = [width.value, height.value]
        ImageGrab.grab(bbox=(x.value, y.value, x.value + width.value, y.value + height.value),
                       xdisplay=os.environ.get('DISPLAY')).save(destination)
        return [width.value, height.value]

    def close_window(self, window):
        event = Event()
        event.client.type, event.client.display, event.client.window = 33, self.display, window
        event.client.message_type, event.client.format = self.atom('WM_PROTOCOLS'), 32
        event.client.data[0] = self.atom('WM_DELETE_WINDOW')
        self.x.XSendEvent(self.display, window, 0, 0, C.byref(event))
        self.x.XFlush(self.display)


def main():
    from run_whole_engine import sample_process
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--observer', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--mode', choices=['edit', 'play'], default='edit')
    parser.add_argument('--seconds', type=float, default=20)
    args = parser.parse_args()
    if not 2 <= args.seconds <= 300:
        parser.error('duration must be 2–300 seconds')
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, SDL_VIDEODRIVER='x11', SDL_AUDIODRIVER='dummy',
               LD_PRELOAD=str(args.observer.resolve()), JUDAS_PRESENT_UNCAPPED='1',
               JUDAS_PRESENT_CAPTURE=str(args.out / 'present.csv'), JUDAS_PROFILE='1',
               JUDAS_PROFILE_DEPTH='4', JUDAS_PROFILE_OUTPUT=str(args.out / 'profile.json'),
               XDG_CONFIG_HOME=str(args.out / 'preferences'), XDG_DATA_HOME=str(args.out / 'data'))
    command = [str(args.editor.resolve()), str(args.project.resolve())]
    x11, window = X11(), None
    with (args.out / 'application.log').open('w') as log:
        process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 30
            while process.poll() is None and not window and time.monotonic() < deadline:
                window = x11.find(process.pid)
                time.sleep(.1)
            if not window:
                raise RuntimeError('the launched editor did not create its own X11 window')
            time.sleep(5)
            if args.mode == 'play':
                x11.key(window, 'F5')
                time.sleep(5)
            begin = time.monotonic_ns()
            samples = [sample_process(process.pid)]
            deadline = time.monotonic() + args.seconds
            while process.poll() is None and time.monotonic() < deadline:
                time.sleep(.2)
                if process.poll() is None:
                    samples.append(sample_process(process.pid))
            end = time.monotonic_ns()
            (args.out / "os.jsonl").write_text("".join(json.dumps(row) + "\n" for row in samples))
            if process.poll() is not None:
                raise RuntimeError('editor exited during capture')
            dimensions, screenshot_error = None, None
            try:
                dimensions = x11.screenshot(window, args.out / 'editor.png')
            except (OSError, RuntimeError, ImportError) as error:
                screenshot_error = str(error)
                dimensions = getattr(x11, "window_pixels", None)
            x11.close_window(window)
            process.wait(timeout=30)
        finally:
            if process.poll() is None:
                if window:
                    x11.close_window(window)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        pass
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
            x11.x.XCloseDisplay(x11.display)
    rows = list(csv.DictReader((args.out / 'present.csv').open()))
    main_window = rows[0]['window_id'] if rows else None
    # Exclude the single interval straddling the warmup/measurement boundary.
    values = [float(row['interval_ms']) for row in rows
              if row['window_id'] == main_window and begin <= int(row['begin_ns']) <= end
              and int(row['begin_ns']) - float(row['interval_ms']) * 1e6 >= begin]
    if not values or process.returncode or any(row['swap_interval'] != '0' for row in rows):
        raise RuntimeError('invalid editor capture; see retained diagnostics')
    profile = json.loads((args.out / 'profile.json').read_text())
    expected_mode = 'editor Play' if args.mode == 'play' else 'editor edit'
    if not any(frame['mode'] == expected_mode for frame in profile['frames']):
        raise RuntimeError('requested editor mode was not observed')
    first = {task['tid']: task for task in samples[0]['tasks']}
    elapsed = (samples[-1]['time_ns'] - samples[0]['time_ns']) / 1e9
    threads = [dict(name=task['name'], tid=task['tid'],
                    percent_one_core=100 * (task['cpu_ticks'] - first[task['tid']]['cpu_ticks']) /
                                     os.sysconf('SC_CLK_TCK') / elapsed)
               for task in samples[-1]['tasks'] if task['tid'] in first]
    memory = [int(line.split()[1]) * 1024 for row in samples
              for line in row['status'].splitlines() if line.startswith('VmRSS:')]
    result = dict(command=command, mode=args.mode, start_ns=begin, end_ns=end, window_pixels=dimensions, screenshot_error=screenshot_error,
                  frames=summarize(values), process_rss_peak_bytes=max(memory, default=None),
                  threads=threads, cpu_core_equivalents=sum(t["percent_one_core"] for t in threads) / 100,
                  editor_sha256=hashlib.sha256(args.editor.read_bytes()).hexdigest(),
                  project_sha256=hashlib.sha256(args.project.read_bytes()).hexdigest(),
                  note='Primary-window present intervals; retained profiler is bounded. '
                       'Editor chrome/viewport size differs from a full-window export; not a matched-resolution GPU comparison.')
    (args.out / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
