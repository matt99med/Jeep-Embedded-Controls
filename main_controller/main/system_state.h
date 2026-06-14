/*
 * system_state.h - shared system state for the main controller
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATE_DISARMED = 0,
    STATE_ARMED,
    STATE_ALARM,        /* triggered while armed */
} arm_state_t;

typedef struct {
    arm_state_t arm_state;
    bool   vehicle_present;     /* VEH_SENSE above VEH_PRESENT_V          */
    bool   charging;            /* charge relay currently closed          */
    float  veh_voltage;         /* last vehicle-sense reading (V)         */
    float  batt_voltage;        /* last aux-battery reading (V)           */
    bool   batt_low;
    bool   batt_critical;
    uint32_t last_activity_ms;  /* for active-window / sleep decisions    */
    uint8_t  last_seq;          /* last ESP-NOW sequence accepted         */
} system_state_t;

/* The single global state instance lives in main.c */
extern system_state_t g_state;

/* High-level actions (implemented in gpio_handler.c) */
void sys_set_armed(bool armed);
void sys_trigger_alarm(const char *reason);
void sys_clear_alarm(void);

/* Charge-relay decision, called each wake / periodically */
void sys_update_charge_control(void);

/* Battery state evaluation from the latest ADC reads */
void sys_update_battery_status(void);