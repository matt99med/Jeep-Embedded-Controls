#include "gpio_handler.h"
#include "driver/gpio.h"

// ─── Configure all GPIO pins ────────────────────────
void gpio_handler_init(void)
{
    // Configure outputs
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_CHASE_LIGHT) |
                        (1ULL << PIN_POD_LIGHT)   |
                        (1ULL << PIN_SPOT_LIGHT)  |
                        (1ULL << PIN_FOG_LIGHT)   |
                        (1ULL << PIN_LIGHTBAR)    |
                        (1ULL << PIN_IGN_KILL)    |
                        (1ULL << PIN_SIREN)       |
                        (1ULL << PIN_BLINK),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);

    // Configure inputs
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << PIN_SW1)     |
                        (1ULL << PIN_SW2)     |
                        (1ULL << PIN_SW3)     |
                        (1ULL << PIN_SW4)     |
                        (1ULL << PIN_SW5)     |
                        (1ULL << PIN_HOOD_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_conf);

    // Configure SIM7600 control pins as outputs
    gpio_config_t sim_conf = {
        .pin_bit_mask = (1ULL << PIN_SIM_PWRKEY) |
                        (1ULL << PIN_SIM_RESET),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sim_conf);

    // Safe default states — everything off at boot
    gpio_set_level(PIN_CHASE_LIGHT, 0);
    gpio_set_level(PIN_POD_LIGHT,   0);
    gpio_set_level(PIN_SPOT_LIGHT,  0);
    gpio_set_level(PIN_FOG_LIGHT,   0);
    gpio_set_level(PIN_LIGHTBAR,    0);
    gpio_set_level(PIN_IGN_KILL,    1); // 1 = kill switch OFF (engine runs)
    gpio_set_level(PIN_SIREN,       0);
    gpio_set_level(PIN_BLINK,       0);
    gpio_set_level(PIN_SIM_PWRKEY,  0);
    gpio_set_level(PIN_SIM_RESET,   0);
}

// ─── Output setters ─────────────────────────────────
void set_chase_light(bool state) { gpio_set_level(PIN_CHASE_LIGHT, state); }
void set_pod_light(bool state)   { gpio_set_level(PIN_POD_LIGHT,   state); }
void set_spot_light(bool state)  { gpio_set_level(PIN_SPOT_LIGHT,  state); }
void set_fog_light(bool state)   { gpio_set_level(PIN_FOG_LIGHT,   state); }
void set_lightbar(bool state)    { gpio_set_level(PIN_LIGHTBAR,    state); }
void set_siren(bool state)       { gpio_set_level(PIN_SIREN,       state); }
void set_blink(bool state)       { gpio_set_level(PIN_BLINK,       state); }

void set_ign_kill(bool state)
{
    // Inverted logic — 0 = kill engine, 1 = engine runs
    gpio_set_level(PIN_IGN_KILL, !state);
}

// ─── Input getters ──────────────────────────────────
bool get_sw1(void)     { return gpio_get_level(PIN_SW1);     }
bool get_sw2(void)     { return gpio_get_level(PIN_SW2);     }
bool get_sw3(void)     { return gpio_get_level(PIN_SW3);     }
bool get_sw4(void)     { return gpio_get_level(PIN_SW4);     }
bool get_sw5(void)     { return gpio_get_level(PIN_SW5);     }
bool get_hood_sw(void) { return gpio_get_level(PIN_HOOD_SW); }