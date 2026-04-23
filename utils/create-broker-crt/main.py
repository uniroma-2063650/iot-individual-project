import subprocess
import argparse

arg_parser = argparse.ArgumentParser("create-csr")
arg_parser.add_argument("--out-key", help="Created broker private key path", default="broker.key")
arg_parser.add_argument("--out-cert", help="Created broker certificate path", default="broker.crt")
args = arg_parser.parse_args()

subprocess.run([
    "openssl", "req", "-out", args.out_crt, "-newkey", "rsa:2048", "-keyout", args.out_key, "-x509", "-days", "365"
], check=True)

print("\nCreated CRT:")
with open(args.out_crt, "r") as f:
    print(f.read())
