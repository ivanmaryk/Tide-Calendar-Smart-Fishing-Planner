# tide_calendar.ts
/**
 * 🌊 Tide Calendar – Smart Fishing Planner (TypeScript Edition)
 * Fully typed, harmonic prediction, ASCII charts, lunar phase, fishing forecast
 */

import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import * as readline from 'readline';

// ─── Types ──────────────────────────────────────────────────────────────────

interface HarmonicMap {
    M2: number;
    S2: number;
    N2: number;
    K2: number;
    M4: number;
}

interface TideStationData {
    name: string;
    latitude: number;
    longitude: number;
    harmonics: HarmonicMap;
    timezoneOffset: number;
}

interface ExtremaResult {
    time: Date;
    height: number;
    type: 'High' | 'Low';
}

interface ProfileData {
    profiles: TideStationData[];
    currentIndex: number;
}

// ─── Constants ──────────────────────────────────────────────────────────────

const CACHE_DIR = path.join(os.homedir(), '.tide_calendar');
const PROFILES_FILE = path.join(CACHE_DIR, 'profiles.json');
const DEFAULT_HARMONICS: HarmonicMap = { M2: 1.2, S2: 0.4, N2: 0.2, K2: 0.1, M4: 0.1 };
const LUNAR_CYCLE_DAYS = 29.530588853;

// ─── Colors ──────────────────────────────────────────────────────────────────

const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    dim: '\x1b[2m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    magenta: '\x1b[35m',
    cyan: '\x1b[36m',
};

const c = (str: string, color: string): string => `${color}${str}${colors.reset}`;

// ─── Tide Model ─────────────────────────────────────────────────────────────

class TideStation {
    constructor(
        public name: string,
        public latitude: number = 0,
        public longitude: number = 0,
        public harmonics: HarmonicMap = { ...DEFAULT_HARMONICS },
        public timezoneOffset: number = 0
    ) {}
}

class TidePredictor {
    private epoch: Date = new Date(2000, 0, 1);
    private constituents: Record<string, { speed: number; phase: number }> = {
        M2: { speed: 14.49205211, phase: 0.0 },
        S2: { speed: 15.0, phase: 0.0 },
        N2: { speed: 14.49669388, phase: 0.0 },
        K2: { speed: 15.04106864, phase: 0.0 },
        M4: { speed: 28.98410422, phase: 0.0 },
    };

    constructor(public station: TideStation) {}

    private _daysSinceEpoch(dt: Date): number {
        return (dt.getTime() - this.epoch.getTime()) / (1000 * 60 * 60 * 24);
    }

    private _moonPhase(dt: Date): number {
        return (this._daysSinceEpoch(dt) / LUNAR_CYCLE_DAYS) % 1.0;
    }

    private _tideHeight(dt: Date): number {
        const days = this._daysSinceEpoch(dt);
        let height = 0;
        for (const [name, amp] of Object.entries(this.station.harmonics)) {
            if (this.constituents[name]) {
                const speed = this.constituents[name].speed;
                const phase = this.constituents[name].phase;
                const arg = (speed * days + phase) * Math.PI / 180;
                height += amp * Math.cos(arg);
            }
        }
        return height + 1.0;
    }

    predictTide(dt: Date): { height: number; trend: string } {
        const h = this._tideHeight(dt);
        const dt2 = new Date(dt.getTime() + 5 * 60000);
        const h2 = this._tideHeight(dt2);
        const trend = h2 > h ? 'rising' : 'falling';
        return { height: h, trend };
    }

    getExtrema(dt: Date, hours: number = 12.5): ExtremaResult[] {
        const results: ExtremaResult[] = [];
        const step = 5 * 60000;
        let current = new Date(dt.getTime() - hours/2 * 3600000);
        const end = new Date(dt.getTime() + hours/2 * 3600000);
        let prevH = this._tideHeight(current);
        current = new Date(current.getTime() + step);
        while (current <= end) {
            const h = this._tideHeight(current);
            const nextH = this._tideHeight(new Date(current.getTime() + step));
            if ((h - prevH) * (nextH - h) < 0) {
                const ext = this._refineExtremum(new Date(current.getTime() - step), new Date(current.getTime() + step));
                if (ext) {
                    const [hExt, tExt] = ext;
                    const type = hExt > 1.0 ? 'High' : 'Low';
                    results.push({ time: tExt, height: hExt, type });
                }
            }
            prevH = h;
            current = new Date(current.getTime() + step);
        }
        return results;
    }

