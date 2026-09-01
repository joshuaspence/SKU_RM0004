/* vim: set ai et ts=4 sw=4: */
#include "st7735.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include "rpiInfo.h"

int i2cd = -1;

/*
 * Set display coordinates
 */
void lcd_set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    // col address set
    i2c_write_command(X_COORDINATE_REG, x0 + ST7735_XSTART, x1 + ST7735_XSTART);
    // row address set
    i2c_write_command(Y_COORDINATE_REG, y0 + ST7735_YSTART, y1 + ST7735_YSTART);
    // write to RAM
    i2c_write_command(CHAR_DATA_REG, 0x00, 0x00);

    i2c_write_command(SYNC_REG, 0x00, 0x01);
}

/*
 * Display a single character
 */
void lcd_write_char(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
    uint32_t i, b, j;

    lcd_set_address_window(x, y, x + font.width - 1, y + font.height - 1);

    for (i = 0; i < font.height; i++)
    {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++)
        {
            if ((b << j) & 0x8000)
            {
                i2c_write_data(color >> 8, color & 0xFF);
            }
            else
            {
                i2c_write_data(bgcolor >> 8, bgcolor & 0xFF);
            }
        }
    }
}

void lcd_write_ch(uint16_t x, uint16_t y, char ch, FontType font, uint16_t color, uint16_t bgcolor)
{
    switch (font)
    {
    case FontType_7x10:
        lcd_write_char(x, y, ch, Font_7x10, color, bgcolor);
        break;
    case FontType_8x16:
        lcd_write_char(x, y, ch, Font_8x16, color, bgcolor);
        break;
    case FontType_11x18:
        lcd_write_char(x, y, ch, Font_11x18, color, bgcolor);
        break;
    case FontType_16x26:
        lcd_write_char(x, y, ch, Font_16x26, color, bgcolor);
        break;
    }
}

/*
 * display string
 */
void lcd_write_string(uint16_t x, uint16_t y, char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{

    while (*str)
    {
        if (x + font.width >= ST7735_WIDTH)
        {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7735_HEIGHT)
            {
                break;
            }

            if (*str == ' ')
            {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        lcd_write_char(x, y, *str, font, color, bgcolor);
        i2c_write_command(SYNC_REG, 0x00, 0x01);
        x += font.width;
        str++;
    }
}

void lcd_write_str(uint16_t x, uint16_t y, char *str, FontType font, uint16_t color, uint16_t bgcolor)
{
    switch (font)
    {
    case FontType_7x10:
        lcd_write_string(x, y, str, Font_7x10, color, bgcolor);
        break;
    case FontType_8x16:
        lcd_write_string(x, y, str, Font_8x16, color, bgcolor);
        break;
    case FontType_11x18:
        lcd_write_string(x, y, str, Font_11x18, color, bgcolor);
        break;
    case FontType_16x26:
        lcd_write_string(x, y, str, Font_16x26, color, bgcolor);
        break;
    }
}

/*
 * fill rectangle
 */
void lcd_fill_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t buff[320] = {0};
    uint16_t count = 0;
    // clipping
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
        return;
    if ((x + w - 1) >= ST7735_WIDTH)
        w = ST7735_WIDTH - x;
    if ((y + h - 1) >= ST7735_HEIGHT)
        h = ST7735_HEIGHT - y;
    lcd_set_address_window(x, y, x + w - 1, y + h - 1);

    for (count = 0; count < w; count++)
    {
        buff[count * 2] = color >> 8;
        buff[count * 2 + 1] = color & 0xFF;
    }
    for (y = h; y > 0; y--)
    {
        i2c_burst_transfer(buff, sizeof(uint16_t) * w);
    }
}

/*
 * fill screen
 */

void lcd_fill_screen(uint16_t color)
{
    lcd_fill_rectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
    i2c_write_command(SYNC_REG, 0x00, 0x01);
}

void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
    uint16_t col = h - y;
    uint16_t row = w - x;
    lcd_set_address_window(x, y, x + w - 1, y + h - 1);
    i2c_burst_transfer(data, sizeof(uint16_t) * col * row);
}

