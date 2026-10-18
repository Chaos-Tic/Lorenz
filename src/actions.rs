//! File actions for the third wheel. They work on files and folders and never
//! replace an existing destination.
use crate::{executable, file_url, publish_as, publish_dir};
use anyhow::{Context, Result, bail, ensure};
use serde::Serialize;
use std::{
    env,
    ffi::OsString,
    fs,
    io::Write,
    path::{Path, PathBuf},
    process::{Command, Stdio},
};

#[derive(Serialize, Clone)]
pub struct Action {
    pub id: &'static str,
    pub label: &'static str,
    pub glyph: &'static str,
    /// The desktop must ask for a destination folder first.
    pub needs_destination: bool,
}
const ALL: [Action; 8] = [
    Action {
        id: "share",
        label: "Share",
        glyph: "share",
        needs_destination: false,
    },
    Action {
        id: "float",
        label: "Float",
        glyph: "float",
        needs_destination: false,
    },
    Action {
        id: "copy",
        label: "Copy",
        glyph: "copy",
        needs_destination: false,
    },
    Action {
        id: "copy-to",
        label: "Copy to…",
        glyph: "copy-to",
        needs_destination: true,
    },
    Action {
        id: "move-to",
        label: "Move to…",
        glyph: "move",
        needs_destination: true,
    },
    Action {
        id: "path",
        label: "Copy path",
        glyph: "path",
        needs_destination: false,
    },
    Action {
        id: "terminal",
        label: "Terminal",
        glyph: "terminal",
        needs_destination: false,
    },
    Action {
        id: "reveal",
        label: "Show in folder",
        glyph: "folder",
        needs_destination: false,
    },
];

fn wayland() -> bool {
    env::var_os("WAYLAND_DISPLAY").is_some()
}
/// A clipboard writer reading from stdin, for text or a URI list.
fn clipboard(mime: &str) -> Option<(PathBuf, Vec<String>)> {
    if cfg!(windows) {
        return None;
    }
    if wayland() {
        if let Some(p) = executable("wl-copy") {
            return Some((p, vec!["--type".into(), mime.into()]));
        }
    }
    executable("xclip").map(|p| {
        (
            p,
            ["-selection", "clipboard", "-t", mime]
                .map(String::from)
                .to_vec(),
        )
    })
}
fn windows_clipboard(paths: &[PathBuf], files: bool) -> Result<()> {
    // PowerShell joins arguments after -Command into the script, so paths are
    // sent on stdin and read by a fixed script; they are never parsed as code.
    let script = if files {
        "$p = @([Console]::In.ReadToEnd() -split \"`n\" | Where-Object { $_ }); Set-Clipboard -LiteralPath $p"
    } else {
        "Set-Clipboard -Value ([Console]::In.ReadToEnd().TrimEnd())"
    };
    let mut child = Command::new("powershell.exe")
        .args(["-NoProfile", "-NonInteractive", "-Command", script])
        .stdin(Stdio::piped())
        .stdout(Stdio::null())
        .spawn()
        .context("Cannot start PowerShell")?;
    let text: Vec<String> = paths.iter().map(|p| p.display().to_string()).collect();
    child
        .stdin
        .take()
        .unwrap()
        .write_all(text.join("\n").as_bytes())?;
    ensure!(
        child.wait()?.success(),
        "The clipboard could not be updated"
    );
    Ok(())
}
fn to_clipboard(mime: &str, data: &str) -> Result<()> {
    let (program, args) =
        clipboard(mime).context("Install wl-clipboard (Wayland) or xclip (X11)")?;
    let mut child = Command::new(program)
        .args(args)
        .stdin(Stdio::piped())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()?;
    child.stdin.take().unwrap().write_all(data.as_bytes())?;
    ensure!(
        child.wait()?.success(),
        "The clipboard could not be updated"
    );
    Ok(())
}

