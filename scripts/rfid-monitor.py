#!/usr/bin/env python3
"""Capture the RFID bench log; optionally trigger one UID-bound write test."""
import argparse
import glob
import re
import time
from pathlib import Path
import serial

p = argparse.ArgumentParser()
p.add_argument('--port')
p.add_argument('--seconds', type=float, default=20)
p.add_argument('--reset', action='store_true')
ops = p.add_mutually_exclusive_group()
ops.add_argument('--test-uid', help='Exact uppercase UID of the authorized spare tag')
ops.add_argument('--write-url', help='Write an HTTP(S) NDEF link; requires --uid')
ops.add_argument('--erase', action='store_true', help='Clear NDEF and old payload bytes; requires --uid')
p.add_argument('--uid', help='Exact UID for --write-url or --erase')
p.add_argument('--log', default='.local/rfid-reader.log')
a = p.parse_args()
ports = glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/cu.usbserial*')
if not a.port and len(ports) != 1:
    p.error('Specify --port: expected one USB serial device')
target_uid = a.test_uid or a.uid
if (a.write_url or a.erase) and not a.uid:
    p.error('--write-url and --erase require --uid')
if a.uid and not (a.write_url or a.erase):
    p.error('--uid requires --write-url or --erase')
if target_uid and not re.fullmatch(r'(?:[0-9A-F]{8}|[0-9A-F]{14}|[0-9A-F]{20})', target_uid):
    p.error('--test-uid must be an 8, 14, or 20 digit uppercase hexadecimal UID')
if a.reset and target_uid:
    p.error('Capture startup first; run a write test separately without --reset')
if a.write_url and (not a.write_url.startswith(('https://', 'http://')) or len(a.write_url) > 139 or any(ord(c) <= 32 or ord(c) >= 127 for c in a.write_url)):
    p.error('URL must be ASCII HTTP(S), at most 139 characters, with no whitespace')
Path(a.log).parent.mkdir(parents=True, exist_ok=True)
s = serial.Serial(port=None, baudrate=115200, timeout=0.25)
s.dtr = False
s.rts = False
s.port = a.port or ports[0]
s.open()
with s, open(a.log, 'ab', buffering=0) as f:
    if a.reset:
        s.rts = True
        time.sleep(0.2)
        s.rts = False
    if target_uid:
        # Some USB adapters reset the chip on open; allow the worker to start.
        time.sleep(3)
        if a.write_url:
            command = 'rfid-url ' + target_uid + ' ' + a.write_url
        elif a.erase:
            command = 'rfid-erase ' + target_uid
        else:
            command = 'rfid-test ' + target_uid
        s.write((command + '\n').encode())
        s.flush()
    end = time.monotonic() + a.seconds
    while time.monotonic() < end:
        data = s.read(4096)
        if data:
            f.write(data)
            print(data.decode(errors='replace'), end='', flush=True)
