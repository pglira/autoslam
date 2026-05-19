// traj-viewer: visually inspect SLAM experiment trajectories vs. KITTI ground truth.
//
// Left  panel: list of experiments (from experiments/expNNNN_*).
// Right panel: top-down trajectory plot — ground truth + predicted, per sequence.
//
// Auto-refreshes while open: re-scans the experiments dir, re-parses results.tsv,
// and reloads cached trajectories whose files have been modified on disk.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant, SystemTime};

use eframe::egui;
use egui::{Color32, RichText, ScrollArea, Stroke};
use egui_plot::{Line, MarkerShape, Plot, PlotPoints, Points};

const REPO_DEFAULT: &str = "/data2/repos/autoslam";
const DEFAULT_REFRESH_SECS: u64 = 2;

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
    // per-sequence translation error % parsed from results.tsv
    dev_seq: HashMap<String, f64>,
    full_seq: HashMap<String, f64>,
}

impl Experiment {
    fn seq_error(&self, seq: &str, pred_set: &str) -> Option<f64> {
        match pred_set {
            "preds_dev" => self.dev_seq.get(seq).copied(),
            "preds_full" => self.full_seq.get(seq).copied(),
            _ => None, // preds_dev_recheck has no aggregated per-seq column
        }
    }
}

// "exp0027_tight-1m" -> 27
fn exp_id_num(name: &str) -> Option<u32> {
    let rest = name.strip_prefix("exp")?;
    let digits: String = rest.chars().take_while(|c| c.is_ascii_digit()).collect();
    digits.parse().ok()
}

// Cached trajectory + the source file mtime at the time of load. The mtime lets
// the refresh tick detect when the file has changed underneath us.
type CachedTraj = (Vec<[f64; 2]>, SystemTime);

