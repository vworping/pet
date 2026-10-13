# PET OS 6.20261013.1

This release includes the current Hello typography and flow, NFC controls, Constellation grid and inspection layouts, UMB eduroam support, and venue Wi-Fi service diagnostics.

Hello no longer waits forever for public time servers in the default SDK configuration. Certificate chain/hostname verification and the existing certificate-date policy are unchanged. The Wi-Fi screen distinguishes network association from Hello service reachability. WPA3 personal joins permit both SAE password-element methods.

## Verification

- ESP-IDF v5.4.2 build: passed; firmware fits the existing 4 MiB application partition.
- Native display/navigation, enterprise certificate adapter, protocol and socket tests: passed.
- Cloud transport, scheduling and durable outbox tests under sanitizers: passed. Tests simulate an unset clock with no SNTP reply, and also check that builds enabling certificate dates still block HTTPS until time is synchronized.
- Device flash and boot: passed on the connected ESP32-S3. Bootloader, partition table, and firmware write hashes verified. USB confirms version 6.20261013.1, display/touch initialization, working commands, and ten scanned networks. The NFC reader is not detected on this board; USB setup now stays available and reader initialization retries safely.
- Hello provisioning: the existing endpoint/device key was saved over USB, and the hosted service accepted the credential (HTTP 200) with the ESP32 HTTP client user agent. A live device-to-service test still requires an available Wi-Fi network.
- Website publication: deployed from portfolio commit `cbd460b`. The live `pet-os.js` and `pet-os.css` match the project preview byte for byte. Browser flow and screen bounds pass at desktop, tablet, and phone widths.

An actual BU eduroam connection and a future Accenture event network still require on-site validation. Browser sign-in pages, company-specific trust profiles and 5 GHz-only networks are not covered by ordinary password entry. NFC tag interaction requires the physical reader and a supported tag.

Firmware SHA-256: `dd34b2db3cd5dc8137b30173865f19b68939c1b4dc1bbb7f4f0fbdca88ad9f69`.
