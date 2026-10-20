# Security policy

## Reporting a vulnerability

Please report privately through GitHub: **Security → Report a vulnerability**
on this repository. Do not open a public issue for a vulnerability. Include
the Lorenz version (`lorenz --version`), the platform, and the smallest file or
steps that reproduce it. You will get an answer as soon as possible; there is
no bounty.

Supported: the latest release. Older releases receive no fixes.

## Threat model

Lorenz converts files that may come from anyone, using engines installed on
the computer (FFmpeg, ImageMagick, Pandoc, Poppler, qpdf, Ghostscript,
LibreOffice, bsdtar). Those engines parse the content; a flaw in an engine is
a flaw Lorenz exposes. Lorenz reduces what an engine can be made to do:

- **Originals are never written.** Output goes to a private staging folder
  next to the source and is moved into place only after the engine succeeds.
- **No shell.** Engines are started with an argument list, never through a
  shell, so file names cannot inject commands.
- **ImageMagick** runs with a policy that refuses script, text, URL, SVG and
  `@file` coders and every delegate; the coder is chosen by Lorenz, not
  guessed from the content.
- **Pandoc** runs with `--sandbox`; documents may embed only media extracted
  from themselves.
- **FFmpeg** may open local files and pipes only (`-protocol_whitelist file,pipe`).
- **LibreOffice** uses a private, throwaway profile and output folder.
- **Archives** are extracted by bsdtar without `-P`: absolute paths, `..`
  components and writes through links are refused; the folder appears only
  after a complete extraction.
- **Time limit and cancellation** stop the engine and every process it
  started (its process group on Linux, its process tree on Windows).

## Update check

At most once a day, the desktop (tray companion and window) makes one anonymous
HTTPS request to `api.github.com/repos/Chaos-Tic/Lorenz/releases/latest`. It
sends no identifier, cookie, file name or content; GitHub sees the address of
the request, as with any download. When a newer release exists, Lorenz shows
how to update (package manager, winget, or the release page); nothing is
downloaded or run automatically. Turn it off in **Settings → Check for
updates once a day**. Conversions and the `lorenz` core never use the network.

## Known limits

- Engines run as your user, without further confinement (no bubblewrap,
  namespaces or AppContainer) and without memory or CPU limits; a malformed
  file can use a lot of memory until the time limit stops it.
- File types are chosen from the extension; content detection is planned.
- Windows installers are not code-signed; see
  [docs/VERIFYING.md](docs/VERIFYING.md).
- The KWin extension runs inside the compositor and must be rebuilt for each
  KWin version.

These limits are tracked in the README's
[Roadmap and known gaps](README.md#roadmap-and-known-gaps).
