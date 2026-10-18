//! Real tool, office and archive tests. Optional engines are skipped when absent.
use metroid::{convert, executable, menu, tools};
use std::{fs, path::Path, path::PathBuf, process::Command};

fn have(names: &[&str]) -> bool {
    let missing: Vec<_> = names.iter().filter(|n| executable(n).is_none()).collect();
    if !missing.is_empty() {
        eprintln!("SKIP: {missing:?} absent");
    }
    missing.is_empty()
}
fn ids(kind: &str, paths: &[PathBuf]) -> Vec<String> {
    menu(kind, paths)
        .unwrap()
        .into_iter()
        .map(|i| i.id)
        .collect()
}
fn one(results: Vec<(PathBuf, anyhow::Result<PathBuf>)>) -> PathBuf {
    assert_eq!(results.len(), 1);
    results.into_iter().next().unwrap().1.unwrap()
}
fn magick(args: &[&str], output: &Path) {
    let status = Command::new(executable("magick").unwrap())
        .args(args)
        .arg(output)
        .status()
        .unwrap();
    assert!(status.success());
}
fn image_size(path: &Path) -> String {
    let out = Command::new(executable("magick").unwrap())
        .args(["identify", "-format", "%wx%h"])
        .arg(path)
        .output()
        .unwrap();
    String::from_utf8(out.stdout).unwrap()
}

