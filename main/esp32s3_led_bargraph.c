/* include */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

/* define */
#define LED_GPIO        2
#define LED_GPIO_PORT   8
#define BUTTON_GPIO     13
#define HIGH            1
#define LOW             0
#define BUTTON_DELAY_MS 10
#define DUTY_MAX        0xFF
#define DUTY_STEP       16
#define FADE_DELAY_MS   100

#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_DUTY_RES     LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY_HZ 5000

char g_led_gpio[LED_GPIO_PORT] = {42, 41, 40, 39, 38, 47, 21, 20};
unsigned char g_duty[DUTY_STEP] = {0, 0, 0, 0, 0, 0, 0, 0,
                           DUTY_MAX>>0, DUTY_MAX>>1, DUTY_MAX>>2, DUTY_MAX>>3, DUTY_MAX>>4, DUTY_MAX>>5, DUTY_MAX>>6, DUTY_MAX>>7};

static const char *TAG = "pwm_fade";

/* Prototype */
void app_main(void);
static void pwm_init(void);
static void button_init(void);

/* Function */
void app_main(void)
{
    int button_raw = HIGH;
    int button_raw_prev = HIGH;
    int button_stable = HIGH;
    int running = 0;
    int duty_step = 0;
    int fade_step = 0;
    int i = 0;

    TickType_t last_change = xTaskGetTickCount();

    pwm_init();
    button_init();

    /* 無限ループ */
    while(1)
    {   
        /* ボタン入力の読み取り */
        button_raw = gpio_get_level(BUTTON_GPIO);

        /* 生の入力が変化したら安定時間の計測をやり直す */
        if (button_raw != button_raw_prev) {
            button_raw_prev = button_raw;
            last_change = xTaskGetTickCount();
        }

        /* 30ms以上安定した入力だけを採用する */
        if (((xTaskGetTickCount() - last_change) >= pdMS_TO_TICKS(30)) && (button_raw != button_stable)) {
            button_stable = button_raw;

            /* 押下(HIGH -> LOW)で開始・停止を切り替える */
            if (button_stable == LOW) {
                running = !running;
                ESP_LOGI(TAG, "LED animation: %s", running ? "START" : "STOP");

                /* 停止時は全LEDを消灯 */
                if (!running) {
                    for (i = 0; i < LED_GPIO_PORT; i++) {
                        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, i, 0));
                    }

                    for (i = 0; i < LED_GPIO_PORT; i++) {
                        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, i));
                    }
                }
            }
        }

        /* 動作中だけ、残光付きLEDパターンを更新 */
        if (running && (fade_step == 0)) {
            for (i = 0; i < LED_GPIO_PORT; i++) {
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, i, (g_duty[(duty_step + i) % DUTY_STEP])));
            }

            for (i = 0; i < LED_GPIO_PORT; i++) {
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, i));
            }

            duty_step = (duty_step + 1) % DUTY_STEP;
        }

        fade_step = (fade_step + 1) % (FADE_DELAY_MS / BUTTON_DELAY_MS);

        /* ボタン監視周期：約10ms */
        vTaskDelay(pdMS_TO_TICKS(BUTTON_DELAY_MS));
    }
}

static void pwm_init(void)
{
    int i = 0;
    
    /* PWMのタイマー設定 */
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQUENCY_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    /* PWM出力チャンネルの設定 */
    ledc_channel_config_t channel_config[LED_GPIO_PORT] = {};
    for (i = 0; i < LED_GPIO_PORT; i++) {
        (channel_config + i)->gpio_num   = (int)g_led_gpio[i];
        (channel_config + i)->speed_mode = LEDC_MODE;
        (channel_config + i)->channel    = i;
        (channel_config + i)->intr_type  = LEDC_INTR_DISABLE;
        (channel_config + i)->timer_sel  = LEDC_TIMER;
        (channel_config + i)->duty       = 0;
        (channel_config + i)->hpoint     = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&(channel_config[i])));

        ESP_LOGI(TAG, "LEDC initialized: GPIO=%d, freq=%d Hz", g_led_gpio[i], LEDC_FREQUENCY_HZ);
    }
}

static void button_init(void)
{
    gpio_config_t button_cnfig = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&button_cnfig));
}