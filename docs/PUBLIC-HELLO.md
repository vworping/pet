# Public Hello and NFC cards — 2026-09-28

## Hello framework redesign — 2026-10-12

Hello now uses the OS Archivo and DM Sans fonts throughout. Introduction has a
larger, left-aligned bracket greeting that cycles words in the single OS green;
the phone-form state uses a static “Your turn.” heading. Navigation and process
steps retain the standard OS styling. Guest details and NFC actions share a
consistent column grid, with device status on one full-width line. “Prepare tag”
retains the existing NFC arm/review/save behavior without the “Card time” copy.

The preview has one context-sensitive demo button, Reset, and the existing
motion control (hidden when the portfolio supplies its page-level control).
The clean demo QR opens https://mindofaxd.com/pet; it never creates a live
introduction. Device QR codes continue to use the encounter URL. Both use an
unbroken matrix and a quiet border, without text over the code.

Native tests cover loading continuity, all Hello/NFC states, long guest text,
and 100 navigation transitions. Browser checks cover the simplified flow,
single-line system labels, original OS fonts, and mobile screen bounds.
The temporary ESP-IDF SDK is absent, so a full device build and physical flash
remain unverified. Native rendering and the portfolio build were verified.

## Interactive OS preview parity — 2026-10-07

The portfolio embeds a snapshot of this browser preview. After updating and
checking the files under `design/`, run `node site/sync-pet.mjs` from the sibling
`axd-site` directory to refresh the snapshot and rebuild its local preview. The
sync preserves portfolio copy, enclosure photos, and the separate portfolio
adapters; it does not publish or push. Firmware-only changes still need to be
mirrored in the browser preview before syncing.

`design/pet-badge-concept.html` now follows the current layouts in
`firmware/main/pet_ui.c`: the animated bracketed home greeting, current project
buttons, Inside PET hardware panel, Fraunces Hello titles, active step bar,
opening indicator, phone-form view, expired-link view, and connected card panel.
The browser loads local copies of the same font sources used by the firmware.
The previous HTML preview still contained the older home and local Wi-Fi Hello
designs even though the firmware and native screenshots had already moved on.

Preview controls simulate link readiness, phone events, and a card tap. The QR
is a labeled, non-scannable placeholder; no live introduction or card is created.
The browser checks in `.local/check-os-parity.cjs` cover greeting motion, all
three Hello steps, card review/save, expiry, the Hello count, and three preview
widths. Constellation retains its current print cards and inspection interaction.


## Title spacing and loading continuity — 2026-09-29

The introduction left-aligns a smaller regular-weight “A” and “hello.” around a
larger bold “small”, with the title moved lower. The phone-form view
left-aligns “Hello...” above a single-line “say it back?”. Both use the existing
GREEN accent, with no added teal. The headings use Fraunces throughout; Connect
uses medium weight, while the opening title contrasts regular and bold.
The loading indicator moves more slowly and keeps its animation object while
Wi-Fi, clock, retry, and error captions update, avoiding full-screen rebuilds
and animation restarts during opening. Motion preference changes still apply.

Native UI, protocol, and socket checks pass, including preserving the loading
indicator through a clock-stage transition. The firmware build passes. The update was flashed to the connected badge;
flash hashes verified. Perceived animation smoothness still needs an on-device
visual check. Flash log: `.local/title-flash.log`.
Rendered preview: `.local/title-preview.png`. Build and native test logs:
`.local/title-build.log` and `.local/title-native.log`.

## Hello refinement — 2026-09-29

