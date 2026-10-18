use anyhow::{Context, Result, bail, ensure};
use serde::{Deserialize, Serialize};
use std::{
    env,
    fs::{self, File},
    io::{Read, Seek, SeekFrom},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant},
};

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Conversion {
    pub category: String,
    pub inputs: Vec<String>,
    pub outputs: Vec<String>,
    pub backend: String,
    pub args: Vec<String>,
}
#[derive(Deserialize)]
struct Registry {
    conversion: Vec<Conversion>,
}
pub fn registry() -> Vec<Conversion> {
    toml::from_str::<Registry>(include_str!("../registry/conversions.toml"))
        .expect("valid bundled registry")
        .conversion
}
pub fn executable(name: &str) -> Option<PathBuf> {
    env::split_paths(&env::var_os("PATH")?)
        .flat_map(|p| {
            if cfg!(windows) {
                vec![p.join(format!("{name}.exe"))]
            } else {
                vec![p.join(name)]
            }
        })
        .find(|p| {
            if !p.is_file() {
                return false;
            }
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                p.metadata()
                    .is_ok_and(|m| m.permissions().mode() & 0o111 != 0)
            }
            #[cfg(not(unix))]
            {
                true
            }
        })
}
pub fn extension(path: &Path) -> String {
    path.extension()
        .unwrap_or_default()
        .to_string_lossy()
        .to_ascii_lowercase()
}
#[derive(Serialize)]
pub struct Format {
    pub target: String,
    pub category: String,
    pub backend: String,
    pub available: bool,
}
pub fn formats(input: &Path) -> Vec<Format> {
    let ext = extension(input);
    registry()
        .into_iter()
        .filter(|r| r.inputs.contains(&ext))
        .flat_map(|r| {
            let available = executable(&r.backend).is_some();
            r.outputs
                .into_iter()
                .filter(|o| o != &ext)
                .map(move |target| Format {
                    target,
                    category: r.category.clone(),
                    backend: r.backend.clone(),
                    available,
                })
        })
        .collect()
}
#[derive(Serialize)]
pub struct Backend {
    pub name: String,
    pub available: bool,
    pub path: Option<PathBuf>,
}
#[derive(Serialize)]
pub struct Doctor {
    pub version: &'static str,
    pub os: &'static str,
    pub session: String,
    pub desktop: String,
    pub backends: Vec<Backend>,
}
pub fn doctor() -> Doctor {
    Doctor {
        version: env!("CARGO_PKG_VERSION"),
        os: env::consts::OS,
        session: env::var("XDG_SESSION_TYPE").unwrap_or_else(|_| "native".into()),
        desktop: env::var("XDG_CURRENT_DESKTOP").unwrap_or_default(),
        backends: [
            "magick",
            "ffmpeg",
            "pandoc",
            "pdftotext",
            "pdftoppm",
            "qpdf",
        ]
        .into_iter()
        .map(|name| {
            let path = executable(name);
            Backend {
                name: name.into(),
                available: path.is_some(),
                path,
            }
        })
        .collect(),
    }
}

// Publishing uses a hard link on the same filesystem: atomic, and refuses any
// existing destination (including dangling symlinks). Sources are never opened writable.
pub fn publish(source: &Path, input: &Path, directory: &Path, target: &str) -> Result<PathBuf> {
    let stem = input
        .file_stem()
        .context("Nom de fichier manquant")?
        .to_string_lossy();
    for index in 1..10000 {
        let suffix = if index == 1 {
            String::new()
        } else {
            format!("-{index}")
        };
        let output = directory.join(format!("{stem}-metroid{suffix}.{target}"));
        match fs::hard_link(source, &output) {
            Ok(()) => return Ok(output),
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(e) => {
                return Err(e)
                    .context("Publication atomique impossible sur ce système de fichiers");
            }
        }
    }
    bail!("Trop de fichiers de sortie portant ce nom")
}

pub fn run_backend(
    program: &Path,
    args: &[String],
    log_dir: &Path,
    cancel: Option<&Path>,
    timeout: u64,
) -> Result<()> {
    let log_path = log_dir.join("backend.log");
    let log = File::create(&log_path)?;
    let mut child = Command::new(program)
        .args(args)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(log)
        .spawn()
        .context("Impossible de lancer le moteur")?;
    let start = Instant::now();
    loop {
        if let Some(status) = child.try_wait()? {
            if status.success() {
                return Ok(());
            }
            let mut file = File::open(log_path)?;
            let size = file.metadata()?.len();
            file.seek(SeekFrom::Start(size.saturating_sub(8192)))?;
            let mut bytes = Vec::new();
            file.read_to_end(&mut bytes)?;
            bail!(
                "Moteur en échec ({status}) : {}",
                String::from_utf8_lossy(&bytes)
            );
        }
        let cancelled = cancel.is_some_and(Path::exists);
        if cancelled || start.elapsed() > Duration::from_secs(timeout) {
            let _ = child.kill();
            let _ = child.wait();
            bail!(if cancelled {
                "Conversion annulée"
            } else {
                "Délai de conversion dépassé"
            });
        }
        thread::sleep(Duration::from_millis(80));
    }
}

