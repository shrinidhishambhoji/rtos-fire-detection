/**
 * Fire Detection and Extinguishing System using RTOS
 *
 * Platform  : LPC1768 (ARM Cortex-M3)
 * RTOS      : RTX Kernel (Keil MDK)
 * Authors   : Raksha Kulal, Shrinidhi Shambhoji, Krithika Patgar, Parveen Khatal
 * Institute : KLE Technological University — Dept. of Electrical & Electronics Engineering
 * Date      : January 2024
 *
 * Description:
 *   Monitors two flame sensors across two floors of a building.
 *   On fire detection, activates buzzers and DC pump relays per floor.
 *   Uses 5 concurrent RTOS tasks communicating via RTX event flags.
 *
 * Task overview:
 *   job1 — Fire detection (reads both sensors, sets events for other tasks)
 *   job2 — Fire suppression: both floors (activates buzzer + relay for both)
 *   job3 — Fire suppression: floor 1 only
 *   job4 — Fire suppression: floor 2 only
 *   job5 — Deactivation (turns off all buzzers/relays when fire is gone)
 *
 * Hardware connections (LPC1768 GPIO Port 0):
 *   Pin 27 — Buzzer floor 1       (BUZZER_PIN_1)
 *   Pin 23 — Relay / DC pump 1    (RELAY_1)
 *   Pin 25 — Flame sensor 1 OUT   (FLAME_SENSOR_OUT_PIN_1)
 *   Pin  0 — Buzzer floor 2       (BUZZER_PIN_2)
 *   Pin  1 — Relay / DC pump 2    (RELAY_2)
 *   Pin 10 — Flame sensor 2 OUT   (FLAME_SENSOR_OUT_PIN_2)
 */

#include <LPC17xx.h>
#include <RTL.h>
#include <stdint.h>
#include <stdio.h>

/* ── Task handles ─────────────────────────────────────────────────── */
OS_TID tsk1, tsk2, tsk3, tsk4, tsk5;

/* ── RTX result codes ─────────────────────────────────────────────── */
OS_RESULT RE1, RE2, RE3, RE4, RE5;

/* ── GPIO pin definitions ─────────────────────────────────────────── */
#define BUZZER_PIN_1          27
#define RELAY_1               23
#define FLAME_SENSOR_OUT_PIN_1 25

#define BUZZER_PIN_2           0
#define RELAY_2                1
#define FLAME_SENSOR_OUT_PIN_2 10

/* ── Execution counters (for profiling / debug) ───────────────────── */
int cnt1, cnt2, cnt3, cnt4, cnt5, i;

/* ── Sensor value struct ──────────────────────────────────────────── */
typedef struct {
    int sensor1Value;
    int sensor2Value;
} FlameSensorValues;

/* ── Forward declarations ─────────────────────────────────────────── */
FlameSensorValues readBothFlameSensors(void);
__task void job1(void);
__task void job2(void);
__task void job3(void);
__task void job4(void);
__task void job5(void);

/* ================================================================== */
/*  Helper: control a floor's buzzer                                   */
/*  floor=1|2, state=1→ON (active-low), state=0→OFF                   */
/* ================================================================== */
void controlBuzzer(int floor, int state) {
    if (floor == 1) {
        if (!state)
            LPC_GPIO0->FIOSET |= (1 << BUZZER_PIN_1);   /* assert buzzer */
        else
            LPC_GPIO0->FIOCLR |= (1 << BUZZER_PIN_1);   /* deassert      */
    }
    if (floor == 2) {
        if (!state)
            LPC_GPIO0->FIOSET |= (1 << BUZZER_PIN_2);
        else
            LPC_GPIO0->FIOCLR |= (1 << BUZZER_PIN_2);
    }
}

/* ================================================================== */
/*  Helper: control a floor's relay (DC pump)                          */
/* ================================================================== */
void controlRelay(int floor, int state) {
    if (floor == 1) {
        if (state)
            LPC_GPIO0->FIOSET |= (1 << RELAY_1);
        else
            LPC_GPIO0->FIOCLR |= (1 << RELAY_1);
    }
    if (floor == 2) {
        if (state)
            LPC_GPIO0->FIOSET |= (1 << RELAY_2);
        else
            LPC_GPIO0->FIOCLR |= (1 << RELAY_2);
    }
}