/// Known terminals and their working-directory arguments; `{dir}` is
/// replaced inside these trusted templates only. Others inherit the folder.
const TERMINALS: [(&str, &[&str]); 15] = [
    ("xdg-terminal-exec", &[]),
    ("konsole", &["--workdir", "{dir}"]),
    ("kgx", &["--working-directory={dir}"]),
    ("gnome-terminal", &["--working-directory={dir}"]),
    ("ptyxis", &["--working-directory={dir}"]),
    ("xfce4-terminal", &["--working-directory={dir}"]),
    ("mate-terminal", &["--working-directory={dir}"]),
    ("tilix", &["--working-directory={dir}"]),
    ("lxterminal", &["--working-directory={dir}"]),
    ("alacritty", &["--working-directory", "{dir}"]),
    ("kitty", &["--directory", "{dir}"]),
    ("foot", &["--working-directory={dir}"]),
    ("wezterm", &["start", "--cwd", "{dir}"]),
    ("ghostty", &[]),
    ("xterm", &[]),
];
fn terminal() -> Option<(PathBuf, &'static [&'static str])> {
    if cfg!(windows) {
        return executable("wt")
            .map(|p| (p, &["-d", "{dir}"][..]))
            .or_else(|| executable("cmd").map(|p| (p, &[][..])));
    }
    let desktop = env::var("XDG_CURRENT_DESKTOP").unwrap_or_default();
    let preferred: &[&str] = if desktop.contains("KDE") {
        &["konsole"]
    } else if desktop.contains("GNOME") {
        &["ptyxis", "kgx", "gnome-terminal"]
    } else if desktop.contains("XFCE") {
        &["xfce4-terminal"]
    } else {
        &[]
    };
    let order = preferred
        .iter()
        .copied()
        .chain(TERMINALS.iter().map(|t| t.0));
    for name in order {
        if let (Some(path), Some(t)) = (executable(name), TERMINALS.iter().find(|t| t.0 == name)) {
            return Some((path, t.1));
        }
    }
    None
}
fn open_terminal(directory: &Path) -> Result<()> {
    let (program, template) = terminal().context("No terminal emulator found")?;
    let folder = directory.to_string_lossy();
    let mut command = Command::new(program);
    command
        .current_dir(directory)
        .args(template.iter().map(|a| a.replace("{dir}", &folder)));
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(0x0000_0010); // CREATE_NEW_CONSOLE
    }
    command
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .context("Cannot start the terminal")?;
    Ok(())
}
fn floater() -> Option<PathBuf> {
    ["dragon-drop", "dragon", "ripdrag"]
        .into_iter()
        .find_map(executable)
}

pub fn available(paths: &[PathBuf]) -> Vec<Action> {
    if paths.is_empty() || !paths.iter().all(|p| p.exists()) {
        return Vec::new();
    }
    let files = paths.iter().all(|p| p.is_file());
    ALL.iter()
        .filter(|a| match a.id {
            "share" => files && !cfg!(windows) && executable("kdeconnect-handler").is_some(),
            "float" => floater().is_some(),
            "copy" | "path" => cfg!(windows) || clipboard("text/plain").is_some(),
            "terminal" => terminal().is_some(),
            _ => true,
        })
        .cloned()
        .collect()
}

