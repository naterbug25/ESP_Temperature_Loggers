
# ----------------------------------------

import csv
import json
import os
import smtplib
import ssl
import subprocess
import threading
import time
from datetime import datetime
from email.mime.text import MIMEText
from email.header import Header
import requests

# ============ USER CONFIG ============
NETWORK_PREFIX = "192.168.1."  # <-- change for your LAN 
IP_RANGE = range(2, 254)
SCAN_INTERVAL = 15 * 60  # seconds (30 minutes)
LOG_INTERVAL = 10 * 60   # seconds (10 minutes)

# Email settings (use Yahoo SMTP here, but can be adapted)
SMTP_SERVER = "smtp.mail.yahoo.com"
SMTP_PORT = 465
EMAIL_USER = "nathanhuber2014@yahoo.com" 
EMAIL_PASS = "fkxbdgzsjsobieal"
EMAIL_TO = "4193058328@tmomail.net"
EMAIL_TO = "nathanhuber2014@gmail.com"

JSON_ENDPOINT = "/json"  # must match your ESP8266 endpoint
CSV_BASENAME = "Temperature_DB"
# ====================================


# Global state
devices = {}   # {ip: {"name": str, "status": str, "active": bool}}
lock = threading.Lock()


# ---------- EMAIL SENDER ----------
def send_email(subject, body):
    try:
        msg = MIMEText(body, "plain", "utf-8")
        msg["Subject"] = Header(subject, "utf-8")
        msg["From"] = EMAIL_USER
        msg["To"] = EMAIL_TO

        context = ssl.create_default_context()
        with smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT, context=context) as server:
            server.login(EMAIL_USER, EMAIL_PASS)
            server.sendmail(EMAIL_USER, [EMAIL_TO], msg.as_string())
        print(f"Email sent: {subject}")
    except Exception as e:
        print(f"Email error: {e}")


# ---------- DEVICE DISCOVERY ----------
def ping(ip):
    """Return True if the IP responds to ping."""
    try:
        result = subprocess.run(
            ["ping", "-n", "1", "-w", "300", ip],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return result.returncode == 0
    except Exception:
        return False


def discover_devices():
    """Scan the local network for ESP8266 temperature units."""
    global devices
    print("Scanning network for ESP devices...")
    found = {}

    for i in IP_RANGE:
        ip = f"{NETWORK_PREFIX}{i}"
        try:
            if not ping(ip):
                continue
            response = requests.get(f"http://{ip}{JSON_ENDPOINT}", timeout=2)
            print(f"http://{ip}{JSON_ENDPOINT}")
            if response.status_code == 200:
                data = response.json()
                if "device" in data and "temperature" in data:
                    found[ip] = {
                        "name": data["device"],
                        "status": data.get("status", "ok"),
                        "active": True
                    }
                    print(found[ip])
        except Exception:
            continue

    with lock:
        # Update device list
        for ip, info in found.items():
            devices[ip] = info
        # Mark missing ones as inactive
        for ip in list(devices.keys()):
            if ip not in found:
                devices[ip]["active"] = False

    # Email when new devices are found or previously offline ones come back online
    if found:
        device_list = "\n".join(f"{info['name']} ({ip})" for ip, info in found.items())
        send_email("ESP Network Scan Results", f"Active devices:\n{device_list}")

    print(f"Scan complete. Active devices: {len(found)}")


# ---------- LOGGING ----------
def get_csv_filename():
    now = datetime.now()
    return f"{CSV_BASENAME}_{now.year}{now.month:02d}.csv"

import csv
import os
from datetime import datetime
import requests

def log_temperatures():
    global devices
    filename = get_csv_filename()
    now_str = datetime.now().strftime("%m/%d/%Y %H:%M")

    with lock:
        if not devices:
            print("No devices to log yet.")
            return
        device_names = sorted([info["name"] for info in devices.values()])

    # Prepare the row with default "Disconnected" values
    row = {"DateTime": now_str}
    for name in device_names:
        row[name] = "Disconnected"

    # Fetch data
    for ip, info in devices.items():
        name = info["name"]
        if not info["active"]:
            continue

        try:
            r = requests.get(f"http://{ip}{JSON_ENDPOINT}", timeout=3)
            data = r.json()
            temp = data.get("temperature", "Disconnected")
            row[name] = temp

            # Email if out of range
            if data.get("status") == "out_of_range":
                send_email(
                    f"ALERT: {name} out of range",
                    f"Device {name} ({ip}) reports temperature out of range.\n"
                    f"Temperature: {temp} °F\nTarget: {data.get('target')} °F ± {data.get('tolerance')} °F"
                )

        except Exception:
            row[name] = "Disconnected"
            if info["active"]:
                info["active"] = False
                send_email(f"Device Offline: {name}", f"The device {name} at {ip} is offline.")

    # Make sure columns are ordered consistently
    fieldnames = ["DateTime"] + device_names
    file_exists = os.path.isfile(filename)

    # ✅ Write CSV using commas, quoted fields
    with open(filename, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=fieldnames,
            delimiter=",",
            quotechar='"',
            quoting=csv.QUOTE_ALL
        )
        if not file_exists or os.path.getsize(filename) == 0:
            writer.writeheader()
        writer.writerow(row)

    print(f"✅ Logged data at {now_str}")

# ---------- BACKGROUND LOOPS ----------
def scan_loop():
    while True:
        try:
            discover_devices()
        except Exception as e:
            print(f"Scan error: {e}")
        time.sleep(SCAN_INTERVAL)


def log_loop():
    while True:
        try:
            log_temperatures()
        except Exception as e:
            print(f"Log error: {e}")
        time.sleep(LOG_INTERVAL)


# ---------- MAIN ----------
if __name__ == "__main__":
    print("Starting ESP temperature monitor...")
    discover_devices()  # initial scan

    threading.Thread(target=scan_loop, daemon=True).start()
    threading.Thread(target=log_loop, daemon=True).start()

    # Keep alive
    while True:
        time.sleep(60)
