from datetime import datetime
import paho.mqtt.client as mqtt
import argparse
import struct

arg_parser = argparse.ArgumentParser("mqtt-sub")
arg_parser.add_argument("-address", help="Broker address", default="127.0.0.1")
arg_parser.add_argument("-port", help="Broker port", default="1883")
arg_parser.add_argument("--topic", help="Topic", default="topic/aggregate")
arg_parser.add_argument("--timestamps", help="Show Timestamps", action='store_true')
args = arg_parser.parse_args()

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected")
    client.subscribe("topic/aggregate")

def on_message(client, userdata, msg):
    end_seconds, = struct.unpack("<f", msg.payload[:4])
    seconds_per_value, = struct.unpack("<f", msg.payload[4:8])
    size, = struct.unpack("!I", msg.payload[8:12])
    values = struct.unpack("<" + "f" * size, msg.payload[12:])"
    print(f"{datetime.utcnow().strftime("%H:%M:%S.%f: ") if args.timestamps else ""}{msg.topic}: {end_seconds:.2f} s, {size} value{"s" if size != 1 else ""} (1 value per {(seconds_per_value * 1000):.2f} ms), [{", ".join(f"{value:.2f}" for value in values)}]")

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect(args.address, int(args.port), 60)

mqttc.loop_forever()
