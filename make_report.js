const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  AlignmentType, HeadingLevel, BorderStyle, WidthType, ShadingType,
  Header, Footer, PageNumber, LevelFormat
} = require("docx");
const fs = require("fs");

const ACCENT      = "1F4E79";
const ACCENT_LIGHT = "D6E4F0";
const GRAY        = "3A3A3A";
const BORDER_COLOR = "AAAAAA";

const border   = { style: BorderStyle.SINGLE, size: 1, color: BORDER_COLOR };
const borders  = { top: border, bottom: border, left: border, right: border };
const noBorder = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
const noBord   = { top: noBorder, bottom: noBorder, left: noBorder, right: noBorder };

const h1 = (text) => new Paragraph({
  heading: HeadingLevel.HEADING_1,
  spacing: { before: 360, after: 160 },
  children: [new TextRun({ text, bold: true, size: 30, font: "Arial", color: ACCENT })]
});

const h2 = (text) => new Paragraph({
  heading: HeadingLevel.HEADING_2,
  spacing: { before: 240, after: 100 },
  children: [new TextRun({ text, bold: true, size: 24, font: "Arial", color: ACCENT })]
});

const body = (text, opts = {}) => new Paragraph({
  spacing: { after: 120 },
  children: [new TextRun({ text, size: 22, font: "Arial", color: GRAY, ...opts })]
});

const bullet = (text) => new Paragraph({
  numbering: { reference: "bullets", level: 0 },
  spacing: { after: 80 },
  children: [new TextRun({ text, size: 22, font: "Arial", color: GRAY })]
});

const step = (text) => new Paragraph({
  numbering: { reference: "steps", level: 0 },
  spacing: { after: 100 },
  children: [new TextRun({ text, size: 22, font: "Arial", color: GRAY })]
});

const code = (text) => new Paragraph({
  spacing: { after: 120 },
  indent: { left: 720 },
  shading: { fill: "F2F2F2", type: ShadingType.CLEAR },
  children: [new TextRun({ text, size: 20, font: "Courier New", color: "1a1a1a" })]
});

const spacer = () => new Paragraph({ spacing: { after: 100 }, children: [new TextRun("")] });

const hCell = (text, w) => new TableCell({
  borders, width: { size: w, type: WidthType.DXA },
  shading: { fill: ACCENT, type: ShadingType.CLEAR },
  margins: { top: 80, bottom: 80, left: 120, right: 120 },
  children: [new Paragraph({ children: [new TextRun({ text, bold: true, size: 20, font: "Arial", color: "FFFFFF" })] })]
});

const dCell = (text, w, shade = false) => new TableCell({
  borders, width: { size: w, type: WidthType.DXA },
  shading: { fill: shade ? ACCENT_LIGHT : "FFFFFF", type: ShadingType.CLEAR },
  margins: { top: 80, bottom: 80, left: 120, right: 120 },
  children: [new Paragraph({ children: [new TextRun({ text, size: 20, font: "Arial", color: GRAY })] })]
});

// ── Feature table ─────────────────────────────────────────────────
const featureTable = () => new Table({
  width: { size: 9026, type: WidthType.DXA },
  columnWidths: [1900, 2200, 3026, 1900],
  rows: [
    new TableRow({ children: [hCell("Feature", 1900), hCell("Formula Basis", 2200), hCell("What It Detects", 3026), hCell("Unit", 1900)] }),
    new TableRow({ children: [dCell("RMS", 1900), dCell("sqrt(mean(x²))", 2200), dCell("Average vibration energy level", 3026), dCell("V", 1900)] }),
    new TableRow({ children: [dCell("Peak-to-Peak", 1900, true), dCell("max(x) − min(x)", 2200, true), dCell("Mechanical looseness", 3026, true), dCell("V", 1900, true)] }),
    new TableRow({ children: [dCell("Kurtosis", 1900), dCell("E[(x−μ)⁴] / σ⁴", 2200), dCell("Bearing damage, shock events", 3026), dCell("—", 1900)] }),
    new TableRow({ children: [dCell("Crest Factor", 1900, true), dCell("max|x| / RMS", 2200, true), dCell("Crack and impulse faults", 3026, true), dCell("—", 1900, true)] }),
    new TableRow({ children: [dCell("FFT Energy", 1900), dCell("Σ|FFT(x)|²", 2200), dCell("Dominant frequency-domain energy", 3026), dCell("V²", 1900)] }),
  ]
});

