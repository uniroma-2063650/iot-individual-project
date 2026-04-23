import { SerialPort } from "serialport";
import * as readline from "node:readline/promises";

const path = process.argv[2] ?? "/dev/tty.usbserial-10";
const baudRate = Number(process.argv[3] ?? 19600);

console.log(`Reading from ${path} at ${baudRate} Hz`);

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
});

const port = new SerialPort(
    {
        path,
        baudRate,
    },
    (error) => {
        if (!error) return;
        console.error("Couldn't open serial port:");
        console.error(error.message);
        process.exit(1);
    }
);

port.on("data", (data) => {
    process.stdout.write(data.toString());
});

rl.on("line", (data) => {
    port.write(data, "utf8", (error) => {
        if (!error) return;
        console.error("Couldn't write to serial port:");
        console.error(error.message);
        process.exit(1);
    });
});

port.on("error", (error) => {
    console.error("Serial port error:");
    console.error(error.message);
    process.exit(1);
});

