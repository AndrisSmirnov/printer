#include "thermal_printer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <ctype.h>


#define TAG "THERMAL_PRINTER"

// Размер буфера UART
#define BUF_SIZE (1024)

// === Вспомогательные функции для управления тайм-аутами ===

// Устанавливает время, после которого можно продолжить отправку данных на принтер
static void Adafruit_Thermal_timeoutSet(Adafruit_Thermal *printer, uint32_t x) {
    if (!printer->dtr_enabled) {
        printer->resume_time = esp_timer_get_time() + x;
        ESP_LOGD(TAG, "Timeout set for %llu us", printer->resume_time);
    }
}

// Ждет, пока принтер не будет готов принять новые данные
static void Adafruit_Thermal_timeoutWait(Adafruit_Thermal *printer) {
    if (printer->dtr_enabled) {
        // Если используется пин DTR, ждем, пока принтер не даст сигнал готовности
        while (gpio_get_level(printer->dtr_pin) == 1) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    } else {
        // Если DTR не используется, ждем до момента resume_time
        while ((int64_t)(esp_timer_get_time() - printer->resume_time) < 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
}

// Отправляет данные на принтер и устанавливает тайм-аут для ожидания
static void Adafruit_Thermal_writeBytes(Adafruit_Thermal *printer, const uint8_t *data, size_t length) {
    // Ждем, пока принтер будет готов принять данные
    Adafruit_Thermal_timeoutWait(printer);

    // Отправляем данные через UART
    uart_write_bytes(printer->uart_num, (const char *)data, length);

    // Логируем отправленные байты
    ESP_LOGD(TAG, "Sent bytes:");
    for (size_t i = 0; i < length; i++) {
        ESP_LOGD(TAG, "0x%02X ", data[i]);
    }

    // Устанавливаем тайм-аут в зависимости от количества отправленных байт
    Adafruit_Thermal_timeoutSet(printer, length * BYTE_TIME);
}

// === Функции инициализации и настройки принтера ===

// Инициализация структуры принтера и настройка UART
void Adafruit_Thermal_init(Adafruit_Thermal *printer, int uart_num, uint8_t tx_pin, int rx_pin, uint8_t dtr_pin) {
    printer->uart_num = uart_num;
    printer->tx_pin = tx_pin;
    printer->rx_pin = rx_pin;
    printer->dtr_pin = dtr_pin;
    printer->dtr_enabled = false;

    // Настройка параметров UART
    uart_config_t uart_config = {
        .baud_rate = BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(printer->uart_num, &uart_config));

    // Установка пинов UART
    ESP_ERROR_CHECK(uart_set_pin(printer->uart_num, printer->tx_pin, printer->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Установка драйвера UART
    ESP_ERROR_CHECK(uart_driver_install(printer->uart_num, BUF_SIZE, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART initialized on UART%d with TX pin %d and RX pin %d", printer->uart_num, printer->tx_pin, printer->rx_pin);

    // Настройка пина DTR, если он используется
    if (printer->dtr_pin != 255) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << printer->dtr_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_LOGI(TAG, "DTR pin configured on GPIO%d", printer->dtr_pin);
    } else {
        ESP_LOGI(TAG, "DTR pin not used");
    }
}


// Начальная настройка принтера после инициализации
void Adafruit_Thermal_begin(Adafruit_Thermal *printer, uint16_t firmware_version) {
    printer->firmware = firmware_version;
    ESP_LOGI(TAG, "Initializing printer with firmware version %d", firmware_version);

    // Устанавливаем тайм-аут на 0.5 секунды для инициализации принтера
    Adafruit_Thermal_timeoutSet(printer, 500000L);

    // Пробуждаем принтер (если он был в режиме сна)
    // Adafruit_Thermal_wake(printer);

    // // Сбрасываем настройки принтера к значениям по умолчанию
    // Adafruit_Thermal_reset(printer);

    // // Настраиваем параметры нагрева печатающей головки
    // Adafruit_Thermal_setHeatConfig(printer, 20, 120, 80);

    uint8_t set_codepage[] = {0x1B, 0x74, 6};  // Устанавливаем кодовую страницу Windows-1251 (ESC t 6)
    Adafruit_Thermal_writeBytes(printer, set_codepage, sizeof(set_codepage));

    uint8_t set_charset[] = {0x1B, 0x52, 6};  // Устанавливаем русский набор символов (ESC R 6)
    Adafruit_Thermal_writeBytes(printer, set_charset, sizeof(set_charset));




    // Если используется DTR, включаем его
    if (printer->dtr_pin < 255) {
        uint8_t data[] = {ASCII_GS, 'a', (1 << 5)};
        Adafruit_Thermal_writeBytes(printer, data, 3);
        printer->dtr_enabled = true;
        ESP_LOGI(TAG, "DTR handshake enabled");
    }

    // Устанавливаем времена печати и подачи бумаги
    printer->dot_print_time = 30000;
    printer->dot_feed_time = 2100;
    printer->max_chunk_height = 255;
}

// === Функции печати ===

// Отправляет одиночный символ на принтер
// Отправляет одиночный символ на принтер
size_t Adafruit_Thermal_write(Adafruit_Thermal *printer, uint8_t c) {
    if (c != '\r') { // Игнорируем символ возврата каретки
        Adafruit_Thermal_timeoutWait(printer);

        // Отправляем символ через UART
        uart_write_bytes(printer->uart_num, (const char *)&c, 1);
        ESP_LOGD(TAG, "Written character: 0x%02X", c);

        uint32_t d = BYTE_TIME;

        // Проверяем, нужно ли перейти на новую строку
        if ((c == '\n') || (printer->column == printer->max_column)) {
            // Рассчитываем время для печати новой строки
            d += (printer->prev_byte == '\n') ?
                ((printer->char_height + printer->line_spacing) * printer->dot_feed_time) :
                ((printer->char_height * printer->dot_print_time) + (printer->line_spacing * printer->dot_feed_time));

            // Сбрасываем колонку
            printer->column = 0;

            // Отправляем также символ возврата каретки (если нужен для принтера)
            uint8_t carriage_return = '\r';
            uart_write_bytes(printer->uart_num, (const char *)&carriage_return, 1);
            ESP_LOGD(TAG, "Sent carriage return 0x%02X", carriage_return);

        } else {
            printer->column++;
        }

        // Устанавливаем тайм-аут
        Adafruit_Thermal_timeoutSet(printer, d);
        printer->prev_byte = c;
    }
    return 1;
}

// Печатает строку целиком
void Adafruit_Thermal_print(Adafruit_Thermal *printer, const char *str) {
    ESP_LOGI(TAG, "Printing string: %s", str);
    size_t len = strlen(str);  // Получаем длину строки
    Adafruit_Thermal_timeoutWait(printer);  // Ждем, пока принтер будет готов
    uart_write_bytes(printer->uart_num, str, len);  // Отправляем всю строку целиком через UART
    Adafruit_Thermal_timeoutSet(printer, len * BYTE_TIME);  // Устанавливаем тайм-аут в зависимости от длины строки
}

// Печатает строку с переводом строки
void Adafruit_Thermal_println(Adafruit_Thermal *printer, const char *str) {
    Adafruit_Thermal_print(printer, str);  // Печатаем строку целиком
    const char newline = '\n';  // Добавляем символ новой строки
    uart_write_bytes(printer->uart_num, &newline, 1);  // Отправляем символ новой строки
    Adafruit_Thermal_timeoutSet(printer, BYTE_TIME);  // Устанавливаем тайм-аут для новой строки
}


// === Функции форматирования текста ===

// Устанавливает размер шрифта
void Adafruit_Thermal_setSize(Adafruit_Thermal *printer, char value) {
    ESP_LOGI(TAG, "Setting size to %c", value);
    switch (toupper((unsigned char)value)) {
        case 'S': // Маленький размер
            printer->print_mode = 0x00;
            printer->char_height = 24;
            printer->max_column = 32;
            break;
        case 'M': // Средний размер (двойная высота)
            printer->print_mode = 0x01;
            printer->char_height = 48;
            printer->max_column = 32;
            break;
        case 'L': // Большой размер (двойная ширина и высота)
            printer->print_mode = 0x11;
            printer->char_height = 48;
            printer->max_column = 16;
            break;
        default:
            printer->print_mode = 0x00;
            printer->char_height = 24;
            printer->max_column = 32;
            break;
    }
    uint8_t data[] = {ASCII_GS, '!', printer->print_mode};
    Adafruit_Thermal_writeBytes(printer, data, 3);
    printer->prev_byte = '\n';
}

// Включает жирный шрифт
void Adafruit_Thermal_boldOn(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Enabling bold text");
    printer->print_mode |= 0x08; // Устанавливаем бит жирного шрифта
    uint8_t data[] = {ASCII_ESC, '!', printer->print_mode};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Выключает жирный шрифт
void Adafruit_Thermal_boldOff(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Disabling bold text");
    printer->print_mode &= ~0x08; // Сбрасываем бит жирного шрифта
    uint8_t data[] = {ASCII_ESC, '!', printer->print_mode};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Включает подчеркивание (можно задать толщину линии)
void Adafruit_Thermal_underlineOn(Adafruit_Thermal *printer, uint8_t weight) {
    ESP_LOGI(TAG, "Enabling underline with weight %d", weight);
    if (weight > 2) weight = 2; // Ограничиваем толщину до 2
    uint8_t data[] = {ASCII_ESC, '-', weight};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Выключает подчеркивание
void Adafruit_Thermal_underlineOff(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Disabling underline");
    uint8_t data[] = {ASCII_ESC, '-', 0};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Включает инверсный режим (белый текст на черном фоне)
void Adafruit_Thermal_inverseOn(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Enabling inverse mode");
    uint8_t data[] = {ASCII_GS, 'B', 1};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Выключает инверсный режим
void Adafruit_Thermal_inverseOff(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Disabling inverse mode");
    uint8_t data[] = {ASCII_GS, 'B', 0};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Устанавливает выравнивание текста (L - влево, C - по центру, R - вправо)
void Adafruit_Thermal_justify(Adafruit_Thermal *printer, char value) {
    ESP_LOGI(TAG, "Setting justification to %c", value);
    uint8_t pos = 0;
    switch (toupper((unsigned char)value)) {
        case 'L':
            pos = 0;
            break;
        case 'C':
            pos = 1;
            break;
        case 'R':
            pos = 2;
            break;
        default:
            pos = 0;
            break;
    }
    uint8_t data[] = {ASCII_ESC, 'a', pos};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// === Функции управления принтером ===

// Подает бумагу на указанное количество строк
void Adafruit_Thermal_feed(Adafruit_Thermal *printer, uint8_t x) {
    ESP_LOGI(TAG, "Feeding %d lines", x);
    if (printer->firmware >= 264) {
        uint8_t data[] = {ASCII_ESC, 'd', x};
        Adafruit_Thermal_writeBytes(printer, data, 3);
        Adafruit_Thermal_timeoutSet(printer, printer->dot_feed_time * printer->char_height);
        printer->prev_byte = '\n';
        printer->column = 0;
    } else {
        while (x--) {
            Adafruit_Thermal_write(printer, '\n');
        }
    }
}

// Сбрасывает принтер к заводским настройкам
void Adafruit_Thermal_reset(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Resetting printer");
    uint8_t data[] = {ASCII_ESC, '@'};
    Adafruit_Thermal_writeBytes(printer, data, 2);
    printer->prev_byte = '\n';
    printer->column = 0;
    printer->max_column = 32;
    printer->char_height = 24;
    printer->line_spacing = 6;
    printer->barcode_height = 50;

    // Настраиваем табуляцию (для принтеров с версией прошивки >= 2.64)
    if (printer->firmware >= 264) {
        uint8_t tab_data[] = {ASCII_ESC, 'D', 4, 8, 12, 16, 20, 24, 28, 0};
        Adafruit_Thermal_writeBytes(printer, tab_data, 10);
        ESP_LOGD(TAG, "Tab stops configured");
    }
}

// Пробуждает принтер из режима сна
void Adafruit_Thermal_wake(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Waking up the printer");
    Adafruit_Thermal_timeoutSet(printer, 10000L);

    // Отправляем специальный байт для пробуждения
    uint8_t wake_data = 255;
    uart_write_bytes(printer->uart_num, (const char *)&wake_data, 1);

    if (printer->firmware >= 264) {
        vTaskDelay(50 / portTICK_PERIOD_MS); // Задержка 50 мс
        uint8_t data[] = {ASCII_ESC, '8', 0, 0}; // Выключаем режим сна
        Adafruit_Thermal_writeBytes(printer, data, 4);
    } else {
        // Для старых принтеров отправляем несколько нулевых байт
        for (uint8_t i = 0; i < 10; i++) {
            uint8_t zero = 0;
            Adafruit_Thermal_writeBytes(printer, &zero, 1);
            Adafruit_Thermal_timeoutSet(printer, 10000L);
        }
    }
}

// Отправляет принтер в режим сна
void Adafruit_Thermal_sleep(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Putting the printer to sleep");
    Adafruit_Thermal_sleepAfter(printer, 1);
}

// Настраивает автоматический переход принтера в режим сна после заданного количества секунд
void Adafruit_Thermal_sleepAfter(Adafruit_Thermal *printer, uint16_t seconds) {
    ESP_LOGI(TAG, "Printer will sleep after %d seconds", seconds);
    if (printer->firmware >= 264) {
        uint8_t data[] = {ASCII_ESC, '8', (uint8_t)(seconds & 0xFF), (uint8_t)(seconds >> 8)};
        Adafruit_Thermal_writeBytes(printer, data, 4);
    } else {
        uint8_t data[] = {ASCII_ESC, '8', (uint8_t)seconds};
        Adafruit_Thermal_writeBytes(printer, data, 3);
    }
}

// Проверяет наличие бумаги в принтере
bool Adafruit_Thermal_hasPaper(Adafruit_Thermal *printer) {
    ESP_LOGI(TAG, "Checking if the printer has paper");

    // Так как TX от принтера не подключен, мы не можем получать данные
    ESP_LOGW(TAG, "Printer TX not connected; cannot check paper status");
    return true; // Предполагаем, что бумага есть
}

// === Вспомогательные функции ===

// Настраивает параметры нагрева печатающей головки
void Adafruit_Thermal_setHeatConfig(Adafruit_Thermal *printer, uint8_t dots, uint8_t time, uint8_t interval) {
    ESP_LOGI(TAG, "Setting heat config: dots=%d, time=%d, interval=%d", dots, time, interval);
    uint8_t data[] = {ASCII_ESC, '7', dots, time, interval};
    Adafruit_Thermal_writeBytes(printer, data, 5);
}

// Настраивает плотность печати
void Adafruit_Thermal_setPrintDensity(Adafruit_Thermal *printer, uint8_t density, uint8_t breakTime) {
    ESP_LOGI(TAG, "Setting print density: density=%d, breakTime=%d", density, breakTime);
    uint8_t data[] = {ASCII_DC2, '#', (uint8_t)((density << 5) | breakTime)};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Устанавливает набор символов
void Adafruit_Thermal_setCharset(Adafruit_Thermal *printer, uint8_t val) {
    ESP_LOGI(TAG, "Setting charset to %d", val);
    if (val > 15) val = 15;
    uint8_t data[] = {ASCII_ESC, 'R', val};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}

// Устанавливает кодовую страницу
void Adafruit_Thermal_setCodePage(Adafruit_Thermal *printer, uint8_t val) {
    ESP_LOGI(TAG, "Setting code page to %d", val);
    if (val > 47) val = 47;
    uint8_t data[] = {ASCII_ESC, 't', val};
    Adafruit_Thermal_writeBytes(printer, data, 3);
}


// Печатает штрих-код на принтере
void Adafruit_Thermal_printBarcode(Adafruit_Thermal *printer, const uint8_t *data, size_t length, uint8_t type) {
    ESP_LOGI(TAG, "Printing barcode");

    // Устанавливаем высоту штрих-кода (если нужно)
    uint8_t height[] = {ASCII_GS, 'h', printer->barcode_height};  // по умолчанию высота установлена
    Adafruit_Thermal_writeBytes(printer, height, 3);

    // Устанавливаем тип штрих-кода
    uint8_t barcode_type[] = {ASCII_GS, 'k', type};  // тип штрих-кода, например CODE39
    Adafruit_Thermal_writeBytes(printer, barcode_type, 3);

    // Отправляем данные штрих-кода
    Adafruit_Thermal_writeBytes(printer, data, length);

    // Добавляем символ завершения, если нужно
    Adafruit_Thermal_write(printer, '\0');  // Завершение штрих-кода
}


// Печатает QR-код на принтере
void Adafruit_Thermal_printQRCode(Adafruit_Thermal *printer, const char *qr_data) {
    ESP_LOGI(TAG, "Printing QR Code");

    uint16_t len = strlen(qr_data) + 3;  // Длина данных + 3 байта заголовка

    // Устанавливаем размер модуля QR-кода (размер точки)
    uint8_t module_size[] = {ASCII_GS, '(', 'k', 0x03, 0x00, 0x31, 0x43, 0x03};  // размер модуля = 3
    Adafruit_Thermal_writeBytes(printer, module_size, 8);

    // Устанавливаем уровень коррекции ошибок
    uint8_t error_correction[] = {ASCII_GS, '(', 'k', 0x03, 0x00, 0x31, 0x45, 0x30};  // коррекция ошибок 48 (L)
    Adafruit_Thermal_writeBytes(printer, error_correction, 8);

    // Устанавливаем длину данных QR-кода
    uint8_t store_data[] = {ASCII_GS, '(', 'k', len & 0xFF, len >> 8, 0x31, 0x50, 0x30};  // начало данных
    Adafruit_Thermal_writeBytes(printer, store_data, 8);

    // Передаем данные QR-кода
    Adafruit_Thermal_writeBytes(printer, (const uint8_t *)qr_data, strlen(qr_data));

    // Печать QR-кода
    uint8_t print_qr[] = {ASCII_GS, '(', 'k', 0x03, 0x00, 0x31, 0x51, 0x30};
    Adafruit_Thermal_writeBytes(printer, print_qr, 8);
}


// Печатает битмап на принтере
void Adafruit_Thermal_printBitmap(Adafruit_Thermal *printer, const uint8_t *bitmap_data, uint16_t width, uint16_t height) {
    // Проверка, что ширина кратна 8
    if (width % 8 != 0) {
        ESP_LOGE(TAG, "Width must be a multiple of 8.");
        return;
    }

    uint16_t row_bytes = width / 8;  // Количество байт на строку

    uint8_t cmd[5];  // Буфер для команды ESC *

    // Проходим по каждой строке
    for (uint16_t y = 0; y < height; y += 24) {  // Печатаем блоками по 24 пикселя в высоту
        uint16_t chunk_height = (height - y > 24) ? 24 : (height - y);  // Определяем высоту блока

        // Команда ESC * m nL nH для начала печати графики
        cmd[0] = ASCII_ESC;
        cmd[1] = '*';
        cmd[2] = 0x00;  // Режим 8 пикселей вертикально
        cmd[3] = row_bytes & 0xFF;  // nL - младший байт ширины
        cmd[4] = (row_bytes >> 8) & 0xFF;  // nH - старший байт ширины

        // Отключаем логирование, чтобы избежать случайных выводов в UART
        esp_log_level_set(TAG, ESP_LOG_NONE);

        // Отправляем команду на принтер
        Adafruit_Thermal_writeBytes(printer, cmd, 5);

        // Печатаем строку за строкой (24 пикселя вертикально)
        for (uint16_t i = 0; i < chunk_height; i++) {
            const uint8_t *row_data = &bitmap_data[(y + i) * row_bytes];
            Adafruit_Thermal_writeBytes(printer, row_data, row_bytes);
        }

        // Включаем логирование обратно после отправки данных
        esp_log_level_set(TAG, ESP_LOG_DEBUG);

        // Добавляем пробел между строками
        Adafruit_Thermal_feed(printer, 1);
    }
}
