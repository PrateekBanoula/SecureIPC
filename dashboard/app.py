from flask import Flask, jsonify, render_template, request
import socket
import json
import os
from datetime import datetime

app = Flask(__name__)

SOCKET_PATH = "/tmp/secureipc.sock"
HISTORY_FILE = os.path.join(
    os.path.dirname(__file__), "data", "history.json"
)

def communicate_with_manager(command):
    """Send a request using the existing SecureIPC socket protocol."""

    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.settimeout(180)

    try:
        client.connect(SOCKET_PATH)
        client.sendall((command + "\n").encode())
        client.shutdown(socket.SHUT_WR)

        response = []

        while True:
            chunk = client.recv(4096)
            if not chunk:
                break
            response.append(chunk)

        return b"".join(response).decode(errors="replace")

    finally:
        client.close()


def load_history():
    if not os.path.exists(HISTORY_FILE):
        return []

    try:
        with open(HISTORY_FILE, "r") as file:
            return json.load(file)
    except (json.JSONDecodeError, OSError):
        return []


def save_history(entry):
    history = load_history()
    history.insert(0, entry)

    with open(HISTORY_FILE, "w") as file:
        json.dump(history[:50], file, indent=4)


@app.route("/")
def dashboard():
    return render_template("index.html")


@app.route("/api/status")
def status():
    try:
        response = communicate_with_manager("STATUS")

        return jsonify({
            "connected": "SUCCESS" in response,
            "response": response.strip()
        })

    except (OSError, TimeoutError) as error:
        return jsonify({
            "connected": False,
            "error": str(error)
        }), 503


@app.route("/api/analyze", methods=["POST"])
def analyze():
    started = datetime.now()

    try:
        response = communicate_with_manager("ANALYZE")

        completed = datetime.now()

        success = (
            "SUCCESS: Dataset analysis completed." in response
        )

        entry = {
            "timestamp": started.isoformat(timespec="seconds"),
            "completed_at": completed.isoformat(timespec="seconds"),
            "status": "COMPLETED" if success else "FAILED",
            "response": response.strip()
        }

        save_history(entry)

        return jsonify(entry), 200 if success else 500

    except (OSError, TimeoutError) as error:
        return jsonify({
            "status": "FAILED",
            "error": str(error)
        }), 503



ALLOWED_DATASETS = {
    "data/dataset.csv",
    "data/dataset_1000.csv",
    "data/dataset_5000.csv",
    "data/dataset_10000.csv",
}


@app.route("/api/config", methods=["GET", "POST"])
def configuration():
    if request.method == "GET":
        try:
            response = communicate_with_manager("GETCONFIG").strip()
            parts = response.split()

            if len(parts) != 3 or parts[0] != "CONFIG":
                return jsonify({
                    "success": False,
                    "error": response
                }), 502

            return jsonify({
                "success": True,
                "dataset": parts[1],
                "workers": int(parts[2])
            })

        except (OSError, TimeoutError, ValueError) as error:
            return jsonify({
                "success": False,
                "error": str(error)
            }), 503

    data = request.get_json(silent=True) or {}
    dataset = data.get("dataset")
    workers = data.get("workers")

    if dataset not in ALLOWED_DATASETS:
        return jsonify({
            "success": False,
            "error": "Invalid dataset selection."
        }), 400

    if isinstance(workers, bool) or not isinstance(workers, int) or workers not in (1, 2, 3):
        return jsonify({
            "success": False,
            "error": "Worker count must be 1, 2 or 3."
        }), 400

    try:
        response = communicate_with_manager(
            f"CONFIG {dataset} {workers}"
        ).strip()

        if not response.startswith("SUCCESS: Configuration updated."):
            return jsonify({
                "success": False,
                "error": response
            }), 400

        return jsonify({
            "success": True,
            "dataset": dataset,
            "workers": workers,
            "response": response
        })

    except (OSError, TimeoutError) as error:
        return jsonify({
            "success": False,
            "error": str(error)
        }), 503


@app.route("/api/history")
def history():
    return jsonify(load_history())


if __name__ == "__main__":
    app.run(
        host="127.0.0.1",
        port=5000,
        debug=False,
        threaded=True
    )
