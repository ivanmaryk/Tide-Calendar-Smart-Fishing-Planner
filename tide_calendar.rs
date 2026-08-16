# tide_calendar.rs
/**
 * 🌊 Tide Calendar – Smart Fishing Planner (Rust Edition)
 * Advanced: harmonic tide prediction, ASCII charts, lunar phase, fishing forecast
 * Dependencies: chrono, colored, serde, serde_json
 */

use chrono::{DateTime, Duration, TimeZone, Utc};
use colored::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::io::{self, Write, BufRead};
use std::path::PathBuf;

// ─── Types ──────────────────────────────────────────────────────────────────

#[derive(Debug, Serialize, Deserialize, Clone)]
struct HarmonicMap {
    M2: f64,
    S2: f64,
    N2: f64,
    K2: f64,
    M4: f64,
}

impl Default for HarmonicMap {
    fn default() -> Self {
        HarmonicMap { M2: 1.2, S2: 0.4, N2: 0.2, K2: 0.1, M4: 0.1 }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
struct TideStation {
    name: String,
    latitude: f64,
    longitude: f64,
    harmonics: HarmonicMap,
    timezone_offset: f64,
}

#[derive(Debug, Serialize, Deserialize)]
struct ProfileData {
    profiles: Vec<TideStation>,
    current_index: usize,
}

#[derive(Debug)]
struct ExtremaResult {
    time: DateTime<Utc>,
    height: f64,
    typ: String,
}

// ─── Constants ──────────────────────────────────────────────────────────────

const LUNAR_CYCLE_DAYS: f64 = 29.530588853;
const CACHE_DIR: &str = ".tide_calendar";
const PROFILES_FILE: &str = "profiles.json";

// ─── Tide Model ─────────────────────────────────────────────────────────────

struct TidePredictor {
    station: TideStation,
    epoch: DateTime<Utc>,
    constituents: HashMap<String, (f64, f64)>, // (speed, phase)
}

impl TidePredictor {
    fn new(station: TideStation) -> Self {
        let epoch = Utc.with_ymd_and_hms(2000, 1, 1, 0, 0, 0).unwrap();
        let mut constituents = HashMap::new();
        constituents.insert("M2".to_string(), (14.49205211, 0.0));
        constituents.insert("S2".to_string(), (15.0, 0.0));
        constituents.insert("N2".to_string(), (14.49669388, 0.0));
        constituents.insert("K2".to_string(), (15.04106864, 0.0));
        constituents.insert("M4".to_string(), (28.98410422, 0.0));
        TidePredictor { station, epoch, constituents }
    }

    fn days_since_epoch(&self, dt: DateTime<Utc>) -> f64 {
        (dt - self.epoch).num_seconds() as f64 / 86400.0
    }

    fn moon_phase(&self, dt: DateTime<Utc>) -> f64 {
        (self.days_since_epoch(dt) / LUNAR_CYCLE_DAYS) % 1.0
    }

    fn tide_height(&self, dt: DateTime<Utc>) -> f64 {
        let days = self.days_since_epoch(dt);
        let mut height = 0.0;
        let amps = [
            ("M2", self.station.harmonics.M2),
            ("S2", self.station.harmonics.S2),
            ("N2", self.station.harmonics.N2),
            ("K2", self.station.harmonics.K2),
            ("M4", self.station.harmonics.M4),
        ];
        for (name, amp) in amps.iter() {
            if let Some((speed, phase)) = self.constituents.get(*name) {
                let arg = (speed * days + phase) * std::f64::consts::PI / 180.0;
                height += amp * arg.cos();
            }
        }
        height + 1.0
    }

    fn predict_tide(&self, dt: DateTime<Utc>) -> (f64, String) {
        let h = self.tide_height(dt);
        let h2 = self.tide_height(dt + Duration::minutes(5));
        let trend = if h2 > h { "rising".to_string() } else { "falling".to_string() };
        (h, trend)
    }

    fn get_extrema(&self, dt: DateTime<Utc>, hours: f64) -> Vec<ExtremaResult> {
        let mut results = Vec::new();
        let step = Duration::minutes(5);
        let start = dt - Duration::hours((hours / 2.0) as i64);
        let end = dt + Duration::hours((hours / 2.0) as i64);
        let mut current = start;
        let mut prev_h = self.tide_height(current);
        current = current + step;
        while current <= end {
            let h = self.tide_height(current);
            let next_h = self.tide_height(current + step);
            if (h - prev_h) * (next_h - h) < 0.0 {
                if let Some(ext) = self.refine_extremum(current - step, current + step) {
                    let typ = if ext.height > 1.0 { "High".to_string() } else { "Low".to_string() };
                    results.push(ExtremaResult { time: ext.time, height: ext.height, typ });
                }
            }
            prev_h = h;
            current = current + step;
        }
        results
    }

    fn refine_extremum(&self, start: DateTime<Utc>, end: DateTime<Utc>) -> Option<ExtremaResult> {
        let mut s = start;
        let mut e = end;
        for _ in 0..10 {
            let mid = s + (e - s) / 2;
            let h_mid = self.tide_height(mid);
            let h_left = self.tide_height(mid - Duration::minutes(1));
            let h_right = self.tide_height(mid + Duration::minutes(1));
            if h_mid > h_left && h_mid > h_right {
                return Some(ExtremaResult { time: mid, height: h_mid, typ: "".to_string() });
            }
            if h_mid < h_left && h_mid < h_right {
                return Some(ExtremaResult { time: mid, height: h_mid, typ: "".to_string() });
            }
            if h_left > h_right {
                e = mid;
            } else {
                s = mid;
            }
        }
        None
    }
}

// ─── Fishing Forecast ──────────────────────────────────────────────────────

fn fishing_forecast(height: f64, moon_phase: f64, dt: DateTime<Utc>) -> String {
    let hour = dt.hour();
    let range_factor = (height - 1.0).abs();
    let moon_factor = 1.0 - (moon_phase - 0.5).abs() * 2.0;
    let time_factor = if (5..=7).contains(&hour) || (18..=20).contains(&hour) {
        1.0
    } else if (8..=10).contains(&hour) || (15..=17).contains(&hour) {
        0.7
    } else {
        0.4
    };
    let score = (range_factor * 2.0 + moon_factor * 1.5 + time_factor * 2.0) / 5.5;
    if score > 0.8 { "Excellent".to_string() }
    else if score > 0.6 { "Good".to_string() }
    else if score > 0.4 { "Fair".to_string() }
    else { "Poor".to_string() }
}

// ─── ASCII Tide Chart ──────────────────────────────────────────────────────

fn draw_tide_chart(predictor: &TidePredictor, dt: DateTime<Utc>, hours: i64, width: usize, height: usize) -> String {
    let start = dt;
    let end = dt + Duration::hours(hours);
    let step = Duration::minutes(5);
    let mut times = Vec::new();
    let mut heights = Vec::new();
    let mut cur = start;
    while cur <= end {
        let (h, _) = predictor.predict_tide(cur);
        times.push(cur);
        heights.push(h);
        cur = cur + step;
    }
    if heights.is_empty() {
        return "No data".to_string();
    }
    let min_h = heights.iter().cloned().fold(f64::INFINITY, f64::min);
    let max_h = heights.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
    let range = max_h - min_h;
    if range == 0.0 {
        return format!("Tide is flat at {:.2}m", min_h);
    }
    let norm: Vec<usize> = heights.iter().map(|h| (((h - min_h) / range * (height - 1) as f64) as usize)).collect();
    let mut lines = Vec::new();
    for row in (0..height).rev() {
        let mut line = String::new();
        for (i, &val) in norm.iter().enumerate() {
            if val >= row {
                if i > 0 && norm[i-1] >= row {
                    line.push('─');
                } else {
                    line.push('┌');
                }
            } else {
                line.push(' ');
            }
        }
        lines.push(line);
    }
    // X axis
    let mut x_axis = " ".to_string();
    let mut last_pos = 0;
    let step_idx = times.len() / (hours as usize / 4);
    let step_idx = if step_idx < 1 { 1 } else { step_idx };
    for i in (0..times.len()).step_by(step_idx) {
        if i >= times.len() { break; }
        let label = times[i].format("%H:%M").to_string();
        let pos = i;
        if pos > last_pos {
            x_axis.push_str(&" ".repeat(pos - last_pos));
        }
        x_axis.push_str(&label);
        last_pos = pos;
    }
    if last_pos < times.len() - 1 {
        x_axis.push_str(&" ".repeat(times.len() - 1 - last_pos));
    }
    let mut chart = lines.join("\n") + "\n" + &x_axis;
    chart.push_str(&format!("\nMin: {:.2}m  Max: {:.2}m", min_h, max_h));
    chart
}

// ─── Profile Manager ──────────────────────────────────────────────────────

struct ProfileManager {
    profiles: Vec<TideStation>,
    current_index: usize,
    file_path: PathBuf,
}

impl ProfileManager {
    fn new() -> Self {
        let home = std::env::var("HOME").or_else(|_| std::env::var("USERPROFILE")).unwrap_or_else(|_| ".".to_string());
        let dir = PathBuf::from(home).join(CACHE_DIR);
        fs::create_dir_all(&dir).unwrap();
        let file_path = dir.join(PROFILES_FILE);
        let mut pm = ProfileManager { profiles: Vec::new(), current_index: 0, file_path };
        pm.load();
        pm
    }

    fn load(&mut self) {
        if let Ok(raw) = fs::read_to_string(&self.file_path) {
            if let Ok(data) = serde_json::from_str::<ProfileData>(&raw) {
                self.profiles = data.profiles;
                self.current_index = data.current_index;
                if self.current_index >= self.profiles.len() {
                    self.current_index = 0;
                }
                return;
            }
        }
        self.init_default();
    }

    fn init_default(&mut self) {
        self.profiles = vec![TideStation {
            name: "Default Coast".to_string(),
            latitude: 0.0,
            longitude: 0.0,
            harmonics: HarmonicMap::default(),
            timezone_offset: 0.0,
        }];
        self.current_index = 0;
        self.save();
    }

    fn save(&self) {
        let data = ProfileData {
            profiles: self.profiles.clone(),
            current_index: self.current_index,
        };
        let raw = serde_json::to_string_pretty(&data).unwrap();
        let _ = fs::write(&self.file_path, raw);
    }

    fn add_profile(&mut self, name: String, lat: f64, lon: f64, harmonics: Option<HarmonicMap>) {
        let h = harmonics.unwrap_or_default();
        self.profiles.push(TideStation {
            name,
            latitude: lat,
            longitude: lon,
            harmonics: h,
            timezone_offset: 0.0,
        });
        self.current_index = self.profiles.len() - 1;
        self.save();
    }

    fn get_current(&self) -> TideStation {
        self.profiles[self.current_index].clone()
    }

    fn list_profiles(&self) -> Vec<String> {
        self.profiles.iter().map(|p| p.name.clone()).collect()
    }

    fn set_current_by_name(&mut self, name: &str) -> bool {
        for (i, p) in self.profiles.iter().enumerate() {
            if p.name == name {
                self.current_index = i;
                self.save();
                return true;
            }
        }
        false
    }
}

// ─── Main App ──────────────────────────────────────────────────────────────

struct TideApp {
    reader: io::Stdin,
    profile_mgr: ProfileManager,
    station: TideStation,
    predictor: TidePredictor,
}

impl TideApp {
    fn new() -> Self {
        let mut app = TideApp {
            reader: io::stdin(),
            profile_mgr: ProfileManager::new(),
            station: TideStation::default(),
            predictor: TidePredictor::new(TideStation::default()),
        };
        app.station = app.profile_mgr.get_current();
        app.predictor = TidePredictor::new(app.station.clone());
        app
    }

    fn ask(&self, prompt: &str) -> String {
        print!("{}", prompt);
        io::stdout().flush().unwrap();
        let mut line = String::new();
        self.reader.lock().read_line(&mut line).unwrap();
        line.trim().to_string()
    }

    fn ask_float(&self, prompt: &str) -> f64 {
        loop {
            let ans = self.ask(prompt);
            if let Ok(val) = ans.parse::<f64>() {
                return val;
            }
            println!("{}", "Please enter a valid number.".yellow());
        }
    }

    fn ask_confirm(&self, prompt: &str) -> bool {
        let ans = self.ask(&format!("{} (yes/no): ", prompt));
        let a = ans.to_lowercase();
        a == "yes" || a == "y"
    }

    fn show_menu(&self) {
        println!("\n{}", "═".repeat(50).cyan());
        println!("{}", "🌊 TIDE CALENDAR".bright().cyan());
        println!("{}", "═".repeat(50).cyan());
        println!("  Location: {}", self.station.name);
        println!("  Lat: {:.2}  Lon: {:.2}", self.station.latitude, self.station.longitude);
        println!("{}", "═".repeat(50).cyan());
        println!("  1. 🌊 Tide Chart (next 24h)");
        println!("  2. 📅 Weekly Tide Forecast");
        println!("  3. 🎣 Fishing Forecast");
        println!("  4. 🌙 Lunar Phase Info");
        println!("  5. 🗺️  Change Location");
        println!("  6. 🛠️  Add Location");
        println!("  0. 🚪 Exit");
        println!("{}", "═".repeat(50).cyan());
    }

    fn show_tide_chart(&self) {
        let now = Utc::now();
        let chart = draw_tide_chart(&self.predictor, now, 24, 60, 10);
        println!("\n{}", "📈 Tide Chart (next 24h)".bright());
        println!("{}", chart);
    }

    fn show_weekly_forecast(&self) {
        let now = Utc::now();
        println!("\n{}", "📅 Weekly Tide Forecast".bright());
        println!("{}", "─".repeat(60).dimmed());
        for day in 0..7 {
            let dt = now + Duration::days(day);
            let extrema = self.predictor.get_extrema(dt, 24.0);
            let mut highs = Vec::new();
            let mut lows = Vec::new();
            for e in extrema {
                if e.typ == "High" {
                    highs.push(format!("{} {:.2}m", e.time.format("%H:%M"), e.height));
                } else {
                    lows.push(format!("{} {:.2}m", e.time.format("%H:%M"), e.height));
                }
            }
            let moon = self.predictor.moon_phase(dt);
            let moon_emojis = ["🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"];
            let moon_str = moon_emojis[((moon * 8.0) as usize) % 8];
            println!("  {} {}", dt.format("%a %b %d"), moon_str);
            let high_str = if highs.is_empty() { "—".to_string() } else { highs.join(", ") };
            let low_str = if lows.is_empty() { "—".to_string() } else { lows.join(", ") };
            println!("    Highs: {}", high_str);
            println!("    Lows:  {}", low_str);
        }
        println!("{}", "─".repeat(60).dimmed());
    }

    fn show_fishing_forecast(&self) {
        let now = Utc::now();
        println!("\n{}", "🎣 Fishing Forecast".bright());
        for hour in 0..7 {
            let dt = now + Duration::hours(hour);
            let (h, trend) = self.predictor.predict_tide(dt);
            let moon = self.predictor.moon_phase(dt);
            let rating = fishing_forecast(h, moon, dt);
            let symbols = [("Excellent", "⭐⭐⭐"), ("Good", "⭐⭐"), ("Fair", "⭐"), ("Poor", "—")];
            let color_map = [("Excellent", "green"), ("Good", "cyan"), ("Fair", "yellow"), ("Poor", "red")];
            let sym = symbols.iter().find(|(r, _)| *r == rating).map(|(_, s)| *s).unwrap_or("—");
            let col = color_map.iter().find(|(r, _)| *r == rating).map(|(_, c)| c).unwrap_or("white");
            let colored_rating = match *col {
                "green" => rating.green(),
                "cyan" => rating.cyan(),
                "yellow" => rating.yellow(),
                "red" => rating.red(),
                _ => rating.normal(),
            };
            println!("  {} – {:.2}m ({}) → {} {}", dt.format("%H:%M"), h, trend, colored_rating, sym);
        }
    }

    fn show_lunar_phase(&self) {
        let now = Utc::now();
        let moon = self.predictor.moon_phase(now);
        let phase_names = ["New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
                           "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent"];
        let idx = ((moon * 8.0) as usize) % 8;
        let phase = phase_names[idx];
        let emojis = ["🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"];
        let emoji = emojis[idx];
        let illumination = (moon * 2.0 * std::f64::consts::PI).cos().abs() * 100.0;
        println!("\n🌙 Lunar Phase: {} {}", emoji, phase);
        println!("   Illumination: {:.1}%", illumination);
    }

    fn change_location(&mut self) {
        let names = self.profile_mgr.list_profiles();
        if names.is_empty() {
            println!("{}", "No locations available.".yellow());
            return;
        }
        println!("Select location:");
        for (i, name) in names.iter().enumerate() {
            println!("  {}. {}", i+1, name);
        }
        let ans = self.ask("Number: ");
        if let Ok(idx) = ans.parse::<usize>() {
            if idx >= 1 && idx <= names.len() {
                let name = &names[idx-1];
                self.profile_mgr.set_current_by_name(name);
                self.station = self.profile_mgr.get_current();
                self.predictor = TidePredictor::new(self.station.clone());
                println!("{}", format!("✅ Switched to {}", self.station.name).green());
                return;
            }
        }
        println!("{}", "Invalid number.".red());
    }

    fn add_location(&mut self) {
        let name = self.ask("Location name: ");
        let lat = self.ask_float("Latitude (e.g., 50.0): ");
        let lon = self.ask_float("Longitude (e.g., -5.0): ");
        self.profile_mgr.add_profile(name.clone(), lat, lon, None);
        self.station = self.profile_mgr.get_current();
        self.predictor = TidePredictor::new(self.station.clone());
        println!("{}", format!("✅ Added {}", name).green());
    }

    fn run(&mut self) {
        println!("{}", "\n🌊 Tide Calendar – Smart Fishing Planner".bright().cyan());
        println!("{}", "Know the tides, catch the big ones!".dimmed());

        loop {
            self.show_menu();
            let choice = self.ask("Your choice: ");
            match choice.as_str() {
                "1" => self.show_tide_chart(),
                "2" => self.show_weekly_forecast(),
                "3" => self.show_fishing_forecast(),
                "4" => self.show_lunar_phase(),
                "5" => self.change_location(),
                "6" => self.add_location(),
                "0" => {
                    println!("{}", "👋 Tight lines!".cyan());
                    break;
                }
                _ => println!("{}", "❌ Invalid choice.".red()),
            }
            if choice != "0" {
                print!("\nPress Enter to continue...");
                io::stdout().flush().unwrap();
                let mut _dummy = String::new();
                io::stdin().read_line(&mut _dummy).unwrap();
            }
        }
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

fn main() {
    let mut app = TideApp::new();
    app.run();
}
