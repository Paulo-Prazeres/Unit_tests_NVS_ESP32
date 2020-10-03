#ifndef NVS_BLOB_EXAMPLE_MAIN_H_
#define NVS_BLOB_EXAMPLE_MAIN_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"

void execute_schedule_action();
void process_command(char* jsonCommand);
QueueHandle_t xQueue_BLE_Received_Data;

#endif