// traj-viewer: visually inspect SLAM experiment trajectories vs. KITTI ground truth.
//
// Left  panel: list of experiments (from experiments/expNNNN_*).
// Right panel: top-down trajectory plot — ground truth + predicted, per sequence.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

use eframe::egui;
use egui::{Color32, RichText, ScrollArea, Stroke};
use egui_plot::{Line, Plot, PlotPoints};

const REPO_DEFAULT: &str = "/data2/repos/autoslam";

fn main() -> eframe::Result<()> {
    let repo = std::env::args().nth(1).unwrap_or_else(|| REPO_DEFAULT.to_string());
    let repo = PathBuf::from(repo);

    let native_options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([1400.0, 900.0]),
        ..Default::default()
    };
    eframe::run_native(
        "autoslam traj-viewer",
        native_options,
        Box::new(move |_cc| Ok(Box::new(App::new(repo)))),
    )
}

#[derive(Clone)]
struct Experiment {
    dir: PathBuf,
    name: String,
    dev_trans_pct: Option<f64>,
    full_trans_pct: Option<f64>,
    status: String,
    description: String,
}

struct App {
    repo: PathBuf,
    experiments: Vec<Experiment>,
    selected_idx: Option<usize>,
    // pred_set: "preds_dev" or "preds_full"
    pred_set: String,
    selected_seq: String,
    available_seqs: Vec<String>,
    show_gt: bool,
    show_pred: bool,
    // cache: (exp_idx, pred_set, seq) -> trajectory xz
    pred_cache: HashMap<(usize, String, String), Vec<[f64; 2]>>,
    gt_cache: HashMap<String, Vec<[f64; 2]>>,
    error_msg: Option<String>,
}

impl App {
    fn new(repo: PathBuf) -> Self {
        let mut experiments = scan_experiments(&repo);
        // newest exp first (by name reverse)
        experiments.sort_by(|a, b| b.name.cmp(&a.name));
        let mut app = Self {
            repo,
            experiments,
            selected_idx: None,
            pred_set: "preds_full".into(),
            selected_seq: "00".into(),
            available_seqs: Vec::new(),
            show_gt: true,
            show_pred: true,
            pred_cache: HashMap::new(),
            gt_cache: HashMap::new(),
            error_msg: None,
        };
        if !app.experiments.is_empty() {
            app.select(0);
        }
        app
    }

    fn select(&mut self, idx: usize) {
        self.selected_idx = Some(idx);
        self.refresh_available_seqs();
        if !self.available_seqs.contains(&self.selected_seq) {
            if let Some(first) = self.available_seqs.first() {
                self.selected_seq = first.clone();
            }
        }
    }

    fn refresh_available_seqs(&mut self) {
        self.available_seqs.clear();
        let Some(idx) = self.selected_idx else { return };
        let exp = &self.experiments[idx];
        let pred_dir = exp.dir.join(&self.pred_set);
        let mut seqs: Vec<String> = match fs::read_dir(&pred_dir) {
            Ok(rd) => rd
                .filter_map(|e| e.ok())
                .filter_map(|e| {
                    let p = e.path();
                    if p.extension().and_then(|x| x.to_str()) == Some("txt") {
                        p.file_stem().and_then(|s| s.to_str()).map(|s| s.to_string())
                    } else {
                        None
                    }
                })
                .collect(),
            Err(_) => Vec::new(),
        };
        seqs.sort();
        self.available_seqs = seqs;
    }

    fn pred_traj(&mut self, exp_idx: usize, seq: &str) -> Option<&Vec<[f64; 2]>> {
        let key = (exp_idx, self.pred_set.clone(), seq.to_string());
        if !self.pred_cache.contains_key(&key) {
            let path = self.experiments[exp_idx]
                .dir
                .join(&self.pred_set)
                .join(format!("{seq}.txt"));
            match load_kitti_xz(&path) {
                Ok(xz) => {
                    self.pred_cache.insert(key.clone(), xz);
                }
                Err(e) => {
                    self.error_msg = Some(format!("pred load failed {}: {}", path.display(), e));
                    return None;
                }
            }
        }
        self.pred_cache.get(&key)
    }

