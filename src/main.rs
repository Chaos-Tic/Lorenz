use anyhow::{Context, Result, bail};
use clap::{Parser, Subcommand};
use std::{
    io::{self, BufRead, Write},
    path::PathBuf,
    process::{Command, Stdio},
};

#[derive(Parser)]
#[command(version, about = "Metroid — conversions locales, fichiers préservés")]
struct Cli {
    #[command(subcommand)]
    command: Action,
}
#[derive(Subcommand)]
enum Action {
    /// Diagnostic des moteurs locaux
    Doctor {
        #[arg(long)]
        json: bool,
    },
    /// Formats possibles pour un fichier
    Formats {
        input: PathBuf,
        #[arg(long)]
        json: bool,
    },
    /// Convertir un ou plusieurs fichiers, sans écraser les originaux
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
    /// Ouvrir le bureau natif
    Gui { files: Vec<PathBuf> },
    /// Choisir un format avec fuzzel, wofi ou rofi
    Pick {
        input: PathBuf,
        #[arg(long, default_value = "fuzzel", value_parser = ["fuzzel", "wofi", "rofi"])]
        menu: String,
    },
    /// Fusionner plusieurs PDF avec qpdf
    MergePdf {
        #[arg(required = true)]
        inputs: Vec<PathBuf>,
        #[arg(long, default_value = ".")]
        output_dir: PathBuf,
    },
    /// Worker JSON Lines sur stdin/stdout, sans port réseau
    Serve,
    /// Installer le lanceur Linux et les menus contextuels utilisateur
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
            .context("Bureau absent : installez metroid-desktop")?
    };
    Command::new(program).args(files).spawn()?;
    Ok(())
}
fn integrate() -> Result<()> {
    #[cfg(windows)]
    {
        bail!("Utilisez scripts/install-windows.ps1 pour le menu Envoyer vers");
    }
    #[cfg(not(windows))]
    {
        use std::fs;
        use std::os::unix::fs::PermissionsExt;
        let exe = std::env::current_exe()?.with_file_name("metroid-desktop");
        anyhow::ensure!(
            exe.is_file(),
            "Installez d'abord les deux exécutables côte à côte"
        );
        let exe = exe.to_string_lossy();
        anyhow::ensure!(
            !exe.contains(['\n', '\r', '%', '`', '$', '"', '\\', '\'']),
            "Chemin d'installation non pris en charge"
        );
        let home = PathBuf::from(std::env::var_os("HOME").context("HOME absent")?);
        let data = std::env::var_os("XDG_DATA_HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|| home.join(".local/share"));
        let launcher = format!(
            "[Desktop Entry]\nType=Application\nName=Metroid\nComment=Convertir des fichiers localement\nExec=\"{exe}\" %F\nIcon=metroid\nTerminal=false\nCategories=Utility;Graphics;\n"
        );
        for (relative, contents) in [
            ("applications/metroid.desktop", launcher),
            (
                "kio/servicemenus/metroid.desktop",
                format!(
                    "[Desktop Entry]\nType=Service\nMimeType=application/octet-stream;\nActions=metroid;\nX-KDE-Priority=TopLevel\n[Desktop Action metroid]\nName=Convertir avec Metroid\nIcon=metroid\nExec=\"{exe}\" %F\n"
                ),
            ),
            (
                "nemo/actions/metroid.nemo_action",
                format!(
                    "[Nemo Action]\nName=Convertir avec Metroid\nComment=Conversions locales\nExec=\"{exe}\" %F\nSelection=Any\nExtensions=any;\nQuote=double\n"
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
            println!("Installé : {}", path.display());
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
                        if b.available { "OK     " } else { "ABSENT " },
                        b.name
                    );
                }
                println!("La disponibilité des codecs dépend des moteurs installés.");
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
                        if f.available { "" } else { " (absent)" }
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
                bail!("Aucune conversion disponible");
            }
            let mut child = Command::new(&menu)
                .arg("--dmenu")
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
            anyhow::ensure!(formats.contains(&target), "Sélection inconnue");
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
