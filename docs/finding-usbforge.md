# Finding USBForge after install

After installing a package, look in your **application menu** for:

- **USBForge Builder** — create/write ISOs
- **USBForge Live** — USB Lab (test, docs, diagnostics)

Or run from a terminal:

```bash
usbforge-builder
usbforge-live
```

If the menu entries are missing (older 1.1.0 packages had a broken desktop file), upgrade to **1.1.1+** or run:

```bash
sudo update-desktop-database
sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor
```

Then log out/in or search again for "USBForge".

## Updates

- In the app: click **Check Updates** / **Check for Updates**
- Or in a terminal: `usbforge-update`

The updater downloads the matching `.deb` / `.rpm` / tarball from GitHub Releases and installs it, then refreshes the menu.