struct App {
    repo: PathBuf,
    experiments: Vec<Experiment>,
    selected_idx: Option<usize>,
    // pred_set: "preds_dev" | "preds_full" | "preds_dev_recheck"
    pred_set: String,
    selected_seq: String,
    available_seqs: Vec<String>,
    show_gt: bool,
    show_pred: bool,
    // keyed by exp_name (not idx) so caches survive list re-sorts when new
    // experiments appear in the directory.
    pred_cache: HashMap<(String, String, String), CachedTraj>,
    gt_cache: HashMap<String, CachedTraj>,
    error_msg: Option<String>,
    // auto-refresh
    auto_refresh: bool,
    refresh_interval: Duration,
    last_refresh: Instant,
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
            auto_refresh: true,
            refresh_interval: Duration::from_secs(DEFAULT_REFRESH_SECS),
            last_refresh: Instant::now(),
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
        let exp_name = self.experiments[exp_idx].name.clone();
        let key = (exp_name, self.pred_set.clone(), seq.to_string());
        if !self.pred_cache.contains_key(&key) {
            let path = self.experiments[exp_idx]
                .dir
                .join(&self.pred_set)
                .join(format!("{seq}.txt"));
            match load_kitti_xz(&path) {
                Ok(xz) => {
                    let mtime = file_mtime(&path);
                    self.pred_cache.insert(key.clone(), (xz, mtime));
                }
                Err(e) => {
                    self.error_msg = Some(format!("pred load failed {}: {}", path.display(), e));
                    return None;
                }
            }
        }
        self.pred_cache.get(&key).map(|(v, _)| v)
    }

    fn gt_traj(&mut self, seq: &str) -> Option<&Vec<[f64; 2]>> {
        if !self.gt_cache.contains_key(seq) {
            let path = self
                .repo
                .join("data/dataset/poses")
                .join(format!("{seq}.txt"));
            match load_kitti_xz(&path) {
                Ok(xz) => {
                    let mtime = file_mtime(&path);
                    self.gt_cache.insert(seq.to_string(), (xz, mtime));
                }
                Err(e) => {
                    // GT might not exist for held-out sequences; not fatal.
                    self.error_msg = Some(format!("gt load failed {}: {}", path.display(), e));
                    return None;
                }
            }
        }
        self.gt_cache.get(seq).map(|(v, _)| v)
    }

    // Re-scan experiments dir, re-parse results.tsv, and refresh any cached
    // trajectories whose source file mtimes have advanced. Tolerates transient
    // read failures (e.g. a file being written): keeps the old entry on error.
    fn refresh_data(&mut self) {
        let prev_name = self
            .selected_idx
            .and_then(|i| self.experiments.get(i).map(|e| e.name.clone()));

        let mut new_exps = scan_experiments(&self.repo);
        new_exps.sort_by(|a, b| b.name.cmp(&a.name));
        self.experiments = new_exps;
        self.selected_idx = match prev_name {
            Some(n) => self
                .experiments
                .iter()
                .position(|e| e.name == n)
                .or_else(|| (!self.experiments.is_empty()).then_some(0)),
            None => (!self.experiments.is_empty()).then_some(0),
        };

        // available seqs may have grown (a new sequence file just appeared)
        self.refresh_available_seqs();

        // reload pred trajectories whose mtime advanced
        let pred_keys: Vec<(String, String, String)> = self.pred_cache.keys().cloned().collect();
        for key in pred_keys {
            let (exp_name, pred_set, seq) = &key;
            let path = self
                .repo
                .join("experiments")
                .join(exp_name)
                .join(pred_set)
                .join(format!("{seq}.txt"));
            let on_disk = file_mtime(&path);
            let cached_mtime = self.pred_cache.get(&key).map(|(_, m)| *m);
            if cached_mtime.map(|m| on_disk > m).unwrap_or(false) {
                if let Ok(xz) = load_kitti_xz(&path) {
                    self.pred_cache.insert(key, (xz, on_disk));
                }
                // on read/parse failure (likely a partial write) keep old entry; retry next tick
            }
        }

        // reload gt trajectories whose mtime advanced
        let gt_keys: Vec<String> = self.gt_cache.keys().cloned().collect();
        for seq in gt_keys {
            let path = self
                .repo
                .join("data/dataset/poses")
                .join(format!("{seq}.txt"));
            let on_disk = file_mtime(&path);
            let cached_mtime = self.gt_cache.get(&seq).map(|(_, m)| *m);
            if cached_mtime.map(|m| on_disk > m).unwrap_or(false) {
                if let Ok(xz) = load_kitti_xz(&path) {
                    self.gt_cache.insert(seq, (xz, on_disk));
                }
            }
        }

        self.last_refresh = Instant::now();
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        // periodic refresh — re-scan experiments + invalidate stale caches
        if self.auto_refresh && self.last_refresh.elapsed() >= self.refresh_interval {
            self.refresh_data();
        }

        // handle keyboard: up/down to navigate experiments
        if ctx.input(|i| i.key_pressed(egui::Key::ArrowUp)) {
            if let Some(idx) = self.selected_idx {
                if idx > 0 {
                    self.select(idx - 1);
                }
            }
        }
        if ctx.input(|i| i.key_pressed(egui::Key::ArrowDown)) {
            if let Some(idx) = self.selected_idx {
                if idx + 1 < self.experiments.len() {
                    self.select(idx + 1);
                }
            }
        }

        // ---- left: experiment list ----
        egui::SidePanel::left("experiments")
            .resizable(true)
            .default_width(280.0)
            .show(ctx, |ui| {
                ui.heading("experiments");
                ui.label(format!("{} found", self.experiments.len()));
                ui.separator();
                ScrollArea::vertical().auto_shrink([false; 2]).show(ui, |ui| {
                    let mut new_sel = None;
                    for (i, e) in self.experiments.iter().enumerate() {
                        let selected = self.selected_idx == Some(i);
                        let label = format_exp_label_compact(e);
                        if ui
                            .selectable_label(selected, RichText::new(label).monospace().size(11.0))
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

        // ---- bottom: per-sequence translation error across experiments ----
        egui::TopBottomPanel::bottom("err_chart")
            .resizable(true)
            .default_height(260.0)
            .min_height(120.0)
            .show(ctx, |ui| {
                let pred_set_label = match self.pred_set.as_str() {
                    "preds_dev" => "dev",
                    "preds_full" => "full",
                    "preds_dev_recheck" => "dev_recheck",
                    other => other,
                };
                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new(format!(
                            "translation error % across experiments — seq {}, {}",
                            self.selected_seq, pred_set_label,
                        ))
                        .strong(),
                    );
                });

                let selected_name = self
                    .selected_idx
                    .and_then(|i| self.experiments.get(i).map(|e| e.name.clone()));

                let mut pts: Vec<[f64; 2]> = Vec::new();
                let mut sel_pt: Option<[f64; 2]> = None;
                for e in &self.experiments {
                    let Some(id) = exp_id_num(&e.name) else { continue };
                    let Some(err) = e.seq_error(&self.selected_seq, &self.pred_set) else {
                        continue;
                    };
                    let p = [id as f64, err];
                    pts.push(p);
                    if selected_name.as_deref() == Some(e.name.as_str()) {
                        sel_pt = Some(p);
                    }
                }
                pts.sort_by(|a, b| a[0].partial_cmp(&b[0]).unwrap());

                if pts.is_empty() {
                    ui.colored_label(
                        Color32::GRAY,
                        format!(
                            "no per-seq data in results.tsv for seq {} / {}",
                            self.selected_seq, pred_set_label,
                        ),
                    );
                    return;
                }

                Plot::new("err_chart_plot")
                    .show_grid(true)
                    .legend(egui_plot::Legend::default())
                    .x_axis_label("experiment id")
                    .y_axis_label("translation error %")
                    .show(ui, |plot_ui| {
                        plot_ui.line(
                            Line::new(PlotPoints::from(pts.clone()))
                                .name("trans %")
                                .stroke(Stroke::new(1.5, Color32::from_rgb(120, 170, 220))),
                        );
                        plot_ui.points(
                            Points::new(PlotPoints::from(pts.clone()))
                                .name("exp")
                                .radius(3.5)
                                .shape(MarkerShape::Circle)
                                .color(Color32::from_rgb(120, 170, 220)),
                        );
                        if let Some(p) = sel_pt {
                            plot_ui.points(
                                Points::new(PlotPoints::from(vec![p]))
                                    .name("selected")
                                    .radius(6.5)
                                    .shape(MarkerShape::Diamond)
                                    .color(Color32::from_rgb(230, 90, 90)),
                            );
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
                ui.separator();
                ui.checkbox(&mut self.auto_refresh, "auto");
                if ui.button("refresh").clicked() {
                    self.refresh_data();
                }
                let secs = self.last_refresh.elapsed().as_secs();
                ui.label(
                    RichText::new(format!("({}s ago)", secs))
                        .weak()
                        .monospace()
                        .size(11.0),
                );
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

        // schedule the next wake — egui is reactive by default, so without this
        // the auto-refresh tick would only fire on user input. Wake a bit more
        // often than the refresh interval so the "Ns ago" label stays current.
        ctx.request_repaint_after(Duration::from_millis(500));
    }
}

fn format_exp_label_compact(e: &Experiment) -> String {
    let tag = match e.status.as_str() {
        "keep" => "✓",
        "reject" => "✗",
        "" => " ",
        _ => "?",
    };
    let dev = e
        .dev_trans_pct
        .map(|v| format!("{:.2}%", v))
        .unwrap_or_else(|| "-".into());
    let full = e
        .full_trans_pct
        .map(|v| format!("{:.2}%", v))
        .unwrap_or_else(|| "-".into());
    format!("{} {} d={} f={}", tag, e.name, dev, full)
}

fn file_mtime(path: &Path) -> SystemTime {
    fs::metadata(path)
        .and_then(|m| m.modified())
        .unwrap_or(SystemTime::UNIX_EPOCH)
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
            dev_seq: row.map(|r| r.dev_seq.clone()).unwrap_or_default(),
            full_seq: row.map(|r| r.full_seq.clone()).unwrap_or_default(),
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
    dev_seq: HashMap<String, f64>,
    full_seq: HashMap<String, f64>,
}

// Full sequences in canonical order — `full_per_seq` is a slash-separated list
// of 11 values in this order.
const FULL_SEQ_ORDER: [&str; 11] = [
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10",
];

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
    let i_full_per_seq = idx("full_per_seq");
    // any `dev_seq_NN` column → (col_idx, seq_name)
    let dev_seq_cols: Vec<(usize, String)> = cols
        .iter()
        .enumerate()
        .filter_map(|(i, c)| c.strip_prefix("dev_seq_").map(|s| (i, s.to_string())))
        .collect();

    let (Some(i_exp), Some(i_status), Some(i_dev), Some(i_full), Some(i_desc)) =
        (i_exp, i_status, i_dev, i_full, i_desc)
    else {
        return out;
    };

    for line in lines {
        let f: Vec<&str> = line.split('\t').collect();
        if f.len() <= [i_exp, i_status, i_dev, i_full, i_desc].into_iter().max().unwrap_or(0) {
            continue;
        }
        let name = f[i_exp].to_string();

        let mut dev_seq = HashMap::new();
        for (col, seq) in &dev_seq_cols {
            if *col < f.len() {
                if let Ok(v) = f[*col].parse::<f64>() {
                    dev_seq.insert(seq.clone(), v);
                }
            }
        }

        let mut full_seq = HashMap::new();
        if let Some(col) = i_full_per_seq {
            if col < f.len() {
                for (i, tok) in f[col].split('/').enumerate() {
                    if let Some(seq) = FULL_SEQ_ORDER.get(i) {
                        if let Ok(v) = tok.parse::<f64>() {
                            full_seq.insert((*seq).to_string(), v);
                        }
                    }
                }
            }
        }

        let row = ResultRow {
            status: f[i_status].to_string(),
            dev_trans_pct: f[i_dev].parse().ok(),
            full_trans_pct: f[i_full].parse().ok(),
            description: f.get(i_desc).copied().unwrap_or("").to_string(),
            dev_seq,
            full_seq,
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
