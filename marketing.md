# Pi Provisioner

### Get your Raspberry Pi online — straight from your phone.

No monitor. No keyboard. No fiddling with `wpa_supplicant.conf` on an SD card.
Pi Provisioner connects to a headless Raspberry Pi over Bluetooth and sets up its
WiFi in seconds.

---

## The problem

Setting up WiFi on a headless Pi means pulling the SD card, hand-editing config
files, or hunting for a spare screen and keyboard. It's slow, error-prone, and
painful to do at scale or in the field.

## The solution

Pi Provisioner pairs a lightweight on-device service with a clean mobile app.
Open the app, tap your Pi, pick the network, type the password — done. The Pi
joins the network and reports back its IP address right in the app.

## Key features

- **Phone-to-Pi over Bluetooth LE** — works before the Pi has any network at all.
- **Sees what the Pi sees** — the app lists the WiFi networks in range of the
  *Pi*, not your phone, so you pick the right one the first time.
- **Live status** — watch the connection go from *connecting* to *connected*,
  with the assigned IP shown instantly. Failures are reported clearly.
- **Secure by design** — credentials travel only over a bonded, encrypted
  Bluetooth link. Nothing is sent to the cloud.
- **Works with modern and legacy Pis** — automatically detects and drives either
  NetworkManager or wpa_supplicant.
- **Built for the field** — name and remember your devices; reprovision in
  seconds when you move them to a new site.
- **Cross-platform** — native iOS and Android from a single app.

## How it works

1. **Scan** — the app discovers nearby Pis advertising over Bluetooth.
2. **Connect & pair** — a secure, bonded Bluetooth link is established.
3. **Choose** — pick from the networks the Pi can see, or enter one manually.
4. **Provision** — send the credentials; the Pi connects and reports its IP.

## Who it's for

- **Makers & hobbyists** standing up a new Pi without dragging out peripherals.
- **Integrators & installers** deploying fleets of headless devices on customer
  sites with unknown networks.
- **Product teams** shipping Pi-based appliances that customers set up themselves.

## Why it's different

No SD-card surgery. No serial cables. No cloud account. No guesswork about which
network is in range. Just a phone and a few taps.

---

*Pi Provisioner — by XPRO Slovenija. Learn more: developer@xpro.si*
