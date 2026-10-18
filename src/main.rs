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
    command: Action,
}
#[derive(Subcommand)]
enum Action {
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
    /// JSON Lines worker over stdin/stdout, without network sockets
    Serve,
    /// Install the Linux launcher and user context menus
    Integrate,
}
fn print_json(value: &impl serde::Serialize) -> Result<()> {
    println!("{}", serde_json::to_string(value)?);
    Ok(())
}
fn gui(files: &[PathBuf]) -> Result<()> {
    let name = if cfg!(windows) {
        "metroid-desktop.exe"
    } else {
        "metroid-desktop"
    };
    let sibling = std::env::current_exe()?.with_file_name(name);
    let program = if sibling.is_file() {
        sibling
    } else {
        metroid::executable("metroid-desktop")
            .context("Desktop missing: install metroid-desktop")?
    };
    Command::new(program).args(files).spawn()?;
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
        let home = PathBuf::from(std::env::var_os("HOME").context("HOME is not set")?);
        let data = std::env::var_os("XDG_DATA_HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|| home.join(".local/share"));
        let launcher = format!(
            "[Desktop Entry]\nType=Application\nName=Metroid\nComment=Convert files locally\nExec=\"{exe}\" %F\nIcon=metroid\nTerminal=false\nCategories=Utility;Graphics;\n"
        );
        for (relative, contents) in [
            ("applications/metroid.desktop", launcher),
            (
                "kio/servicemenus/metroid.desktop",
                format!(
                    "[Desktop Entry]\nType=Service\nMimeType=application/octet-stream;\nActions=metroid;\nX-KDE-Priority=TopLevel\n[Desktop Action metroid]\nName=Convert with Metroid\nIcon=metroid\nExec=\"{exe}\" %F\n"
                ),
            ),
            (
                "nemo/actions/metroid.nemo_action",
                format!(
                    "[Nemo Action]\nName=Convert with Metroid\nComment=Local file conversion\nExec=\"{exe}\" %F\nSelection=Any\nExtensions=any;\nQuote=double\n"
                ),
            ),
            (
                "nautilus/scripts/Metroid",
                format!("#!/bin/sh\nexec '{exe}' \"$@\"\n"),
            ),
        ] {
            let path = data.join(relative);
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
        Action::Doctor { json } => {
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
        Action::Formats { input, json } => {
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
        Action::Convert {
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
        Action::Gui { files } => gui(&files)?,
        Action::MergePdf { inputs, output_dir } => {
            println!("{}", metroid::merge_pdf(&inputs, &output_dir)?.display())
        }
        Action::Pick { input, menu } => {
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
        Action::Serve => {
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
        Action::Integrate => integrate()?,
    }
    Ok(())
}
