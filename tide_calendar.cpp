# tide_calendar.cpp
/**
 * 🌊 Tide Calendar – Smart Fishing Planner (C++ Edition)
 * Advanced: harmonic tide prediction, ASCII charts, lunar phase, fishing forecast
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <limits>

// ─── Constants ──────────────────────────────────────────────────────────────

const double LUNAR_CYCLE_DAYS = 29.530588853;
const std::string CACHE_DIR = ".tide_calendar";
const std::string PROFILES_FILE = "profiles.json";

// ─── Colors ──────────────────────────────────────────────────────────────────

#ifdef _WIN32
#include <windows.h>
HANDLE hConsole;
void setColor(int color) { SetConsoleTextAttribute(hConsole, color); }
#define RESET_COLOR setColor(7)
#define COLOR_RED setColor(12)
#define COLOR_GREEN setColor(10)
#define COLOR_YELLOW setColor(14)
#define COLOR_BLUE setColor(9)
#define COLOR_MAGENTA setColor(13)
#define COLOR_CYAN setColor(11)
#define COLOR_BRIGHT setColor(15)
#define COLOR_DIM setColor(8)
#else
#define RESET_COLOR std::cout << "\x1b[0m"
#define COLOR_RED std::cout << "\x1b[31m"
#define COLOR_GREEN std::cout << "\x1b[32m"
#define COLOR_YELLOW std::cout << "\x1b[33m"
#define COLOR_BLUE std::cout << "\x1b[34m"
#define COLOR_MAGENTA std::cout << "\x1b[35m"
#define COLOR_CYAN std::cout << "\x1b[36m"
#define COLOR_BRIGHT std::cout << "\x1b[1m"
#define COLOR_DIM std::cout << "\x1b[2m"
#endif

#define C(str, color) color << str << RESET_COLOR

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string get_home_dir() {
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
#else
    const char* h = std::getenv("HOME");
#endif
    return h ? std::string(h) : ".";
}

std::time_t to_time_t(int year, int month, int day, int hour = 0, int min = 0, int sec = 0) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    return std::mktime(&tm);
}

std::string format_time(const std::time_t& t, const std::string& fmt) {
    std::tm* tm = std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(tm, fmt.c_str());
    return oss.str();
}

double days_since_epoch(const std::time_t& t) {
    std::time_t epoch = to_time_t(2000, 1, 1);
    return std::difftime(t, epoch) / 86400.0;
}

// ─── Tide Model ─────────────────────────────────────────────────────────────

struct HarmonicMap {
    double M2, S2, N2, K2, M4;
    HarmonicMap() : M2(1.2), S2(0.4), N2(0.2), K2(0.1), M4(0.1) {}
};

struct TideStation {
    std::string name;
    double latitude;
    double longitude;
    HarmonicMap harmonics;
    double timezoneOffset;
};

struct ExtremaResult {
    std::time_t time;
    double height;
    std::string type;
};

class TidePredictor {
public:
    TidePredictor(const TideStation& station) : station(station) {
        epoch = to_time_t(2000, 1, 1);
        constituents["M2"] = {14.49205211, 0.0};
        constituents["S2"] = {15.0, 0.0};
        constituents["N2"] = {14.49669388, 0.0};
        constituents["K2"] = {15.04106864, 0.0};
        constituents["M4"] = {28.98410422, 0.0};
    }

    double moonPhase(std::time_t t) const {
        double days = days_since_epoch(t);
        return std::fmod(days / LUNAR_CYCLE_DAYS, 1.0);
    }

    double tideHeight(std::time_t t) const {
        double days = days_since_epoch(t);
        double height = 0.0;
        std::map<std::string, double> amps = {
            {"M2", station.harmonics.M2},
            {"S2", station.harmonics.S2},
            {"N2", station.harmonics.N2},
            {"K2", station.harmonics.K2},
            {"M4", station.harmonics.M4}
        };
        for (const auto& [name, amp] : amps) {
            auto it = constituents.find(name);
            if (it != constituents.end()) {
                double arg = (it->second.first * days + it->second.second) * M_PI / 180.0;
                height += amp * std::cos(arg);
            }
        }
        return height + 1.0;
    }

    std::pair<double, std::string> predictTide(std::time_t t) const {
        double h = tideHeight(t);
        double h2 = tideHeight(t + 5 * 60);
        std::string trend = (h2 > h) ? "rising" : "falling";
        return {h, trend};
    }

    std::vector<ExtremaResult> getExtrema(std::time_t t, double hours) const {
        std::vector<ExtremaResult> results;
        int step = 5 * 60; // seconds
        std::time_t start = t - static_cast<std::time_t>(hours/2 * 3600);
        std::time_t end = t + static_cast<std::time_t>(hours/2 * 3600);
        std::time_t current = start;
        double prevH = tideHeight(current);
        current += step;
        while (current <= end) {
            double h = tideHeight(current);
            double nextH = tideHeight(current + step);
            if ((h - prevH) * (nextH - h) < 0) {
                auto ext = refineExtremum(current - step, current + step);
                if (ext.has_value()) {
                    auto [hExt, tExt] = ext.value();
                    std::string type = (hExt > 1.0) ? "High" : "Low";
                    results.push_back({tExt, hExt, type});
                }
            }
            prevH = h;
            current += step;
        }
        return results;
    }

private:
    TideStation station;
    std::time_t epoch;
    std::map<std::string, std::pair<double, double>> constituents;

    std::optional<std::pair<double, std::time_t>> refineExtremum(std::time_t start, std::time_t end) const {
        for (int i = 0; i < 10; i++) {
            std::time_t mid = start + (end - start) / 2;
            double hMid = tideHeight(mid);
            double hLeft = tideHeight(mid - 60);
            double hRight = tideHeight(mid + 60);
            if (hMid > hLeft && hMid > hRight) return {{hMid, mid}};
            if (hMid < hLeft && hMid < hRight) return {{hMid, mid}};
            if (hLeft > hRight) end = mid;
            else start = mid;
        }
        return std::nullopt;
    }
};

// ─── Fishing Forecast ──────────────────────────────────────────────────────

std::string fishingForecast(double height, double moonPhase, std::time_t t) {
    std::tm* tm = std::localtime(&t);
    int hour = tm->tm_hour;
    double rangeFactor = std::abs(height - 1.0);
    double moonFactor = 1.0 - std::abs(moonPhase - 0.5) * 2.0;
    double timeFactor = 0.4;
    if ((hour >= 5 && hour <= 7) || (hour >= 18 && hour <= 20)) timeFactor = 1.0;
    else if ((hour > 7 && hour < 11) || (hour > 15 && hour < 18)) timeFactor = 0.7;
    double score = (rangeFactor * 2.0 + moonFactor * 1.5 + timeFactor * 2.0) / 5.5;
    if (score > 0.8) return "Excellent";
    if (score > 0.6) return "Good";
    if (score > 0.4) return "Fair";
    return "Poor";
}

// ─── ASCII Tide Chart ──────────────────────────────────────────────────────

std::string drawTideChart(const TidePredictor& predictor, std::time_t t, int hours, int width, int height) {
    std::time_t start = t;
    std::time_t end = t + hours * 3600;
    int step = 5 * 60;
    std::vector<std::time_t> times;
    std::vector<double> heights;
    std::time_t cur = start;
    while (cur <= end) {
        auto [h, _] = predictor.predictTide(cur);
        times.push_back(cur);
        heights.push_back(h);
        cur += step;
    }
    if (heights.empty()) return "No data";
    double minH = *std::min_element(heights.begin(), heights.end());
    double maxH = *std::max_element(heights.begin(), heights.end());
    double range = maxH - minH;
    if (range == 0.0) return "Tide is flat.";
    std::vector<int> norm;
    for (double h : heights) {
        norm.push_back(static_cast<int>(std::floor((h - minH) / range * (height - 1))));
    }
    std::vector<std::string> lines;
    for (int row = height - 1; row >= 0; --row) {
        std::string line;
        for (size_t i = 0; i < norm.size(); ++i) {
            if (norm[i] >= row) {
                if (i > 0 && norm[i-1] >= row) line += "─";
                else line += "┌";
            } else {
                line += " ";
            }
        }
        lines.push_back(line);
    }
    // X axis
    std::string xAxis = " ";
    int lastPos = 0;
    int stepIdx = times.size() / (hours / 4);
    if (stepIdx < 1) stepIdx = 1;
    for (size_t i = 0; i < times.size(); i += stepIdx) {
        if (i >= times.size()) break;
        std::string label = format_time(times[i], "%H:%M");
        int pos = i;
        if (pos > lastPos) xAxis += std::string(pos - lastPos, ' ');
        xAxis += label;
        lastPos = pos;
    }
    if (lastPos < (int)times.size() - 1) {
        xAxis += std::string(times.size() - 1 - lastPos, ' ');
    }
    std::string chart;
    for (const auto& line : lines) chart += line + "\n";
    chart += xAxis + "\n";
    char buf[64];
    snprintf(buf, sizeof(buf), "Min: %.2fm  Max: %.2fm", minH, maxH);
    chart += buf;
    return chart;
}

// ─── Profile Manager ──────────────────────────────────────────────────────

class ProfileManager {
public:
    ProfileManager() {
        std::string home = get_home_dir();
        cacheDir = home + "/" + CACHE_DIR;
        std::filesystem::create_directories(cacheDir);
        filePath = cacheDir + "/" + PROFILES_FILE;
        load();
    }

    void load() {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            initDefault();
            return;
        }
        // Simplified: we read JSON manually (in production use a library like nlohmann/json)
        // For brevity, we use a placeholder: just init default
        // A real implementation would parse JSON.
        initDefault();
    }

    void save() {
        // Simplified: write default profile
        // In a real implementation, serialize to JSON.
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << "{\"profiles\":[{\"name\":\"Default Coast\",\"latitude\":0,\"longitude\":0,\"harmonics\":{\"M2\":1.2,\"S2\":0.4,\"N2\":0.2,\"K2\":0.1,\"M4\":0.1},\"timezoneOffset\":0}],\"currentIndex\":0}";
            file.close();
        }
    }

    void initDefault() {
        profiles.clear();
        TideStation def;
        def.name = "Default Coast";
        def.latitude = 0.0;
        def.longitude = 0.0;
        def.harmonics = HarmonicMap();
        def.timezoneOffset = 0.0;
        profiles.push_back(def);
        currentIndex = 0;
        save();
    }

    void addProfile(const std::string& name, double lat, double lon, const HarmonicMap& h = HarmonicMap()) {
        TideStation st;
        st.name = name;
        st.latitude = lat;
        st.longitude = lon;
        st.harmonics = h;
        st.timezoneOffset = 0.0;
        profiles.push_back(st);
        currentIndex = profiles.size() - 1;
        save();
    }

    TideStation getCurrent() const {
        if (profiles.empty()) {
            // return default
            TideStation def;
            def.name = "Default Coast";
            return def;
        }
        return profiles[currentIndex];
    }

    std::vector<std::string> listProfiles() const {
        std::vector<std::string> names;
        for (const auto& p : profiles) names.push_back(p.name);
        return names;
    }

    bool setCurrentByName(const std::string& name) {
        for (size_t i = 0; i < profiles.size(); ++i) {
            if (profiles[i].name == name) {
                currentIndex = i;
                save();
                return true;
            }
        }
        return false;
    }

private:
    std::string cacheDir, filePath;
    std::vector<TideStation> profiles;
    size_t currentIndex = 0;
};

// ─── Main App ──────────────────────────────────────────────────────────────

class TideApp {
public:
    TideApp() : profileMgr() {
        station = profileMgr.getCurrent();
        predictor = new TidePredictor(station);
    }

    ~TideApp() { delete predictor; }

    void run() {
        std::cout << "\033[2J\033[1;1H";
        std::cout << C("\n🌊 Tide Calendar – Smart Fishing Planner", COLOR_BRIGHT) << C("", COLOR_CYAN) << std::endl;
        std::cout << C("Know the tides, catch the big ones!", COLOR_DIM) << std::endl;

        while (true) {
            showMenu();
            std::string choice = ask("Your choice: ");
            if (choice == "1") showTideChart();
            else if (choice == "2") showWeeklyForecast();
            else if (choice == "3") showFishingForecast();
            else if (choice == "4") showLunarPhase();
            else if (choice == "5") changeLocation();
            else if (choice == "6") addLocation();
            else if (choice == "0") {
                std::cout << C("👋 Tight lines!", COLOR_CYAN) << std::endl;
                break;
            } else {
                std::cout << C("❌ Invalid choice.", COLOR_RED) << std::endl;
            }
            if (choice != "0") {
                std::cout << "\nPress Enter to continue...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cin.get();
            }
        }
    }

private:
    ProfileManager profileMgr;
    TideStation station;
    TidePredictor* predictor;

    std::string ask(const std::string& prompt) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        return trim(line);
    }

    double askDouble(const std::string& prompt) {
        while (true) {
            std::string ans = ask(prompt);
            try {
                return std::stod(ans);
            } catch (...) {
                std::cout << C("Please enter a valid number.", COLOR_YELLOW) << std::endl;
            }
        }
    }

    bool askConfirm(const std::string& prompt) {
        std::string ans = ask(prompt + " (yes/no): ");
        std::string lower = ans;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "yes" || lower == "y";
    }

    void showMenu() {
        std::time_t now = std::time(nullptr);
        std::cout << "\n" << C(std::string(50, '═'), COLOR_CYAN) << std::endl;
        std::cout << C("🌊 TIDE CALENDAR", COLOR_BRIGHT) << C("", COLOR_CYAN) << std::endl;
        std::cout << C(std::string(50, '═'), COLOR_CYAN) << std::endl;
        std::cout << "  Location: " << station.name << std::endl;
        std::cout << "  Lat: " << std::fixed << std::setprecision(2) << station.latitude << "  Lon: " << station.longitude << std::endl;
        std::cout << C(std::string(50, '═'), COLOR_CYAN) << std::endl;
        std::cout << "  1. 🌊 Tide Chart (next 24h)" << std::endl;
        std::cout << "  2. 📅 Weekly Tide Forecast" << std::endl;
        std::cout << "  3. 🎣 Fishing Forecast" << std::endl;
        std::cout << "  4. 🌙 Lunar Phase Info" << std::endl;
        std::cout << "  5. 🗺️  Change Location" << std::endl;
        std::cout << "  6. 🛠️  Add Location" << std::endl;
        std::cout << "  0. 🚪 Exit" << std::endl;
        std::cout << C(std::string(50, '═'), COLOR_CYAN) << std::endl;
    }

    void showTideChart() {
        std::time_t now = std::time(nullptr);
        std::string chart = drawTideChart(*predictor, now, 24, 60, 10);
        std::cout << "\n" << C("📈 Tide Chart (next 24h)", COLOR_BRIGHT) << std::endl;
        std::cout << chart << std::endl;
    }

    void showWeeklyForecast() {
        std::time_t now = std::time(nullptr);
        std::cout << "\n" << C("📅 Weekly Tide Forecast", COLOR_BRIGHT) << std::endl;
        std::cout << C(std::string(60, '─'), COLOR_DIM) << std::endl;
        for (int day = 0; day < 7; ++day) {
            std::time_t dt = now + day * 86400;
            auto extrema = predictor->getExtrema(dt, 24.0);
            std::vector<std::string> highs, lows;
            for (const auto& e : extrema) {
                if (e.type == "High") {
                    highs.push_back(format_time(e.time, "%H:%M") + " " + std::to_string(e.height).substr(0,4) + "m");
                } else {
                    lows.push_back(format_time(e.time, "%H:%M") + " " + std::to_string(e.height).substr(0,4) + "m");
                }
            }
            double moon = predictor->moonPhase(dt);
            const char* moonEmojis[] = {"🌑","🌒","🌓","🌔","🌕","🌖","🌗","🌘"};
            std::string moonStr = moonEmojis[static_cast<int>(std::floor((moon*8))) % 8];
            std::cout << "  " << format_time(dt, "%a %b %d") << " " << moonStr << std::endl;
            std::string hStr = highs.empty() ? "—" : join(highs, ", ");
            std::string lStr = lows.empty() ? "—" : join(lows, ", ");
            std::cout << "    Highs: " << hStr << std::endl;
            std::cout << "    Lows:  " << lStr << std::endl;
        }
        std::cout << C(std::string(60, '─'), COLOR_DIM) << std::endl;
    }

    void showFishingForecast() {
        std::time_t now = std::time(nullptr);
        std::cout << "\n" << C("🎣 Fishing Forecast", COLOR_BRIGHT) << std::endl;
        for (int hour = 0; hour < 7; ++hour) {
            std::time_t dt = now + hour * 3600;
            auto [h, trend] = predictor->predictTide(dt);
            double moon = predictor->moonPhase(dt);
            std::string rating = fishingForecast(h, moon, dt);
            std::map<std::string, std::string> symbols = {{"Excellent","⭐⭐⭐"},{"Good","⭐⭐"},{"Fair","⭐"},{"Poor","—"}};
            std::map<std::string, std::string> colorMap = {{"Excellent",COLOR_GREEN},{"Good",COLOR_CYAN},{"Fair",COLOR_YELLOW},{"Poor",COLOR_RED}};
            std::string sym = symbols[rating];
            std::string col = colorMap[rating];
            std::cout << "  " << format_time(dt, "%H:%M") << " – " << std::fixed << std::setprecision(2) << h << "m (" << trend << ") → " << C(rating, col) << " " << sym << std::endl;
        }
    }

    void showLunarPhase() {
        std::time_t now = std::time(nullptr);
        double moon = predictor->moonPhase(now);
        const char* phaseNames[] = {"New Moon","Waxing Crescent","First Quarter","Waxing Gibbous","Full Moon","Waning Gibbous","Last Quarter","Waning Crescent"};
        int idx = static_cast<int>(std::floor((moon*8))) % 8;
        std::string phase = phaseNames[idx];
        const char* emojis[] = {"🌑","🌒","🌓","🌔","🌕","🌖","🌗","🌘"};
        std::string emoji = emojis[idx];
        double illumination = std::abs(std::cos(moon * 2 * M_PI)) * 100.0;
        std::cout << "\n🌙 Lunar Phase: " << emoji << " " << phase << std::endl;
        std::cout << "   Illumination: " << std::fixed << std::setprecision(1) << illumination << "%" << std::endl;
    }

    void changeLocation() {
        auto names = profileMgr.listProfiles();
        if (names.empty()) {
            std::cout << C("No locations available.", COLOR_YELLOW) << std::endl;
            return;
        }
        std::cout << "Select location:" << std::endl;
        for (size_t i = 0; i < names.size(); ++i) {
            std::cout << "  " << i+1 << ". " << names[i] << std::endl;
        }
        std::string ans = ask("Number: ");
        int idx = std::stoi(ans) - 1;
        if (idx >= 0 && idx < (int)names.size()) {
            profileMgr.setCurrentByName(names[idx]);
            station = profileMgr.getCurrent();
            delete predictor;
            predictor = new TidePredictor(station);
            std::cout << C("✅ Switched to " + station.name, COLOR_GREEN) << std::endl;
        } else {
            std::cout << C("Invalid number.", COLOR_RED) << std::endl;
        }
    }

    void addLocation() {
        std::string name = ask("Location name: ");
        double lat = askDouble("Latitude (e.g., 50.0): ");
        double lon = askDouble("Longitude (e.g., -5.0): ");
        profileMgr.addProfile(name, lat, lon);
        station = profileMgr.getCurrent();
        delete predictor;
        predictor = new TidePredictor(station);
        std::cout << C("✅ Added " + name, COLOR_GREEN) << std::endl;
    }

    std::string join(const std::vector<std::string>& v, const std::string& delim) {
        std::ostringstream oss;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) oss << delim;
            oss << v[i];
        }
        return oss.str();
    }
};

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
#ifdef _WIN32
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    try {
        TideApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << C("❌ Unexpected error: ", COLOR_RED) << e.what() << std::endl;
        return 1;
    }
    return 0;
}
