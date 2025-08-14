# 🥋 KarateAI — Real‑Time Detection & Classification of Karate Strikes

**KarateAI** is a wearable **embedded ML** system that detects and classifies karate strikes **in real time** and displays the result on a **web UI**.  
It runs a trained Edge Impulse model on an **Arduino Nano 33 BLE Sense Rev2**, streams the latest prediction to a **Flask** endpoint, and the browser dashboard shows the strike with an animation and optional sound.

---

## ✨ Key Features

- **On‑device ML** (Edge Impulse deployment) for low‑latency strike recognition  
- **Real‑time feedback** on a clean web interface (strike name + animation + visual cue)  
- **Wearable setup** (sensor embedded in glove) with wireless capabilities  
- **Simple local server** that reads predictions from serial and serves them to the browser

---

## 📁 Project Structure

```
KarateAI/
├── app.py                         # Flask server: serves UI + exposes /prediction (reads serial)
├── index.html                     # Web UI (fetches /prediction; shows GIFs + plays sounds)
├── karate_ble_connect.ino         # Arduino: BLE + Edge Impulse inferencing (+ Serial prints)
├── nano_ble33_sense_rev2_fusion.ino   # Arduino: sensor fusion + EI pipeline (reference)
└── static/
    ├── gyaku.gif                  # Animation for Gyaku Zuki
    └── kisami.gif                 # Animation for Kisami Zuki
```
> If your layout differs, adjust paths in `app.py` and `index.html` accordingly.

---

## 🔩 Hardware

- **Arduino Nano 33 BLE Sense Rev2** (built‑in IMU)
- **Glove** (mount board securely; add strain relief)
- **USB cable** for programming & serial
- Optional: **power bank** for untethered demos

---

## 🧰 Software & Libraries

### PC
- **Python 3.9+**
- **Flask** and **pyserial** (`pip install flask pyserial`)
- Modern browser (Chrome/Edge/Brave/Firefox)

### Arduino IDE (or Arduino CLI)
Install these via **Library Manager**:
- `ArduinoBLE`
- `Arduino_BMI270_BMM150`
- `Arduino_LPS22HB`
- `Arduino_HS300x`
- `Arduino_APDS9960`

From **Edge Impulse**:
- Export your trained project as **Arduino Library** and install it (`Sketch > Include Library > Add .ZIP Library`).  
  Your sketch should include something like: `#include <your_project_inferencing.h>`.

---

## 🧪 Data → Model (Edge Impulse)

1. **Collect data** with the Nano 33 BLE Sense Rev2 (or upload CSV).  
   Classes typically: `GyakuZuki`, `KisamiZuki`, `Idle`.
2. **Create Impulse**: Choose appropriate IMU DSP blocks (or raw + minimal DSP), then a small NN classifier.
3. **Train & Validate**: Monitor accuracy, confusion matrix, and on‑device latency.
4. **Deploy**: `Deploy > Arduino Library` → download `.zip`, then install it in Arduino IDE.

> Keep the model light for on‑device inference. Check RAM and Flash usage in the Arduino build logs.

---

## 🔧 Flash the Firmware

### Option A — `karate_ble_connect.ino` (recommended)
- Runs **Edge Impulse inference** and (commonly) prints the predicted label to **Serial**  
  (ensure you `Serial.println(predicted_label)` after each inference window).
- Steps:
  1. Open `karate_ble_connect.ino`
  2. **Tools > Board**: *Arduino Nano 33 BLE Sense*
  3. **Tools > Port**: select your device
  4. **Sketch > Verify/Upload**

### Option B — `nano_ble33_sense_rev2_fusion.ino`
- Reference/fusion sketch showing sensor configuration + EI pipeline.  
  Useful to customize sensor reads before merging into the main sketch.

---

## 🖥️ Run the Web UI (Flask)

The Flask app **reads predictions from Serial** and exposes them at `/prediction`.  
The front‑end (`index.html`) fetches that JSON every second and updates the page.

1. **Install deps**
   ```bash
   pip install flask pyserial
   ```
2. **Set the serial port** in `app.py`:
   ```python
   SERIAL_PORT = "COM13"   # Windows example; Linux/Mac: "/dev/ttyACM0" or similar
   BAUD_RATE = 115200
   ```
3. **Point Flask to your template**
   - Either move `index.html` to `templates/index.html`, **or**
   - Initialize Flask with a custom template folder in `app.py`:
     ```python
     app = Flask(__name__, template_folder=".")
     ```
4. **Run**
   ```bash
   python app.py
   ```
   Visit **http://localhost:5000** — the dashboard will display `Gyaku Zuki`, `Kisami Zuki`, or `Idle` with matching animation.

---

## 🔌 Data Flow (Local Demo)

1. **Arduino** runs the EI model → produces a label (e.g., `gyaku_zuki`, `kisami_zuki`, `idle`).
2. **Arduino** outputs the label via **Serial** (and/or BLE NUS if implemented).
3. **Flask (`app.py`)** reads the Serial stream, stores the latest label, and serves JSON at **GET `/prediction`**.
4. **Front‑end (`index.html`)** polls `/prediction` every second and:
   - Updates the **strike name**
   - Shows the corresponding **GIF** (`static/gyaku.gif`, `static/kisami.gif`)
   - Plays optional **audio** (ensure the browser allows autoplay or click once to enable sound)

---

## ⚙️ Configuration

- **Serial Port**: Update `SERIAL_PORT` in `app.py` to match your system.
- **Baud Rate**: Use `115200` in both Arduino sketch and `app.py`.
- **Assets**: Keep GIFs (and any audio) in `static/` and reference via `/static/...` in the HTML.

---

## 🧯 Troubleshooting

- **Flask can’t find `index.html`**  
  Move file into `templates/` or set `template_folder="."` in `Flask(...)`.

- **Serial port busy / not found**  
  Close Arduino Serial Monitor; confirm COM/tty path; on Linux add your user to `dialout` group.

- **No predictions**  
  Ensure the sketch prints labels to Serial; confirm baud rate; test with Arduino Serial Monitor.

- **Static assets not loading**  
  Verify files exist under `static/` and your HTML references `/static/...`.

---

## 👥 Team

- **Thilina Chandrasekara (s16013)**
- **Hashani Kulathunga (s16039)**
- **Kalani Keshila (s16041)**
- **Kanishka Wimalasooriya (s16128)**

---

## 🙏 Acknowledgments

- **Edge Impulse** for the embedded ML workflow  
- **Arduino** and the library authors  
- Everyone who helped with data collection & testing

---

## 📜 License

Add your preferred license (e.g., MIT) in a `LICENSE` file.