// ── Technology stack table ────────────────────────────────────────
const techTable = () => new Table({
  width: { size: 9026, type: WidthType.DXA },
  columnWidths: [2400, 6626],
  rows: [
    new TableRow({ children: [hCell("Layer", 2400), hCell("Technologies Used", 6626)] }),
    new TableRow({ children: [dCell("Hardware", 2400), dCell("ESP32, ADXL335 analog vibration sensor", 6626)] }),
    new TableRow({ children: [dCell("Communication", 2400, true), dCell("WiFi, HTTP POST (JSON payload), WebSocket", 6626, true)] }),
    new TableRow({ children: [dCell("Server", 2400), dCell("Python, Flask, flask_sock, SQLite", 6626)] }),
    new TableRow({ children: [dCell("Machine Learning", 2400, true), dCell("scikit-learn: One-Class SVM, StandardScaler, Pipeline — NumPy, joblib", 6626, true)] }),
    new TableRow({ children: [dCell("Frontend", 2400), dCell("HTML5, CSS3, JavaScript, Chart.js, WebSocket API", 6626)] }),
    new TableRow({ children: [dCell("Version Control", 2400, true), dCell("Git, GitHub — github.com/Hymnelaus1/vibration_project", 6626, true)] }),
  ]
});

// ── Architecture flow ─────────────────────────────────────────────
const archFlow = () => new Table({
  width: { size: 9026, type: WidthType.DXA },
  columnWidths: [1800, 426, 1800, 426, 1800, 426, 2348],
  rows: [
    new TableRow({
      children: [
        new TableCell({ borders, width: { size: 1800, type: WidthType.DXA }, shading: { fill: ACCENT, type: ShadingType.CLEAR }, margins: { top: 100, bottom: 100, left: 80, right: 80 },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Vibration Sensor\n(ADC)", bold: true, size: 18, font: "Arial", color: "FFFFFF" })] })] }),
        new TableCell({ borders: noBord, width: { size: 426, type: WidthType.DXA },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "→", size: 24, font: "Arial", color: ACCENT, bold: true })] })] }),
        new TableCell({ borders, width: { size: 1800, type: WidthType.DXA }, shading: { fill: ACCENT, type: ShadingType.CLEAR }, margins: { top: 100, bottom: 100, left: 80, right: 80 },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "ESP32\n(WiFi POST)", bold: true, size: 18, font: "Arial", color: "FFFFFF" })] })] }),
        new TableCell({ borders: noBord, width: { size: 426, type: WidthType.DXA },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "→", size: 24, font: "Arial", color: ACCENT, bold: true })] })] }),
        new TableCell({ borders, width: { size: 1800, type: WidthType.DXA }, shading: { fill: ACCENT, type: ShadingType.CLEAR }, margins: { top: 100, bottom: 100, left: 80, right: 80 },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Flask Server\n+ ML Model", bold: true, size: 18, font: "Arial", color: "FFFFFF" })] })] }),
        new TableCell({ borders: noBord, width: { size: 426, type: WidthType.DXA },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "→", size: 24, font: "Arial", color: ACCENT, bold: true })] })] }),
        new TableCell({ borders, width: { size: 2348, type: WidthType.DXA }, shading: { fill: ACCENT, type: ShadingType.CLEAR }, margins: { top: 100, bottom: 100, left: 80, right: 80 },
          children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [new TextRun({ text: "Web Dashboard\n(WebSocket)", bold: true, size: 18, font: "Arial", color: "FFFFFF" })] })] }),
      ]
    })
  ]
});