    private _refineExtremum(start: Date, end: Date): [number, Date] | null {
        for (let i = 0; i < 10; i++) {
            const mid = new Date((start.getTime() + end.getTime()) / 2);
            const hMid = this._tideHeight(mid);
            const hLeft = this._tideHeight(new Date(mid.getTime() - 60000));
            const hRight = this._tideHeight(new Date(mid.getTime() + 60000));
            if (hMid > hLeft && hMid > hRight) return [hMid, mid];
            if (hMid < hLeft && hMid < hRight) return [hMid, mid];
            if (hLeft > hRight) end = mid;
            else start = mid;
        }
        return null;
    }
}

// ─── Fishing Forecast ──────────────────────────────────────────────────────

function fishingForecast(height: number, moonPhase: number, dt: Date): string {
    const hour = dt.getHours();
    const rangeFactor = Math.abs(height - 1.0);
    const moonFactor = 1.0 - Math.abs(moonPhase - 0.5) * 2;
    let timeFactor: number;
    if ((hour >= 5 && hour <= 7) || (hour >= 18 && hour <= 20)) timeFactor = 1.0;
    else if ((hour > 7 && hour < 11) || (hour > 15 && hour < 18)) timeFactor = 0.7;
    else timeFactor = 0.4;
    const score = (rangeFactor * 2 + moonFactor * 1.5 + timeFactor * 2) / 5.5;
    if (score > 0.8) return 'Excellent';
    if (score > 0.6) return 'Good';
    if (score > 0.4) return 'Fair';
    return 'Poor';
}

// ─── ASCII Tide Chart ──────────────────────────────────────────────────────

function drawTideChart(predictor: TidePredictor, dt: Date, hours: number = 24, width: number = 60, height: number = 10): string {
    const start = dt;
    const end = new Date(dt.getTime() + hours * 3600000);
    const step = 5 * 60000;
    const times: Date[] = [];
    const heights: number[] = [];
    let cur = new Date(start);
    while (cur <= end) {
        const { height } = predictor.predictTide(cur);
        times.push(cur);
        heights.push(height);
        cur = new Date(cur.getTime() + step);
    }
    if (heights.length === 0) return 'No data';
    const minH = Math.min(...heights);
    const maxH = Math.max(...heights);
    const range = maxH - minH;
    if (range === 0) return 'Tide is flat.';
    const norm = heights.map(h => Math.floor((h - minH) / range * (height - 1)));
    const lines: string[] = [];
    for (let row = height - 1; row >= 0; row--) {
        let line = '';
        for (let i = 0; i < norm.length; i++) {
            if (norm[i] >= row) {
                if (i > 0 && norm[i-1] >= row) line += '─';
                else line += '┌';
            } else {
                line += ' ';
            }
        }
        lines.push(line);
    }
    // X axis
    let xAxis = ' ';
    let lastPos = 0;
    const stepIdx = Math.floor(times.length / (hours / 4));
    for (let i = 0; i < times.length; i += stepIdx) {
        if (i >= times.length) break;
        const t = times[i];
        const label = t.toTimeString().slice(0,5);
        const pos = i;
        if (pos > lastPos) xAxis += ' '.repeat(pos - lastPos);
        xAxis += label;
        lastPos = pos;
    }
    if (lastPos < times.length - 1) xAxis += ' '.repeat(times.length - 1 - lastPos);
    let chart = lines.join('\n') + '\n' + xAxis;
    chart += `\nMin: ${minH.toFixed(2)}m  Max: ${maxH.toFixed(2)}m`;
    return chart;
}

// ─── Profile Manager ──────────────────────────────────────────────────────

class ProfileManager {
    private profiles: TideStation[] = [];
    private currentIndex: number = 0;

    constructor() {
        if (!fs.existsSync(CACHE_DIR)) fs.mkdirSync(CACHE_DIR, { recursive: true });
        this._load();
    }

    private _load(): void {
        if (fs.existsSync(PROFILES_FILE)) {
            try {
                const raw = fs.readFileSync(PROFILES_FILE, 'utf8');
                const data: ProfileData = JSON.parse(raw);
                this.profiles = data.profiles.map(p => new TideStation(p.name, p.latitude, p.longitude, p.harmonics, p.timezoneOffset));
                this.currentIndex = data.currentIndex || 0;
                if (this.currentIndex >= this.profiles.length) this.currentIndex = 0;
            } catch (_) {
                this._initDefault();
            }
        } else {
            this._initDefault();
        }
    }

