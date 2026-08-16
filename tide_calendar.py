# tide_calendar.py
#!/usr/bin/env python3
"""
🌊 Tide Calendar – Smart Fishing Planner (Python Edition)
Advanced: harmonic tide prediction, ASCII charts, lunar phase, fishing forecast
"""

import json
import math
import os
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass

try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.prompt import Prompt, Confirm
    from rich.progress import Progress, SpinnerColumn, TextColumn
    from rich import box
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False
    print("⚠️  Install 'rich' for enhanced UI: pip install rich")


# ─── Constants ──────────────────────────────────────────────────────────────

CACHE_DIR = Path.home() / ".tide_calendar"
PROFILES_FILE = CACHE_DIR / "profiles.json"
# Simplified harmonic constants for a generic location (amplitude in meters)
DEFAULT_HARMONICS = {"M2": 1.2, "S2": 0.4, "N2": 0.2, "K2": 0.1, "M4": 0.1}
LUNAR_CYCLE_DAYS = 29.530588853
TIDE_PERIOD_HOURS = 12.4206012  # M2 period

# ─── Colors ──────────────────────────────────────────────────────────────────

def c(text: str, color: str) -> str:
    colors = {
        "reset": "\033[0m", "bright": "\033[1m", "dim": "\033[2m",
        "red": "\033[31m", "green": "\033[32m", "yellow": "\033[33m",
        "blue": "\033[34m", "magenta": "\033[35m", "cyan": "\033[36m"
    }
    return f"{colors.get(color, '')}{text}{colors['reset']}"


# ─── Tide Model ─────────────────────────────────────────────────────────────

@dataclass
class TideStation:
    name: str
    latitude: float = 0.0
    longitude: float = 0.0
    harmonics: Dict[str, float] = None
    timezone_offset: float = 0.0

    def __post_init__(self):
        if self.harmonics is None:
            self.harmonics = DEFAULT_HARMONICS.copy()


class TidePredictor:
    """Simple harmonic tide prediction using major constituents."""

    def __init__(self, station: TideStation):
        self.station = station
        # Reference epoch: 2000-01-01 00:00 UTC (astronomical constants)
        self.epoch = datetime(2000, 1, 1)
        # Astronomical arguments (in radians per day) for major constituents
        self.constituents = {
            "M2": {"speed": 14.49205211, "phase": 0.0},   # degrees/day
            "S2": {"speed": 15.00000000, "phase": 0.0},
            "N2": {"speed": 14.49669388, "phase": 0.0},
            "K2": {"speed": 15.04106864, "phase": 0.0},
            "M4": {"speed": 28.98410422, "phase": 0.0},
        }

    def _days_since_epoch(self, dt: datetime) -> float:
        return (dt - self.epoch).total_seconds() / 86400.0

    def _moon_phase(self, dt: datetime) -> float:
        """Return moon phase (0=New, 0.5=Full) normalized to [0,1]."""
        # Simplified: use lunar cycle
        days = self._days_since_epoch(dt)
        phase = (days / LUNAR_CYCLE_DAYS) % 1.0
        return phase

    def _tide_height(self, dt: datetime) -> float:
        """Calculate tide height (meters) at given datetime."""
        days = self._days_since_epoch(dt)
        height = 0.0
        for name, amp in self.station.harmonics.items():
            if name in self.constituents:
                speed = self.constituents[name]["speed"]
                phase = self.constituents[name]["phase"]
                # Add nodal corrections and phase lag (simplified)
                arg = math.radians(speed * days + phase)
                height += amp * math.cos(arg)
        # Add mean sea level (1.0 m default)
        return height + 1.0

    def predict_tide(self, dt: datetime) -> Tuple[float, str]:
        """Return (height, trend) where trend is 'rising' or 'falling'."""
        h = self._tide_height(dt)
        # Estimate trend by forward difference
        dt2 = dt + timedelta(minutes=5)
        h2 = self._tide_height(dt2)
        trend = "rising" if h2 > h else "falling"
        return h, trend

    def get_extrema(self, dt: datetime, hours: float = 12.5) -> List[Tuple[datetime, float, str]]:
        """Find high and low tides within the given time window."""
        results = []
        step = timedelta(minutes=5)
        current = dt - timedelta(hours=hours/2)
        end = dt + timedelta(hours=hours/2)
        prev_h = self._tide_height(current)
        current += step
        while current <= end:
            h = self._tide_height(current)
            if (h - prev_h) * (self._tide_height(current + step) - h) < 0:
                # extremum detected – refine by scanning smaller steps
                ext = self._refine_extremum(current - step, current + step)
                if ext:
                    h_ext, t_ext = ext
                    typ = "High" if h_ext > 1.0 else "Low"
                    results.append((t_ext, h_ext, typ))
            prev_h = h
            current += step
        return results

    def _refine_extremum(self, start: datetime, end: datetime) -> Optional[Tuple[float, datetime]]:
        """Binary search for exact extremum."""
        for _ in range(10):
            mid = start + (end - start) / 2
            h_mid = self._tide_height(mid)
            h_left = self._tide_height(mid - timedelta(minutes=1))
            h_right = self._tide_height(mid + timedelta(minutes=1))
            if h_mid > h_left and h_mid > h_right:
                return (h_mid, mid)
            elif h_mid < h_left and h_mid < h_right:
                return (h_mid, mid)
            elif h_left > h_right:
                end = mid
            else:
                start = mid
        return None


