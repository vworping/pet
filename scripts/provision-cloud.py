#!/usr/bin/env python3
"""Provision the saved hosted endpoint/key over USB without printing credentials."""
import argparse,json,time,glob
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port');p.add_argument('--credentials',default='.local/pet-cloud-credentials.json');a=p.parse_args()
c=json.loads(Path(a.credentials).read_text());ports=glob.glob('/dev/cu.usbmodem*')
if not a.port and len(ports)!=1:p.error('Specify --port')
s=serial.Serial(port=None,baudrate=115200,timeout=.5);s.dtr=False;s.rts=False;s.port=a.port or ports[0];s.open()
with s:
 time.sleep(3);s.reset_input_buffer();s.write(('pet-cloud '+c['base']+' '+c['device_key']+'\n').encode());s.flush();end=time.monotonic()+12;data=b''
 while time.monotonic()<end:
  data+=s.read(2048)
  if b'Public Hello setup saved' in data:print('Public Hello endpoint and badge credential saved.');break
 else:raise SystemExit('Provisioning was not confirmed; finish the current Hello and sync queued cards before retrying.')
