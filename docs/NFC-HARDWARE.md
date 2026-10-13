# NFC card handoff — hardware proposal

The user wants Hello's topic to become **How we connected** on the page opened
by a physical NFC-stickered card. They confirmed there is no reader attached.
The working v6 Hello firmware is unchanged. The card bench now stores and
displays the conversation note, but automatic transfer from the badge is not
implemented and no public card host exists yet.

## Size constraint and revised prototype parts

The user requires hardware no larger than **1.25 × 1.25 × 0.4 inches**
(31.75 × 31.75 × 10.16 mm) and explicitly prohibits changing the enclosure.
The user clarified that this limit applies to **everything together**, including
reader, interface hardware, antenna and wiring. The individually small modules
below are research candidates, not a qualified shopping list.

| Part | Manufacturer dimensions, L × W × thickness | Inches | Listed price |
| --- | --- | --- | --- |
| [ELECHOUSE PN7160 MINI V1 — I2C](https://www.elechouse.com/product/pn7160-mini-v1-i2c/), SKU PN7160_MINI_I2C | 26.937 × 20.066 × 5.5 mm | 1.061 × 0.790 × 0.217 | $29.80 |
| [M5Stamp Extend I/O](https://shop.m5stack.com/products/m5stamp-extend-i-o-module-stm32f0), SKU S002 | 16 × 15 × 4.7 mm | 0.630 × 0.591 × 0.185 | $6.50 |

Prices checked September 24, 2026; cables, tax and shipping are additional.
Use the reader's included **25 × 10 mm antenna**, not the included 50 × 40 mm
antenna. Its thickness was not specified in the reviewed documentation.
The matching 6-pin 1.25 mm host cable is sold separately. The Stamp supports
direct soldered wires; do not add tall breadboard headers for this installation.

Sources: reader [mechanical and connector specifications](https://www.elechouse.com/docs/pn7160-mini-v1-i2c/datasheet.html)
and Stamp [dimension drawing](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1031/S002-model-size-STAMP-IO.pdf).
The reader specifies ±0.20 mm PCB outline tolerance and explicitly requires
extra space for mating plugs and cable bends. The Stamp dimensions describe the
module, not an attached Grove plug or header assembly.

**This is a component-size check, not a completed enclosure-fit validation.**
Do not stack the two full module envelopes: 5.5 + 4.7 = 10.2 mm, already above
the 10.16 mm limit before insulation and wiring. Separate mounting locations
do not satisfy the user's combined-space requirement. Cable routing, antenna
placement and remaining battery clearance still need
physical verification. No enclosure files have been changed. No new firmware
has been flashed for this proposal.

The previous Waveshare PN532 HAT / DFR0627 proposal is superseded. In
particular, the DFR0627 alone is 32.5 mm long, exceeding the new 31.75 mm limit.

## Electrical integration proposal

The [Waveshare board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-5)
reserves I2C ranges 0x20–0x27 and 0x30–0x3F for the CH422G and 0x5D for touch.
PN532 uses 7-bit address 0x24 (often written as the 8-bit value 0x48), confirmed
by [Adafruit's PN532 driver](https://github.com/adafruit/Adafruit-PN532/blob/master/Adafruit_PN532.h).
A direct PN532 connection to that bus would therefore conflict. The exposed
RS485 terminals are not TTL UART pins. The current board driver also explicitly
writes to CH422G at 0x24 and 0x38 during touch reset.

The PN7160 MINI instead defaults to **0x28** and the
[Stamp IO defaults to **0x45**](https://docs.m5stack.com/en/stamp/stamp_io),
outside those documented onboard address ranges. Both use 3.3 V logic.
Proposed wiring: share badge I2C SDA/SCL/GND with both modules; connect reader
IRQ to a Stamp digital input and reader VEN to a Stamp digital output. The
Stamp supplies two control pins, not a UART bridge. Never copy the reader's
generic ESP32 GPIO4/GPIO5 example onto this badge: those pins are already used
by the touch/display hardware.

The [reader hardware guide](https://www.elechouse.com/docs/pn7160-mini-v1-i2c/hardware-integration.html)
describes an active-high IRQ held until data is read and VEN enable/reset.
Polling IRQ through the Stamp and driving VEN through it is a proposed custom
transport adaptation; the stock direct-GPIO example does not implement this.
Use bounded transactions and a worker task so polling cannot block LVGL.

Power the modules from a verified suitable supply with common ground. The
[Stamp schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/568/Sch_StampIO.pdf)
shows a 5 V input and onboard 3.3 V regulator. Power the reader separately
from that small regulator; its input accepts 3.3–5.5 V and its datasheet calls
for at least 300 mA available supply capacity. Verify the badge's spare power
budget and match the reader's software power preset to its actual input.

Driver integration, power measurements, reading the user's sticker type and
RF performance inside the existing enclosure remain untested. No purchase has
been made and no physical fit is being certified from PCB dimensions alone.

## Intended badge flow

After an introduction: **Add to card → Tap card here → Review → Save**.
Read the sticker's existing permanent card URL and bind its card ID to the
current topic. Keep the sticker URL stable. Persist pending assignments on the
badge and show a distinct pending state until the authenticated card service
acknowledges an update. Never report a card as saved based only on detecting it.
No reader should produce an explicit setup state, not an endless tap prompt.

The card bench supports manual conversation notes today; it does not pretend
to be connected to a physical reader or badge.

## Connected reader bench check — 2026-09-26

User connected the nulllaborg RFID-I2C reader to VOUT/GND/SDA/SCL.
User measured VOUT and both disconnected reader signal lines at 3.3 V.
USB-only power; battery disconnected. PET starts normally with all four wires.
A bounded, address-only startup probe was added to firmware/main/main.c,
built with the saved v6 configuration and flashed over /dev/cu.usbmodem101.
The live startup log confirms: `Device acknowledged at 0x28; card reading not yet tested`.
GT911 and the PET UI also initialized successfully. Evidence:
`.local/rfid-detection.log`, `.local/rfid-build.log`, `.local/rfid-flash.log`.
This verifies I2C acknowledgement at the current supply voltage, not card
reading, RF range, reliable 3.3 V operation across conditions, or URL handling.
The manufacturer's module documentation still specifies 5 V supply.

### Reader driver bench workflow

`pet_rfid.c` adds an independent worker using the existing GPIO8/GPIO9 bus.
Register access and RF exchanges have bounded timeouts; selection rejects
collisions and handles 4/7/10-byte UIDs with BCC and CRC checks. The worker logs
UID, ATQA, SAK, and one CRC-verified 16-byte read on presentation. Classic uses
factory Key A FFFFFFFFFFFF for block 4; Type 2 reads pages 4–7.

USB bench commands are available through `scripts/rfid-monitor.py` using the
ESP-IDF Python environment (pyserial). Default operation only reads. To request
one write test on an explicitly chosen spare tag:

```
python scripts/rfid-monitor.py --seconds 20 --test-uid EXACT_UPPERCASE_UID
```

The command expires after 30 seconds and applies only to the specified UID.
Classic writes are fixed to block 4. Type 2 writes require a GET_VERSION response
identifying NTAG213/215/216 or the observed vendor-53 NTAG213-compatible
profile, a writable Type 2 capability container, and unlocked page 8. Writes
are fixed to page 8. The test logs original bytes,
writes an XOR test pattern, verifies the read-back, then restores and verifies
the original. It never writes UID, lock, configuration or sector-trailer data.
Keep the tag stationary throughout: power loss or removal during the test can
prevent restoration; preserve the BACKUP log. This is a serial bench workflow,
not an NDEF URL writer or the Hello card-assignment UI.

Protocol references:
- https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
- https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf
- https://github.com/nulllaborg/rfid/blob/main/examples/ESP32_Micropython/mfrc522_i2c.py

First driver build and flash passed. Hardware initialization reports VersionReg
0x92 with PET and GT911 startup successful. Card read/write results follow below.

### Verified on hardware — 2026-09-26

- Correct antenna initialization preserves TxControlReg's reset polarity bit:
  register reads 0x83 with both RF outputs enabled. Receiver gain is 0x70.
- Included card detected as MIFARE Classic, UID DB69AFA9. Authentication/read
  using factory Key A failed; Classic memory read/write remains unverified.
- User-selected blank sticker UID 5391166BB50001: ATQA 0044, SAK 00.
  CRC-verified pages 4–7 read successfully. GET_VERSION payload is
  `00 53 04 02 01 00 0f 03`: vendor-53 NTAG213-compatible profile, not identified
  as genuine NXP. Capability container `e1 10 12 00` advertises 144 writable
  bytes; page-8 static lock bit is clear.
- UID-bound page-8 test backed up `00 00 00 00`, wrote `5a 5a 5a 5a`, verified
  read-back, restored `00 00 00 00`, and verified restoration. Live log:
  `.local/rfid-sticker-write-verified.log`, result
  `pattern=VERIFIED original=RESTORED+VERIFIED`.
- PET and GT911 initialize successfully with the reader worker. Build, flash,
  CRC/corruption, antenna-polarity, profile guards, and restoration fault tests
  pass. Tests are in `tests/rfid/`; run each Python file from the project root.
- Current binary, symbols and configuration saved in `.local/rfid-release/`.
  The prior normal v6 remains in `.local/v6-release/`.
- Results establish this bench read/write at the user-measured 3.3 V supply;
  range and long-term reliability are not characterized. No NDEF URL has been
  written, and no card-assignment UI or service integration was added.

### NDEF link writing — 2026-09-26

Added pet_ndef.c plus UID-bound `rfid-url UID URL` and `rfid-erase UID` commands.
The NDEF writer supports the verified 144-byte NTAG213 profile and its observed
lock-control TLV. It requires writable CC, unlocked static/dynamic user pages,
and preserves the first five control bytes. Writes are confined to pages 4–39.
The message length is set to zero before updating pages and committed last;
every user byte is read back and compared before reporting success.

Hardware result: `https://mindofaxd.com` written to UID 5391166BB50001 and full
read-back verified. Evidence: `.local/rfid-ndef-url.log`. The URL was left on the
sticker for phone testing. Phone scan has not yet been confirmed.

Commands (using the ESP-IDF Python environment):

```
python scripts/rfid-monitor.py --uid 5391166BB50001 --write-url https://mindofaxd.com
python scripts/rfid-monitor.py --uid 5391166BB50001 --erase
```

Erase sets an empty NDEF TLV and zeroes the old payload throughout the remaining
user area, retaining the memory-control TLV. It does not touch UID, capability
container, lock bits, passwords or configuration. No permanent locking is used.
The erase path passed native transaction tests, but has not been executed on the
physical sticker yet. NDEF encoding, capacity limits, page boundaries, lock
refusal and interrupted-write ordering tests also passed. Current firmware
saved in `.local/ndef-release/`; previous reader firmware retained.

Phone and erase verification — 2026-09-26:
User confirmed the phone opened the written link. At their request, erased UID
5391166BB50001. Hardware reported `NDEF ERASE VERIFIED`: full user-memory
read-back matches the empty NDEF image with old payload bytes zeroed. Evidence:
`.local/rfid-ndef-erase.log`. Sticker remains unlocked and reusable.

## Public Hello integration — 2026-09-27

Public encounter pages and unlisted permanent card pages are deployed. Firmware now parses HTTPS NDEF card URLs, requires an explicit review/save, persists pending assignments, and waits for server acknowledgement before displaying Updated. Provisioning helpers are in scripts/provision-cloud.py and scripts/provision-card.py. See [current setup and evidence](PUBLIC-HELLO.md).

After recabling, reader initialization at 0x28 / VersionReg 0x92 and normal touch/display startup were verified again. No physical tag was detected in this session, so RF reading and the complete tap/assignment flow after recabling remain pending.

## On-device card clearing — 2026-09-28

The completed Hello screen now offers **Clear tag** beside **Card time** (the
business-card assignment action).
After the user chooses Clear tag, PET asks them to remove any card and tap one
tag. The reader uses the existing bounded NDEF writer to empty the NDEF message,
zero the previous link payload, and verify the entire writable user image by
read-back. It refuses locked, unsupported, non-NDEF-formatted, or unreadable
tags; UID, capability container, lock bits, passwords, and configuration remain
untouched. The UI reports success only after verification. Clearing removes an
existing phone link and is limited to the verified NTAG213-compatible Type 2
profile.

The firmware including this action and the Hello screen updates built and was
flashed to the attached PET on 2026-09-28. The flash hash verified; startup log
confirmed the LCD, GT911 touch controller, and RFID reader at 0x28 initialized.
The newer handwritten screen layout has now been reflashed with Caveat, an
OFL-licensed font. Its generated LVGL subsets are in `firmware/main/fonts/`;
the license is beside them. The on-screen clear flow itself has not yet been
exercised with a physical tag; the underlying NDEF erase operation was
previously verified on UID 5391166BB50001.
