/* include */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

/* define */
#define LED_GPIO_PORT       8
#define BUTTON_GPIO         13
#define HIGH                1
#define LOW                 0
#define BUTTON_DELAY_MS     10
#define DUTY_MAX            0xFF
#define DUTY_PATTERN_LENGTH 16
#define FADE_DELAY_MS       100

#define LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_TIMER        LEDC_TIMER_0
#define LEDC_DUTY_RES     LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY_HZ 5000

#define DEBOUNCE_TIME_MS  30

typedef enum {
    BUTTON_EVENT_TOGGLE
} button_event_t;

gpio_num_t g_led_gpio[LED_GPIO_PORT] = {
    GPIO_NUM_42, GPIO_NUM_41, 
    GPIO_NUM_40, GPIO_NUM_39, 
    GPIO_NUM_38, GPIO_NUM_47, 
    GPIO_NUM_21, GPIO_NUM_20
};
unsigned char g_duty[DUTY_PATTERN_LENGTH] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    DUTY_MAX>>0, DUTY_MAX>>1,
    DUTY_MAX>>2, DUTY_MAX>>3, 
    DUTY_MAX>>4, DUTY_MAX>>5, 
    DUTY_MAX>>6, DUTY_MAX>>7
};

static const char *TAG = "pwm_fade";

static QueueHandle_t g_button_event_queue;

/* Prototype */
void app_main(void);
static void button_task(void *pvParameters);
static void led_task(void *pvParameters);
static void pwm_init(void);
static void button_init(void);

/* Function */
void app_main(void)
{
    pwm_init();
    button_init();

    g_button_event_queue = xQueueCreate(4, sizeof(button_event_t));
    configASSERT(g_button_event_queue != NULL);

    BaseType_t result;

    result = xTaskCreate(
        button_task, 
        "button_task", 
        3072, 
        NULL, 
        5, 
        NULL
    );
    configASSERT(result == pdPASS);

    result = xTaskCreate(
        led_task,
        "led_task",
        4096,
        NULL,
        5,
        NULL
    );
    configASSERT(result == pdPASS);
}

static void button_task(void *pvParameters)
{
    int button_raw      = HIGH;
    int button_raw_prev = HIGH;
    int button_stable   = HIGH;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        /* ボタン入力の読み取り */
        button_raw = gpio_get_level(BUTTON_GPIO);

        /* 生の入力が変化したら安定時間の計測をやり直す */
        if (button_raw != button_raw_prev) {
            button_raw_prev = button_raw;
            last_change = xTaskGetTickCount();
        }

        /* 30ms以上安定した入力だけを採用する */
        if (((xTaskGetTickCount() - last_change) >= 
            pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) && 
            (button_raw != button_stable)) {
            
            button_stable = button_raw;

            /* プルアップ入力なので、LOWが押下 */
            if (button_stable == LOW) {
                button_event_t event = BUTTON_EVENT_TOGGLE;

                /* キューが満杯ならイベントを送らず、待たない */
                xQueueSend(g_button_event_queue, &event, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_DELAY_MS));
    }
}

static void led_task(void *pvParameters)
{
    button_event_t event;
    int running     = 0;
    int duty_step   = 0;

    TickType_t last_fade = xTaskGetTickCount();

    while (1) {
        /* ボタンイベントを待つ。最大10msで次の処理へ */
        if (xQueueReceive(
            g_button_event_queue, 
            &event, 
            pdMS_TO_TICKS(BUTTON_DELAY_MS)) == pdTRUE) {
            
            if (event == BUTTON_EVENT_TOGGLE) {
                running = !running;

                ESP_LOGI(TAG, "LEDanimation: %s", 
                        running ? "START" : "STOP");
                
                if (!running) {
                    for (int i = 0; i < LED_GPIO_PORT; i++) {
                        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, i, 0));
                        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, i));
                    }
                }
                else {
                    /* 再開時はすぐに更新できるようにする */
                    last_fade = xTaskGetTickCount();
                }
            }
        }

        /* 100ms周期でアニメーションを更新 */
        TickType_t now = xTaskGetTickCount();
        
        if (running && (now - last_fade >= pdMS_TO_TICKS(FADE_DELAY_MS))) {

            for (int i = 0; i < LED_GPIO_PORT; i++) {
                unsigned int duty = g_duty[(duty_step + i) % DUTY_PATTERN_LENGTH];

                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, i, duty));
            }

            for (int i = 0; i < LED_GPIO_PORT; i++) {
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, i));
            }

            duty_step = (duty_step + 1) % DUTY_PATTERN_LENGTH;
            last_fade = now;
        }
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