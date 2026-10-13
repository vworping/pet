#!/usr/bin/env python3
"""Select a single USB serial endpoint; refuse ambiguity instead of guessing."""
import glob
import sys
ports = sorted(set(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/cu.usbserial*')))
if len(ports) != 1:
    sys.exit('Expected one USB serial device. Set PET_PORT explicitly. Found: ' + ', '.join(ports))
print(ports[0])