#[test]
fn image_tools_keep_sources_and_names() {
    if !have(&["magick"]) {
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let photo = d.path().join("holiday photo.jpg");
    let png = d.path().join("diagram.png");
    magick(&["-size", "64x32", "plasma:"], &photo);
    magick(&["-size", "40x20", "xc:blue"], &png);
    let original = fs::read(&photo).unwrap();
    let both = [photo.clone(), png.clone()];
    let offered = ids("tools", &both);
    assert!(offered.contains(&"compress".into()) && offered.contains(&"rotate-right".into()));
    let results = tools::run("compress", &both, None, None, 60).unwrap();
    assert_eq!(results.len(), 2);
    for (_, output) in &results {
        assert!(output.as_ref().unwrap().exists());
    }
    assert_eq!(
        results[0].1.as_ref().unwrap().file_name().unwrap(),
        "holiday photo-compressed.jpg"
    );
    let rotated =
        one(tools::run("rotate-right", std::slice::from_ref(&photo), None, None, 60).unwrap());
    assert_eq!(image_size(&rotated), "32x64");
    let half =
        one(tools::run("resize-half", std::slice::from_ref(&photo), None, None, 60).unwrap());
    assert_eq!(image_size(&half), "32x16");
    let again =
        one(tools::run("resize-half", std::slice::from_ref(&photo), None, None, 60).unwrap());
    assert_eq!(again.file_name().unwrap(), "holiday photo-half-2.jpg");
    assert_eq!(fs::read(&photo).unwrap(), original);
}

#[test]
fn pdf_tools_merge_split_and_render() {
    if !have(&["magick", "qpdf", "pdftoppm", "gs"]) {
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let a = d.path().join("a.pdf");
    let b = d.path().join("b.pdf");
    magick(&["-size", "60x80", "xc:red"], &a);
    magick(
        &["-size", "60x80", "xc:blue", "-size", "60x80", "xc:green"],
        &b,
    );
    assert!(ids("tools", &[a.clone(), b.clone()]).contains(&"merge".into()));
    assert!(!ids("tools", std::slice::from_ref(&a)).contains(&"merge".into()));
    let merged = one(tools::run("merge", &[a.clone(), b.clone()], None, None, 60).unwrap());
    assert_eq!(merged.file_name().unwrap(), "Merged.pdf");
    let pages = Command::new(executable("qpdf").unwrap())
        .arg("--show-npages")
        .arg(&merged)
        .output()
        .unwrap();
    assert_eq!(String::from_utf8(pages.stdout).unwrap().trim(), "3");
    let split = one(tools::run("split", std::slice::from_ref(&merged), None, None, 60).unwrap());
    assert!(split.is_dir() && split.file_name().unwrap() == "Merged-pages");
    assert_eq!(fs::read_dir(&split).unwrap().count(), 3);
    let images = one(tools::run("pages-png", std::slice::from_ref(&b), None, None, 60).unwrap());
    assert_eq!(fs::read_dir(&images).unwrap().count(), 2);
    let small = one(tools::run("compress", std::slice::from_ref(&a), None, None, 60).unwrap());
    assert!(fs::read(small).unwrap().starts_with(b"%PDF"));
}

#[test]
fn media_tools() {
    if !have(&["ffmpeg"]) {
        return;
    }
    let ffmpeg = executable("ffmpeg").unwrap();
    let d = tempfile::tempdir().unwrap();
    let clip = d.path().join("clip.mkv");
    let status = Command::new(&ffmpeg)
        .args([
            "-v",
            "error",
            "-f",
            "lavfi",
            "-i",
            "testsrc=size=96x64:rate=10",
            "-f",
            "lavfi",
        ])
        .args(["-i", "sine=frequency=440", "-t", "1", "-shortest"])
        .arg(&clip)
        .status()
        .unwrap();
    assert!(status.success());
    let offered = ids("tools", std::slice::from_ref(&clip));
    for id in ["mute", "speed", "frame", "resize-half"] {
        assert!(offered.contains(&id.into()), "{id} missing");
    }
    let frame = one(tools::run("frame", std::slice::from_ref(&clip), None, None, 60).unwrap());
    assert!(fs::read(frame).unwrap().starts_with(b"\x89PNG"));
    for id in ["mute", "speed", "resize-half"] {
        one(tools::run(id, std::slice::from_ref(&clip), None, None, 60).unwrap());
    }
    let wav = d.path().join("tone.wav");
    let status = Command::new(&ffmpeg)
        .args([
            "-v",
            "error",
            "-f",
            "lavfi",
            "-i",
            "sine=frequency=440:duration=0.5",
        ])
        .arg(&wav)
        .status()
        .unwrap();
    assert!(status.success());
    let mono = one(tools::run("mono", std::slice::from_ref(&wav), None, None, 60).unwrap());
    assert_eq!(mono.file_name().unwrap(), "tone-mono.wav");
    one(tools::run("normalize", &[wav], None, None, 60).unwrap());
}

#[test]
fn archives_round_trip_without_extraction_escape() {
    if !have(&["bsdtar"]) {
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let folder = d.path().join("project");
    fs::create_dir(&folder).unwrap();
    fs::write(folder.join("notes.txt"), "hello").unwrap();
    let odd = d.path().join("-odd @name.txt");
    fs::write(&odd, "odd").unwrap();
    let archive = one(tools::run("zip", &[folder.clone(), odd.clone()], None, None, 60).unwrap());
    assert_eq!(archive.file_name().unwrap(), "Archive.zip");
    let tgz = convert(&archive, "tgz", None, None, 60).unwrap();
    assert_eq!(tgz.file_name().unwrap(), "Archive.tgz");
    let out = one(tools::run("extract", &[tgz], None, None, 60).unwrap());
    assert_eq!(out.file_name().unwrap(), "Archive");
    assert_eq!(
        fs::read_to_string(out.join("project/notes.txt")).unwrap(),
        "hello"
    );
    assert_eq!(
        fs::read_to_string(out.join("-odd @name.txt")).unwrap(),
        "odd"
    );
    // An archive entry with ".." must not be written outside the new folder.
    let evil = d.path().join("evil.tar");
    let staging = d.path().join("staging/a/b");
    fs::create_dir_all(&staging).unwrap();
    fs::write(d.path().join("staging/escape.txt"), "x").unwrap();
    let status = Command::new(executable("bsdtar").unwrap())
        .current_dir(&staging)
        .args(["-P", "-cf"])
        .arg(&evil)
        .arg("../../escape.txt")
        .status()
        .unwrap();
    assert!(status.success());
    let before = fs::read_dir(d.path()).unwrap().count();
    let _ = tools::run("extract", &[evil], None, None, 60);
    let after: Vec<_> = fs::read_dir(d.path())
        .unwrap()
        .map(|e| e.unwrap().file_name())
        .collect();
    assert!(after.len() <= before + 1);
    assert!(!d.path().join("escape.txt").exists());
}

#[test]
fn office_documents_slides_and_sheets() {
    if !have(&["soffice"]) {
        return;
    }
    let d = tempfile::tempdir().unwrap();
    let csv = d.path().join("budget été.csv");
    fs::write(&csv, "item,cost\nrent,800\nfood,300\n").unwrap();
    assert!(ids("convert", std::slice::from_ref(&csv)).contains(&"xlsx".into()));
    let xlsx = convert(&csv, "xlsx", None, None, 180).unwrap();
    assert_eq!(xlsx.file_name().unwrap(), "budget été.xlsx");
    assert!(fs::read(&xlsx).unwrap().starts_with(b"PK"));
    let back = convert(&xlsx, "csv", None, None, 180).unwrap();
    assert!(fs::read_to_string(back).unwrap().contains("rent,800"));
    let pdf = convert(&xlsx, "pdf", None, None, 180).unwrap();
    assert!(fs::read(pdf).unwrap().starts_with(b"%PDF"));
    let rtf = d.path().join("letter.rtf");
    fs::write(&rtf, "{\\rtf1\\ansi Hello from RTF.\\par}").unwrap();
    let pdf = convert(&rtf, "pdf", None, None, 180).unwrap();
    assert!(fs::read(pdf).unwrap().starts_with(b"%PDF"));
    // A minimal flat ODF presentation, turned into PPTX by LibreOffice itself.
    let deck = d.path().join("deck.fodp");
    fs::write(&deck, concat!(
        r#"<?xml version="1.0" encoding="UTF-8"?><office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" "#,
        r#"xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" "#,
        r#"xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" office:version="1.3" "#,
        r#"office:mimetype="application/vnd.oasis.opendocument.presentation"><office:body><office:presentation>"#,
        r#"<draw:page draw:name="One"><draw:frame svg:x="2cm" svg:y="2cm" svg:width="10cm" svg:height="3cm">"#,
        r#"<draw:text-box><text:p>Hello slide</text:p></draw:text-box></draw:frame></draw:page>"#,
        r#"</office:presentation></office:body></office:document>"#,
    ))
    .unwrap();
    let status = Command::new(executable("soffice").unwrap())
        .arg(format!(
            "-env:UserInstallation=file://{}/profile",
            d.path().display()
        ))
        .args(["--headless", "--convert-to", "pptx", "--outdir"])
        .arg(d.path())
        .arg(&deck)
        .status()
        .unwrap();
    assert!(status.success());
    let pptx = d.path().join("deck.pptx");
    let png = convert(&pptx, "png", None, None, 180).unwrap();
    assert!(fs::read(png).unwrap().starts_with(b"\x89PNG"));
    let pdf = convert(&pptx, "pdf", None, None, 180).unwrap();
    assert!(fs::read(pdf).unwrap().starts_with(b"%PDF"));
    let odp = convert(&pptx, "odp", None, None, 180).unwrap();
    assert!(fs::read(odp).unwrap().starts_with(b"PK"));
}