fn copy_tree(source: &Path, target: &Path) -> Result<()> {
    let kind = fs::symlink_metadata(source)?.file_type();
    if kind.is_symlink() {
        #[cfg(unix)]
        std::os::unix::fs::symlink(fs::read_link(source)?, target)?;
        #[cfg(not(unix))]
        fs::copy(source, target)?;
    } else if kind.is_dir() {
        fs::create_dir(target)?;
        for entry in fs::read_dir(source)? {
            let entry = entry?;
            copy_tree(&entry.path(), &target.join(entry.file_name()))?;
        }
    } else {
        fs::copy(source, target)?;
    }
    Ok(())
}
fn split_name(path: &Path) -> (OsString, String) {
    let stem = path.file_stem().unwrap_or_default().to_os_string();
    let ext = path.extension().map(|e| e.to_string_lossy().into_owned());
    (stem, ext.unwrap_or_default())
}
fn crosses_device(error: &std::io::Error) -> bool {
    error.kind() == std::io::ErrorKind::CrossesDevices
}
/// Copies (or moves) one source into `destination` under a free name.
fn transfer(source: &Path, destination: &Path, remove: bool) -> Result<PathBuf> {
    let name = source.file_name().context("Missing file name")?;
    ensure!(
        source.parent() != Some(destination),
        "The item is already in this folder"
    );
    ensure!(
        !destination.starts_with(source),
        "A folder cannot be copied into itself"
    );
    if source.is_dir() {
        if remove {
            let mut target = destination.join(name);
            for index in 2..10000 {
                if fs::symlink_metadata(&target).is_err() {
                    break;
                }
                let mut candidate = name.to_os_string();
                candidate.push(format!("-{index}"));
                target = destination.join(candidate);
            }
            ensure!(
                fs::symlink_metadata(&target).is_err(),
                "Too many folders with this name"
            );
            return match fs::rename(source, &target) {
                Ok(()) => Ok(target),
                Err(e) if crosses_device(&e) => {
                    bail!("Moving folders between drives is not supported; use Copy to…")
                }
                Err(e) => Err(e.into()),
            };
        }
        let workspace = tempfile::Builder::new()
            .prefix(".metroid-")
            .tempdir_in(destination)?;
        let staged = workspace.path().join("result");
        copy_tree(source, &staged)?;
        return publish_dir(&staged, name, destination);
    }
    let (stem, ext) = split_name(source);
    if remove {
        // A hard link publishes atomically without replacing anything; the
        // source name is removed only once the new name exists.
        match publish_as(source, &stem, destination, &ext) {
            Ok(target) => {
                fs::remove_file(source)?;
                return Ok(target);
            }
            Err(e)
                if e.downcast_ref::<std::io::Error>()
                    .is_some_and(crosses_device) => {}
            Err(e) => return Err(e),
        }
    }
    let workspace = tempfile::Builder::new()
        .prefix(".metroid-")
        .tempdir_in(destination)?;
    let staged = workspace.path().join("copy");
    fs::copy(source, &staged)?;
    fs::File::open(&staged)?.sync_all()?;
    ensure!(
        fs::metadata(&staged)?.len() == fs::metadata(source)?.len(),
        "The copy is incomplete"
    );
    let target = publish_as(&staged, &stem, destination, &ext)?;
    if remove {
        fs::remove_file(source)?;
    }
    Ok(target)
}

fn reveal(paths: &[PathBuf]) -> Result<()> {
    if cfg!(windows) {
        Command::new("explorer.exe")
            .arg(format!("/select,{}", paths[0].display()))
            .spawn()?;
        return Ok(());
    }
    if let Some(dbus) = executable("dbus-send") {
        let uris: Vec<String> = paths.iter().map(|p| file_url(p)).collect();
        let status = Command::new(dbus)
            .args([
                "--session",
                "--dest=org.freedesktop.FileManager1",
                "--type=method_call",
                "/org/freedesktop/FileManager1",
                "org.freedesktop.FileManager1.ShowItems",
            ])
            .arg(format!("array:string:{}", uris.join(",")))
            .arg("string:")
            .status();
        if status.is_ok_and(|s| s.success()) {
            return Ok(());
        }
    }
    let folder = paths[0].parent().unwrap_or(Path::new("/"));
    Command::new(executable("xdg-open").context("No file manager found")?)
        .arg(folder)
        .spawn()?;
    Ok(())
}