    private _initDefault(): void {
        this.profiles = [new TideStation('Default Coast', 0, 0)];
        this.currentIndex = 0;
        this._save();
    }

    private _save(): void {
        const data: ProfileData = {
            profiles: this.profiles.map(p => ({
                name: p.name,
                latitude: p.latitude,
                longitude: p.longitude,
                harmonics: p.harmonics,
                timezoneOffset: p.timezoneOffset
            })),
            currentIndex: this.currentIndex
        };
        fs.writeFileSync(PROFILES_FILE, JSON.stringify(data, null, 2));
    }

    addProfile(name: string, lat: number, lon: number, harmonics?: HarmonicMap): void {
        const h = harmonics || { ...DEFAULT_HARMONICS };
        this.profiles.push(new TideStation(name, lat, lon, h));
        this.currentIndex = this.profiles.length - 1;
        this._save();
    }

    getCurrent(): TideStation {
        return this.profiles[this.currentIndex] || new TideStation('Default');
    }

    listProfiles(): string[] {
        return this.profiles.map(p => p.name);
    }

    setCurrentByName(name: string): boolean {
        for (let i = 0; i < this.profiles.length; i++) {
            if (this.profiles[i].name === name) {
                this.currentIndex = i;
                this._save();
                return true;
            }
        }
        return false;
    }
}

// ─── Main App ──────────────────────────────────────────────────────────────

class TideApp {
    private rl: readline.Interface;
    private profileMgr: ProfileManager;
    private station: TideStation;
    private predictor: TidePredictor;

    constructor() {
        this.rl = readline.createInterface({ input: process.stdin, output: process.stdout });
        this.profileMgr = new ProfileManager();
        this.station = this.profileMgr.getCurrent();
        this.predictor = new TidePredictor(this.station);
    }

    private _ask(prompt: string): Promise<string> {
        return new Promise(resolve => this.rl.question(prompt, resolve));
    }

    private _askConfirm(prompt: string): Promise<boolean> {
        return this._ask(prompt + ' (yes/no): ').then(ans => ans.trim().toLowerCase() === 'yes' || ans.trim().toLowerCase() === 'y');
    }

    private async showMenu(): Promise<void> {
        console.log('\n' + c('═'.repeat(50), colors.cyan));
        console.log(c('🌊 TIDE CALENDAR', colors.bright + colors.cyan));
        console.log(c('═'.repeat(50), colors.cyan));
        console.log(`  Location: ${this.station.name}`);
        console.log(`  Lat: ${this.station.latitude.toFixed(2)}  Lon: ${this.station.longitude.toFixed(2)}`);
        console.log(c('═'.repeat(50), colors.cyan));
        console.log('  1. 🌊 Tide Chart (next 24h)');
        console.log('  2. 📅 Weekly Tide Forecast');
        console.log('  3. 🎣 Fishing Forecast');
        console.log('  4. 🌙 Lunar Phase Info');
        console.log('  5. 🗺️  Change Location');
        console.log('  6. 🛠️  Add Location');
        console.log('  0. 🚪 Exit');
        console.log(c('═'.repeat(50), colors.cyan));
    }

    private showTideChart(): void {
        const now = new Date();
        const chart = drawTideChart(this.predictor, now, 24);
        console.log('\n' + c('📈 Tide Chart (next 24h)', colors.bright));
        console.log(chart);
    }

    private showWeeklyForecast(): void {
        const now = new Date();
        console.log('\n' + c('📅 Weekly Tide Forecast', colors.bright));
        console.log(c('─'.repeat(60), colors.dim));
        for (let day = 0; day < 7; day++) {
            const dt = new Date(now.getTime() + day * 86400000);
            const extrema = this.predictor.getExtrema(dt, 24);
            const highs = extrema.filter(e => e.type === 'High').map(e => `${e.time.toTimeString().slice(0,5)} ${e.height.toFixed(2)}m`);
            const lows = extrema.filter(e => e.type === 'Low').map(e => `${e.time.toTimeString().slice(0,5)} ${e.height.toFixed(2)}m`);
            const moon = this.predictor['_moonPhase'](dt);
            const moonEmojis = ['🌑','🌒','🌓','🌔','🌕','🌖','🌗','🌘'];
            const moonStr = moonEmojis[Math.floor((moon * 8) % 8)];
            console.log(`  ${dt.toDateString()} ${moonStr}`);
            console.log(`    Highs: ${highs.join(', ') || '—'}`);
            console.log(`    Lows:  ${lows.join(', ') || '—'}`);
        }
        console.log(c('─'.repeat(60), colors.dim));
    }

