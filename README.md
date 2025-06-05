# 🌐 Computer Networks Projects

[![C](https://img.shields.io/badge/C-Language-blue.svg?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Networks](https://img.shields.io/badge/Computer-Networks-orange.svg?style=for-the-badge)](https://en.wikipedia.org/wiki/Computer_network)
[![FTP](https://img.shields.io/badge/FTP-Protocol-green.svg?style=for-the-badge)](https://en.wikipedia.org/wiki/File_Transfer_Protocol)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 📋 Overview

This repository contains two computer networks projects developed for the Computer Networks (RCOM) course, demonstrating practical implementations of data link layer protocols and application layer protocols.

## 🔧 Technologies

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![TCP/IP](https://img.shields.io/badge/TCP/IP-Protocol-blue?style=for-the-badge)
![FTP](https://img.shields.io/badge/FTP-Protocol-green?style=for-the-badge)

## ✨ Projects

### 📡 TP1: Serial Communication Protocol Implementation

A robust implementation of a data link layer protocol for serial communications featuring:

- **Error Detection**: Byte stuffing and frame validation
- **Flow Control**: Stop-and-wait ARQ protocol implementation
- **Reliable Transfer**: Acknowledgment and retransmission mechanisms
- **Performance Testing**: Tools for measuring throughput and error rates
- **Configurable Parameters**: Frame size, baudrate, and error injection

#### Features
- Split architecture with transmitter and receiver components
- Configurable frame error ratio (FER) for testing
- Variable transmission parameters
- Comprehensive debugging and logging

### 🔄 TP2: FTP Client Implementation

A command-line FTP client implementation featuring:

- **URL Parsing**: Automatic extraction of username, password, host, and path
- **FTP Commands**: Implementation of standard FTP protocol commands
- **Active/Passive Mode**: Support for different connection modes
- **Error Handling**: Robust error detection and recovery
- **File Download**: Complete file retrieval from FTP servers

## 🗂️ Repository Structure

```
.
├── TP1/                         # Serial Communication Protocol
│   ├── src/                     # Source code
│   │   ├── emissor/            # Transmitter implementation
│   │   ├── receptor/           # Receiver implementation
│   │   ├── app_utils.c         # Common utilities for the application
│   │   ├── protocol_utils.c    # Protocol-specific utilities
│   │   └── README.md           # TP1 usage instructions
│   └── relatorio_tp1_t7_g4.pdf # Project report
│
├── TP2/                         # FTP Client Implementation
│   ├── clientTCP.c             # TCP client implementation
│   ├── download.c              # File download functionality
│   ├── getip.c                 # DNS resolution utility
│   └── Makefile                # Build configuration
│
└── README.md                    # Repository overview
```

## 🚀 Installation

### Prerequisites

- C compiler (gcc or clang)
- Linux environment (tested on Ubuntu/Debian)
- Make build system

### Building TP1 (Serial Communication)

1. Navigate to the transmitter or receiver directory:
   ```bash
   cd TP1/src/emissor    # For transmitter
   # OR
   cd TP1/src/receptor   # For receiver
   ```

2. Compile the application:
   ```bash
   make app
   ```

3. For recompilation:
   ```bash
   make again
   ```

### Building TP2 (FTP Client)

1. Navigate to the TP2 directory:
   ```bash
   cd TP2
   ```

2. Compile the application:
   ```bash
   make
   ```

## 📚 Usage

### Running the Serial Communication (TP1)

#### Transmitter:
```bash
./app /dev/ttySx file_name [--frame-size 1000 -B 38400]
```
Parameters:
- `/dev/ttySx`: Serial port device
- `file_name`: File to transmit
- `--frame-size`: Maximum frame size (optional, default 1000)
- `-B`: Baudrate (optional, default 38400)

#### Receiver:
```bash
./app /dev/ttySx [-B 38400 -FER 0 -D 0]
```
Parameters:
- `/dev/ttySx`: Serial port device
- `-B`: Baudrate (optional, default 38400)
- `-FER`: Frame Error Ratio, 0-100% (optional, default 0)
- `-D`: Enable processing delay (optional, default 0)

### Running the FTP Client (TP2)

```bash
./download ftp://[<user>:<password>@]<host>/<url-path>
```

Example:
```bash
./download ftp://anonymous:pass@ftp.up.pt/pub/README
```

## 🔍 Implementation Details

### Serial Communication Protocol
- Implementation of a reliable data transfer protocol over unreliable channels
- Frame structure with FLAG, ADDRESS, CONTROL fields and data payload
- Error detection with byte stuffing and frame validation
- State machine for protocol handling

### FTP Client
- RFC-compliant FTP implementation
- Support for both anonymous and authenticated access
- Passive mode data transfer
- URL parsing and validation
- Robust error handling

## 📊 Performance

- The serial communication protocol achieves reliable data transfer even with injected errors
- Configurable parameters allow optimization for different scenarios
- The FTP client successfully downloads files from standard FTP servers

## 👥 Contributors

- Group Members (T07-G04)

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
