#include "NEO_6M.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static uint8_t rx_byte;

static volatile uint8_t rx_buffer[GPS_RX_BUFFER_SIZE];

static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static char line_buffer[GPS_LINE_BUFFER_SIZE];

static uint16_t line_index = 0;

static GPS_Data_t gps_data;

static volatile uint8_t gps_data_updated = 0;

void GPS_Init(UART_HandleTypeDef *huart)
{
    HAL_UART_Receive_IT(huart,
                        &rx_byte,
                        1);
}

void GPS_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        uint16_t next_head;

        next_head =
            (rx_head + 1) % GPS_RX_BUFFER_SIZE;

        if(next_head != rx_tail)
        {
            rx_buffer[rx_head] = rx_byte;

            rx_head = next_head;
        }

        HAL_UART_Receive_IT(huart,
                            &rx_byte,
                            1);
    }
}

void GPS_Process(void)
{
    while(rx_tail != rx_head)
    {
        char c;

        c = rx_buffer[rx_tail];

        rx_tail =
            (rx_tail + 1) % GPS_RX_BUFFER_SIZE;

        if(c == '\n')
        {
            char *fields[20] = {0};

            uint8_t field_count = 0;

            if(line_index > 0 &&
               line_buffer[line_index - 1] == '\r')
            {
                line_index--;
            }

            line_buffer[line_index] = '\0';

            if(strncmp(line_buffer,
                       "$GPGGA",
                       6) == 0)
            {
                fields[field_count++] =
                    line_buffer;

                for(uint16_t i = 0;
                    line_buffer[i] != '\0';
                    i++)
                {
                    if(line_buffer[i] == ',')
                    {
                        line_buffer[i] = '\0';

                        if(field_count < 20)
                        {
                            fields[field_count++] =
                                &line_buffer[i + 1];
                        }
                    }
                }

                strncpy(gps_data.utc_time,
                        fields[1],
                        sizeof(gps_data.utc_time) - 1);

                gps_data.utc_time[
                    sizeof(gps_data.utc_time) - 1] = '\0';

                strncpy(gps_data.latitude_raw,
                        fields[2],
                        sizeof(gps_data.latitude_raw) - 1);

                gps_data.latitude_raw[
                    sizeof(gps_data.latitude_raw) - 1] = '\0';

                gps_data.latitude_direction =
                    fields[3][0];

                strncpy(gps_data.longitude_raw,
                        fields[4],
                        sizeof(gps_data.longitude_raw) - 1);

                gps_data.longitude_raw[
                    sizeof(gps_data.longitude_raw) - 1] = '\0';

                gps_data.longitude_direction =
                    fields[5][0];

                gps_data.fix_quality =
                    atoi(fields[6]);

                gps_data.satellites =
                    atoi(fields[7]);

                strncpy(gps_data.altitude_raw,
                        fields[9],
                        sizeof(gps_data.altitude_raw) - 1);

                gps_data.altitude_raw[
                    sizeof(gps_data.altitude_raw) - 1] = '\0';

                gps_data_updated = 1;
            }

            line_index = 0;
        }
        else
        {
            if(line_index <
               (GPS_LINE_BUFFER_SIZE - 1))
            {
                line_buffer[line_index++] = c;
            }
            else
            {
                line_index = 0;
            }
        }
    }
}

void GPS_GetDataCopy(GPS_Data_t *dest)
{
    memcpy(dest,
           &gps_data,
           sizeof(GPS_Data_t));
}

uint8_t GPS_IsDataUpdated(void)
{
    return gps_data_updated;
}

void GPS_ClearDataUpdatedFlag(void)
{
    gps_data_updated = 0;
}
