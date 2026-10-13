# Current public Hello service

The public service is now live at https://pet-hello.vworp.chatgpt.site.
Source: [hosted/](hosted/README.md). Badge/card setup and verification:
[Public Hello](../docs/PUBLIC-HELLO.md).

The Python files in this directory remain the original local bench. The notes
below describe that earlier bench and are retained as history.

---

# NFC card assignment bench

The existing cards have NFC stickers. Write a permanent HTTPS address to each
sticker once, then change the page attached to that address in software.
Rewriting a temporary badge IP/session token per encounter would make a poor
take-away card: it expires and works only while connected to the badge.

## What is implemented

- A unique opaque `/c/<id>` address for each card.
- SQLite-backed public title, note and destination; assignment updates retain
  the ID/address, survive process restarts and increment a revision.
- A separate public **How we connected** field (80 characters, matching Hello's
  topic limit). The owner can edit it per card; blank notes stay hidden. Existing
  databases migrate without changing card addresses or clearing assignments.
- A guest page that refreshes assignments within about two seconds.
- An owner dashboard protected by an unguessable token for create/list/update.
- Safe text rendering, HTTPS-only destination links, bounds and input checks.
- Only public card content is stored. No guest contact information is collected;
  a conversation note entered by the owner is visible to anyone with the link.

Run `python3 cards/server.py`. The server listens on **127.0.0.1:8767 only**.
The private owner URL is saved in `.local/card-owner-url.txt`; keep its fragment
secret. Open the dashboard, create a test card, open its guest page, then edit
and assign. Both tabs are currently open with a test ATLA/EAR card.

Use `--public-base https://YOUR-CARD-HOST` only when a permanent host exists.
That flag configures printable NFC addresses; it does not publish anything.
The local test links must not be written to the physical stickers.

## Not yet connected

- No NFC stickers have been written, locked or verified on a phone.
- No public host or DNS entry has been deployed.
- This local bench is not wired to the physical badge. A public NFC tap cannot
  notify an offline badge by itself. It needs an authenticated internet relay
  while the badge is online, or a real phone companion as a BLE bridge.
- The user confirmed there is no NFC reader attached to the badge. A direct
  card-to-badge tap needs hardware plus a driver. See
  [NFC hardware notes](../docs/NFC-HARDWARE.md) for the proposed parts and the
  board's I2C address conflict. No NFC firmware or tap control has been flashed.
- Python's development HTTP server is a local test server, not a production
  HTTPS service. Production needs HTTPS termination, durable storage/backups,
  owner authentication and an authenticated badge event transport.

## Real-time test already performed

Created a card and opened its guest page. Reassigned its title/note from Andre
Weiss to ATLA/EAR in the owner tab; the already-open guest tab updated at the
same address. Store tests confirm persistence and idempotency. HTTP tests
confirm unauthorized create/list/update are rejected and public reads work.

Before handing cards out: choose the permanent host, deploy the card pages,
write each full HTTPS card URL to its sticker with an NFC-capable phone/reader,
read it back, and test the same tag before/after a software reassignment and
away from the badge Wi-Fi. Do not lock tags until those checks pass.
