# PET public Hello

Hosted counterpart to PET's ESP32 firmware. Preserves the original Hello/card styling.

- `/h/<128-bit random id>`: one introduction, ten-minute submission window.
- `/c/<128-bit random id>`: permanent NFC address; public card fields only.
- `/admin#<owner key>`: private dashboard access. Key is removed from the address bar on load; reopen the saved owner link after a reload.
- No directory or sitemap; all responses carry `noindex`, `nofollow`, `noarchive`, `no-store`, and `no-referrer`. These pages are unlisted, not access-controlled once someone has a card link.
- Required explicit consent stores name, topic, timestamp, and consent version privately. Separate optional choices share the name in the card greeting and the topic on the card. Existing introductions default to no name sharing. No email, location, device fingerprint, or analytics scripts.
- Owner can export JSON and delete an encounter, which clears its card topic and personalized greeting. Encounters have stable IDs for later analytics; names are not treated as unique people and no relationships between guests are inferred.
- Owner and badge use different runtime secrets. Public callers cannot enumerate records or assign cards. Authenticated badge assignments are atomic and idempotent. A used card or previously assigned encounter rejects replacement.

## Local development

```
npm ci
npm run db:generate  # only after schema changes; retain deployed migrations
npm run build
npm test
node scripts/preview.mjs
```

Preview: `http://127.0.0.1:8777/admin#local-preview-owner-key-00000000000000`.
Preview credentials and data are local test fixtures, not production credentials.

Production uses Cloudflare-compatible Worker output, D1, and Sites hosting. `PET_ADMIN_KEY` and `PET_DEVICE_KEY` are runtime secrets, never bundled. `db/schema.ts` and generated `drizzle/` migrations define storage. Source and build output contain no encounter data.

## Badge contract

All badge endpoints require `X-PET-Device`:
- `GET /api/device/stats`: current saved introduction count as `{connections}`; excludes unsubmitted and deleted Hellos.
- `POST /api/device/encounters` with `{id}`: idempotently opens Hello; returns URL/expiry.
- `GET /api/device/encounters/<id>`: private introduction and form-open state.
- `POST /api/device/encounters/<id>/close`: cancels an unsubmitted Hello.
- `POST /api/device/assign` with `{event_id, card_id, encounter_id}`: binds a completed Hello to an unused card. Retrying the same event returns success without incrementing revision again.
- `POST /api/device/cards` with `{id}`: creates or returns the badge-generated random card ID and its permanent `/c/<id>` URL. Repeating a request is safe, including after a lost response.

PET stays in Wi-Fi station mode. HTTPS validates certificates and waits for clock synchronization. Its eight-item NVS queue is committed before showing Saved; server acknowledgement is required for Updated. Current encounter ID survives restart and its introduction is fetched again. Failed card conflicts are retained for attention; choose another card while the encounter is still open. Do not hand out a card as updated until PET confirms it.

After **Card time**, PET creates the card record over Wi-Fi. A blank, writable NTAG213-profile tag can be tapped directly on PET; the badge writes and verifies its permanent URL before allowing the conversation to be saved. The same generated ID is retried so a lost HTTP response does not create duplicate pages. The card endpoint uses the existing public card template and D1 row; it does not upload a separate HTML file. Publish the updated worker before enabling this flow on a badge.

The existing Wi-Fi setup supports ordinary/open Wi-Fi. Enterprise credentials for eduroam still require a separate firmware addition and on-campus verification.