// ── Document ──────────────────────────────────────────────────────
const doc = new Document({
  numbering: {
    config: [
      { reference: "bullets", levels: [{ level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } } }] },
      { reference: "steps", levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } } }] },
    ]
  },
  styles: {
    default: { document: { run: { font: "Arial", size: 22 } } },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 30, bold: true, font: "Arial", color: ACCENT },
        paragraph: { spacing: { before: 360, after: 160 }, outlineLevel: 0 } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 24, bold: true, font: "Arial", color: ACCENT },
        paragraph: { spacing: { before: 240, after: 100 }, outlineLevel: 1 } },
    ]
  },
  sections: [{
    properties: {
      page: {
        size: { width: 11906, height: 16838 },
        margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
      }
    },
    headers: {
      default: new Header({ children: [
        new Paragraph({
          alignment: AlignmentType.RIGHT,
          border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: ACCENT, space: 1 } },
          children: [new TextRun({ text: "ESP32 Vibration Anomaly Detection — Project Report", size: 18, font: "Arial", color: "888888" })]
        })
      ]})
    },
    footers: {
      default: new Footer({ children: [
        new Paragraph({
          alignment: AlignmentType.CENTER,
          border: { top: { style: BorderStyle.SINGLE, size: 4, color: ACCENT, space: 1 } },
          children: [
            new TextRun({ text: "Page ", size: 18, font: "Arial", color: "888888" }),
            new TextRun({ children: [PageNumber.CURRENT], size: 18, font: "Arial", color: "888888" }),
            new TextRun({ text: " of ", size: 18, font: "Arial", color: "888888" }),
            new TextRun({ children: [PageNumber.TOTAL_PAGES], size: 18, font: "Arial", color: "888888" }),
          ]
        })
      ]})
    },
    children: [

      // ── COVER PAGE ────────────────────────────────────────────
      new Paragraph({ spacing: { before: 1400, after: 0 }, children: [new TextRun("")] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 160 },
        children: [new TextRun({ text: "PROJECT REPORT", bold: true, size: 40, font: "Arial", color: ACCENT })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 120 },
        children: [new TextRun({ text: "ESP32-Based Vibration Sensor System", size: 28, font: "Arial", color: GRAY })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 600 },
        children: [new TextRun({ text: "with Machine Learning Anomaly Detection", bold: true, size: 28, font: "Arial", color: GRAY })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 80 },
        children: [new TextRun({ text: "Author: Hymnelaus1", size: 22, font: "Arial", color: GRAY })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 80 },
        children: [new TextRun({ text: "Date: April 2026", size: 22, font: "Arial", color: GRAY })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 80 },
        children: [new TextRun({ text: "Repository: github.com/Hymnelaus1/vibration_project", size: 22, font: "Arial", color: GRAY })] }),

      // ── 1. INTRODUCTION ───────────────────────────────────────
      new Paragraph({ pageBreakBefore: true, children: [new TextRun("")] }),
      h1("1. Introduction"),
      body(
        "This project implements a real-time machine health monitoring system using an ESP32 microcontroller " +
        "equipped with a vibration sensor. Raw analog readings are transmitted over WiFi to a Python-based server, " +
        "which stores the data, extracts signal features, and applies a One-Class SVM model to detect anomalies. " +
        "Results are pushed to a live web dashboard via WebSocket."
      ),
      body(
        "The system is designed to be low-cost, easy to deploy, and capable of detecting mechanical faults " +
        "such as bearing damage, imbalance, and looseness without requiring labeled anomaly data during training."
      ),

      // ── 2. SYSTEM ARCHITECTURE ────────────────────────────────
      spacer(),
      h1("2. System Architecture"),
      body("The pipeline consists of four stages connected in sequence:"),
      spacer(),
      archFlow(),
      spacer(),
      bullet("The vibration sensor outputs an analog voltage signal sampled by the ESP32 ADC."),
      bullet("ESP32 sends each reading as a JSON payload via HTTP POST over WiFi."),
      bullet("The Flask server buffers readings in 64-sample windows, extracts features, and runs the SVM model."),
      bullet("Anomaly flags and raw readings are broadcast to all connected dashboards via WebSocket."),

      // ── 3. FILE STRUCTURE ─────────────────────────────────────
      spacer(),
      h1("3. File Structure & Implementation"),

      h2("3.1  server/sunucu.py — Flask REST API Server"),
      body("The central component of the system. Manages data ingestion, feature computation, ML inference, database storage, and real-time broadcasting."),
      bullet("POST /data: Receives JSON from ESP32 containing sensor_id, raw ADC value, and voltage."),
      bullet("A 64-sample sliding window (Python deque) is maintained per sensor ID."),
      bullet("When the window is full, five signal features are extracted via features.py."),
      bullet("If a trained model exists (model.pkl), an anomaly score and label are computed."),
      bullet("All readings are persisted to SQLite (sensor_data.db)."),
      bullet("Results are broadcast to all WebSocket clients via flask_sock."),
      bullet("GET /readings: Returns the last 60 readings as JSON."),
      bullet("model.pkl is loaded automatically at startup if it exists."),
      spacer(),

      h2("3.2  server/features.py — Signal Feature Extraction"),
      body("Computes five time-domain and frequency-domain features from each 64-sample voltage window:"),
      spacer(),
      featureTable(),
      spacer(),

      h2("3.3  server/dashboard.html — Real-Time Web Dashboard"),
      body("A single-page application (SPA) built with plain HTML, CSS, and JavaScript — no framework dependency."),
      bullet("Chart.js renders a live dual-axis chart: voltage (left axis) and raw ADC / 1000 (right axis)."),
      bullet("On connection, the last 60 historical readings are fetched from the server."),
      bullet("Each new reading updates the chart and the data table without page reload."),
      bullet("Connection status is shown as LIVE or RECONNECTING."),
      bullet("Auto-reconnect logic retries every 3 seconds on disconnect."),
      spacer(),

      h2("3.4  ml/train_model.py — One-Class SVM Training"),
      body("Trains the anomaly detection model using only normal operating data — no anomaly labels required."),
      bullet("Reads rms, peak_to_peak, kurtosis, crest_factor, fft_energy columns from SQLite."),
      bullet("StandardScaler normalises the features (essential for SVM: features span very different numeric ranges)."),
      bullet("One-Class SVM is trained with kernel=RBF, nu=0.05, gamma=scale."),
      bullet("The scaler and model are saved together as a scikit-learn Pipeline in ml/model.pkl."),
      bullet("nu=0.05 allows up to 5% of training samples to be treated as outliers."),
      bullet("At inference time, predict() returns -1 for anomaly and +1 for normal."),
      spacer(),

      h2("3.5  esp32/ — Firmware Guide"),
      bullet("Documents the expected POST payload format and sensor wiring (GPIO34 analog input)."),
      bullet("Recommended sensor: ADXL335 — analog output, 0–3.3 V, directly compatible with ESP32 ADC."),
      spacer(),

      h2("3.6  stm32/ — Embedded Inference (Planned)"),
      bullet("Next phase: convert the trained model to C code using the emlearn library."),
      bullet("STM32 will handle feature extraction and SVM inference locally on-device."),
      bullet("ESP32 will only be responsible for WiFi communication and data forwarding."),
      bullet("This eliminates the need for a server connection during field deployment."),

      // ── 4. TECHNOLOGIES ───────────────────────────────────────
      spacer(),
      h1("4. Technologies Used"),
      spacer(),
      techTable(),

      // ── 5. SETUP & USAGE ──────────────────────────────────────
      spacer(),
      h1("5. Setup & Usage"),
      step("Install Python dependencies:"),
      code("pip install flask flask-sock scikit-learn numpy joblib"),
      step("Start the server:"),
      code("python server/sunucu.py"),
      step("Power on the ESP32 under normal operating conditions and collect at least 50 data windows."),
      step("Train the model:"),
      code("python ml/train_model.py"),
      step("Restart the server. Anomalies will now be flagged in real time on the dashboard."),
      spacer(),

      // ── 6. API REFERENCE ──────────────────────────────────────
      h1("6. API Reference"),
      spacer(),
      new Table({
        width: { size: 9026, type: WidthType.DXA },
        columnWidths: [1200, 2200, 5626],
        rows: [
          new TableRow({ children: [hCell("Method", 1200), hCell("Endpoint", 2200), hCell("Description", 5626)] }),
          new TableRow({ children: [dCell("POST", 1200), dCell("/data", 2200), dCell("Receive sensor reading from ESP32", 5626)] }),
          new TableRow({ children: [dCell("GET", 1200, true), dCell("/readings", 2200, true), dCell("Return last 60 readings as JSON", 5626, true)] }),
          new TableRow({ children: [dCell("GET", 1200), dCell("/", 2200), dCell("Serve the live web dashboard", 5626)] }),
          new TableRow({ children: [dCell("WS", 1200, true), dCell("/ws", 2200, true), dCell("WebSocket endpoint for real-time updates", 5626, true)] }),
        ]
      }),
      spacer(),
      body("Example POST body:"),
      code('{ "sensor_id": "sensor_1", "raw": 2048, "voltage": 1.654 }'),

      // ── 7. CONCLUSION ─────────────────────────────────────────
      spacer(),
      h1("7. Conclusion & Future Work"),
      body(
        "The system demonstrates that real-time vibration monitoring and machine learning based anomaly detection " +
        "can be achieved on low-cost hardware with a minimal software stack. " +
        "The One-Class SVM approach is particularly suitable for industrial deployments where anomaly labels are unavailable, " +
        "as it learns a decision boundary purely from normal operating data."
      ),
      body(
        "Future work will focus on embedded inference on the STM32 microcontroller using the emlearn library, " +
        "which converts trained scikit-learn models to C header files. " +
        "This will enable standalone, server-independent anomaly detection in field conditions. " +
        "Additional improvements include alert notifications (email / buzzer) and multi-sensor support."
      ),
      spacer(),
      body("Source code:", { bold: true }),
      new Paragraph({
        spacing: { after: 80 },
        children: [new TextRun({ text: "https://github.com/Hymnelaus1/vibration_project", size: 22, font: "Arial", color: "0563C1", underline: {} })]
      }),
    ]
  }]
});

Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync(
    "C:\\Users\\redfi\\OneDrive\\Masaüstü\\vibration_project\\vibration_project_report.docx",
    buf
  );
  console.log("Done: vibration_project_report.docx");
}).catch(err => { console.error(err); process.exit(1); });
