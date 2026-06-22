# Privacy Policy

**Pi Provisioner**

_Last updated: 22 June 2026_

Pi Provisioner is built to put you in control of your own devices and your own
network. This policy explains what information the app handles, where it goes,
and — importantly — what it does **not** do.

## The short version

- We do **not** operate any server, account system, or cloud backend.
- We do **not** collect, transmit, sell, or share your personal data.
- We do **not** use analytics, advertising, or tracking SDKs.
- WiFi credentials you enter are sent **only** to your Raspberry Pi, over a
  bonded (encrypted) Bluetooth Low Energy connection.

## What the app handles

**WiFi network names and passwords.** You enter these to provision a Pi. They
are transmitted over the encrypted, paired Bluetooth link directly to the Pi you
selected. They are never sent to XPRO Slovenija or any third party.

**Locally stored data.** To make repeat setups convenient, the app may store on
your device:

- credentials you choose to save, kept in the platform secure store
  (iOS Keychain / Android Keystore via `flutter_secure_storage`);
- friendly names and identifiers for Pis you have provisioned, and app settings
  such as theme.

This data stays on your device. You can remove it by using the app's "forget"
actions or by uninstalling the app.

**Bluetooth device information.** While scanning, the app sees the Bluetooth
advertisements of nearby devices in order to find Pis. This information is used
only on your device, in the moment, to populate the device list. It is not
recorded or transmitted off the device.

## Permissions we request, and why

- **Bluetooth (scan / connect)** — to discover, connect to, and configure your
  Raspberry Pi. On Android the scan permission is declared with
  `neverForLocation`, because the app does not use Bluetooth to derive your
  location.
- **Location (Android, legacy only)** — required by older Android versions
  (API 30 and below) as a precondition for Bluetooth scanning. The app does not
  collect, use, or store your location.

## Data sharing

None. Because there is no backend and no analytics, there is nothing to share.
We do not disclose information to third parties.

## Children's privacy

Pi Provisioner is a technical utility and is not directed at children. It does
not knowingly collect any information from anyone.

## Changes to this policy

If the app's data handling changes in a future release, this document will be
updated and the "Last updated" date above revised.

## Contact

Questions about privacy? Contact XPRO Slovenija at **developer@xpro.si**.
