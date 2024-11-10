#ifndef THERMAL_PRINTER_H
#define THERMAL_PRINTER_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/uart.h"
#include "esp_err.h"

// Внутренние наборы символов для команды ESC R n
#define CHARSET_USA            0
#define CHARSET_FRANCE         1
#define CHARSET_GERMANY        2
#define CHARSET_UK             3
#define CHARSET_DENMARK1       4
#define CHARSET_SWEDEN         5
#define CHARSET_ITALY          6
#define CHARSET_SPAIN1         7
#define CHARSET_JAPAN          8
#define CHARSET_NORWAY         9
#define CHARSET_DENMARK2       10
#define CHARSET_SPAIN2         11
#define CHARSET_LATINAMERICA   12
#define CHARSET_KOREA          13
#define CHARSET_SLOVENIA       14
#define CHARSET_CROATIA        14
#define CHARSET_CHINA          15

// Таблицы кодовых страниц для команды ESC t n
#define CODEPAGE_CP437         0
#define CODEPAGE_KATAKANA      1
#define CODEPAGE_CP850         2
#define CODEPAGE_CP860         3
#define CODEPAGE_CP863         4
#define CODEPAGE_CP865         5
#define CODEPAGE_WCP1251       6
#define CODEPAGE_CP866         7
#define CODEPAGE_MIK           8
#define CODEPAGE_CP755         9
#define CODEPAGE_IRAN          10
#define CODEPAGE_CP862         15
#define CODEPAGE_WCP1252       16
#define CODEPAGE_WCP1253       17
#define CODEPAGE_CP852         18
#define CODEPAGE_CP858         19
#define CODEPAGE_IRAN2         20
#define CODEPAGE_LATVIAN       21
#define CODEPAGE_CP864         22
#define CODEPAGE_ISO_8859_1    23
#define CODEPAGE_CP737         24
#define CODEPAGE_WCP1257       25
#define CODEPAGE_THAI          26
#define CODEPAGE_CP720         27
#define CODEPAGE_CP855         28
#define CODEPAGE_CP857         29
#define CODEPAGE_WCP1250       30
#define CODEPAGE_CP775         31
#define CODEPAGE_WCP1254       32
#define CODEPAGE_WCP1255       33
#define CODEPAGE_WCP1256       34
#define CODEPAGE_WCP1258       35
#define CODEPAGE_ISO_8859_2    36
#define CODEPAGE_ISO_8859_3    37
#define CODEPAGE_ISO_8859_4    38
#define CODEPAGE_ISO_8859_5    39
#define CODEPAGE_ISO_8859_6    40
#define CODEPAGE_ISO_8859_7    41
#define CODEPAGE_ISO_8859_8    42
#define CODEPAGE_ISO_8859_9    43
#define CODEPAGE_ISO_8859_15   44
#define CODEPAGE_THAI2         45
#define CODEPAGE_CP856         46
#define CODEPAGE_CP874         47


enum barcodes {
    UPC_A,
    UPC_E,
    EAN13,
    EAN8,
    CODE39,
    ITF,
    CODABAR,
    CODE93,
    CODE128
};

// Определения ASCII кодов
#define ASCII_TAB '\t'
#define ASCII_LF '\n'
#define ASCII_FF '\f'
#define ASCII_CR '\r'
#define ASCII_DC2 18
#define ASCII_ESC 27
#define ASCII_FS 28
#define ASCII_GS 29

#define BAUDRATE 19200
#define BYTE_TIME (((11L * 1000000L) + (BAUDRATE / 2)) / BAUDRATE)

typedef struct {
    int uart_num;           // Номер UART порта
    uint8_t tx_pin;         // Номер пина TX
    uint8_t rx_pin;         // Номер пина RX (если используется)
    uint8_t dtr_pin;        // Номер пина DTR
    bool dtr_enabled;       // Флаг использования DTR
    uint16_t firmware;      // Версия прошивки принтера
    uint8_t prev_byte;      // Предыдущий отправленный байт
    uint8_t print_mode;     // Режим печати
    uint8_t column;         // Текущий столбец
    uint8_t max_column;     // Максимальный столбец
    uint8_t char_height;    // Высота символа
    uint8_t line_spacing;   // Межстрочный интервал
    uint8_t barcode_height; // Высота штрихкода
    uint64_t resume_time;   // Время возобновления печати
    uint32_t dot_print_time;// Время печати точки
    uint32_t dot_feed_time; // Время подачи точки
    uint8_t max_chunk_height; // Максимальная высота блока данных
} Adafruit_Thermal;

// Функции инициализации и управления принтером
void Adafruit_Thermal_init(Adafruit_Thermal *printer, int uart_num, uint8_t tx_pin, int rx_pin, uint8_t dtr_pin);
void Adafruit_Thermal_begin(Adafruit_Thermal *printer, uint16_t firmware_version);

// Функции печати
size_t Adafruit_Thermal_write(Adafruit_Thermal *printer, uint8_t c);
void Adafruit_Thermal_print(Adafruit_Thermal *printer, const char *str);
void Adafruit_Thermal_println(Adafruit_Thermal *printer, const char *str);

// Функции управления форматированием текста
void Adafruit_Thermal_setSize(Adafruit_Thermal *printer, char value);
void Adafruit_Thermal_boldOn(Adafruit_Thermal *printer);
void Adafruit_Thermal_boldOff(Adafruit_Thermal *printer);
void Adafruit_Thermal_underlineOn(Adafruit_Thermal *printer, uint8_t weight);
void Adafruit_Thermal_underlineOff(Adafruit_Thermal *printer);
void Adafruit_Thermal_inverseOn(Adafruit_Thermal *printer);
void Adafruit_Thermal_inverseOff(Adafruit_Thermal *printer);
void Adafruit_Thermal_justify(Adafruit_Thermal *printer, char value);

// Функции управления принтером
void Adafruit_Thermal_feed(Adafruit_Thermal *printer, uint8_t x);
void Adafruit_Thermal_reset(Adafruit_Thermal *printer);
void Adafruit_Thermal_wake(Adafruit_Thermal *printer);
void Adafruit_Thermal_sleep(Adafruit_Thermal *printer);
void Adafruit_Thermal_sleepAfter(Adafruit_Thermal *printer, uint16_t seconds);
bool Adafruit_Thermal_hasPaper(Adafruit_Thermal *printer);

// Вспомогательные функции
void Adafruit_Thermal_setHeatConfig(Adafruit_Thermal *printer, uint8_t dots, uint8_t time, uint8_t interval);
void Adafruit_Thermal_setPrintDensity(Adafruit_Thermal *printer, uint8_t density, uint8_t breakTime);
void Adafruit_Thermal_setCharset(Adafruit_Thermal *printer, uint8_t val);
void Adafruit_Thermal_setCodePage(Adafruit_Thermal *printer, uint8_t val);


void Adafruit_Thermal_printBarcode(Adafruit_Thermal *printer, const uint8_t *data, size_t length, uint8_t type); 

void Adafruit_Thermal_printQRCode(Adafruit_Thermal *printer, const char *qr_data);
// Функции печати изображений и штрих-кодов
void Adafruit_Thermal_printBitmap(Adafruit_Thermal *printer, const uint8_t *bitmap_data, uint16_t width, uint16_t height);

#endif // THERMAL_PRINTER_H