# TideCalendar.java
/**
 * 🌊 Tide Calendar – Smart Fishing Planner (Java Edition)
 * Advanced: harmonic tide prediction, ASCII charts, lunar phase, fishing forecast
 * Requires: Java 17+
 */

import java.io.*;
import java.nio.file.*;
import java.time.*;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.stream.Collectors;

// ─── Data Classes ──────────────────────────────────────────────────────────

class HarmonicMap {
    public double M2 = 1.2, S2 = 0.4, N2 = 0.2, K2 = 0.1, M4 = 0.1;
}

class TideStation {
    public String name;
    public double latitude;
    public double longitude;
    public HarmonicMap harmonics;
    public double timezoneOffset;

   
