from flask import Flask, render_template, jsonify
import threading
import re
import time
import os
import serial  # ✅ Use pyserial instead of open()

SERIAL_PORT = 'COM13'
BAUD_RATE = 115200

app = Flask(__name__)
latest_label = "Waiting for prediction..."
last_sent_time = 0  # ✅ Add timestamp to throttle updates

def read_serial():
    global latest_label, last_sent_time

    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            print(f"✅ Connected to {SERIAL_PORT}")
            current_output = {}

            while True:
                try:
                    line = ser.readline().decode('utf-8').strip()
                    if not line:
                        continue

                    print("Serial:", line)

                    match = re.match(r"(\w+):\s+([\d\.]+)", line)
                    if match:
                        label = match.group(1)
                        value = float(match.group(2))
                        current_output[label] = value

                    if all(label in current_output for label in ["Idle", "gyakuZuki", "kisamiZuki"]):
                        current_time = time.time()
                        if current_time - last_sent_time >= 1:  # ✅ Only update every 1 seconds
                            top_label = max(current_output.items(), key=lambda x: x[1])
                            latest_label = top_label[0]
                            last_sent_time = current_time
                            print("🔮 Predicted:", latest_label)

                        current_output.clear()

                except Exception as e:
                    print("⚠️ Serial read error:", e)
                    time.sleep(0.1)

        except serial.SerialException as e:
            print(f"❌ SerialException: {e}, retrying in 1 second...")
            time.sleep(1)
        except Exception as e:
            print(f"❌ Unknown error: {e}, retrying in 1 second...")
            time.sleep(1)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/prediction')
def prediction():
    return jsonify({'prediction': latest_label})

if __name__ == '__main__':
    if not app.debug or os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        threading.Thread(target=read_serial, daemon=True).start()

    app.run(debug=False, port=5000)