# ─── Fishing Forecast ──────────────────────────────────────────────────────

def fishing_forecast(tide_height: float, moon_phase: float, time: datetime) -> str:
    """Return a rating: Excellent, Good, Fair, Poor."""
    hour = time.hour
    # Tidal range (amplitude) affects fish activity
    range_factor = abs(tide_height - 1.0)  # deviation from mean
    # Moon phase: new/full moons have stronger tides and better fishing
    moon_factor = 1.0 - abs(moon_phase - 0.5) * 2  # 1 at new/full, 0 at quarter
    # Time of day: dawn/dusk are best
    if 5 <= hour <= 7 or 18 <= hour <= 20:
        time_factor = 1.0
    elif 7 < hour < 11 or 15 < hour < 18:
        time_factor = 0.7
    else:
        time_factor = 0.4
    score = (range_factor * 2 + moon_factor * 1.5 + time_factor * 2) / 5.5
    if score > 0.8:
        return "Excellent"
    elif score > 0.6:
        return "Good"
    elif score > 0.4:
        return "Fair"
    else:
        return "Poor"


# ─── ASCII Tide Chart ──────────────────────────────────────────────────────

def draw_tide_chart(predictor: TidePredictor, dt: datetime, hours: int = 24, width: int = 60, height: int = 10) -> str:
    """Draw an ASCII chart of tide heights over the given period."""
    lines = []
    start = dt
    end = dt + timedelta(hours=hours)
    step = timedelta(minutes=5)
    times = []
    heights = []
    cur = start
    while cur <= end:
        h, _ = predictor.predict_tide(cur)
        times.append(cur)
        heights.append(h)
        cur += step

    if not heights:
        return "No data"

    min_h = min(heights)
    max_h = max(heights)
    range_h = max_h - min_h
    if range_h == 0:
        return "Tide is flat."

    # Normalize to chart height
    norm = [int((h - min_h) / range_h * (height - 1)) for h in heights]

    # Build chart
    chart_lines = []
    for row in range(height - 1, -1, -1):
        line = []
        for i, val in enumerate(norm):
            if val >= row:
                # Draw line
                if i > 0 and norm[i-1] >= row:
                    line.append('─')
                else:
                    line.append('┌')
            else:
                line.append(' ')
        chart_lines.append(''.join(line))

    # Add X axis labels (every 4 hours)
    x_axis = " "
    last_pos = 0
    for i in range(0, len(times), int(len(times) / (hours/4))):
        if i >= len(times): break
        t = times[i]
        label = t.strftime("%H:%M")
        pos = i
        if pos > last_pos:
            x_axis += " " * (pos - last_pos)
        x_axis += label
        last_pos = pos
    if last_pos < len(times) - 1:
        x_axis += " " * (len(times) - 1 - last_pos)

    chart = "\n".join(chart_lines) + "\n" + x_axis
    chart += f"\nMin: {min_h:.2f}m  Max: {max_h:.2f}m"
    return chart


# ─── Profile Manager ──────────────────────────────────────────────────────

