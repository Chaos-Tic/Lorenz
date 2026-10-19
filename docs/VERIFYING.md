# Verifying a download

Every release from 1.1.0 carries `SHA256SUMS`, written by the release job from
the files it publishes. (For 1.0.0 it was added after publication, from the
files GitHub serves.) A matching checksum proves that your file is the one
published on this repository; it does not prove who built it the way a
publisher signature does, and Windows installers are not signed yet.

Download only from the [official channels](../README.md#official-download-channels).

## Linux and macOS

Put the file and `SHA256SUMS` in the same folder, then:

```sh
sha256sum --check --ignore-missing SHA256SUMS
```

`OK` after the file name means it matches. Packages from the AUR, the Fedora
COPR, the Ubuntu PPA and the Chaos-Tic apt repository are checked by the
package manager itself: COPR, Launchpad and the apt repository sign their
indexes, and the AUR PKGBUILD pins the source tarball's SHA-256.

## Windows

In PowerShell, in the download folder:

```powershell
(Get-FileHash .\Lorenz-Setup-x64.exe -Algorithm SHA256).Hash
```

Compare the result with the `Lorenz-Setup-x64.exe` line of `SHA256SUMS`
(case does not matter). Windows SmartScreen may still warn because the
installer is unsigned; do not disable SmartScreen or your antivirus to run it.

## The apt repository key

`https://chaos-tic.github.io/apt/chaos-tic-apt.gpg` must have the fingerprint
`B968 F27E 534B 7AD3 2C2A 026A C874 EC5C A8A6 42AE`:

```sh
gpg --show-keys /usr/share/keyrings/chaos-tic.gpg
```
