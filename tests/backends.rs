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
    fs::write(&input, "1\n00:00:00,000 --> 00:00:01,000\nHello\n").unwrap();
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
    fs::write(&input, "# Hello\n\nA local document.\n").unwrap();
    let html = convert(&input, "html", None, None, 30).unwrap();
    let text = convert(&html, "txt", None, None, 30).unwrap();
    assert!(fs::read_to_string(text).unwrap().contains("Hello"));
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
        assert!(error.to_string().contains("cancelled"));
    });
    let error = metroid::run_backend(&sleeper, &[], d.path(), None, 0).unwrap_err();
    assert!(error.to_string().contains("timed out"));
}

#[test]
fn pdf_text_render_and_merge() {
    if ["pdftotext", "pdftoppm", "qpdf"]
        .iter()
        .any(|name| executable(name).is_none())
    {
        eprintln!("SKIP: PDF tools absent");
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let input = d.path().join("page.pdf");
    let stream = "BT /F1 18 Tf 30 80 Td (Metroid PDF test) Tj ET\n";
    let objects = [
        "<< /Type /Catalog /Pages 2 0 R >>".to_owned(),
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>".to_owned(),
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 120] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>".to_owned(),
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>".to_owned(),
        format!("<< /Length {} >>\nstream\n{stream}endstream",stream.len()),
    ];
    let mut pdf = "%PDF-1.4\n".to_owned();
    let mut offsets = Vec::new();
    for (i, object) in objects.iter().enumerate() {
        offsets.push(pdf.len());
        pdf.push_str(&format!("{} 0 obj\n{object}\nendobj\n", i + 1));
    }
    let xref = pdf.len();
    pdf.push_str("xref\n0 6\n0000000000 65535 f \n");
    for offset in offsets {
        pdf.push_str(&format!("{offset:010} 00000 n \n"));
    }
    pdf.push_str(&format!(
        "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n"
    ));
    fs::write(&input, pdf).unwrap();
    let text = convert(&input, "txt", None, None, 30).unwrap();
    assert!(
        fs::read_to_string(text)
            .unwrap()
            .contains("Metroid PDF test")
    );
    let image = convert(&input, "png", None, None, 30).unwrap();
    assert!(fs::read(image).unwrap().starts_with(b"\x89PNG"));
    let merged = metroid::merge_pdf(&[input.clone(), input], d.path()).unwrap();
    let pages = Command::new(executable("qpdf").unwrap())
        .arg("--show-npages")
        .arg(merged)
        .output()
        .unwrap();
    assert_eq!(String::from_utf8(pages.stdout).unwrap().trim(), "2");
}

#[test]
fn audio_and_video_conversion() {
    let Some(ffmpeg) = executable("ffmpeg") else {
        eprintln!("SKIP: ffmpeg absent");
        return;
    };
    let d = tempfile::tempdir().unwrap();
    let input = d.path().join("sound.wav");
    assert!(
        Command::new(&ffmpeg)
            .args([
                "-v",
                "error",
                "-f",
                "lavfi",
                "-i",
                "sine=frequency=440:duration=0.2"
            ])
            .arg(&input)
            .status()
            .unwrap()
            .success()
    );
    let audio = convert(&input, "flac", None, None, 30).unwrap();
    assert!(fs::read(audio).unwrap().starts_with(b"fLaC"));
    let video = d.path().join("clip.avi");
    assert!(
        Command::new(&ffmpeg)
            .args([
                "-v",
                "error",
                "-f",
                "lavfi",
                "-i",
                "color=c=red:s=32x32:d=0.2",
                "-c:v",
                "rawvideo"
            ])
            .arg(&video)
            .status()
            .unwrap()
            .success()
    );
    let gif = convert(&video, "gif", None, None, 30).unwrap();
    assert!(fs::read(gif).unwrap().starts_with(b"GIF"));
}