/* ================================================================== */
/*  Helper: read digital output of both flame sensors                  */
/* ================================================================== */
FlameSensorValues readBothFlameSensors(void) {
    FlameSensorValues values;
    /* Read from sensor 1 */
    values.sensor1Value = (LPC_GPIO0->FIOPIN >> FLAME_SENSOR_OUT_PIN_1) & 1;
    /* Read from sensor 2 */
    values.sensor2Value = (LPC_GPIO0->FIOPIN >> FLAME_SENSOR_OUT_PIN_2) & 1;
    return values;
}

/* ================================================================== */
/*  job1 — Fire Detection (highest-priority sensing loop)             */
/*  Priority 2 | Spawns and signals jobs 2-5 via event flags          */
/* ================================================================== */
__task void job1(void) {
    os_tsk_prio_self(2);
    tsk1 = os_tsk_self();

    /* Create response tasks at lower priorities */
    os_tsk_create(job2, 1); /* fire suppression: both floors */
    os_tsk_create(job3, 1); /* fire suppression: floor 1     */
    os_tsk_create(job4, 1); /* fire suppression: floor 2     */
    os_tsk_create(job5, 1); /* deactivation task             */

    while (1) {
        /* Read digital values from both flame sensors */
        FlameSensorValues flameValues = readBothFlameSensors();

        if (flameValues.sensor1Value == 1 && flameValues.sensor2Value == 1) {
            /* Fire in both floors → signal job2 */
            os_evt_set(0x0001, tsk1);
        } else if (flameValues.sensor1Value == 1) {
            /* Fire in floor 1 only → signal job3 */
            os_evt_set(0x0001, tsk3);
        } else if (flameValues.sensor2Value == 1) {
            /* Fire in floor 2 only → signal job4 */
            os_evt_set(0x0001, tsk4);
        } else {
            /* No fire detected → signal job5 to turn everything off */
            os_evt_set(0x0001, tsk5);
        }

        cnt1++;
        RE1 = os_evt_wait_or(0x0001, 0x0001);

        if (RE1 == OS_R_EVT) {
            /* Flame detected in both floors — activate buzzer and relay */
            os_evt_set(0x0001, tsk2);
        } else {
            /* No fire present now — turn off buzzer and relay */
            os_evt_set(0x0000, tsk5);
        }
    }
}

/* ================================================================== */
/*  job2 — Fire Suppression: Both Floors                              */
/* ================================================================== */
__task void job2(void) {
    os_tsk_prio_self(4);
    tsk2 = os_tsk_self();

    while (1) {
        RE2 = os_evt_wait_or(0x0001, 0x0001);

        if (RE2 == OS_R_EVT) {
            cnt2++;
            /* Activate buzzers and relays on both floors */
            controlBuzzer(1, 1);
            controlBuzzer(2, 1);
            controlRelay(1, 1);
            controlRelay(2, 1);
        } else {
            os_evt_set(0x0001, tsk5);
        }
    }
}

/* ================================================================== */
/*  job3 — Fire Suppression: Floor 1 Only                            */
/* ================================================================== */
__task void job3(void) {
    os_tsk_prio_self(5);
    tsk3 = os_tsk_self();

    while (1) {
        RE3 = os_evt_wait_or(0x0001, 0x0001);

        if (RE3 == OS_R_EVT) {
            cnt3++;
            /* Flame detected in sensor 1: activate floor 1, deactivate floor 2 */
            controlBuzzer(1, 1);
            controlRelay(1, 1);
            /* Turn off floor 2 */
            controlBuzzer(2, 0);
            controlRelay(2, 0);
        } else {
            os_evt_set(0x0001, tsk5);
        }
    }
}

