# 🔥 Fire Detection & Extinguishing System using RTOS

> **Platform:** LPC1768 (ARM Cortex-M3) | **RTOS:** RTX Kernel (Keil MDK) | **Language:** Embedded C

A real-time, multi-floor fire detection and suppression system built on the **LPC1768 microcontroller** using **RTX RTOS task scheduling and event flags**. Detects fire via IR flame sensors and autonomously triggers buzzer alarms and DC water pumps per floor.

---

## 📌 Project Overview

| Property | Detail |
|---|---|
| Microcontroller | NXP LPC1768 (ARM Cortex-M3, 100 MHz) |
| RTOS | Keil RTX Kernel |
| Sensor | IR Flame Sensor (YG1006 phototransistor, 760–1100 nm) |
| Actuators | 2× Buzzer, 2× DC Pump (via ULN2003 relay driver) |
| Coverage | 2 floors / zones |
| Response time | < 40 µs (optimized code profiling result) |
| Total BOM cost | ₹560 |

---

## 🏗️ System Architecture

```
┌────────────────────────────────────────────────────────┐
│                    RTX RTOS Kernel                     │
│                                                        │
│  job1 (prio 2) ──► Fire Detection (sensing loop)      │
│      │                                                 │
│      ├─── both floors ──► job2 (prio 4)               │
│      ├─── floor 1 only ──► job3 (prio 5)              │
│      ├─── floor 2 only ──► job4 (prio 6)              │
│      └─── no fire      ──► job5 (prio 7)              │
└────────────────────────────────────────────────────────┘
         ▲                        ▼
   Flame Sensors           Buzzers + DC Pumps
   (GPIO inputs)           (GPIO outputs via relay)
```

Tasks communicate exclusively through **RTX event flags** (`os_evt_set` / `os_evt_wait_or`) — no shared memory, no polling delays.

---

## ⚙️ RTOS Design Details

### Tasks

| Task | Priority | Role |
|------|----------|------|
| `job1` | 2 | Reads both flame sensors; dispatches events to response tasks |
| `job2` | 4 | Both floors on fire → activates all buzzers and pumps |
| `job3` | 5 | Floor 1 fire → activates floor 1, deactivates floor 2 |
| `job4` | 6 | Floor 2 fire → activates floor 2, deactivates floor 1 |
| `job5` | 7 | No fire → deactivates all actuators |

### Kernel Objects Used

- **Task Management** (`os_tsk_create`, `os_tsk_prio_self`) — enables true concurrency on the Cortex-M3
- **Event Flags** (`os_evt_set`, `os_evt_wait_or`) — zero-latency inter-task signalling without busy-waiting

### Optimization Techniques

- **Loop Unrolling** — sensor read loop structured to minimize branch overhead
- **Register Allocation** — GPIO helper functions (`controlBuzzer`, `controlRelay`) optimized to keep frequent variables in registers
- **Code Minimization** — bitwise GPIO operations reduce instruction count vs. read-modify-write sequences

---

## 🔌 Hardware Connections (GPIO Port 0)

| Signal | Pin | Direction |
|--------|-----|-----------|
| Buzzer — Floor 1 | P0.27 | Output |
| Relay (DC Pump) — Floor 1 | P0.23 | Output |
| Flame Sensor OUT — Floor 1 | P0.25 | Input |
| Buzzer — Floor 2 | P0.0 | Output |
| Relay (DC Pump) — Floor 2 | P0.1 | Output |
| Flame Sensor OUT — Floor 2 | P0.10 | Input |

> The relay driver IC **ULN2003** is used between the LPC1768 GPIO and the DC pump to handle the higher current load.

---

## 🧪 Test Cases

| Test Case | Sensor 1 | Sensor 2 | Expected Behaviour |
|-----------|----------|----------|--------------------|
| Normal operation | 0 | 0 | All actuators OFF |
| Both floors on fire | 1 | 1 | job2: all buzzers + pumps ON |
| Floor 1 fire only | 1 | 0 | job3: floor 1 ON, floor 2 OFF |
| Floor 2 fire only | 0 | 1 | job4: floor 2 ON, floor 1 OFF |

---

## 📊 Performance Results

| Code Type | Execution Time | Memory Used |
|-----------|---------------|-------------|
| Optimized | 40 µs | 5110 bytes |
| Unoptimized | 45 µs | 5220 bytes |

---

## 🗂️ Repository Structure

```
rtos-fire-detection/
├── src/
│   └── fire_detection.c    # Full annotated source code
├── docs/
│   └── project_report.pdf  # Original KLE Tech project report (Jan 2024)
└── README.md
```

---

## 🛠️ Build Instructions

1. Open **Keil µVision** (MDK-ARM)
2. Create a new project targeting the **LPC1768**
3. Add `src/fire_detection.c` to the project
4. Add the **RTX RTOS** component from the CMSIS pack
5. Build and flash via JTAG/SWD

> **Note:** This project requires the Keil RTX kernel headers (`RTL.h`) and the LPC17xx CMSIS headers (`LPC17xx.h`). Both are bundled with Keil MDK.

---

## 👥 Authors

| Name | Roll No. |
|------|----------|
| Raksha Kulal | 01FE21BEE005 |
| Shrinidhi Shambhoji | 01FE21BEE011 |
| Krithika Patgar | 01FE21BEE017 |
| Parveen Khatal | 01FE21BEE019 |

**Department of Electrical & Electronics Engineering**  
KLE Technological University, Hubballi — January 2024

---

## 📄 License

This project is shared for educational and portfolio purposes.