class ProfileManager:
    def __init__(self):
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        self.profiles: List[TideStation] = []
        self.current_index = 0
        self._load()

    def _load(self):
        if PROFILES_FILE.exists():
            try:
                with open(PROFILES_FILE, 'r') as f:
                    data = json.load(f)
                    for p in data.get("profiles", []):
                        self.profiles.append(TideStation(
                            name=p["name"],
                            latitude=p.get("latitude", 0.0),
                            longitude=p.get("longitude", 0.0),
                            harmonics=p.get("harmonics", DEFAULT_HARMONICS.copy()),
                            timezone_offset=p.get("timezone_offset", 0.0)
                        ))
                    self.current_index = data.get("current_index", 0)
                    if self.current_index >= len(self.profiles):
                        self.current_index = 0
            except Exception:
                self._init_default()
        else:
            self._init_default()

    def _init_default(self):
        self.profiles = [TideStation(name="Default Coast", latitude=0.0, longitude=0.0)]
        self.current_index = 0
        self._save()

    def _save(self):
        data = {
            "profiles": [
                {
                    "name": p.name,
                    "latitude": p.latitude,
                    "longitude": p.longitude,
                    "harmonics": p.harmonics,
                    "timezone_offset": p.timezone_offset
                } for p in self.profiles
            ],
            "current_index": self.current_index
        }
        with open(PROFILES_FILE, 'w') as f:
            json.dump(data, f, indent=2)

    def add_profile(self, name: str, lat: float, lon: float, harmonics: Dict[str, float] = None):
        if harmonics is None:
            harmonics = DEFAULT_HARMONICS.copy()
        self.profiles.append(TideStation(name, lat, lon, harmonics))
        self.current_index = len(self.profiles) - 1
        self._save()

    def get_current(self) -> TideStation:
        if self.profiles:
            return self.profiles[self.current_index]
        return TideStation("Default")

    def list_profiles(self) -> List[str]:
        return [p.name for p in self.profiles]

    def set_current_by_name(self, name: str):
        for i, p in enumerate(self.profiles):
            if p.name == name:
                self.current_index = i
                self._save()
                return True
        return False


# ─── Main App ──────────────────────────────────────────────────────────────

