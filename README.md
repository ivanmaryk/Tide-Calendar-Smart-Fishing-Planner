🌊 Tide Calendar – Smart Fishing Planner
"Know the tides, catch the big ones – plan your fishing trips with science and precision!"

📋 Table of Contents
✨ Features

📁 Repository Structure

🚀 Quick Start

💻 Language Implementations

📊 Data Format

🤝 Contributing

📄 License

✨ Features
Feature	Description
🌊 Tide Predictions	Calculate high and low tides for any date using a simplified harmonic model
📈 ASCII Tide Chart	Visualize the tide curve for the next 24 hours in your terminal
🌙 Lunar Phase	Show current moon phase and its effect on tide amplitude
🎣 Fishing Forecast	Rate fishing activity (Excellent/Good/Fair/Poor) based on tide, moon, and time
📅 Weekly Planner	Preview tide times and heights for the next 7 days
💾 Location Profiles	Save custom tide stations (latitude/longitude or harmonic constants)
🎨 Colorful CLI	Beautiful ANSI‑colored output with progress bars and emojis
⚡ Cross‑Platform	Works on Windows, macOS, and Linux
📁 Repository Structure
text
tide-calendar/
├── README.md
├── python/
│   └── tide_calendar.py
├── javascript/
│   └── tide_calendar.js
├── typescript/
│   └── tide_calendar.ts
├── go/
│   └── tide_calendar.go
├── rust/
│   └── tide_calendar.rs
├── cpp/
│   └── tide_calendar.cpp
├── java/
│   └── TideCalendar.java
└── csharp/
    └── TideCalendar.cs
🚀 Quick Start
Prerequisites
Each language requires its respective runtime/compiler (see individual sections)

Clone & Run
bash
git clone https://github.com/yourusername/tide-calendar.git
cd tide-calendar
# Navigate to your language folder and run
💻 Language Implementations
1. 🐍 Python
bash
cd python
pip install -r requirements.txt  # (numpy, rich)
python tide_calendar.py
Requires: Python 3.8+

2. 🟨 JavaScript (Node.js)
bash
cd javascript
node tide_calendar.js
Requires: Node.js 16+

3. 🟦 TypeScript
bash
cd typescript
npm install -g ts-node
ts-node tide_calendar.ts
Requires: Node.js 16+, TypeScript

4. 🟩 Go
bash
cd go
go run tide_calendar.go
Requires: Go 1.18+

5. 🦀 Rust
bash
cd rust
cargo run
Requires: Rust 1.70+ (dependencies: chrono, colored, serde)

6. ⚙️ C++
bash
cd cpp
g++ -std=c++17 tide_calendar.cpp -o tide_calendar
./tide_calendar
Requires: C++17 compiler

7. ☕ Java
bash
cd java
javac TideCalendar.java
java TideCalendar
Requires: JDK 17+

8. 🔷 C#
bash
cd csharp
dotnet run
Requires: .NET 6.0+

📊 Data Format
All implementations store location profiles and tide data in JSON format in the user's home directory under .tide_calendar/profiles.json. The schema:

json
{
  "profiles": [
    {
      "name": "My Harbour",
      "latitude": 50.0,
      "longitude": -5.0,
      "harmonic_constants": {
        "m2": 1.2,
        "s2": 0.4,
        "m4": 0.1
      },
      "timezone_offset": 0
    }
  ],
  "current_profile": "My Harbour"
}
If no harmonic constants are provided, the app uses a simplified lunar‑based model.

🤝 Contributing
Contributions are welcome! Please:

Fork the repository

Create a feature branch

Commit your changes

Open a Pull Request

📄 License
MIT © 2026 Tide Calendar Team