    fn gt_traj(&mut self, seq: &str) -> Option<&Vec<[f64; 2]>> {
        if !self.gt_cache.contains_key(seq) {
            let path = self
                .repo
                .join("data/dataset/poses")
                .join(format!("{seq}.txt"));
            match load_kitti_xz(&path) {
                Ok(xz) => {
                    self.gt_cache.insert(seq.to_string(), xz);
                }
                Err(e) => {
                    // GT might not exist for held-out sequences; not fatal.
                    self.error_msg = Some(format!("gt load failed {}: {}", path.display(), e));
                    return None;
                }
            }
        }
        self.gt_cache.get(seq)
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        // ---- left: experiment list ----
        egui::SidePanel::left("experiments")
            .resizable(true)
            .default_width(360.0)
            .show(ctx, |ui| {
                ui.heading("experiments");
                ui.label(format!("{} found", self.experiments.len()));
                ui.separator();
                ScrollArea::vertical().show(ui, |ui| {
                    let mut new_sel = None;
                    for (i, e) in self.experiments.iter().enumerate() {
                        let selected = self.selected_idx == Some(i);
                        let label = format_exp_label(e);
                        if ui
                            .selectable_label(selected, RichText::new(label).monospace())
                            .on_hover_text(&e.description)
                            .clicked()
                        {
                            new_sel = Some(i);
                        }
                    }
                    if let Some(i) = new_sel {
                        self.select(i);
                    }
                });
            });

        // ---- right: controls + plot ----
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.horizontal(|ui| {
                // pred set selector
                let mut changed = false;
                changed |= ui
                    .selectable_value(&mut self.pred_set, "preds_dev".into(), "dev")
                    .changed();
                changed |= ui
                    .selectable_value(&mut self.pred_set, "preds_full".into(), "full")
                    .changed();
                changed |= ui
                    .selectable_value(
                        &mut self.pred_set,
                        "preds_dev_recheck".into(),
                        "dev_recheck",
                    )
                    .changed();
                if changed {
                    self.refresh_available_seqs();
                    if !self.available_seqs.contains(&self.selected_seq) {
                        if let Some(s) = self.available_seqs.first() {
                            self.selected_seq = s.clone();
                        }
                    }
                }
                ui.separator();
                ui.label("seq:");
                let seqs = self.available_seqs.clone();
                for s in &seqs {
                    ui.selectable_value(&mut self.selected_seq, s.clone(), s);
                }
                ui.separator();
                ui.checkbox(&mut self.show_gt, "GT");
                ui.checkbox(&mut self.show_pred, "pred");
            });

            if let Some(idx) = self.selected_idx {
                let exp = &self.experiments[idx];
                ui.horizontal(|ui| {
                    ui.label(RichText::new(&exp.name).strong());
                    ui.label(format!("status={}", exp.status));
                    if let Some(d) = exp.dev_trans_pct {
                        ui.label(format!("dev={:.4}%", d));
                    }
                    if let Some(f) = exp.full_trans_pct {
                        ui.label(format!("full={:.4}%", f));
                    }
                });
                ui.label(RichText::new(&exp.description).italics());
            }

            ui.separator();

            // load needed trajectories
            let seq = self.selected_seq.clone();
            let exp_idx = self.selected_idx;
            let pred = if let Some(i) = exp_idx {
                self.pred_traj(i, &seq).cloned()
            } else {
                None
            };
            let gt = self.gt_traj(&seq).cloned();

            let plot = Plot::new("traj")
                .data_aspect(1.0)
                .show_grid(true)
                .legend(egui_plot::Legend::default())
                .x_axis_label("x (m, KITTI camera frame)")
                .y_axis_label("z (m, KITTI camera frame, forward)");

            plot.show(ui, |plot_ui| {
                if self.show_gt {
                    if let Some(g) = &gt {
                        plot_ui.line(
                            Line::new(PlotPoints::from(g.clone()))
                                .name("ground truth")
                                .stroke(Stroke::new(2.0, Color32::from_rgb(60, 200, 60))),
                        );
                    }
                }
                if self.show_pred {
                    if let Some(p) = &pred {
                        plot_ui.line(
                            Line::new(PlotPoints::from(p.clone()))
                                .name("predicted")
                                .stroke(Stroke::new(2.0, Color32::from_rgb(230, 90, 90))),
                        );
                    }
                }
            });

            if let Some(msg) = &self.error_msg {
                ui.colored_label(Color32::YELLOW, msg);
            }
        });
    }
}

