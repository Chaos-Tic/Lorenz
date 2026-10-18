use anyhow::{Context, Result, bail};
use clap::{Parser, Subcommand};
use std::{
    io::{self, BufRead, Write},
    path::PathBuf,
    process::{Command, Stdio},
};

#[derive(Parser)]
#[command(
    version,
    about = "Metroid — local conversion, original files preserved"
)]
struct Cli {
    #[command(subcommand)]
    command: Task,
}
#[derive(Subcommand)]
enum Task {
    /// Inspect locally installed engines
    Doctor {
        #[arg(long)]
        json: bool,
    },
    /// List available output formats for a file
    Formats {
        input: PathBuf,
        #[arg(long)]
        json: bool,
    },
    /// Convert files without overwriting the originals
    Convert {
        #[arg(required = true)]
        inputs: Vec<PathBuf>,
        #[arg(long)]
        to: String,
        #[arg(long)]
        output_dir: Option<PathBuf>,
        #[arg(long)]
        cancel_file: Option<PathBuf>,
        #[arg(long, default_value_t = 3600)]
        timeout: u64,
        #[arg(long)]
        json: bool,
    },
    /// Open the native desktop
    Gui { files: Vec<PathBuf> },
    /// Choose a format with fuzzel, wofi or rofi
    Pick {
        input: PathBuf,
        #[arg(long, default_value = "fuzzel", value_parser = ["fuzzel", "wofi", "rofi"])]
        menu: String,
    },
    /// Merge multiple PDF files using qpdf
    MergePdf {
        #[arg(required = true)]
        inputs: Vec<PathBuf>,
        #[arg(long, default_value = ".")]
        output_dir: PathBuf,
    },
    /// List one wheel's items shared by every path: convert, tools or actions
    Menu {
        #[arg(value_parser = ["convert", "tools", "actions"])]
        kind: String,
        #[arg(required = true)]
        paths: Vec<PathBuf>,
        #[arg(long)]
        json: bool,
    },
    /// Run an editing tool (compress, resize-half, merge, split…)
    Tool {
        id: String,
        #[arg(required = true)]
        paths: Vec<PathBuf>,
        #[arg(long)]
        output_dir: Option<PathBuf>,
        #[arg(long)]
        cancel_file: Option<PathBuf>,
        #[arg(long, default_value_t = 3600)]
        timeout: u64,
        #[arg(long)]
        json: bool,
    },
    /// Run a file action (share, float, copy, copy-to, move-to, path, terminal, reveal)
    Action {
        id: String,
        #[arg(required = true)]
        paths: Vec<PathBuf>,
        /// Destination folder for copy-to and move-to
        #[arg(long)]
        dest: Option<PathBuf>,
        #[arg(long)]
        json: bool,
    },
    /// Start the tray companion at login, or stop doing so
    Autostart {
        #[arg(value_parser = ["enable", "disable", "status"])]
        state: String,
    },
    /// JSON Lines worker over stdin/stdout, without network sockets
    Serve,
    /// Install the Linux launcher and user context menus
    Integrate,
}
fn print_json(value: &impl serde::Serialize) -> Result<()> {
    println!("{}", serde_json::to_string(value)?);
    Ok(())
}
fn report(json: bool, input: &std::path::Path, result: &Result<PathBuf>) -> Result<()> {
    match result {
        Ok(output) if json => {
            print_json(&serde_json::json!({"ok":true,"input":input,"output":output}))
        }
        Ok(output) => {
            println!("{}", output.display());
            Ok(())
        }
        Err(error) if json => {
            print_json(&serde_json::json!({"ok":false,"input":input,"error":format!("{error:#}")}))
        }
        Err(error) => {
            eprintln!("{} : {error:#}", input.display());
            Ok(())
        }
    }
}
fn desktop_executable() -> Result<PathBuf> {
    let name = if cfg!(windows) {
        "metroid-desktop.exe"
    } else {
        "metroid-desktop"
    };
    let sibling = std::env::current_exe()?.with_file_name(name);
    if sibling.is_file() {
        Ok(sibling)
    } else {
        metroid::executable("metroid-desktop").context("Desktop missing: install metroid-desktop")
    }
}
/// An XDG base directory. Empty or relative values are ignored, as the
/// specification requires, so nothing is ever written relative to the cwd.
#[cfg(not(windows))]
fn xdg_dir(variable: &str, fallback: &str) -> Result<PathBuf> {
    if let Some(dir) = std::env::var_os(variable).map(PathBuf::from) {
        if dir.is_absolute() {
            return Ok(dir);
        }
    }
    let home = PathBuf::from(std::env::var_os("HOME").context("HOME is not set")?);
    anyhow::ensure!(home.is_absolute(), "HOME must be an absolute path");
    Ok(home.join(fallback))
}
fn autostart(state: &str) -> Result<()> {
    #[cfg(windows)]
    {
        let key = r"HKCU\Software\Microsoft\Windows\CurrentVersion\Run";
        let status = match state {
            "enable" => Command::new("reg")
                .args(["add", key, "/v", "Metroid", "/t", "REG_SZ", "/f", "/d"])
                .arg(format!(
                    "\"{}\" --background",
                    desktop_executable()?.display()
                ))
                .status()?,
            "disable" => Command::new("reg")
                .args(["delete", key, "/v", "Metroid", "/f"])
                .stderr(Stdio::null())
                .status()?,
            _ => Command::new("reg")
                .args(["query", key, "/v", "Metroid"])
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .status()?,
        };
        if state == "status" {
            println!(
                "{}",
                if status.success() {
                    "enabled"
                } else {
                    "disabled"
                }
            );
        } else if !status.success() && state == "enable" {
            bail!("Cannot update the Run registry key");
        }
        Ok(())
    }
    #[cfg(not(windows))]
    {
        use std::fs;
        let config = xdg_dir("XDG_CONFIG_HOME", ".config")?;
        let entry = config.join("autostart/metroid.desktop");
        match state {
            "enable" => {
                let exe = desktop_executable()?;
                let exe = exe.to_string_lossy();
                anyhow::ensure!(
                    !exe.contains(['\n', '\r', '%', '`', '$', '"', '\\', '\'']),
                    "Unsupported installation path"
                );
                fs::create_dir_all(entry.parent().unwrap())?;
                fs::write(
                    &entry,
                    format!(
                        "[Desktop Entry]\nType=Application\nName=Metroid\nComment=Shift-drag file conversion companion\nExec=\"{exe}\" --background\nIcon=metroid\nTerminal=false\nX-GNOME-Autostart-enabled=true\nX-KDE-autostart-phase=2\n"
                    ),
                )?;
                println!("Enabled: {}", entry.display());
            }
            "disable" => {
                match fs::remove_file(&entry) {
                    Err(e) if e.kind() != std::io::ErrorKind::NotFound => return Err(e.into()),
                    _ => {}
                }
                println!("Disabled");
            }
            _ => println!(
                "{}",
                if entry.is_file() {
                    "enabled"
                } else {
                    "disabled"
                }
            ),
        }
        Ok(())
    }
}
fn gui(files: &[PathBuf]) -> Result<()> {
    Command::new(desktop_executable()?).args(files).spawn()?;
    Ok(())
}
fn integrate() -> Result<()> {
    #[cfg(windows)]
    {
        bail!("Use scripts/install-windows.ps1 for the Send to menu");
    }
    #[cfg(not(windows))]
    {
        use std::fs;
        use std::os::unix::fs::PermissionsExt;
        let exe = std::env::current_exe()?.with_file_name("metroid-desktop");
        anyhow::ensure!(
            exe.is_file(),
            "Install both executables in the same directory first"
        );
        let exe = exe.to_string_lossy();
        anyhow::ensure!(
            !exe.contains(['\n', '\r', '%', '`', '$', '"', '\\', '\'']),
            "Unsupported installation path"
        );
        let data = xdg_dir("XDG_DATA_HOME", ".local/share")?;
        let launcher = format!(
            "[Desktop Entry]\nType=Application\nName=Metroid\nGenericName=File converter\nComment=Convert files locally\nExec=\"{exe}\" %F\nIcon=metroid\nTerminal=false\nCategories=Utility;FileTools;\nActions=manager;\n[Desktop Action manager]\nName=Open batch manager\nExec=\"{exe}\" --manager\n"
        );
        let wheels = [
            ("convert", "Convert with Metroid"),
            ("tools", "Metroid tools"),
            ("actions", "Metroid actions"),
        ];
        let mut files = vec![
            ("applications/metroid.desktop".to_owned(), launcher),
            (
                "kio/servicemenus/metroid.desktop".to_owned(),
                format!(
                    "[Desktop Entry]\nType=Service\nMimeType=all/all;\nActions=convert;tools;actions;\nX-KDE-Priority=TopLevel\nX-KDE-Submenu=Metroid\n{}",
                    wheels
                        .iter()
                        .map(|(id, name)| format!(
                            "[Desktop Action {id}]\nName={name}\nIcon=metroid\nExec=\"{exe}\" --wheel {id} %F\n"
                        ))
                        .collect::<String>()
                ),
            ),
        ];
        for (id, name) in wheels {
            files.push((
                format!("nemo/actions/metroid-{id}.nemo_action"),
                format!(
                    "[Nemo Action]\nName={name}\nComment=Local file processing\nExec=\"{exe}\" --wheel {id} %F\nIcon-Name=metroid\nSelection=Any\nExtensions=any;dir;\nQuote=double\n"
                ),
            ));
            files.push((
                format!("nautilus/scripts/{name}"),
                format!("#!/bin/sh\nexec '{exe}' --wheel {id} \"$@\"\n"),
            ));
        }
        // Remove entries written by earlier versions.
        for old in [
            "nemo/actions/metroid.nemo_action",
            "nautilus/scripts/Metroid",
        ] {
            let _ = fs::remove_file(data.join(old));
        }
        for (relative, contents) in files {
            let path = data.join(&relative);
            fs::create_dir_all(path.parent().unwrap())?;
            fs::write(&path, contents)?;
            if relative.starts_with("nautilus/") {
                fs::set_permissions(&path, fs::Permissions::from_mode(0o755))?;
            }
            println!("Installed: {}", path.display());
        }
        let icon = data.join("icons/hicolor/scalable/apps/metroid.svg");
        fs::create_dir_all(icon.parent().unwrap())?;
        fs::write(icon, include_str!("../assets/metroid.svg"))?;
        Ok(())
    }
}
#[derive(serde::Deserialize)]
struct Request {
    input: PathBuf,
    to: String,
    output_dir: Option<PathBuf>,
}
fn main() -> Result<()> {
    match Cli::parse().command {
        Task::Doctor { json } => {
            let d = metroid::doctor();
            if json {
                print_json(&d)?;
            } else {
                println!(
                    "Metroid {} · {} · {} · {}",
                    d.version, d.os, d.session, d.desktop
                );
                for b in d.backends {
                    println!(
                        "{} {}",
                        if b.available { "OK     " } else { "MISSING" },
                        b.name
                    );
                }
                println!("Codec availability depends on the installed engines.");
            }
        }
        Task::Formats { input, json } => {
            let formats = metroid::formats(&input);
            if json {
                print_json(&formats)?;
            } else {
                for f in formats {
                    println!(
                        "{}\t{}\t{}{}",
                        f.target,
                        f.category,
                        f.backend,
                        if f.available { "" } else { " (missing)" }
                    );
                }
            }
        }
        Task::Convert {
            inputs,
            to,
            output_dir,
            cancel_file,
            timeout,
            json,
        } => {
            let mut failed = false;
            for input in inputs {
                match metroid::convert(
                    &input,
                    &to,
                    output_dir.as_deref(),
                    cancel_file.as_deref(),
                    timeout,
                ) {
                    Ok(output) => {
                        if json {
                            print_json(
                                &serde_json::json!({"ok":true,"input":input,"output":output}),
                            )?;
                        } else {
                            println!("{}", output.display());
                        }
                    }
                    Err(error) => {
                        failed = true;
                        if json {
                            print_json(
                                &serde_json::json!({"ok":false,"input":input,"error":format!("{error:#}")}),
                            )?;
                        } else {
                            eprintln!("{} : {error:#}", input.display());
                        }
                    }
                }
                if cancel_file.as_deref().is_some_and(std::path::Path::exists) {
                    break;
                }
            }
            if failed {
                std::process::exit(1);
            }
        }
        Task::Gui { files } => gui(&files)?,
        Task::MergePdf { inputs, output_dir } => {
            println!("{}", metroid::merge_pdf(&inputs, &output_dir)?.display())
        }
        Task::Pick { input, menu } => {
            let formats: Vec<_> = metroid::formats(&input)
                .into_iter()
                .filter(|f| f.available)
                .map(|f| f.target)
                .collect();
            if formats.is_empty() {
                bail!("No conversions available");
            }
            let mut child = Command::new(&menu)
                .arg(if menu == "rofi" { "-dmenu" } else { "--dmenu" })
                .stdin(Stdio::piped())
                .stdout(Stdio::piped())
                .spawn()?;
            child
                .stdin
                .take()
                .unwrap()
                .write_all(formats.join("\n").as_bytes())?;
            let output = child.wait_with_output()?;
            if !output.status.success() {
                return Ok(());
            }
            let target = String::from_utf8(output.stdout)?.trim().to_owned();
            anyhow::ensure!(formats.contains(&target), "Unknown selection");
            println!(
                "{}",
                metroid::convert(&input, &target, None, None, 3600)?.display()
            );
        }
        Task::Serve => {
            for line in io::stdin().lock().lines() {
                let result = (|| -> Result<PathBuf> {
                    let req: Request = serde_json::from_str(&line?)?;
                    metroid::convert(&req.input, &req.to, req.output_dir.as_deref(), None, 3600)
                })();
                match result {
                    Ok(output) => print_json(&serde_json::json!({"ok":true,"output":output}))?,
                    Err(e) => {
                        print_json(&serde_json::json!({"ok":false,"error":format!("{e:#}")}))?
                    }
                }
                io::stdout().flush()?;
            }
        }
        Task::Menu { kind, paths, json } => {
            let items = metroid::menu(&kind, &paths)?;
            if json {
                print_json(&items)?;
            } else {
                for item in items {
                    println!("{}\t{}\t{}", item.id, item.label, item.category);
                }
            }
        }
        Task::Tool {
            id,
            paths,
            output_dir,
            cancel_file,
            timeout,
            json,
        } => {
            let results = metroid::tools::run(
                &id,
                &paths,
                output_dir.as_deref(),
                cancel_file.as_deref(),
                timeout,
            );
            let results = match results {
                Ok(results) => results,
                Err(error) => vec![(paths[0].clone(), Err(error))],
            };
            let failed = results.iter().any(|(_, r)| r.is_err());
            for (input, result) in &results {
                report(json, input, result)?;
            }
            if failed {
                std::process::exit(1);
            }
        }
        Task::Action {
            id,
            paths,
            dest,
            json,
        } => match metroid::actions::run(&id, &paths, dest.as_deref()) {
            Ok(outputs) => {
                for output in outputs {
                    report(json, &paths[0], &Ok(output))?;
                }
                if json {
                    print_json(&serde_json::json!({"ok":true,"done":true}))?;
                }
            }
            Err(error) => {
                report(json, &paths[0], &Err(error))?;
                std::process::exit(1);
            }
        },
        Task::Autostart { state } => autostart(&state)?,
        Task::Integrate => integrate()?,
    }
    Ok(())
}
