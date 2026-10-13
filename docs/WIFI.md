# Wi-Fi setup

The connected network is pinned in green above the scan results, including when it is absent from the latest scan. Tap it for connection details, editing, or the outlined **Forget this network** action. Forgetting disconnects and deletes saved credentials. Wi-Fi off retains them.

The join screen has an eye button to show/hide the password. Enterprise scan results open with a username field; for hidden networks, use Manual entry and switch Password only to Username + password. Enter the full institutional username (often user@school.edu). Switching modes preserves text until leaving the form. Reopening the form always hides the password.

## UMass Boston eduroam (default profile)

The firmware enables `CONFIG_PET_UMB_EDUROAM` by default. No personal credentials or separate CA download are needed at build time. On PET, select **eduroam**, enter your **full UMB email address** and password, then Connect. The full `@umb.edu` realm is needed when roaming at BU or another institution. If using Manual entry, choose Username + password.

UMB's [Wi-Fi page](https://www.umb.edu/it/software-systems/wifi/) identifies the `umb.edu` server domain and full email login. Its [Android/Linux instructions](https://umassboston.service-now.com/sp?id=kb_article_view&sysparm_article=KB0010950) specify PEAP/MSCHAPv2 and system certificates for newer devices. Checked 2026-09-29. The ESP-IDF EAP client supports that authentication exchange.

The UMB profile enables ESP-IDF's full system CA bundle and requires a certificate DNS name equal to `umb.edu` or beneath it, regardless of the campus hosting the access point. It does not use BU's server or trust settings. Certificate DNS SANs take precedence over the common name. The domain check retains other certificate verification errors and never enables an accept-any-certificate mode.

ESP-IDF 5.4's public server-name API checks an exact hostname rather than a domain suffix. `pet_enterprise.c` isolates a small adapter using the SDK's EAP-only certificate-bundle attachment hook and the Mbed TLS verification callback. It delegates trust-chain validation to the original bundle verifier. HTTP/Hello certificate handling is unchanged. Recheck this integration when upgrading ESP-IDF. Certificate date checks remain governed by the SDK's Mbed TLS configuration; this profile does not add a persistent real-time clock.

The firmware has been compiled and the trust adapter tested locally. A real UMB login, campus connectivity and reboot/reconnection still require hardware testing within eduroam coverage. No user's login was used during development.

## Other institutions

Disable `CONFIG_PET_UMB_EDUROAM` to configure another institution. Obtain its official CA PEM certificate and exact authentication server hostname, save the CA as `firmware/main/enterprise_ca.pem`, and set `CONFIG_PET_ENTERPRISE_DOMAIN` via menuconfig. Build and flash the firmware. A custom CA without a server hostname fails the build; no configured trust profile causes the join to stop with an explanation.

Client-certificate EAP-TLS setup is not implemented. Institutions with other inner authentication requirements may need additional configuration.

Enterprise credentials are stored in the badge's NVS, like its existing personal Wi-Fi credentials, and restored on boot. Forget removes both. This does not add encrypted NVS storage.

Native tests cover connected ordering and highlighting, enterprise field visibility, eye toggling, mode switching without losing typed input, manual-entry reset, navigation and forgetting. Native trust tests also cover DNS boundary matching, malicious suffixes, embedded NULs, SAN/CN precedence, and retention of trust/expiry failures supplied by the TLS library.