Hello now uses the OS green palette, a three-line “A / small / hello.” title,
a shorter subtitle divider, and a tighter “Hello... / say it / back?” layout.
The topic sits below “What's up?” and Done is a standard filled button.
The PET project page uses the same serif family and lists the linked
[Waveshare display](https://www.amazon.com/dp/B0DD7N19FT),
[NULLLAB RC522 I2C reader](https://www.amazon.com/dp/B0H8CGVYVV), and
[3.7 V, 1000 mAh LiPo](https://www.amazon.com/dp/B0FT3C1WRP).

The opening indicator animates without inventing a completion percentage;
its label follows actual Wi-Fi, clock, link creation, and retry states.
Clock sync starts after Wi-Fi connects, with no random startup delay and
alternate time providers after a timeout. HTTPS requests temporarily disable
modem sleep and restore the previous power setting afterward. Certificate
validation remains enabled. Request timing is logged without the bearer URL.

Native layout, loading animation, long text, QR decoding, cloud scheduling,
connection reuse, and power restoration checks passed. Preview:
`.local/hello-refinement-preview.png`. Final release and checksums:
`.local/hello-refinement-release/`.

## Hello layout update — 2026-09-29

The badge's opening, introduction, phone form, expired-link, and completed
Hello screens now share a Fraunces soft serif treatment and pink accents.
The step bar highlights only the current stage: 01 / Introduction while
opening or waiting for a scan, 02 / Say hello after the form opens or a link
ends, and 03 / Connect after submission. The form state keeps the QR visible
beside “Hello... say it back?”. The completed view underlines the guest name
and uses an underlined `[done]` action. Long names and topics fit with smaller
type; the supported guest character set is covered by all guest font sizes.

Opening Hello wakes the cloud worker immediately. The worker reuses its HTTPS
connection, discards failed connections, and prioritizes creation and polling
over old-link cleanup and pending card assignments. Those background requests
resume when the active introduction finishes. The opening screen uses the
final layout while waiting for a confirmed QR URL.

Verification: native UI and protocol checks passed under sanitizers, including
single-step highlighting, all Hello states, long guest text, card controls,
navigation, and 100 transitions. Both rendered QR screens decoded to the
expected fixture URL. Cloud tests passed for durable outbox behavior,
POST/GET connection reuse, failure recovery, scheduling priority, and
cancellation during an HTTP request. The compressed font assets fit the
existing 4 MiB app partition (3,992,272-byte firmware).

Built and flashed to the connected PET; flash hashes verified. USB startup
confirmed LCD, GT911 touch, and RFID reader initialization. Live opening
timing remains unmeasured because the badge's saved Wi-Fi connection was
unavailable. Release files and checksums: `.local/hello-redesign-release/`.
Screen preview: `.local/hello-redesign-preview.png`.

## Public service

Live site: https://pet-hello.vworp.chatgpt.site

The guest uses their own internet connection. PET uses ordinary/open Wi-Fi in station mode, opens a short-lived encounter link, and displays its URL QR. Hello no longer starts the private access point. A submitted introduction is stored by the hosted service before PET receives it, so temporary badge disconnects do not lose it.

## On the badge

1. Settings → Wi-Fi → Scan → select a network → Connect.
2. Home → Hello. Scan its QR on a phone and submit name/topic with consent.
3. On the guest screen: Add to card → tap a blank writable NFC sticker (or a card already linked to this PET site) → Save to card. PET creates the unlisted card page over Wi-Fi, writes its permanent link to a blank tag, and verifies the write before offering Save.
4. **Saved on PET** means the assignment is committed to NVS. **Updated** requires acknowledgement from the hosted server. Only Updated confirms the public page has changed.
5. Done returns home. Pending assignments keep syncing. The eight-entry queue and current encounter ID survive restart.

The reader accepts an HTTPS NDEF URL only when its origin exactly matches the configured host and its path is `/c/<32 lowercase hex characters>`. During Add to card, PET creates a card record with a random ID and retries with that same ID if a request times out. It writes only a canonically empty, writable NTAG213-profile sticker and reads the complete NDEF image back before continuing. Existing data is never overwritten by this flow. Unsupported or already-used cards are rejected by the service. While the same Hello is open, Add to card can replace a permanently rejected queue entry with a new card. If the Hello was already finished, review the unassigned introduction in the owner dashboard.

## Pages and privacy

- `/h/<id>` accepts one introduction for ten minutes; repeated identical submissions are safe.
- `/c/<id>` is a permanent, unguessable card address. There is no public directory or sitemap.
- Pages have noindex/nofollow/noarchive and no-referrer headers. These are unlisted links, not authentication: anyone with a card link can open it.
- Names, timestamps, consent version and topics are private owner records. Only a topic that the guest explicitly opts to share is copied to the card page. Names are not published on cards.
- The owner dashboard supports JSON export and deletion, including clearing the deleted encounter’s shared topic from its card.
- Stable encounter IDs support later analytics. No graph, person deduplication, tracking scripts, inferred guest-to-guest relationships, contact information collection, or location tracking has been added.

## Owner access and provisioning

Owner URL: `.local/public-hello-owner-url.txt` (private; do not publish).
Credentials: `.local/pet-cloud-credentials.json` (mode 0600; never commit).
Hosted source: `cards/hosted/`; the old `cards/server.py` remains a separate local bench.

The badge's endpoint/device key is stored in its `pet_cloud` NVS namespace. Initial endpoint setup is over USB:

```
source scripts/env.sh
python scripts/provision-cloud.py
```

Card creation is part of the on-device **Card time** flow; no card URL needs to be prepared by hand. The USB utility remains available for bench recovery and deliberately selected tags. It uses the exact observed UID, creates/reuses a hosted card, writes the HTTPS NDEF URL, and requires full read-back verification:

```
python scripts/provision-card.py --uid EXACT_UPPERCASE_UID
```

An existing dashboard card can be selected with `--card CARD_ID`. `.local/nfc-inventory.json` remembers the permanent mapping so retries keep the same address. Hold one sticker still throughout the write; no permanent locking is used. The first hosted card is saved in `.local/first-public-card.json`. Sticker `5391166BB50001` contains its permanent URL, with full NDEF read-back verification recorded in `.local/nfc-inventory.json`.

Serial diagnostics: `pet-status`, `pet-wifi-scan`, `pet-hello`, `pet-cancel`, `pet-card-arm`, `pet-card-save`, `pet-done`. Status omits guest name/topic and credentials, but includes the current bearer Hello URL; keep captures private. Serial access on this board can reboot PET.

## Networking

Wi-Fi retries continue after the initial attempts. Scanning an unreachable saved network now pauses reconnection, performs the scan, then resumes reconnecting. The recabled board returned 10 network entries in `.local/public-hello-wifi-scan.log`.

Enterprise/eduroam username, EAP method and certificate provisioning are not yet implemented. Do not treat password-only settings as enterprise support. A working ordinary/open network and internet access are required. HTTPS checks the certificate bundle and waits for SNTP clock synchronization. The guest phone never needs to join PET's network.

## Verification

- Hosted integration suite: role isolation, submission idempotency, expiry/cancellation, competing submissions/assignments, consent/private data separation, deletion/redaction and input bounds pass.
- Automatic card-page creation is published to the live Site, and its matching firmware has been flashed to PET. The blank-tag write flow still needs a physical sticker interaction check.
- Browser: preserved Hello styling, form submission, owner records, card creation and assigning a completed conversation verified in local preview.
- Native UI: card review/save controls, bounds, navigation, QR, Unicode and existing layout checks pass. Card review render inspected.
- Actual firmware queue functions pass AddressSanitizer/UBSan tests with injected NVS write/commit failures, duplicate guards, exact-host checks, capacity, retained pending records after Done, and failed-card retry.
- NDEF parser/writer, interrupted writes, locks, profiles, CRC, and restore tests pass.
- Initial public Hello firmware saved in `.local/public-hello-release/`; the subsequent NFC feedback fix is in `.local/card-tap-fix-release/`, both with SHA-256 hashes. The feedback fix passed the cloud state tests and was built and flashed successfully. ESP-IDF build and USB flash pass. Reader reports 0x28 / VersionReg 0x92, display and touch start normally.
- Live submitted Hello verified on PET over Wi-Fi: USB status reported connected and submitted, matching the hosted encounter. The introduction recovered after a USB-triggered restart. After recabling, sticker `5391166BB50001` was detected, found to have no usable URL, then provisioned with the first permanent card address and read-back verified. The live physical interaction-to-card assignment then succeeded: the original submitted encounter points to that card, its public revision advanced to 2, and its public note matches the topic the guest opted to share. Verification is saved in `.local/card-live-verification.json`.

Settings’ Hellos count reflects saved introductions in the hosted service. PET refreshes it every five seconds while online and idle, including after deletions, and caches the last count across restarts. Active introductions take priority over count refreshes.
