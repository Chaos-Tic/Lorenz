# Desktop guide

[Back to the README](../README.md)

## Convert your first file

1. Open **Metroid** from your application menu.
2. Drag a local file onto the drop area, or click **Add files** (`Ctrl+O`).
3. Select an **Output format**. The round button opens the format wheel.
4. Leave **Save to** set to **Next to the originals**, or choose an existing folder.
5. Click **Convert**. When the status reads **Done**, click **Open last output folder**.

![A PNG converted to WebP in the native desktop](media/desktop-converted.png)

The example uses a generated sample image. Your original file is preserved. The
output becomes `sample-metroid.webp`; another conversion creates a numbered name.

## Work with a batch

Files are processed one at a time. Only formats shared by every queued input are
shown. If incompatible file types have no common output, clear the queue and process
each family separately. Duplicate paths are not added twice.

**Clear** removes all queued rows, not files on disk. Adding files, changing the
format, and changing the destination are disabled during processing. A failed file
does not prevent subsequent files from being processed.

## Read the result

| Status | Meaning | Next step |
| --- | --- | --- |
| Queued | Waiting to start | Select the format and click Convert |
| Conversion… | The local engine is running | Wait or cancel |
| Done | The output has been published | Hover for its path or open the output folder |
| Failed | The engine or publication failed | Hover over the status for details |
| Cancelled | The running file was interrupted | Adjust settings and retry if needed |
| Not processed | The queue stopped before reaching this file | Start another batch |

The progress strip indicates activity; it is not a percentage estimate. The summary
reports completed and failed files. **Open last output folder** refers to the most
recent successful output, including when a later file fails.

## Cancel or close

Click **Cancel** or press `Escape` to stop the current engine and the remaining queue.
Closing during processing requests cancellation and keeps the window open; close it
again after processing stops. The queue is not saved when the application exits.

## Check your engines

**Diagnostics** reports whether each executable is available. Missing engines remove
associated formats from the picker. Installed engines may still lack individual
codecs or supporting data. See [Troubleshooting](TROUBLESHOOTING.md).

**Always on top** requests a floating window, subject to the desktop compositor's
rules. It does not provide system-wide drag interception or a global keyboard shortcut.
