#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_led_init(void);
void ble_led_set(bool on);