uint8_t lcd_begin(void)
{
    // I2C Init
    i2cd = open("/dev/i2c-1", O_RDWR);
    if (i2cd < 0)
    {
        fprintf(stderr, "Device I2C-1 failed to initialize\n");
        return 1;
    }
    if (ioctl(i2cd, I2C_SLAVE_FORCE, I2C_ADDRESS) < 0)
    {
        fprintf(stderr, "Device I2C-1 failed to select address 0x%02X\n", I2C_ADDRESS);
        close(i2cd);
        i2cd = -1;
        return 1;
    }
    return 0;
}

/*
 * Release the i2c descriptor. Safe to call whether or not lcd_begin()
 * succeeded, and safe to call more than once.
 */
void lcd_end(void)
{
    if (i2cd >= 0)
    {
        close(i2cd);
        i2cd = -1;
    }
}

void i2c_write_data(uint8_t high, uint8_t low)
{
    uint8_t msg[3] = {WRITE_DATA_REG, high, low};
    write(i2cd, msg, 3);
    usleep(10);
}

void i2c_write_command(uint8_t command, uint8_t high, uint8_t low)
{
    uint8_t msg[3] = {command, high, low};
    write(i2cd, msg, 3);
    usleep(10);
}

void i2c_burst_transfer(uint8_t *buff, uint32_t length)
{
    uint32_t count = 0;
    i2c_write_command(BURST_WRITE_REG, 0x00, 0x01);
    while (length > count)
    {
        if ((length - count) > BURST_MAX_LENGTH)
        {
            write(i2cd, buff + count, BURST_MAX_LENGTH);
            count += BURST_MAX_LENGTH;
        }
        else
        {
            write(i2cd, buff + count, length - count);
            count += (length - count);
        }
        usleep(700);
    }
    i2c_write_command(BURST_WRITE_REG, 0x00, 0x00);
    i2c_write_command(SYNC_REG, 0x00, 0x01);
}

void lcd_display(uint8_t symbol)
{
    switch (symbol)
    {
    case 0:
        lcd_display_cpuLoad();
        break;
    case 1:
        lcd_display_ram();
        break;
    case 2:
        lcd_display_temp();
        break;
    case 3:
        lcd_display_disk();
        break;
    default:
        break;
    }
}

void lcd_display_percentage(uint8_t val, uint16_t color)
{
    uint8_t count = 0;
    uint8_t xCoordinate = 30;
    /* Round up, so any non-zero reading lights at least one segment, but
       leave the bar empty at exactly zero. Adding 10 before dividing, as
       this did previously, lit a segment at 0%. */
    uint8_t bars = (uint8_t)((val + 9) / 10);
    if (bars > 10)
    {
        bars = 10;
    }
    for (count = 0; count < bars; count++)
    {
        lcd_fill_rectangle(xCoordinate, 60, 6, 10, color);
        xCoordinate += 10;
    }
    for (count = 0; count < 10 - bars; count++)
    {
        lcd_fill_rectangle(xCoordinate, 60, 6, 10, ST7735_GRAY);
        xCoordinate += 10;
    }
}

/* The value field is sized for "100", the widest reading any metric
   produces, plus one character for the unit that follows it. */
#define METRIC_VALUE_DIGITS 3
#define METRIC_ROW_Y 35
#define METRIC_ROW_HEIGHT 20

/*
 * Draw one metric row, "LABEL:value unit", with its bar graph beneath.
 *
 * The three x positions are derived from the label rather than tabulated.
 * That derivation is exactly how the four per-metric functions arrived at
 * their hand-written constants: size the row for the label, three digits
 * and a one-character unit, then centre it. Computing it means a label of
 * a different length lands correctly instead of needing a fresh set of
 * magic numbers worked out by hand.
 */
