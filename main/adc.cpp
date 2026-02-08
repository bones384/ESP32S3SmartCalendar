//
// Created by mkowa on 08.02.2026.
//

#include "adc.hpp"

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool cali_enabled = false;

void adc_init()
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
            .unit_id = ADC_UNIT_1,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(
            adc_oneshot_config_channel(
                    adc_handle,
                    ADC_CHANNEL_0,   // GPIO1
                    &chan_cfg
            )
    );

    adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .chan = ADC_CHANNEL_0,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle) == ESP_OK) {
        cali_enabled = true;
    }
}

int adc_read_mv()
{
    if(!cali_enabled) adc_init();

    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &raw));

    if (cali_enabled) {
        int mv;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));
        return mv;
    }

    /* Fallback if calibration unavailable */
    return (raw * 3900) / 4095;
}

float battery_voltage()
{
    const int adc_mv = adc_read_mv();
    return (adc_mv/1000.f) * 2.0f;
}