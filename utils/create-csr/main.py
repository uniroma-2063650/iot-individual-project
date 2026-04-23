import subprocess
import argparse

arg_parser = argparse.ArgumentParser("create-csr")
arg_parser.add_argument("--out-key", help="Created client private key path", default="client.key")
arg_parser.add_argument("--out-csr", help="Created CSR path", default="client.csr")
args = arg_parser.parse_args()

subprocess.run([
    "openssl", "genrsa", "-out", args.out_key
], check=True)

subprocess.run([
    "openssl", "req", "-out", args.out_csr, "-key", args.out_key, "-new"
], check=True)

print("\nCreated CSR:")
with open(args.out_csr, "r") as f:
    print(f.read())