pub fn convert(
    input: &Path,
    target: &str,
    directory: Option<&Path>,
    cancel: Option<&Path>,
    timeout: u64,
) -> Result<PathBuf> {
    ensure!(!cancel.is_some_and(Path::exists), "Conversion annulée");
    let input = input.canonicalize().context("Fichier source introuvable")?;
    ensure!(input.is_file(), "La source doit être un fichier");
    let target = target.to_ascii_lowercase();
    let rule = registry()
        .into_iter()
        .find(|r| r.inputs.contains(&extension(&input)) && r.outputs.contains(&target))
        .context("Conversion non prise en charge")?;
    ensure!(
        extension(&input) != target,
        "Le fichier est déjà dans ce format"
    );
    let backend = executable(&rule.backend).with_context(|| {
        format!(
            "Moteur absent : {}. Consultez metroid doctor.",
            rule.backend
        )
    })?;
    let directory = directory
        .unwrap_or(input.parent().unwrap())
        .canonicalize()
        .context("Dossier de sortie introuvable")?;
    ensure!(directory.is_dir(), "La destination doit être un dossier");
    let workspace = tempfile::Builder::new()
        .prefix(".metroid-")
        .tempdir_in(&directory)?;
    let output = workspace.path().join(format!("converted.{target}"));
    let stem = workspace.path().join("converted");
    let writer = match target.as_str() {
        "txt" => "plain",
        "md" => "markdown",
        other => other,
    };
    // Expand whole tokens only: file names containing placeholders stay literal.
    let args: Vec<String> = rule
        .args
        .iter()
        .map(|a| match a.as_str() {
            "{input}" => input.to_string_lossy().into_owned(),
            "{output}" => output.to_string_lossy().into_owned(),
            "{output_stem}" => stem.to_string_lossy().into_owned(),
            "{writer}" => writer.to_owned(),
            _ => a.clone(),
        })
        .collect();
    run_backend(&backend, &args, workspace.path(), cancel, timeout)?;
    ensure!(output.is_file(), "Le moteur n'a produit aucun fichier");
    ensure!(!cancel.is_some_and(Path::exists), "Conversion annulée");
    publish(&output, &input, &directory, &target)
}

pub fn merge_pdf(inputs: &[PathBuf], directory: &Path) -> Result<PathBuf> {
    ensure!(inputs.len() >= 2, "Sélectionnez au moins deux PDF");
    let directory = directory.canonicalize()?;
    let workspace = tempfile::Builder::new()
        .prefix(".metroid-")
        .tempdir_in(&directory)?;
    let output = workspace.path().join("merged.pdf");
    let mut args = vec!["--empty".into(), "--pages".into()];
    for input in inputs {
        let input = input.canonicalize()?;
        ensure!(
            input.is_file() && extension(&input) == "pdf",
            "Chaque source doit être un PDF"
        );
        args.push(input.to_string_lossy().into_owned());
        args.push("1-z".into());
    }
    args.extend(["--".into(), output.to_string_lossy().into_owned()]);
    run_backend(
        &executable("qpdf").context("Moteur absent : qpdf")?,
        &args,
        workspace.path(),
        None,
        3600,
    )?;
    publish(&output, Path::new("fusion.pdf"), &directory, "pdf")
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn registry_is_consistent() {
        for r in registry() {
            assert!(r.args.iter().any(|a| a == "{input}"));
            assert!(!r.inputs.is_empty());
            assert!(!r.outputs.is_empty());
            for out in r.outputs {
                assert!(out.chars().all(|c| c.is_ascii_alphanumeric()));
            }
        }
    }
    #[test]
    fn format_case_and_unknown() {
        assert!(
            formats(Path::new("photo.PNG"))
                .iter()
                .any(|f| f.target == "webp")
        );
        assert!(formats(Path::new("data.unknown")).is_empty());
        assert!(
            !formats(Path::new("photo.png"))
                .iter()
                .any(|f| f.target == "png")
        );
    }
    #[test]
    fn never_overwrites() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("staged");
        fs::write(&source, b"new").unwrap();
        let old = d.path().join("photo-metroid.png");
        fs::write(&old, b"old").unwrap();
        let output = publish(&source, Path::new("photo.jpg"), d.path(), "png").unwrap();
        assert_eq!(fs::read(old).unwrap(), b"old");
        assert_eq!(output.file_name().unwrap(), "photo-metroid-2.png");
    }
    #[test]
    fn concurrent_publication_is_safe() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("staged");
        fs::write(&source, b"data").unwrap();
        thread::scope(|s| {
            let handles: Vec<_> = (0..8)
                .map(|_| {
                    s.spawn(|| publish(&source, Path::new("same.jpg"), d.path(), "png").unwrap())
                })
                .collect();
            let outputs: std::collections::HashSet<_> =
                handles.into_iter().map(|h| h.join().unwrap()).collect();
            assert_eq!(outputs.len(), 8);
        });
    }
    #[test]
    fn invalid_and_cancelled_leave_no_output() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("bad.xyz");
        fs::write(&source, b"no").unwrap();
        assert!(convert(&source, "../../escape", None, None, 1).is_err());
        assert!(convert(&source, "png", None, Some(&source), 1).is_err());
        assert_eq!(fs::read_dir(d.path()).unwrap().count(), 1);
    }
}
