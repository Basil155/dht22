#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "driver/rmt.h"

#include "dht22.h"

static struct DhtData *_Head = NULL;

static int _RmtChannel = -1;

//static RingbufHandle_t rb;

void PrintAll(void)
{
    struct DhtData *_next = _Head;

    while (_next != NULL)
    {
        printf("Pin: %i\n", _next->Pin);
        _next = _next->next;
    }
    printf("No more pins...\n");
}

static void dhtTask(void *pvParameter);

int DhtInit(int RmtChannel)
{
    /*
    rmt_config_t rmt_rx;

    rmt_rx.channel = RmtChannel;
    //rmt_rx.gpio_num = DHT_RMT_PIN_NO;
    rmt_rx.clk_div = 80;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;

    rmt_rx.rx_config.filter_en = false;
    rmt_rx.rx_config.filter_ticks_thresh = 0;
    rmt_rx.rx_config.idle_threshold = 500;

    rmt_config(&rmt_rx);
    rmt_driver_install(RmtChannel, 1000, 0);
    rmt_get_ringbuf_handle(RmtChannel, &rb);
    */

    _RmtChannel = RmtChannel;

    xTaskCreate(dhtTask, "dht", 4096 * 2, NULL, 0, NULL);

    return 0;
}

int DhtClose(void)
{
    struct DhtData *_next;

    /*
    if (_RmtChannel != -1)
    {
        rmt_driver_uninstall(_RmtChannel);
        _RmtChannel = -1;
    }
    */

    _RmtChannel = -1;

    while (_Head != NULL)
    {
        _next = _Head->next;
        free(_Head);
        _Head = _next;
    }

    return 0;
}

int DhtAdd(int PinNumber)
{
    struct DhtData *_new;

    DhtRemove(PinNumber);

    _new = (struct DhtData *)malloc(sizeof(struct DhtData));
    _new->Pin = PinNumber;
    _new->Humidity10 = -1;
    _new->Temperature10 = -1;
    _new->next = _Head;
    _Head = _new;

    gpio_set_direction(PinNumber, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(PinNumber, 1);

    return 0;
}

int DhtRemove(int PinNumber)
{
    struct DhtData **_curr = &(_Head);
    struct DhtData *_next;

    while (*(_curr) != NULL)
    {
        _next = (*(_curr))->next;
        if ((*(_curr))->Pin == PinNumber)
        {
            free(*(_curr));
            *(_curr) = _next;
        }
        else
        {
            _curr = &((*_curr)->next);
        }
    }

    return 0;
}

double DhtGetHumidity(int PinNumber)
{
    return 0;
}

double DhtGetTemperature(int PinNumber)
{
    return 0;
}

static void dhtTask(void *pvParameter)
{

    (void)pvParameter;

    size_t i;
    size_t rx_size = 0;
    rmt_item32_t *items = NULL;
    int hh, hl, th, tl, par;

    RingbufHandle_t rb;

    rmt_config_t rmt_rx;

    rmt_rx.channel = _RmtChannel;
    //rmt_rx.gpio_num = DHT_RMT_PIN_NO;
    rmt_rx.clk_div = 80;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;

    rmt_rx.rx_config.filter_en = false;
    rmt_rx.rx_config.filter_ticks_thresh = 0;
    rmt_rx.rx_config.idle_threshold = 500;

    rmt_config(&rmt_rx);
    rmt_driver_install(_RmtChannel, 1000, 0);
    rmt_get_ringbuf_handle(_RmtChannel, &rb);

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    while (1)
    {
        struct DhtData *_next = _Head;
        while (_next != NULL)
        {
            gpio_set_direction(_next->Pin, GPIO_MODE_INPUT_OUTPUT_OD);
            gpio_set_level(_next->Pin, 0);

            vTaskDelay(pdMS_TO_TICKS(15));

            gpio_set_level(_next->Pin, 1);

            rmt_set_pin(_RmtChannel, RMT_MODE_RX, _next->Pin);

            rmt_rx_start(_RmtChannel, true);

            vTaskDelay(1000 / portTICK_PERIOD_MS);

            rmt_rx_stop(_RmtChannel);

            // get items, if there are any
            items = (rmt_item32_t *)xRingbufferReceive(rb, &rx_size, 100);
            while (items)
            {

                printf("  Received %i items for pin %i\n", rx_size / 4, _next->Pin);
                if (((rx_size / 4) == 42) && (items[0].level0 == 0) && (items[0].duration0 >= 75) && (items[0].duration0 <= 85) && (items[0].level1 == 1) && (items[0].duration1 >= 75) && (items[0].duration1 <= 85) && (items[41].duration0 >= 45) && (items[41].duration0 <= 55))
                {

                    hh = 0;
                    hl = 0;
                    th = 0;
                    tl = 0;
                    par = 0;

                    for (i = 1; i <= 8; i++)
                    {
                        hh = hh * 2 + (items[i].duration1 > 49 ? 1 : 0);
                    }
                    for (i = 9; i <= 16; i++)
                    {
                        hl = hl * 2 + (items[i].duration1 > 49 ? 1 : 0);
                    }
                    for (i = 17; i <= 24; i++)
                    {
                        th = th * 2 + (items[i].duration1 > 49 ? 1 : 0);
                    }
                    for (i = 25; i <= 32; i++)
                    {
                        tl = tl * 2 + (items[i].duration1 > 49 ? 1 : 0);
                    }
                    for (i = 33; i <= 40; i++)
                    {
                        par = par * 2 + (items[i].duration1 > 49 ? 1 : 0);
                    }

                    if (((hh + hl + th + tl) & 255) == par)
                    {

                        printf("Humidity: %f%%\n", 0.1 * (hh * 256 + hl));
                        printf("Temperature: %f°C\n", 0.1 * ((th & 127) * 256 + tl) * (th > 127 ? -1 : 1));
                        _next->Humidity10 = hh * 256 + hl;
                        _next->Temperature10 = ((th & 127) * 256 + tl) * (th > 127 ? -1 : 1) + 2732;
                    }
                    else
                    {
                        printf("Wrong patity!\n");
                    }
                }
                else
                {
                    printf("Wrong response!\n");
                }

                // free up data space
                vRingbufferReturnItem(rb, (void *)items);

                items = (rmt_item32_t *)xRingbufferReceive(rb, &rx_size, 100);
            }
            _next = _next->next;

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}