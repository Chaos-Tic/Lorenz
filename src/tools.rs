//! Editing tools for the second wheel: compress, resize, rotate, split, merge…
use crate::{
    Expansion, Staging, base_stem, engine, expand, extension, output_directory, publish_as,
    publish_dir, run_backend,
};
use anyhow::{Context, Result, ensure};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

fn same() -> String {
    "same".into()
}
fn one() -> usize {
    1
}
#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Tool {
    pub id: String,
    pub label: String,
    pub category: String,
    pub glyph: String,
    pub inputs: Vec<String>,
    pub backend: String,
    pub args: Vec<String>,
    #[serde(default = "same")]
    pub output: String,
    #[serde(default)]
    pub suffix: String,
    #[serde(default)]
    pub combine: bool,
    #[serde(default = "one")]
    pub min_files: usize,
    #[serde(default)]
    pub name: String,
}
#[derive(Deserialize)]
struct Registry {
    tool: Vec<Tool>,
}
pub fn registry() -> Vec<Tool> {
    toml::from_str::<Registry>(include_str!("../registry/tools.toml"))
        .expect("valid bundled tool registry")
        .tool
}
impl Tool {
    /// Folders are accepted only by tools declared for any input (`*`).
    pub fn accepts(&self, path: &Path) -> bool {
        if self.inputs.iter().any(|i| i == "*") {
            return path.exists();
        }
        path.is_file() && self.inputs.contains(&extension(path))
    }
}
/// The rule used for a whole selection: the first rule of this id accepting
/// every source. Mixed selections that need different rules are not offered.
pub fn rule_for(id: &str, paths: &[PathBuf]) -> Option<Tool> {
    registry().into_iter().find(|t| {
        t.id == id
            && !paths.is_empty()
            && paths.len() >= t.min_files
            && paths.iter().all(|p| t.accepts(p))
    })
}
/// Per-file tools may use a different rule for each source (JPEG and PNG
/// compression differ); combining tools need one rule for the whole selection.
fn applies(id: &str, paths: &[PathBuf]) -> Option<Tool> {
    let first = registry().into_iter().find(|t| t.id == id)?;
    if first.combine {
        return rule_for(id, paths).filter(|t| engine(&t.backend).is_some());
    }
    let rules: Vec<Tool> = paths
        .iter()
        .map(|p| rule_for(id, std::slice::from_ref(p)))
        .collect::<Option<_>>()?;
    (!rules.is_empty() && rules.iter().all(|t| engine(&t.backend).is_some()))
        .then(|| rules[0].clone())
}
pub fn available(paths: &[PathBuf]) -> Vec<Tool> {
    let mut seen = Vec::<String>::new();
    let mut tools = Vec::new();
    for tool in registry() {
        if seen.contains(&tool.id) {
            continue;
        }
        seen.push(tool.id.clone());
        if let Some(rule) = applies(&tool.id, paths) {
            tools.push(rule);
        }
    }
    tools
}
fn run_once(
    tool: &Tool,
    inputs: &[PathBuf],
    directory: &Path,
    cancel: Option<&Path>,
    timeout: u64,
) -> Result<PathBuf> {
    let backend = engine(&tool.backend)
        .with_context(|| format!("Missing engine: {}. Run metroid doctor.", tool.backend))?;
    let first = &inputs[0];
    let extension = match tool.output.as_str() {
        "same" => extension(first),
        "dir" => String::new(),
        other => other.to_owned(),
    };
    let mut stem = if tool.combine && inputs.len() > 1 {
        std::ffi::OsString::from(&tool.name)
    } else if first.is_dir() {
        first.file_name().unwrap_or_default().to_os_string()
    } else {
        base_stem(first)
    };
    if !tool.suffix.is_empty() {
        stem.push(format!("-{}", tool.suffix));
    }
    let workspace = tempfile::Builder::new()
        .prefix(".metroid-")
        .tempdir_in(directory)?;
    let staging = Staging::new(
        workspace.path(),
        if extension.is_empty() {
            "out"
        } else {
            &extension
        },
    )?;
    let args = expand(
        &tool.args,
        &Expansion {
            inputs,
            output: Some(&staging.output),
            output_dir: Some(&staging.output_dir),
            target: &extension,
            profile: Some(&staging.profile),
        },
    );
    run_backend(&backend, &args, workspace.path(), cancel, timeout)?;
    ensure!(!cancel.is_some_and(Path::exists), "Operation cancelled");
    if tool.output == "dir" {
        publish_dir(&staging.output_dir, &stem, directory)
    } else {
        publish_as(&staging.single_output()?, &stem, directory, &extension)
    }
}
/// Runs a tool. Per-file tools return one result per source; combining tools
/// return a single result. Sources are never modified.
pub fn run(
    id: &str,
    inputs: &[PathBuf],
    directory: Option<&Path>,
    cancel: Option<&Path>,
    timeout: u64,
) -> Result<Vec<(PathBuf, Result<PathBuf>)>> {
    let inputs = inputs
        .iter()
        .map(|p| {
            p.canonicalize()
                .with_context(|| format!("Source not found: {}", p.display()))
        })
        .collect::<Result<Vec<_>>>()?;
    let tool = applies(id, &inputs).context("This tool does not apply to the selection")?;
    if tool.combine {
        let directory = output_directory(&inputs[0], directory)?;
        let result = run_once(&tool, &inputs, &directory, cancel, timeout);
        return Ok(vec![(inputs[0].clone(), result)]);
    }
    let mut results = Vec::new();
    for input in inputs {
        if cancel.is_some_and(Path::exists) {
            break;
        }
        let single = std::slice::from_ref(&input);
        let result = rule_for(id, single)
            .context("This tool does not apply to this file")
            .and_then(|tool| {
                let directory = output_directory(&input, directory)?;
                run_once(&tool, single, &directory, cancel, timeout)
            });
        results.push((input, result));
    }
    Ok(results)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn registry_is_consistent() {
        for t in registry() {
            assert!(t.args.iter().any(|a| a.starts_with("{input")
                || a == "{coded_input}"
                || a == "{relative_inputs}"));
            assert!(t.id.chars().all(|c| c.is_ascii_lowercase() || c == '-'));
            assert!(!t.combine || !t.name.is_empty() || t.min_files <= 1);
            assert!(
                t.output == "same"
                    || t.output == "dir"
                    || t.output.chars().all(|c| c.is_ascii_alphanumeric())
            );
        }
    }
    #[test]
    fn selection_rules() {
        let d = tempfile::tempdir().unwrap();
        let a = d.path().join("a.pdf");
        let b = d.path().join("b.pdf");
        let png = d.path().join("c.png");
        for p in [&a, &b, &png] {
            std::fs::write(p, b"x").unwrap();
        }
        assert!(rule_for("merge", std::slice::from_ref(&a)).is_none());
        assert!(rule_for("merge", &[a.clone(), b.clone()]).is_some());
        assert!(rule_for("merge", &[a.clone(), png.clone()]).is_none());
        assert!(rule_for("zip", &[a.clone(), d.path().to_path_buf()]).is_some());
        assert!(rule_for("compress", &[d.path().to_path_buf()]).is_none());
        assert_eq!(rule_for("compress", &[png]).unwrap().backend, "magick");
    }
}
