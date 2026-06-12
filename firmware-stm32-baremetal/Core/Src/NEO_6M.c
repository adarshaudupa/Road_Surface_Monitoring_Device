#include "NEO_6M.h"
#include "stm32f4xx.h"
#include "uart1.h"
#include "uart2.h"
#include <string.h>
#include <stdlib.h>

static volatile uint8_t  rx_buffer[GPS_RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static char line_buffer[GPS_LINE_BUFFER_SIZE];
static uint16_t line_index = 0;

static GPS_Data_t gps_data;
static volatile uint8_t  gps_data_updated = 0;
static volatile uint32_t gps_overflow_count = 0;
static volatile uint32_t gps_rx_byte_count = 0;

static void GPS_ParseLine(char *line);

void GPS_Init(void)
{
    UART1_Init();
    UART2_SendString("GPS Init\r\n");
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if (sr & ((1U << 3) | (1U << 2) | (1U << 1)))
    {
        (void)USART1->DR;
    }

    while (USART1->SR & (1U << 5))
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFU);
        gps_rx_byte_count++;

        uint16_t next_head = (uint16_t)((rx_head + 1U) % GPS_RX_BUFFER_SIZE);
        if (next_head != rx_tail)
        {
            rx_buffer[rx_head] = byte;
            rx_head = next_head;
        }
        else
        {
            gps_overflow_count++;
        }
    }
}

void GPS_Process(void)
{
    while (rx_tail != rx_head)
    {
        char c = (char)rx_buffer[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % GPS_RX_BUFFER_SIZE);

        if (c == '\n')
        {
            if (line_index > 0 && line_buffer[line_index - 1] == '\r')
                line_index--;

            line_buffer[line_index] = '\0';

            if (line_index > 0)
                GPS_ParseLine(line_buffer);

            line_index = 0;
        }
        else
        {
            if (line_index < (GPS_LINE_BUFFER_SIZE - 1U))
                line_buffer[line_index++] = c;
            else
                line_index = 0;
        }
    }
}

static void GPS_ParseLine(char *line)
{
    if (strncmp(line, "$GPGGA", 6) != 0 && strncmp(line, "$GNGGA", 6) != 0)
        return;

    char *token;
    uint8_t field = 0;

    token = strtok(line, ",");

    while (token != NULL)
    {
        switch (field)
        {
            case 1:
                strncpy(gps_data.utc_time, token, sizeof(gps_data.utc_time) - 1);
                gps_data.utc_time[sizeof(gps_data.utc_time) - 1] = '\0';
                break;

            case 2:
                strncpy(gps_data.latitude_raw, token, sizeof(gps_data.latitude_raw) - 1);
                gps_data.latitude_raw[sizeof(gps_data.latitude_raw) - 1] = '\0';
                break;

            case 3:
                gps_data.latitude_direction = token[0];
                break;

            case 4:
                strncpy(gps_data.longitude_raw, token, sizeof(gps_data.longitude_raw) - 1);
                gps_data.longitude_raw[sizeof(gps_data.longitude_raw) - 1] = '\0';
                break;

            case 5:
                gps_data.longitude_direction = token[0];
                break;

            case 6:
                gps_data.fix_quality = (uint8_t)atoi(token);
                break;

            case 7:
                gps_data.satellites = (uint8_t)atoi(token);
                break;

            case 9:
                strncpy(gps_data.altitude_raw, token, sizeof(gps_data.altitude_raw) - 1);
                gps_data.altitude_raw[sizeof(gps_data.altitude_raw) - 1] = '\0';
                break;
        }

        token = strtok(NULL, ",");
        field++;
    }

    gps_data_updated = 1;
}

void GPS_GetDataCopy(GPS_Data_t *dest)
{
    memcpy(dest, &gps_data, sizeof(GPS_Data_t));
}

uint8_t GPS_IsDataUpdated(void)
{
    return gps_data_updated;
}

void GPS_ClearDataUpdatedFlag(void)
{
    gps_data_updated = 0;
}

uint32_t GPS_GetOverflowCount(void)
{
    return gps_overflow_count;
}

uint32_t GPS_GetRxByteCount(void)
{
    return gps_rx_byte_count;
}
