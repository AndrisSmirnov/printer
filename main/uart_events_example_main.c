#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <string.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include <ctype.h>

#include "thermal_printer.h"

static const char *TAG = "uart_events";


size_t utf8_to_cp1251(const char* utf8_str, char* cp1251_str, size_t cp1251_size) {
    size_t utf8_len = strlen(utf8_str);
    size_t cp1251_len = 0;
    size_t i = 0;

    while (i < utf8_len && cp1251_len < cp1251_size - 1) {
        uint8_t c = utf8_str[i];
        if (c < 0x80) {
            // ASCII символ
            cp1251_str[cp1251_len++] = c;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // Двухбайтовый символ
            if (i + 1 >= utf8_len) break;
            uint16_t unicode_char = ((c & 0x1F) << 6) | (utf8_str[i + 1] & 0x3F);

            // Преобразование Unicode в CP1251
            if (unicode_char >= 0x410 && unicode_char <= 0x44F) {
                // А-я
                cp1251_str[cp1251_len++] = unicode_char - 0x350;
            } else if (unicode_char == 0x401) {
                // Ё
                cp1251_str[cp1251_len++] = 0xA8;
            } else if (unicode_char == 0x451) {
                // ё
                cp1251_str[cp1251_len++] = 0xB8;
            } else {
                // Неизвестный символ
                cp1251_str[cp1251_len++] = '?';
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // Трехбайтовый символ (не используем для кириллицы в CP1251)
            i += 3;
        } else {
            // Неизвестный символ
            cp1251_str[cp1251_len++] = '?';
            i++;
        }
    }
    cp1251_str[cp1251_len] = '\0';
    return cp1251_len;
}

// #include "Adafruit_Thermal.h"
#include "thermal_printer.h"

#include "thermal_printer.h"

// #include "printer.h"
#include "esp_log.h"
#include <string.h>

// static const char* TAG = "PRINTER";

#define PRINTER_UART_NUM UART_NUM_1
#define PRINTER_TX_PIN 18
#define PRINTER_RX_PIN 19
#define PRINTER_BAUD_RATE 19200
#define PRINTER_BUF_SIZE 1024

// Инициализация UART для подключения к принтеру
void printer_init() {
    uart_config_t uart_config = {
        .baud_rate = PRINTER_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(PRINTER_UART_NUM, PRINTER_BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(PRINTER_UART_NUM, &uart_config);
    uart_set_pin(PRINTER_UART_NUM, PRINTER_TX_PIN, PRINTER_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "Printer UART initialized");
}

// Отправка данных на принтер
static void printer_send_command(const char* command, size_t length) {
    uart_write_bytes(PRINTER_UART_NUM, command, length);
}

// Инициализация принтера (сброс)
void printer_reset() {
    const char reset_command[] = {0x1B, 0x40};  // ESC @
    printer_send_command(reset_command, sizeof(reset_command));
    ESP_LOGI(TAG, "Printer reset command sent");
}

// Печать текста
void printer_print_text(const char* text) {
    printer_send_command(text, strlen(text));
    ESP_LOGI(TAG, "Text sent to printer: %s", text);
}

// Установка межстрочного интервала
void printer_set_line_spacing(uint8_t spacing) {
    const char line_spacing_command[] = {0x1B, 0x33, spacing};  // ESC 3 n
    printer_send_command(line_spacing_command, sizeof(line_spacing_command));
    ESP_LOGI(TAG, "Line spacing set to: %d", spacing);
}

// Подать линию бумаги
void printer_feed_line() {
    const char feed_command[] = {0x0A};  // LF (Line Feed)
    printer_send_command(feed_command, sizeof(feed_command));
    ESP_LOGI(TAG, "Line feed command sent");
}

// Печать тестовой страницы
void printer_print_test_page() {
    const char test_command[] = {0x12, 0x54};  // DC2 T (тестовая страница)
    printer_send_command(test_command, sizeof(test_command));
    ESP_LOGI(TAG, "Test page print command sent");
}
// void app_main() {
//     printer_init();              // Инициализация принтера
//     printer_reset();             // Сброс настроек принтера
//     printer_set_line_spacing(60); // Установка межстрочного интервала
//     printer_print_text("Hello, World!\n");  // Печать текста
//     printer_feed_line();         // Подать линию бумаги
//     printer_print_test_page();   // Печать тестовой страницы
// }

void print_receipt(Adafruit_Thermal *printer) {
    // Инициализация принтера
    Adafruit_Thermal_reset(printer);
    
    // Печать заголовка и логотипа
    Adafruit_Thermal_justify(printer, 'C');  // Выравнивание по центру
    Adafruit_Thermal_println(printer, "EcoVend");
    
    // Печать данных о купоне
    Adafruit_Thermal_justify(printer, 'L');  // Выравнивание по левому краю
    Adafruit_Thermal_println(printer, "Coupon: RECYCLING TIME");
    Adafruit_Thermal_println(printer, "Date: 19-05-2021 12:40");
    Adafruit_Thermal_println(printer, "Machine ID: 1621399214145");
    Adafruit_Thermal_println(printer, "Voucher No: 1621399214145");

    // Печать списка товаров
    Adafruit_Thermal_println(printer, "--------------------------------");
    Adafruit_Thermal_println(printer, "Item          Amount  Sub Total");
    Adafruit_Thermal_println(printer, "Voda 350ml     x2          6");
    Adafruit_Thermal_println(printer, "Voda 550ml     x5         10");
    Adafruit_Thermal_println(printer, "--------------------------------");
    
    // Печать итоговой суммы
    Adafruit_Thermal_println(printer, "Total привет: 8");
    
    // Печать штрих-кода
    Adafruit_Thermal_println(printer, "Barcode:");
    uint8_t barcode_data[] = "0123456789";
    Adafruit_Thermal_printBarcode(printer, barcode_data, sizeof(barcode_data), CODE39);

    // Печать QR-кода (зависит от модели принтера)
    Adafruit_Thermal_println(printer, "QR CODE:");
    Adafruit_Thermal_printQRCode(printer, "https://ecovend.co.uk");

    // Adafruit_Thermal_printBitmap(&printer, test_bitmap, 24, 24);

    
    // Завершение печати
    Adafruit_Thermal_feed(printer, 3);
    Adafruit_Thermal_sleep(printer);
}


void app_main() {
    // Устанавливаем уровень логирования на DEBUG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    Adafruit_Thermal printer;
    // Инициализация принтера: UART1, TX пин 18, RX пин не используется, DTR не используется
    Adafruit_Thermal_init(&printer, UART_NUM_1, GPIO_NUM_18, 19, 255);


    //     // Минимальная команда для теста
    // uint8_t commands[] = {
    //     0x1B, 0x40,                           // ESC @ - инициализация принтера
    //     'T', 'e', 's', 't', ' ', 'P', 'r', 'i', 'n', 't',  // "Test Print"
    //     0x0A,                                 // LF - перевод строки
    //     0x1B, 0x64, 0x02,                      // ESC d 2 - пропуск двух строк
    //     // 0x12, 0x54,                           // ESC @ - инициализация принтера
    // };

    // // Отправляем команды через UART
    // uart_write_bytes(printer.uart_num, (const char *)commands, sizeof(commands));
    // // Начальная настройка принтера с версией прошивки 268
    Adafruit_Thermal_begin(&printer, 268);

    // // // Отправляем простую строку напрямую
    // // const char *test_str = "Hello, Printer!\n";
    // // uart_write_bytes(printer.uart_num, test_str, strlen(test_str));
    print_receipt(&printer);

    // Adafruit_Thermal_printBitmap(&printer, test_bitmap, 128, 64);


    char utf8_text[] = "Привет, мир!";  // Текст на кириллице в кодировке UTF-8
    char cp1251_text[100];              // Буфер для текста в CP1251

    // Преобразование UTF-8 в CP1251
    utf8_to_cp1251(utf8_text, cp1251_text, sizeof(cp1251_text));

    // Отправляем преобразованный текст на принтер
    Adafruit_Thermal_println(&printer, cp1251_text);



    // // Добавляем задержку
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // // Пробуем снова напечатать текст через библиотеку
    // Adafruit_Thermal_println(&printer, "Test Print");

    // // Подаем бумагу на 2 строки
    // Adafruit_Thermal_feed(&printer, 2);

    // // Отправляем принтер в режим сна
    // Adafruit_Thermal_sleep(&printer);
}