static void lcd_display_metric(char *label, uint8_t value, char *unit,
                               uint8_t barValue, uint16_t color)
{
    char valueStr[8] = {0};
    uint16_t charWidth = Font_11x18.width;
    uint16_t labelLen = (uint16_t)strlen(label);
    uint16_t rowWidth = (uint16_t)((labelLen + METRIC_VALUE_DIGITS + 1) * charWidth);
    uint16_t labelX = 0;
    uint16_t valueX = 0;
    uint16_t unitX = 0;

    if (rowWidth < ST7735_WIDTH)
    {
        labelX = (uint16_t)((ST7735_WIDTH - rowWidth) / 2);
    }
    valueX = (uint16_t)(labelX + labelLen * charWidth);
    unitX = (uint16_t)(valueX + METRIC_VALUE_DIGITS * charWidth);

    snprintf(valueStr, sizeof(valueStr), "%u", value);
    lcd_fill_rectangle(0, METRIC_ROW_Y, ST7735_WIDTH, METRIC_ROW_HEIGHT, ST7735_BLACK);
    lcd_write_string(labelX, METRIC_ROW_Y, label, Font_11x18, ST7735_WHITE, ST7735_BLACK);
    lcd_write_string(valueX, METRIC_ROW_Y, valueStr, Font_11x18, ST7735_WHITE, ST7735_BLACK);
    lcd_write_string(unitX, METRIC_ROW_Y, unit, Font_11x18, ST7735_WHITE, ST7735_BLACK);
    lcd_display_percentage(barValue, color);
}

void lcd_display_cpuLoad(void)
{
    char iPSource[IP_ADDRESS_LENGTH] = {0};
    uint8_t cpuLoad = get_cpu_message();

    lcd_fill_screen(ST7735_BLACK);
    lcd_fill_rectangle(0, 20, ST7735_WIDTH, 5, ST7735_BLUE);
    if (IP_SWITCH == IP_DISPLAY_OPEN)
    {
        lcd_write_string(0, 0, "IP:", Font_8x16, ST7735_WHITE, ST7735_BLACK);
        get_ip_address_new(iPSource, sizeof(iPSource));                               // Get the IP address of the device's wireless network card
        lcd_write_string(24, 0, iPSource, Font_8x16, ST7735_WHITE, ST7735_BLACK); // Send the IP address to the lower machine
    }
    else
    {
        lcd_write_string(0, 0, CUSTOM_DISPLAY, Font_8x16, ST7735_WHITE, ST7735_BLACK); // Send the IP address to the lower machine
    }
    lcd_display_metric("CPU:", cpuLoad, "%", cpuLoad, ST7735_GREEN);
}

void lcd_display_ram(void)
{
    float Totalram = 0.0;
    float freeram = 0.0;
    uint8_t residue = 0;

    get_cpu_memory(&Totalram, &freeram);
    /* get_cpu_memory() leaves the total at zero if /proc/meminfo cannot be
       read. Dividing by it yields NaN, and converting NaN to an integer is
       undefined behaviour rather than a harmless zero. */
    if (Totalram > 0.0)
    {
        residue = (Totalram - freeram) / Totalram * 100;
    }
    lcd_display_metric("RAM:", residue, "%", residue, ST7735_YELLOW);
}

void lcd_display_temp(void)
{
    uint8_t temp = get_temperature();
    uint8_t barValue = temp;

    /* The bar is a 0-100 scale, so a Fahrenheit reading drives it only
       after being converted back. Guarded because a sub-freezing reading
       would otherwise wrap round on an unsigned type. */
    if (TEMPERATURE_TYPE == FAHRENHEIT)
    {
        barValue = (temp > 32) ? (uint8_t)((temp - 32) / 1.8) : 0;
    }
    lcd_display_metric("TEMP:", temp, TEMPERATURE_TYPE == FAHRENHEIT ? "F" : "C",
                       barValue, ST7735_RED);
}

void lcd_display_disk(void)
{
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t availBytes = 0;
    uint64_t denominator = 0;
    uint8_t residue = 0;

    /* Totalled in bytes rather than whole GiB so that rounding each
       filesystem down no longer skews the percentage on small cards. */
    if (get_disk_usage(&totalBytes, &usedBytes, &availBytes) > 0)
    {
        /* df's Use%: used space over the space actually usable, rounded up
           the same way df rounds it, so the two agree. */
        denominator = usedBytes + availBytes;
        if (denominator > 0)
        {
            residue = (uint8_t)((usedBytes * 100 + denominator - 1) / denominator);
        }
    }
    lcd_display_metric("DISK:", residue, "%", residue, ST7735_BLUE);
}
