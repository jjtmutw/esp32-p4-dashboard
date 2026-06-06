#!/usr/bin/env python3
import argparse
import json
import uuid
from pathlib import Path

import paho.mqtt.client as mqtt


REQUEST_TOPIC = "jj/dashboard/config/request"
RESPONSE_PREFIX = "jj/dashboard/config"
REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG_DIR = REPO_ROOT / "config" / "devices"


def resolve_config_dir(config_dir: str | None) -> Path:
    if not config_dir:
        return DEFAULT_CONFIG_DIR

    path = Path(config_dir)
    if path.is_absolute():
        return path

    cwd_path = path.resolve()
    if cwd_path.exists():
        return cwd_path

    return (REPO_ROOT / path).resolve()


def load_device_config(config_dir: Path, device_id: str) -> dict | None:
    path = config_dir / f"{device_id}.json"
    if not path.exists():
        return None
    with path.open("r", encoding="utf-8") as f:
        config = json.load(f)
    config["device_id"] = device_id
    return config


def mqtt_connect_succeeded(reason_code) -> bool:
    is_failure = getattr(reason_code, "is_failure", None)
    if is_failure is not None:
        return not is_failure
    try:
        return int(reason_code) == 0
    except (TypeError, ValueError):
        return str(reason_code).lower() == "success"


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve ESP32 dashboard config.json over MQTT.")
    parser.add_argument("--broker", default="broker.emqx.io")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--config-dir", default=None)
    parser.add_argument("--client-id", default=None)
    args = parser.parse_args()

    config_dir = resolve_config_dir(args.config_dir)
    client_id = args.client_id or f"jj-dashboard-config-provider-{uuid.uuid4().hex[:8]}"
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=client_id,
        protocol=mqtt.MQTTv311,
    )

    def on_connect(client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
        print(f"connected to {args.broker}:{args.port}: {reason_code}")
        print(f"client id: {client_id}")
        print(f"config dir: {config_dir}")
        if not mqtt_connect_succeeded(reason_code):
            print("broker rejected this connection; check broker auth or choose another client id")
            return
        client.subscribe(REQUEST_TOPIC, qos=1)
        print(f"listening: {REQUEST_TOPIC}")

    def on_message(client: mqtt.Client, userdata, msg: mqtt.MQTTMessage) -> None:
        try:
            request = json.loads(msg.payload.decode("utf-8"))
        except json.JSONDecodeError:
            print("ignored invalid JSON request")
            return

        device_id = request.get("device_id")
        current_version = int(request.get("version", 0) or 0)
        force = bool(request.get("force"))
        if not isinstance(device_id, str) or len(device_id) != 8:
            print(f"ignored request with invalid device_id: {device_id!r}")
            return

        config = load_device_config(config_dir, device_id)
        if config is None:
            print(f"no config file for {device_id} in {config_dir}")
            return

        config_version = int(config.get("version", 1) or 1)
        if config_version <= current_version and not force:
            print(f"{device_id} already has version {current_version}; latest is {config_version}")
            return

        topic = f"{RESPONSE_PREFIX}/{device_id}"
        payload = json.dumps(config, ensure_ascii=False, separators=(",", ":"))
        client.publish(topic, payload, qos=1, retain=False)
        reason = "forced " if force else ""
        print(f"published {reason}{device_id} config version {config_version} to {topic}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.broker, args.port, keepalive=60)
    client.loop_forever()


if __name__ == "__main__":
    main()
