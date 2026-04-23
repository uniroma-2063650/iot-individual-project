import subprocess
import argparse
import os

arg_parser = argparse.ArgumentParser("create-csr")
arg_parser.add_argument("--csr", help="CSR path", default="client.csr")
arg_parser.add_argument("--broker-cert", help="Broker certificate", default="broker.crt")
arg_parser.add_argument("--broker-key", help="Broker private key", default="broker.key")
arg_parser.add_argument("--out-cert", help="Output certificate path", default="client.crt")
args = arg_parser.parse_args()

subprocess.run([
    "openssl", "x509", "-req",
    "-in", args.csr,
    "-CA", args.broker_cert,
    "-CAkey", args.broker_key,
    "-out", args.out_cert,
    "-days", "365",
    "-CAcreateserial"
], check=True)

print("\nCreated certificate:")
with open(args.out_cert, "r") as f:
    print(f.read())