/// Runs an action and returns the new paths it created, if any.
pub fn run(id: &str, paths: &[PathBuf], destination: Option<&Path>) -> Result<Vec<PathBuf>> {
    let paths = paths
        .iter()
        .map(|p| {
            p.canonicalize()
                .with_context(|| format!("Not found: {}", p.display()))
        })
        .collect::<Result<Vec<_>>>()?;
    ensure!(!paths.is_empty(), "Select at least one file");
    ensure!(
        available(&paths).iter().any(|a| a.id == id),
        "This action is not available here"
    );
    match id {
        "copy" | "path" if cfg!(windows) => windows_clipboard(&paths, id == "copy")?,
        "copy" => {
            let uris: Vec<String> = paths.iter().map(|p| file_url(p) + "\r\n").collect();
            to_clipboard("text/uri-list", &uris.concat())?;
        }
        "path" => {
            let text: Vec<String> = paths.iter().map(|p| p.display().to_string()).collect();
            to_clipboard("text/plain;charset=utf-8", &text.join("\n"))?;
        }
        "terminal" => {
            let folder = if paths[0].is_dir() {
                &paths[0]
            } else {
                paths[0].parent().context("Missing parent folder")?
            };
            open_terminal(folder)?;
        }
        "share" => {
            for path in &paths {
                Command::new(executable("kdeconnect-handler").unwrap())
                    .arg(file_url(path))
                    .spawn()?;
            }
        }
        "float" => {
            Command::new(floater().unwrap())
                .args(["-a", "-x"])
                .args(&paths)
                .spawn()?;
        }
        "reveal" => reveal(&paths)?,
        "copy-to" | "move-to" => {
            let destination = destination
                .context("Choose a destination folder")?
                .canonicalize()
                .context("Destination folder not found")?;
            ensure!(destination.is_dir(), "Destination must be a folder");
            return paths
                .iter()
                .map(|p| transfer(p, &destination, id == "move-to"))
                .collect();
        }
        _ => bail!("Unknown action"),
    }
    Ok(Vec::new())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn copy_and_move_never_replace() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("src");
        let target = d.path().join("dst");
        fs::create_dir_all(source.join("album/inner")).unwrap();
        fs::create_dir(&target).unwrap();
        fs::write(source.join("photo.tar.gz"), b"new").unwrap();
        fs::write(source.join("album/inner/a.txt"), b"a").unwrap();
        fs::write(target.join("photo.tar.gz"), b"old").unwrap();
        let copied = run("copy-to", &[source.join("photo.tar.gz")], Some(&target)).unwrap();
        assert_eq!(copied[0].file_name().unwrap(), "photo.tar-2.gz");
        assert_eq!(fs::read(target.join("photo.tar.gz")).unwrap(), b"old");
        let folder = run("copy-to", &[source.join("album")], Some(&target)).unwrap();
        assert_eq!(fs::read(folder[0].join("inner/a.txt")).unwrap(), b"a");
        let moved = run("move-to", &[source.join("album")], Some(&target)).unwrap();
        assert_eq!(moved[0].file_name().unwrap(), "album-2");
        assert!(!source.join("album").exists());
        let moved = run("move-to", &[source.join("photo.tar.gz")], Some(&target)).unwrap();
        assert_eq!(fs::read(&moved[0]).unwrap(), b"new");
        assert!(!source.join("photo.tar.gz").exists());
        assert!(
            run(
                "move-to",
                std::slice::from_ref(&target),
                Some(&target.join("album"))
            )
            .is_err()
        );
        assert_eq!(fs::read_dir(&target).unwrap().count(), 5);
    }
    #[test]
    fn actions_need_existing_sources() {
        assert!(available(&[PathBuf::from("/missing/file")]).is_empty());
        let d = tempfile::tempdir().unwrap();
        let ids: Vec<_> = available(&[d.path().to_path_buf()])
            .iter()
            .map(|a| a.id)
            .collect();
        assert!(ids.contains(&"copy-to") && ids.contains(&"reveal") && !ids.contains(&"share"));
    }
}