    private showFishingForecast(): void {
        const now = new Date();
        console.log('\n' + c('🎣 Fishing Forecast', colors.bright));
        for (let hour = 0; hour < 7; hour++) {
            const dt = new Date(now.getTime() + hour * 3600000);
            const { height, trend } = this.predictor.predictTide(dt);
            const moon = this.predictor['_moonPhase'](dt);
            const rating = fishingForecast(height, moon, dt);
            const symbols = { Excellent: '⭐⭐⭐', Good: '⭐⭐', Fair: '⭐', Poor: '—' };
            const colorMap = { Excellent: colors.green, Good: colors.cyan, Fair: colors.yellow, Poor: colors.red };
            const sym = symbols[rating];
            const col = colorMap[rating];
            console.log(`  ${dt.toTimeString().slice(0,5)} – ${height.toFixed(2)}m (${trend}) → ${c(rating, col)} ${sym}`);
        }
    }

    private showLunarPhase(): void {
        const now = new Date();
        const moon = this.predictor['_moonPhase'](now);
        const phaseNames = ['New Moon','Waxing Crescent','First Quarter','Waxing Gibbous',
                           'Full Moon','Waning Gibbous','Last Quarter','Waning Crescent'];
        const idx = Math.floor((moon * 8) % 8);
        const phase = phaseNames[idx];
        const emojis = ['🌑','🌒','🌓','🌔','🌕','🌖','🌗','🌘'];
        const emoji = emojis[idx];
        const illumination = Math.abs(Math.cos(moon * 2 * Math.PI)) * 100;
        console.log(`\n🌙 Lunar Phase: ${emoji} ${phase}`);
        console.log(`   Illumination: ${illumination.toFixed(1)}%`);
    }

    private async changeLocation(): Promise<void> {
        const names = this.profileMgr.listProfiles();
        if (names.length === 0) {
            console.log(c('No locations available.', colors.yellow));
            return;
        }
        console.log('Select location:');
        names.forEach((n, i) => console.log(`  ${i+1}. ${n}`));
        const ans = await this._ask('Number: ');
        const idx = parseInt(ans) - 1;
        if (idx >= 0 && idx < names.length) {
            this.profileMgr.setCurrentByName(names[idx]);
            this.station = this.profileMgr.getCurrent();
            this.predictor = new TidePredictor(this.station);
            console.log(c(`✅ Switched to ${this.station.name}`, colors.green));
        } else {
            console.log(c('Invalid number.', colors.red));
        }
    }

    private async addLocation(): Promise<void> {
        const name = await this._ask('Location name: ');
        const lat = parseFloat(await this._ask('Latitude (e.g., 50.0): '));
        const lon = parseFloat(await this._ask('Longitude (e.g., -5.0): '));
        this.profileMgr.addProfile(name, lat, lon);
        this.station = this.profileMgr.getCurrent();
        this.predictor = new TidePredictor(this.station);
        console.log(c(`✅ Added ${name}`, colors.green));
    }

    async run(): Promise<void> {
        console.clear();
        console.log(c('\n🌊 Tide Calendar – Smart Fishing Planner', colors.bright + colors.cyan));
        console.log(c('Know the tides, catch the big ones!', colors.dim));

        while (true) {
            await this.showMenu();
            const choice = await this._ask('Your choice: ');
            switch (choice.trim()) {
                case '1': this.showTideChart(); break;
                case '2': this.showWeeklyForecast(); break;
                case '3': this.showFishingForecast(); break;
                case '4': this.showLunarPhase(); break;
                case '5': await this.changeLocation(); break;
                case '6': await this.addLocation(); break;
                case '0':
                    console.log(c('👋 Tight lines!', colors.cyan));
                    this.rl.close();
                    return;
                default:
                    console.log(c('❌ Invalid choice.', colors.red));
            }
            if (choice !== '0') {
                console.log('\nPress Enter to continue...');
                await this._ask('');
            }
        }
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

const main = async (): Promise<void> => {
    try {
        const app = new TideApp();
        await app.run();
    } catch (e: any) {
        console.error(c(`❌ Unexpected error: ${e.message}`, colors.red));
        process.exit(1);
    }
};

main();