/* ================================================================== */
/*  job4 — Fire Suppression: Floor 2 Only                            */
/* ================================================================== */
__task void job4(void) {
    os_tsk_prio_self(6);
    tsk4 = os_tsk_self();

    while (1) {
        RE4 = os_evt_wait_or(0x0001, 0x0001);

        if (RE4 == OS_R_EVT) {
            cnt4++;
            /* Flame detected in sensor 2: activate floor 2, deactivate floor 1 */
            controlBuzzer(2, 1);
            controlRelay(2, 1);
            /* Turn off floor 1 */
            controlBuzzer(1, 0);
            controlRelay(1, 0);
        } else {
            os_evt_set(0x0001, tsk5);
        }
    }
}

/* ================================================================== */
/*  job5 — Deactivation (turn off all buzzers and relays)             */
/* ================================================================== */
__task void job5(void) {
    os_tsk_prio_self(7);
    tsk5 = os_tsk_self();

    while (1) {
        FlameSensorValues flameValues = readBothFlameSensors();
        RE5 = os_evt_wait_or(0x0001, 0x0001);

        if (RE5 == OS_R_EVT) {
            cnt5++;

            if (flameValues.sensor1Value == 0 && flameValues.sensor2Value == 0) {
                /* Flame no longer detected in both sensors — deactivate everything */
                controlBuzzer(1, 0);
                controlBuzzer(2, 0);
                controlRelay(1, 0);
                controlRelay(2, 0);
            } else if (flameValues.sensor1Value == 0) {
                controlBuzzer(1, 0);
                controlRelay(1, 0);
            } else if (flameValues.sensor2Value == 0) {
                controlBuzzer(2, 0);
                controlRelay(2, 0);
            }

            /* Re-evaluate and signal appropriate task */
            if (flameValues.sensor1Value == 1 && flameValues.sensor2Value == 1) {
                os_evt_set(0x001, tsk2);
            } else if (flameValues.sensor1Value == 1) {
                os_evt_set(0x001, tsk3);
            } else if (flameValues.sensor2Value == 1) {
                os_evt_set(0x001, tsk4);
            } else {
                os_evt_set(0x001, tsk5);
            }
        }
    }
}

/* ================================================================== */
/*  main — GPIO initialisation + RTOS boot                            */
/* ================================================================== */
int main(void) {
    cnt1 = 0; cnt2 = 0; cnt3 = 0; cnt4 = 0; cnt5 = 0;

    /* ---- Floor 1: Buzzer (P0.27) ---------------------------------- */
    LPC_PINCON->PINSEL1 &= ~(3 << 22);          /* clear pin-function bits */
    LPC_GPIO0->FIODIR   |=  (1 << BUZZER_PIN_1); /* set as output           */

    /* ---- Floor 1: Relay (P0.23) ----------------------------------- */
    LPC_PINCON->PINSEL1 &= ~(3 << 14);
    LPC_GPIO0->FIODIR   |=  (1 << RELAY_1);

    /* ---- Floor 1: Flame sensor OUT (P0.25) — input ---------------- */
    LPC_PINCON->PINSEL1 &= ~(3 << 18);
    LPC_GPIO0->FIODIR   &= ~(1 << FLAME_SENSOR_OUT_PIN_1);

    /* ---- Floor 2: Buzzer (P0.0) ----------------------------------- */
    LPC_PINCON->PINSEL0 &= ~(3 << 0);           /* clear pin-function bits */
    LPC_GPIO0->FIODIR   |=  (1 << BUZZER_PIN_2);

    /* ---- Floor 2: Relay (P0.1) ------------------------------------ */
    LPC_PINCON->PINSEL0 &= ~(3 << 2);
    LPC_GPIO0->FIODIR   |=  (1 << RELAY_2);

    /* ---- Floor 2: Flame sensor OUT (P0.10) — input ---------------- */
    LPC_PINCON->PINSEL0 &= ~(3 << 20);
    LPC_GPIO0->FIODIR   &= ~(1 << FLAME_SENSOR_OUT_PIN_2);

    /* ---- Start RTX kernel, boot with job1 as the initial task ----- */
    os_sys_init(job1);

    while (1);  /* control never reaches here — RTOS takes over */
}