fn format_exp_label(e: &Experiment) -> String {
    let dev = e
        .dev_trans_pct
        .map(|v| format!("{:>7.4}", v))
        .unwrap_or_else(|| "      -".into());
    let full = e
        .full_trans_pct
        .map(|v| format!("{:>7.4}", v))
        .unwrap_or_else(|| "      -".into());
    let tag = match e.status.as_str() {
        "keep" => "K",
        "reject" => "R",
        "" => " ",
        _ => "?",
    };
    format!("{tag} {}  dev={dev} full={full}", e.name)
}

fn scan_experiments(repo: &Path) -> Vec<Experiment> {
    let mut out = Vec::new();

    // 1. enumerate experiment directories
    let exp_dir = repo.join("experiments");
    let dirs: Vec<PathBuf> = match fs::read_dir(&exp_dir) {
        Ok(rd) => rd
            .filter_map(|e| e.ok())
            .map(|e| e.path())
            .filter(|p| p.is_dir())
            .filter(|p| {
                p.file_name()
                    .and_then(|n| n.to_str())
                    .map(|n| n.starts_with("exp"))
                    .unwrap_or(false)
            })
            .collect(),
        Err(_) => Vec::new(),
    };

    // 2. parse results.tsv into name -> row map
    let results = parse_results(&repo.join("results.tsv"));

    for d in dirs {
        let name = d.file_name().unwrap().to_string_lossy().to_string();
        let row = results.get(&name);
        out.push(Experiment {
            dir: d,
            name: name.clone(),
            dev_trans_pct: row.and_then(|r| r.dev_trans_pct),
            full_trans_pct: row.and_then(|r| r.full_trans_pct),
            status: row.map(|r| r.status.clone()).unwrap_or_default(),
            description: row.map(|r| r.description.clone()).unwrap_or_default(),
        });
    }
    out
}

#[derive(Default)]
struct ResultRow {
    status: String,
    dev_trans_pct: Option<f64>,
    full_trans_pct: Option<f64>,
    description: String,
}

fn parse_results(path: &Path) -> HashMap<String, ResultRow> {
    let mut out = HashMap::new();
    let Ok(text) = fs::read_to_string(path) else { return out };
    let mut lines = text.lines();
    let Some(hdr) = lines.next() else { return out };
    let cols: Vec<&str> = hdr.split('\t').collect();
    let idx = |name: &str| cols.iter().position(|c| *c == name);
    let i_exp = idx("exp");
    let i_status = idx("status");
    let i_dev = idx("dev_trans_pct");
    let i_full = idx("full_trans_pct");
    let i_desc = idx("description");
    let (Some(i_exp), Some(i_status), Some(i_dev), Some(i_full), Some(i_desc)) =
        (i_exp, i_status, i_dev, i_full, i_desc)
    else {
        return out;
    };
    for line in lines {
        let f: Vec<&str> = line.split('\t').collect();
        if f.len() <= i_desc.max(i_status).max(i_dev).max(i_full).max(i_exp) {
            continue;
        }
        let name = f[i_exp].to_string();
        let row = ResultRow {
            status: f[i_status].to_string(),
            dev_trans_pct: f[i_dev].parse().ok(),
            full_trans_pct: f[i_full].parse().ok(),
            description: f[i_desc].to_string(),
        };
        out.insert(name, row);
    }
    out
}

// Load a KITTI pose file (N lines, 12 floats per line = row-major 3x4).
// Return projection onto the (x, z) plane — the natural top-down view in KITTI
// camera frame (x right, y down, z forward).
fn load_kitti_xz(path: &Path) -> Result<Vec<[f64; 2]>, String> {
    let text = fs::read_to_string(path).map_err(|e| e.to_string())?;
    let mut out = Vec::new();
    for (lineno, line) in text.lines().enumerate() {
        let toks: Vec<&str> = line.split_whitespace().collect();
        if toks.is_empty() {
            continue;
        }
        if toks.len() != 12 {
            return Err(format!("line {}: got {} tokens, want 12", lineno + 1, toks.len()));
        }
        let tx: f64 = toks[3].parse().map_err(|e: std::num::ParseFloatError| {
            format!("line {}: tx parse: {}", lineno + 1, e)
        })?;
        let tz: f64 = toks[11].parse().map_err(|e: std::num::ParseFloatError| {
            format!("line {}: tz parse: {}", lineno + 1, e)
        })?;
        out.push([tx, tz]);
    }
    Ok(out)
}
