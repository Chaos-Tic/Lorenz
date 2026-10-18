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

pub mod actions;
pub mod tools;

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Conversion {
    pub category: String,
    #[serde(default)]
    pub glyph: String,
    pub inputs: Vec<String>,
    pub outputs: Vec<String>,
    pub backend: String,
    pub args: Vec<String>,
    /// Optional second run of the same engine, reading what `args` staged.
    #[serde(default)]
    pub then: Vec<String>,
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
/// Resolves a registry engine, accepting the names distributions really ship:
/// ImageMagick 6 provides only `convert`, Windows Ghostscript is `gswin64c`,
/// and the `tar` included with Windows 10 and later is bsdtar.
pub fn engine(name: &str) -> Option<PathBuf> {
    let aliases: &[&str] = match name {
        "magick" if cfg!(unix) => &["magick", "convert"],
        "gs" if cfg!(windows) => &["gswin64c", "gswin32c", "gs"],
        "bsdtar" if cfg!(windows) => &["bsdtar", "tar"],
        "soffice" => &["soffice", "libreoffice"],
        _ => &[name],
    };
    aliases
        .iter()
        .find_map(|alias| executable(alias))
        .or_else(|| windows_install(name))
        .map(|path| real_path(&path))
}
/// Windows installers often leave engines off `PATH`, and winget's links are
/// only added to `PATH` for processes started after the installation.
fn windows_install(name: &str) -> Option<PathBuf> {
    if !cfg!(windows) {
        return None;
    }
    let var = |key: &str| env::var_os(key).map(PathBuf::from);
    let program_files = [var("ProgramFiles"), var("ProgramFiles(x86)")];
    // (base, folder prefix, path inside the newest matching folder)
    let places: &[(Option<PathBuf>, &str, &str)] = &match name {
        "soffice" => [(
            program_files[0].clone(),
            "LibreOffice",
            "program/soffice.exe",
        )]
        .to_vec(),
        "magick" => [(program_files[0].clone(), "ImageMagick", "magick.exe")].to_vec(),
        "qpdf" => [(program_files[0].clone(), "qpdf", "bin/qpdf.exe")].to_vec(),
        "gs" => [(
            program_files[0].clone().map(|p| p.join("gs")),
            "gs",
            "bin/gswin64c.exe",
        )]
        .to_vec(),
        "pandoc" => [
            (var("LOCALAPPDATA"), "Pandoc", "pandoc.exe"),
            (program_files[0].clone(), "Pandoc", "pandoc.exe"),
        ]
        .to_vec(),
        _ => Vec::new(),
    };
    let links = var("LOCALAPPDATA")
        .map(|p| p.join("Microsoft/WinGet/Links").join(format!("{name}.exe")))
        .filter(|p| p.is_file());
    places
        .iter()
        .filter_map(|(base, prefix, inner)| {
            let mut folders: Vec<_> = fs::read_dir(base.as_ref()?)
                .ok()?
                .filter_map(|e| e.ok().map(|e| e.path()))
                .filter(|p| {
                    p.file_name().and_then(|n| n.to_str()).is_some_and(|n| {
                        n.to_ascii_lowercase()
                            .starts_with(&prefix.to_ascii_lowercase())
                    })
                })
                .collect();
            folders.sort();
            folders
                .into_iter()
                .rev()
                .map(|f| f.join(inner))
                .find(|p| p.is_file())
        })
        .next()
        .or(links)
}
/// winget installs portable engines as links; running the real file lets
/// Windows find the libraries stored beside it.
fn real_path(path: &Path) -> PathBuf {
    if !cfg!(windows) {
        return path.to_path_buf();
    }
    match fs::canonicalize(path) {
        Ok(real) => {
            let text = real.to_string_lossy();
            match text.strip_prefix(r"\\?\") {
                Some(rest) if !rest.starts_with("UNC") => PathBuf::from(rest),
                _ => real,
            }
        }
        Err(_) => path.to_path_buf(),
    }
}
pub fn extension(path: &Path) -> String {
    path.extension()
        .unwrap_or_default()
        .to_string_lossy()
        .to_ascii_lowercase()
}
/// The source name without its extension; `notes.tar.gz` gives `notes`.
pub fn base_stem(path: &Path) -> std::ffi::OsString {
    let stem = Path::new(path.file_stem().unwrap_or(path.as_os_str()));
    if ["gz", "bz2", "xz", "zst"].contains(&extension(path).as_str()) && extension(stem) == "tar" {
        return stem.file_stem().unwrap_or_default().to_os_string();
    }
    stem.as_os_str().to_os_string()
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
            let available = engine(&r.backend).is_some();
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
/// One wheel sector: a format, a tool or a file action.
#[derive(Serialize)]
pub struct MenuItem {
    pub id: String,
    pub label: String,
    pub category: String,
    pub glyph: String,
}
fn family(category: &str) -> &str {
    category.split(" · ").next().unwrap_or(category)
}
/// Items shared by every selected path, in registry order.
pub fn menu(kind: &str, paths: &[PathBuf]) -> Result<Vec<MenuItem>> {
    Ok(match kind {
        "convert" => {
            let mut common: Option<Vec<MenuItem>> = None;
            for path in paths {
                let mut items = Vec::<MenuItem>::new();
                if path.is_file() {
                    let ext = extension(path);
                    for rule in registry().into_iter().filter(|r| r.inputs.contains(&ext)) {
                        if engine(&rule.backend).is_none() {
                            continue;
                        }
                        for target in rule.outputs.iter().filter(|o| **o != ext) {
                            if !items.iter().any(|i| &i.id == target) {
                                items.push(MenuItem {
                                    id: target.clone(),
                                    label: target.to_uppercase(),
                                    category: family(&rule.category).into(),
                                    glyph: rule.glyph.clone(),
                                });
                            }
                        }
                    }
                }
                common = Some(match common {
                    None => items,
                    Some(previous) => previous
                        .into_iter()
                        .filter(|p| items.iter().any(|i| i.id == p.id))
                        .collect(),
                });
            }
            common.unwrap_or_default()
        }
        "tools" => tools::available(paths)
            .into_iter()
            .map(|t| MenuItem {
                id: t.id,
                label: t.label,
                category: t.category,
                glyph: t.glyph,
            })
            .collect(),
        "actions" => actions::available(paths)
            .into_iter()
            .map(|a| MenuItem {
                id: a.id.into(),
                label: a.label.into(),
                category: "Actions".into(),
                glyph: a.glyph.into(),
            })
            .collect(),
        _ => bail!("Unknown wheel: use convert, tools or actions"),
    })
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
            "soffice",
            "pdftotext",
            "pdftoppm",
            "qpdf",
            "gs",
            "bsdtar",
        ]
        .into_iter()
        .map(|name| {
            let path = engine(name);
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
    publish_as(source, &base_stem(input), directory, target)
}
fn candidate(stem: &std::ffi::OsStr, index: usize, extension: &str) -> std::ffi::OsString {
    let mut name = stem.to_os_string();
    if index > 1 {
        name.push(format!("-{index}"));
    }
    if !extension.is_empty() {
        name.push(format!(".{extension}"));
    }
    name
}
pub fn publish_as(
    source: &Path,
    stem: &std::ffi::OsStr,
    directory: &Path,
    extension: &str,
) -> Result<PathBuf> {
    ensure!(!stem.is_empty(), "Missing file name");
    for index in 1..10000 {
        let output = directory.join(candidate(stem, index, extension));
        match fs::hard_link(source, &output) {
            Ok(()) => return Ok(output),
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(e) => {
                return Err(e)
                    .context("Atomic output publication is not supported on this filesystem");
            }
        }
    }
    bail!("Too many output files with this name")
}
/// Publishes a completed folder: a new directory is created exclusively, then
/// the finished entries are moved into it on the same filesystem.
pub fn publish_dir(source: &Path, stem: &std::ffi::OsStr, directory: &Path) -> Result<PathBuf> {
    ensure!(!stem.is_empty(), "Missing folder name");
    let entries: Vec<_> = fs::read_dir(source)?.collect::<std::io::Result<_>>()?;
    ensure!(!entries.is_empty(), "The engine did not produce any file");
    for index in 1..10000 {
        let output = directory.join(candidate(stem, index, ""));
        match fs::create_dir(&output) {
            Ok(()) => {
                for entry in entries {
                    fs::rename(entry.path(), output.join(entry.file_name()))?;
                }
                return Ok(output);
            }
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(e) => return Err(e).context("Cannot create the output folder"),
        }
    }
    bail!("Too many output folders with this name")
}

/// Values available to argument templates. Only registry tokens are matched,
/// so placeholders inside file names always remain literal.
#[derive(Default)]
pub struct Expansion<'a> {
    pub inputs: &'a [PathBuf],
    pub output: Option<&'a Path>,
    pub output_dir: Option<&'a Path>,
    pub target: &'a str,
    pub profile: Option<&'a Path>,
    pub workspace: Option<&'a Path>,
}
fn file_url(path: &Path) -> String {
    let text = path.to_string_lossy().replace('\\', "/");
    let mut url = String::from(if text.starts_with('/') {
        "file://"
    } else {
        "file:///"
    });
    for byte in text.bytes() {
        if byte.is_ascii_alphanumeric() || b"/-._~:".contains(&byte) {
            url.push(byte as char);
        } else {
            url.push_str(&format!("%{byte:02X}"));
        }
    }
    url
}
pub fn expand(args: &[String], values: &Expansion) -> Vec<String> {
    let lossy = |p: &Path| p.to_string_lossy().into_owned();
    let input = values.inputs.first().map(|p| lossy(p)).unwrap_or_default();
    let mut expanded = Vec::new();
    for arg in args {
        match arg.as_str() {
            "{input}" => expanded.push(input.clone()),
            "{inputs}" => expanded.extend(values.inputs.iter().map(|p| lossy(p))),
            "{archive_input}" => expanded.push(format!("@{input}")),
            // ImageMagick decodes with the coder named by the extension, never by
            // content sniffing (which could select MVG or another script coder).
            "{coded_input}" => {
                let coder = match extension(Path::new(&input)).as_str() {
                    "jpg" => "jpeg".to_owned(),
                    "tif" => "tiff".to_owned(),
                    other => other.to_owned(),
                };
                expanded.push(format!("{coder}:{input}"));
            }
            // bsdtar reads "-C dir name" pairs; names that bsdtar would treat
            // as options or nested archives are made explicitly relative.
            "{relative_inputs}" => {
                for path in values.inputs {
                    let name = path.file_name().unwrap_or_default().to_string_lossy();
                    expanded.push("-C".into());
                    expanded.push(lossy(path.parent().unwrap_or(Path::new("."))));
                    expanded.push(if name.starts_with(['-', '@']) {
                        format!("./{name}")
                    } else {
                        name.into_owned()
                    });
                }
            }
            "{output}" => expanded.push(values.output.map(lossy).unwrap_or_default()),
            "{output_stem}" => expanded.push(
                values
                    .output
                    .map(|p| lossy(&p.with_extension("")))
                    .unwrap_or_default(),
            ),
            "{output_dir}" => expanded.push(values.output_dir.map(lossy).unwrap_or_default()),
            "{target}" => expanded.push(values.target.to_owned()),
            "{writer}" => expanded.push(
                match values.target {
                    "txt" => "plain",
                    "md" => "markdown",
                    other => other,
                }
                .to_owned(),
            ),
            "{office_profile}" => expanded.push(format!(
                "-env:UserInstallation={}",
                file_url(values.profile.unwrap_or(Path::new("profile")))
            )),
            "{stage}" | "{media}" | "{pandoc_filter}" => {
                let name = match arg.as_str() {
                    "{stage}" => "stage.json",
                    "{media}" => "media",
                    _ => "pandoc/safe.lua",
                };
                expanded.push(
                    values
                        .workspace
                        .map(|w| lossy(&w.join(name)))
                        .unwrap_or_default(),
                )
            }
            other => match (other.strip_prefix("{output_dir}/"), values.output_dir) {
                (Some(name), Some(dir)) => expanded.push(lossy(&dir.join(name))),
                _ => expanded.push(other.to_owned()),
            },
        }
    }
    expanded
}
/// Distribution builds of pandoc cannot write DOCX or EPUB with `--sandbox`
/// (their data files are not embedded). Documents are therefore read in the
/// sandbox, with media extracted into the workspace, then written outside it.
/// The writer would embed any file an image or metadata names, so this filter
/// keeps only images that are exactly one of the extracted files, a few plain
/// metadata fields, and no raw HTML in EPUB (whose writer also embeds files
/// named by raw `<img>` tags).
const PANDOC_FILTER: &str = r#"
local media = os.getenv("METROID_MEDIA") or "\0"
local embeds = FORMAT:match("docx") or FORMAT:match("odt") or FORMAT:match("epub") or FORMAT:match("pptx")
-- Exact paths of the files the sandboxed reader extracted. Escapes are refused
-- outright: the writer could decode them into another path.
-- Windows paths may mix both separators, so paths are compared with "/".
local function slashes(path) return (path:gsub("\\", "/")) end
local extracted = {}
local function collect(dir)
  local ok, names = pcall(pandoc.system.list_directory, dir)
  if not ok then return end
  for _, name in ipairs(names) do
    local path = dir .. "/" .. name
    if pcall(pandoc.system.list_directory, path) then collect(path) else extracted[slashes(path)] = path end
  end
end
collect(media)
-- The listed path itself replaces the source, so the writer opens exactly the
-- file that was checked.
local function allowed(src)
  if src:find("%", 1, true) or src:find("..", 1, true) then return nil end
  return extracted[slashes(src)]
end
local keep = { title = true, subtitle = true, author = true, date = true, lang = true,
  dir = true, abstract = true, keywords = true, subject = true, description = true }
return {{
  Image = function(image)
    if not embeds then return end
    local path = allowed(image.src)
    if not path then return image.caption end
    image.src = path
    return image
  end,
  RawInline = function(raw) if FORMAT:match("epub") then return {} end end,
  RawBlock = function(raw) if FORMAT:match("epub") then return {} end end,
  Meta = function(meta)
    for key in pairs(meta) do if not keep[key] then meta[key] = nil end end
    return meta
  end,
}}
"#;
/// Restrictive engine settings, written into each private workspace:
/// - an ImageMagick policy refusing script, text, URL and `@file` coders;
/// - a LibreOffice profile that never updates links nor runs active content;
/// - a pandoc filter for writers that embed files (see `PANDOC_FILTER`).
///
/// `run_backend` points ImageMagick at the policy when it exists.
fn harden(workspace: &Path) -> Result<()> {
    let magick = workspace.join("magick");
    fs::create_dir_all(&magick)?;
    fs::write(
        magick.join("policy.xml"),
        r#"<policymap>
  <policy domain="coder" rights="none" pattern="{MVG,MSL,TEXT,LABEL,CAPTION,URL,HTTP,HTTPS,FTP,EPHEMERAL,SHOW,WIN,X,VID,INLINE,SVG,MSVG,RSVG}"/>
  <policy domain="delegate" rights="none" pattern="*"/>
  <policy domain="path" rights="none" pattern="@*"/>
</policymap>
"#,
    )?;
    fs::create_dir_all(workspace.join("pandoc"))?;
    fs::write(workspace.join("pandoc/safe.lua"), PANDOC_FILTER)?;
    let user = workspace.join("profile/user");
    fs::create_dir_all(&user)?;
    let item = |path: &str, name: &str, value: &str| {
        format!(
            r#"<item oor:path="{path}"><prop oor:name="{name}" oor:op="fuse"><value>{value}</value></prop></item>"#
        )
    };
    fs::write(
        user.join("registrymodifications.xcu"),
        [
            r#"<?xml version="1.0" encoding="UTF-8"?><oor:items xmlns:oor="http://openoffice.org/2001/registry" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">"#.to_owned(),
            item("/org.openoffice.Office.Common/Security/Scripting", "DisableActiveContent", "true"),
            item("/org.openoffice.Office.Common/Security/Scripting", "BlockUntrustedRefererLinks", "true"),
            item("/org.openoffice.Office.Common/Security/Scripting", "MacroSecurityLevel", "3"),
            item("/org.openoffice.Office.Writer/Content/Update", "Link", "0"),
            item("/org.openoffice.Office.Calc/Content/Update", "Link", "1"),
            "</oor:items>\n".to_owned(),
        ]
        .concat(),
    )?;
    Ok(())
}
/// Private destinations inside a workspace for one engine run.
pub struct Staging {
    pub output: PathBuf,
    pub output_dir: PathBuf,
    pub profile: PathBuf,
}
impl Staging {
    pub fn new(workspace: &Path, extension: &str) -> Result<Self> {
        let output_dir = workspace.join("result");
        fs::create_dir(&output_dir)?;
        harden(workspace)?;
        Ok(Self {
            output: workspace.join(format!("converted.{extension}")),
            output_dir,
            profile: workspace.join("profile"),
        })
    }
    /// The file produced by the engine: `{output}`, or the single file an
    /// engine wrote into `{output_dir}` under a name of its own choosing.
    pub fn single_output(&self) -> Result<PathBuf> {
        if self.output.is_file() {
            return Ok(self.output.clone());
        }
        let mut files = fs::read_dir(&self.output_dir)?
            .filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| p.is_file());
        match (files.next(), files.next()) {
            (Some(file), None) => Ok(file),
            (None, _) => bail!("The engine did not produce an output file"),
            _ => bail!("The engine produced several files"),
        }
    }
}
pub fn output_directory(input: &Path, directory: Option<&Path>) -> Result<PathBuf> {
    let directory = directory
        .unwrap_or(input.parent().context("Missing parent folder")?)
        .canonicalize()
        .context("Output directory not found")?;
    ensure!(directory.is_dir(), "Destination must be a directory");
    Ok(directory)
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
    let mut command = Command::new(program);
    let policy = log_dir.join("magick");
    if policy.is_dir() {
        command.env("MAGICK_CONFIGURE_PATH", &policy);
    }
    command.env("METROID_MEDIA", log_dir.join("media"));
    let mut child = command
        .args(args)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(log)
        .spawn()
        .context("Cannot start conversion engine")?;
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
                "Conversion engine failed ({status}): {}",
                String::from_utf8_lossy(&bytes)
            );
        }
        let cancelled = cancel.is_some_and(Path::exists);
        if cancelled || start.elapsed() > Duration::from_secs(timeout) {
            let _ = child.kill();
            let _ = child.wait();
            bail!(if cancelled {
                "Conversion cancelled"
            } else {
                "Conversion timed out"
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
    ensure!(!cancel.is_some_and(Path::exists), "Conversion cancelled");
    let input = input.canonicalize().context("Source file not found")?;
    ensure!(input.is_file(), "Source must be a regular file");
    let target = target.to_ascii_lowercase();
    let rule = registry()
        .into_iter()
        .find(|r| r.inputs.contains(&extension(&input)) && r.outputs.contains(&target))
        .context("Unsupported conversion")?;
    ensure!(
        extension(&input) != target,
        "The file is already in this format"
    );
    let backend = engine(&rule.backend)
        .with_context(|| format!("Missing engine: {}. Run metroid doctor.", rule.backend))?;
    let directory = output_directory(&input, directory)?;
    let workspace = tempfile::Builder::new()
        .prefix(".metroid-")
        .tempdir_in(&directory)?;
    let staging = Staging::new(workspace.path(), &target)?;
    let inputs = [input.clone()];
    let values = Expansion {
        inputs: &inputs,
        output: Some(&staging.output),
        output_dir: Some(&staging.output_dir),
        target: &target,
        profile: Some(&staging.profile),
        workspace: Some(workspace.path()),
    };
    for step in [&rule.args, &rule.then] {
        if !step.is_empty() {
            run_backend(
                &backend,
                &expand(step, &values),
                workspace.path(),
                cancel,
                timeout,
            )?;
        }
    }
    let output = staging.single_output()?;
    ensure!(!cancel.is_some_and(Path::exists), "Conversion cancelled");
    publish(&output, &input, &directory, &target)
}

pub fn merge_pdf(inputs: &[PathBuf], directory: &Path) -> Result<PathBuf> {
    ensure!(inputs.len() >= 2, "Select at least two PDF files");
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
            "Each source must be a PDF file"
        );
        args.push(input.to_string_lossy().into_owned());
        args.push("1-z".into());
    }
    args.extend(["--".into(), output.to_string_lossy().into_owned()]);
    run_backend(
        &executable("qpdf").context("Missing engine: qpdf")?,
        &args,
        workspace.path(),
        None,
        3600,
    )?;
    publish(&output, Path::new("merged.pdf"), &directory, "pdf")
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn registry_is_consistent() {
        for r in registry() {
            assert!(
                r.args
                    .iter()
                    .any(|a| ["{input}", "{coded_input}", "{archive_input}"].contains(&a.as_str()))
            );
            assert!(!r.glyph.is_empty());
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
    fn preserves_filename() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("staged");
        fs::write(&source, b"new").unwrap();
        let output = publish(&source, Path::new("été.final 01.jpg"), d.path(), "webp").unwrap();
        assert_eq!(output.file_name().unwrap(), "été.final 01.webp");
    }
    #[test]
    fn never_overwrites() {
        let d = tempfile::tempdir().unwrap();
        let source = d.path().join("staged");
        fs::write(&source, b"new").unwrap();
        let old = d.path().join("photo.png");
        fs::write(&old, b"old").unwrap();
        let output = publish(&source, Path::new("photo.jpg"), d.path(), "png").unwrap();
        assert_eq!(fs::read(old).unwrap(), b"old");
        assert_eq!(output.file_name().unwrap(), "photo-2.png");
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
