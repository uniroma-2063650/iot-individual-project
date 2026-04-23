import paho.mqtt.client as mqtt
import argparse
import struct

arg_parser = argparse.ArgumentParser("mqtt-sub")
arg_parser.add_argument("-address", help="Broker address", default="127.0.0.1")
arg_parser.add_argument("-port", help="Broker port", default="1883")
arg_parser.add_argument("--topic", help="Topic", default="topic/aggregate")
args = arg_parser.parse_args()

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected")
    client.subscribe("topic/aggregate")

def on_message(client, userdata, msg):
    time, = struct.unpack("<f", msg.payload[:4])
    size, = struct.unpack("!I", msg.payload[4:8])
    values = struct.unpack("<" + "f" * size, msg.payload[8:])
    print(f"{msg.topic}: {time:.2f} s, {size} values, [{", ".join(f"{value:.2f}" for value in values)}]")

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect(args.address, int(args.port), 60)

mqttc.loop_forever()
