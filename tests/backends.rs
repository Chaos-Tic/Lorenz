//! Real backend tests. Optional engines are tested when installed; CI installs them.
use metroid::{convert, executable};
use std::{fs, path::Path, process::Command};
#[test]
fn image_roundtrip_with_unusual_filename() {
    let Some(magick) = executable("magick") else {
        eprintln!("SKIP: magick absent");
        return;
    };
    let d = tempfile::tempdir().unwrap();
    let input = d.path().join("photo é {output} ; $(literal).png");
    assert!(
        Command::new(magick)
            .args(["-size", "32x24", "xc:red"])
            .arg(&input)
            .status()
            .unwrap()
            .success()
    );
    let original = fs::read(&input).unwrap();
    let out = convert(&input, "webp", None, None, 30).unwrap();
    assert!(fs::read(out).unwrap().starts_with(b"RIFF"));
    assert_eq!(fs::read(input).unwrap(), original);
}
#[test]
fn subtitle_conversion_and_failed_cleanup() {
    if executable("ffmpeg").is_none() {
        eprintln!("SKIP: ffmpeg absent");
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let input = d.path().join("captions.srt");
    fs::write(&input, "1\n00:00:00,000 --> 00:00:01,000\nBonjour\n").unwrap();
    let out = convert(&input, "vtt", None, None, 30).unwrap();
    assert!(fs::read_to_string(out).unwrap().contains("WEBVTT"));
    let broken = d.path().join("broken.mp4");
    fs::write(&broken, "not video").unwrap();
    let before = fs::read_dir(d.path()).unwrap().count();
    assert!(convert(&broken, "mp3", None, None, 30).is_err());
    assert_eq!(fs::read_dir(d.path()).unwrap().count(), before);
}
#[test]
fn document_roundtrip() {
    if executable("pandoc").is_none() {
        eprintln!("SKIP: pandoc absent");
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let input = d.path().join("notes.md");
    fs::write(&input, "# Bonjour\n\nUn document local.\n").unwrap();
    let html = convert(&input, "html", None, None, 30).unwrap();
    let text = convert(&html, "txt", None, None, 30).unwrap();
    assert!(fs::read_to_string(text).unwrap().contains("Bonjour"));
}
#[test]
fn cli_reports_errors_as_json() {
    let output = Command::new(env!("CARGO_BIN_EXE_metroid"))
        .args([
            "convert",
            "--to",
            "png",
            "--json",
            "--",
            "/missing/file.jpg",
        ])
        .output()
        .unwrap();
    assert!(!output.status.success());
    let result: serde_json::Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(result["ok"], false);
}
#[test]
fn serve_recovers_after_invalid_request() {
    use std::io::Write;
    use std::process::Stdio;
    let mut child = Command::new(env!("CARGO_BIN_EXE_metroid"))
        .arg("serve")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    child
        .stdin
        .take()
        .unwrap()
        .write_all(b"invalid\n{\"input\":\"absent.jpg\",\"to\":\"png\"}\n")
        .unwrap();
    let out = child.wait_with_output().unwrap();
    assert!(out.status.success());
    assert_eq!(String::from_utf8(out.stdout).unwrap().lines().count(), 2);
}
#[test]
fn rejects_directories() {
    assert!(convert(Path::new("."), "png", None, None, 1).is_err());
}
#[cfg(unix)]
#[test]
fn backend_cancellation_and_timeout_reap_process() {
    use std::os::unix::fs::PermissionsExt;
    let d = tempfile::tempdir().unwrap();
    let sleeper = d.path().join("sleeper");
    // exec prevents leaving an extra shell child behind.
    fs::write(&sleeper, "#!/bin/sh\nexec sleep 20\n").unwrap();
    fs::set_permissions(&sleeper, fs::Permissions::from_mode(0o755)).unwrap();
    let cancel = d.path().join("cancel");
    std::thread::scope(|s| {
        s.spawn(|| {
            std::thread::sleep(std::time::Duration::from_millis(200));
            fs::write(&cancel, "").unwrap();
        });
        let error = metroid::run_backend(&sleeper, &[], d.path(), Some(&cancel), 10).unwrap_err();
        assert!(error.to_string().contains("annulée"));
    });
    let error = metroid::run_backend(&sleeper, &[], d.path(), None, 0).unwrap_err();
    assert!(error.to_string().contains("Délai"));
}
