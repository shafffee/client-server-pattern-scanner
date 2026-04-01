# Malware pattern scanner

A C++17 multi-application project for malware pattern scanning, including a TCP client, a scanning server, and a statistics viewer for Linux.

## Components

- **server** — loads malware patterns from a config file, accepts TCP clients, scans incoming file content, and keeps threat statistics
- **client** — reads a file, sends it to the server, receives the scan result, and prints it
- **stats viewer** — connects to the server through a FIFO-based IPC channel and prints current statistics

## Build

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

---

## How to run

Advice: open **two or three terminal windows**.

### 1. Start the server

From the `build` directory:

```sh
./src/server/server_app ../test_data/config.json 8080
```

The server will:

* load patterns from `config.json`
* start listening on port `8080`
* accept clients
* keep scan statistics
* serve statistics requests through FIFO

### 2. Run the client

In another terminal, from the `build` directory:

```sh
./src/client/client_app ../test_data/clean_hello.txt 8080
```

You can pick some other test files from here or make one yourself:
```sh
../test_data/clean_hello.txt
../test_data/clean_note.txt
../test_data/infected_virus.txt
../test_data/infected_trojan.txt
../test_data/infected_multiple.txt
```

Expected result:

* the client connects to the server
* the server scans the file
* the client prints scan report

### 3. View current statistics

In another terminal, from the `build` directory:

```sh
./src/stats_viewer/stats_viewer_app
```

Expected result:

* the stats viewer connects to the server through FIFO
* it prints the number of checked files
* it prints detected patterns and how many times they were reported

---

## Test data

The repository includes a small `test_data` folder with ready-to-use files:

* one config file with sample malware patterns
* clean files
* infected files with one or more matching patterns

## Notes

* this project targets **Linux**
* config and statistics payloads use **JSON**