class TideApp:
    def __init__(self):
        self.console = Console() if RICH_AVAILABLE else None
        self.profile_mgr = ProfileManager()
        self.station = self.profile_mgr.get_current()
        self.predictor = TidePredictor(self.station)

    def show_menu(self):
        if self.console:
            panel = Panel(
                f"[bold cyan]🌊 Tide Calendar[/bold cyan]\n"
                f"  Location: {self.station.name}\n"
                f"  Lat: {self.station.latitude:.2f}  Lon: {self.station.longitude:.2f}",
                title="📋 Main Menu",
                border_style="blue"
            )
            self.console.print(panel)
            self.console.print(" [1] 🌊 Tide Chart (next 24h)")
            self.console.print(" [2] 📅 Weekly Tide Forecast")
            self.console.print(" [3] 🎣 Fishing Forecast")
            self.console.print(" [4] 🌙 Lunar Phase Info")
            self.console.print(" [5] 🗺️  Change Location")
            self.console.print(" [6] 🛠️  Add Location")
            self.console.print(" [0] 🚪 Exit")
        else:
            print("\n" + "="*50)
            print(c("🌊 TIDE CALENDAR", "bright"))
            print("="*50)
            print(f"  Location: {self.station.name}")
            print(f"  Lat: {self.station.latitude:.2f}  Lon: {self.station.longitude:.2f}")
            print("="*50)
            print("  1. 🌊 Tide Chart (next 24h)")
            print("  2. 📅 Weekly Tide Forecast")
            print("  3. 🎣 Fishing Forecast")
            print("  4. 🌙 Lunar Phase Info")
            print("  5. 🗺️  Change Location")
            print("  6. 🛠️  Add Location")
            print("  0. 🚪 Exit")
            print("="*50)

    def show_tide_chart(self):
        now = datetime.now()
        chart = draw_tide_chart(self.predictor, now, hours=24)
        if self.console:
            self.console.print(Panel(chart, title="📈 Tide Chart (next 24h)", border_style="green"))
        else:
            print("\n📈 Tide Chart (next 24h)")
            print(chart)

    def show_weekly_forecast(self):
        now = datetime.now()
        if self.console:
            table = Table(title="📅 Weekly Tide Forecast", box=box.ROUNDED)
            table.add_column("Date", style="cyan")
            table.add_column("High Tides", style="green")
            table.add_column("Low Tides", style="red")
            table.add_column("Moon Phase")
        else:
            print("\n📅 Weekly Tide Forecast")
            print(c("-"*60, "dim"))

        for day in range(7):
            dt = now + timedelta(days=day)
            extrema = self.predictor.get_extrema(dt, hours=24)
            highs = [f"{t.strftime('%H:%M')} {h:.2f}m" for t, h, typ in extrema if typ == "High"]
            lows = [f"{t.strftime('%H:%M')} {h:.2f}m" for t, h, typ in extrema if typ == "Low"]
            moon = self.predictor._moon_phase(dt)
            moon_str = ["🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"][int((moon * 8) % 8)]
            if self.console:
                table.add_row(
                    dt.strftime("%a %d"),
                    "\n".join(highs) or "—",
                    "\n".join(lows) or "—",
                    moon_str
                )
            else:
                print(f"  {dt.strftime('%a %d')}  {moon_str}")
                print(f"    Highs: {', '.join(highs) if highs else '—'}")
                print(f"    Lows:  {', '.join(lows) if lows else '—'}")
                print()

        if self.console:
            self.console.print(table)
        else:
            print(c("-"*60, "dim"))

    def show_fishing_forecast(self):
        now = datetime.now()
        # Get next 6 hours of tide heights and forecast
        if self.console:
            self.console.print("[bold]🎣 Fishing Forecast[/bold]")
        else:
            print("\n🎣 Fishing Forecast")
        for hour in range(0, 7):
            dt = now + timedelta(hours=hour)
            h, trend = self.predictor.predict_tide(dt)
            moon = self.predictor._moon_phase(dt)
            rating = fishing_forecast(h, moon, dt)
            symbol = {"Excellent": "⭐⭐⭐", "Good": "⭐⭐", "Fair": "⭐", "Poor": "—"}[rating]
            color = {"Excellent": "green", "Good": "cyan", "Fair": "yellow", "Poor": "red"}[rating]
            if self.console:
                self.console.print(f"  {dt.strftime('%H:%M')} – {h:.2f}m ({trend}) → {c(rating, color)} {symbol}")
            else:
                print(f"  {dt.strftime('%H:%M')} – {h:.2f}m ({trend}) → {c(rating, color)} {symbol}")

    def show_lunar_phase(self):
        now = datetime.now()
        moon = self.predictor._moon_phase(now)
        phase_names = ["New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
                       "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent"]
        idx = int((moon * 8) % 8)
        phase = phase_names[idx]
        emoji = ["🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"][idx]
        if self.console:
            self.console.print(Panel(f"{emoji} {phase}\nIllumination: {abs(math.cos(moon * 2 * math.pi)) * 100:.1f}%", title="🌙 Lunar Phase", border_style="magenta"))
        else:
            print(f"\n🌙 Lunar Phase: {emoji} {phase}")
            print(f"   Illumination: {abs(math.cos(moon * 2 * math.pi)) * 100:.1f}%")

    def change_location(self):
        names = self.profile_mgr.list_profiles()
        if not names:
            print("No locations available.")
            return
        if self.console:
            self.console.print("[bold]Select location:[/bold]")
            for i, name in enumerate(names):
                self.console.print(f"  [{i+1}] {name}")
            choice = Prompt.ask("Number", choices=[str(i+1) for i in range(len(names))])
        else:
            print("Select location:")
            for i, name in enumerate(names):
                print(f"  {i+1}. {name}")
            choice = input("Number: ").strip()
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(names):
                self.profile_mgr.set_current_by_name(names[idx])
                self.station = self.profile_mgr.get_current()
                self.predictor = TidePredictor(self.station)
                print(c(f"✅ Switched to {self.station.name}", "green"))
            else:
                print(c("Invalid number.", "red"))
        except ValueError:
            print(c("Invalid input.", "red"))

    def add_location(self):
        if self.console:
            name = Prompt.ask("Location name")
            lat = float(Prompt.ask("Latitude (e.g., 50.0)"))
            lon = float(Prompt.ask("Longitude (e.g., -5.0)"))
        else:
            name = input("Location name: ").strip()
            lat = float(input("Latitude (e.g., 50.0): ").strip())
            lon = float(input("Longitude (e.g., -5.0): ").strip())
        self.profile_mgr.add_profile(name, lat, lon)
        self.station = self.profile_mgr.get_current()
        self.predictor = TidePredictor(self.station)
        print(c(f"✅ Added {name}", "green"))

    def run(self):
        if self.console:
            self.console.print(Panel.fit("[bold cyan]🌊 Tide Calendar – Smart Fishing Planner[/bold cyan]", border_style="cyan"))
        else:
            print(c("\n🌊 Tide Calendar – Smart Fishing Planner", "bright"))
            print(c("Know the tides, catch the big ones!", "dim"))

        while True:
            self.show_menu()
            if self.console:
                choice = Prompt.ask("Your choice", choices=["0","1","2","3","4","5","6"])
            else:
                choice = input("Your choice: ").strip()

            if choice == "1":
                self.show_tide_chart()
            elif choice == "2":
                self.show_weekly_forecast()
            elif choice == "3":
                self.show_fishing_forecast()
            elif choice == "4":
                self.show_lunar_phase()
            elif choice == "5":
                self.change_location()
            elif choice == "6":
                self.add_location()
            elif choice == "0":
                print(c("👋 Tight lines!", "cyan"))
                break
            else:
                print(c("❌ Invalid choice.", "red"))

            if choice != "0":
                if self.console:
                    self.console.print("\n[dim]Press Enter to continue...[/dim]")
                    input()
                else:
                    input("\nPress Enter to continue...")


if __name__ == "__main__":
    try:
        app = TideApp()
        app.run()
    except KeyboardInterrupt:
        print("\n👋 Goodbye!")
        sys.exit(0)
    except Exception as e:
        print(c(f"❌ Unexpected error: {e}", "red"))
        sys.exit(1)
