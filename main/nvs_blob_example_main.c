/* Non-Volatile Storage (NVS) Read and Write a Blob - Example

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "string.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "nvs_blob_example_main.h"
#include "BLE_functions_mair.h"

#define STORAGE_NAMESPACE "Storage"
#define SCHEDULES_STORAGE_NAMESPACE "schedList"
#define LOADS_STORAGE_NAMESPACE "loadList"

uint8_t numberOfLoadsGlobal;

typedef struct events_dates_and_reps
{
    uint8_t repetions;
    uint64_t date;
}Date_and_Reps;

typedef struct events_of_load
{
  char loadName[20];
  uint8_t pinNumber;
  Date_and_Reps* eventsON;
  Date_and_Reps* eventsOFF;
  uint8_t numOfEventsON;
  uint8_t numOfEventsOFF;

} LoadEvent;

void print_nvs_stats(char* partitionName)
{
    nvs_stats_t nvs_stats;
    nvs_get_stats(partitionName, &nvs_stats);
    printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d), NameSpacesCount = %d\n",
    nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries, nvs_stats.namespace_count);
}

/* Save new run time value in NVS
   by first reading a table of previously saved values
   and then adding the new value at the end of the table.
   Return an error if anything goes wrong
   during this process.
 */

esp_err_t save_schedule_time(char* loadName, int loadState, int schedTime, int repetions)
{
    uint64_t scheduleTime = schedTime;
    uint8_t reps = repetions;
    char loadKey[16];
    memset(loadKey, 0, sizeof(loadKey));
    if(loadState == 0) sprintf(loadKey, "%sOFF",loadName);
    else sprintf(loadKey, "%sON",loadName);
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open_from_partition("MyNvs", SCHEDULES_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, loadKey, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Read previously saved blob if available
    Date_and_Reps* loadSchedList = malloc(required_size + sizeof(Date_and_Reps));
    if (required_size > 0) {
        err = nvs_get_blob(my_handle, loadKey, loadSchedList, &required_size);
        if (err != ESP_OK) {
            free(loadSchedList);
            return err;
        }
    }

    // Write value including previously saved blob if available
    required_size += sizeof(Date_and_Reps);
    loadSchedList[required_size / sizeof(Date_and_Reps) - 1].date = scheduleTime;
    loadSchedList[required_size / sizeof(Date_and_Reps) - 1].repetions = reps;
    err = nvs_set_blob(my_handle, loadKey, loadSchedList, required_size);
    free(loadSchedList);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    printf("Date registered successfully!\n");

    print_nvs_stats("MyNvs");    

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

/* Read from NVS and print restart counter
   and the table with run times.
   Return an error if anything goes wrong
   during this process.
 */
esp_err_t print_load_sched_list(char* loadName)
{
    char loadKey[16];    
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open_from_partition("MyNvs", SCHEDULES_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    for(int x = 0; x < 2; x++)
    {
        memset(loadKey, 0, sizeof(loadKey));
        if(x == 0) sprintf(loadKey, "%sON",loadName);
        else sprintf(loadKey, "%sOFF",loadName);
        // Read run time blob for Load Name ON/OFF
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        // obtain required memory space to store blob being read from NVS
        err = nvs_get_blob(my_handle, loadKey, NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        printf("Schedulements for %s:\n",loadKey);
        if (required_size == 0) {
            printf("Nothing saved yet!\n");
        } else {
            Date_and_Reps* loadSchedList = malloc(required_size);
            err = nvs_get_blob(my_handle, loadKey, loadSchedList, &required_size);
            if (err != ESP_OK) {
                free(loadSchedList);
                return err;
            }
            for (int i = 0; i < required_size / sizeof(Date_and_Reps); i++) {
                printf("Sched %d: Date: %lld / Repetitions: %d\n", i + 1, loadSchedList[i].date, loadSchedList[i].repetions);
            }
            free(loadSchedList);
        }
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

uint32_t check_string_parameter(const cJSON *json, char *par)
{
     if (json == NULL)
    {   
        printf ("ERROR: Object \"%s\" not found!\n", par);
        return 0;
    }
    else if(!cJSON_IsString(json) || (json->valuestring == NULL))
    {
        printf("ERROR: Parameter \"%s\" is NOT a STRING: %d\n", par, json->valueint);
        return 0;
    }
    return 1;       
}

uint8_t check_number_parameter(const cJSON *json, char *par)
{
     if (json == NULL)
    {   
        printf ("ERROR: Object \"%s\" not found!\n", par);
        return 0;
    }
    else if(!cJSON_IsNumber(json))
    {
        printf("ERROR: Parameter \"%s\" is NOT a NUMBER: \"%s\"\n", par, json->valuestring);
        return 0;
    }
    return 1;
       
}

void register_new_load(char* loadName, uint8_t pinNumber)
{
    nvs_handle_t my_loadList_handle;
    esp_err_t err = nvs_open_from_partition("MyNvs", LOADS_STORAGE_NAMESPACE, NVS_READWRITE, &my_loadList_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening Load List NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");

        // Write
        printf("Saving Load in NVS ... ");
        err = nvs_set_u8(my_loadList_handle, loadName, pinNumber);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_loadList_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

         print_nvs_stats("MyNvs");

        // Close
        nvs_close(my_loadList_handle);
    }

}

void read_load_list()
{
    nvs_handle_t my_loadList_handle;
    esp_err_t err = nvs_open_from_partition("MyNvs", LOADS_STORAGE_NAMESPACE, NVS_READWRITE, &my_loadList_handle);
    uint8_t loadPinRead;
    if (err != ESP_OK) {
        printf("Error (%s) opening Load List NVS handle!\n", esp_err_to_name(err));
    } else
    {
        printf("Done\n");

        //Listing all key-values inside namespace "loadList"
        // Example of listing all the key-value pairs of any type under specified partition and namespace
        // Note: no need to release iterator obtained from nvs_entry_find function when
        //       nvs_entry_find or nvs_entry_next function return NULL, indicating no other
        //       element for specified criteria was found.               

        nvs_iterator_t it = nvs_entry_find("MyNvs", LOADS_STORAGE_NAMESPACE, NVS_TYPE_ANY);
        while (it != NULL) {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info);
                it = nvs_entry_next(it); 
                printf("key '%s', type '%d' \n", info.key, info.type);               
                if(info.type == NVS_TYPE_U8) 
                {
                    err = nvs_get_u8(my_loadList_handle, info.key, &loadPinRead);
                    switch (err) {
                        case ESP_OK:
                        printf("Pin Number: %d  \n", loadPinRead);
                        break;
                        case ESP_ERR_NVS_NOT_FOUND:
                            printf("The value is not initialized yet!\n");
                        break;
                        default :
                            printf("Error (%s) reading!\n", esp_err_to_name(err));
                    }
                }
                
            };
        
        // Close
        nvs_close(my_loadList_handle);    
    }
    printf("End of read load list function.\n");

}

LoadEvent* return_sched_from_NVS()
{
    LoadEvent* loadEventsArray;
    uint8_t numberOfLoads = 0;
    nvs_handle_t my_loadList_handle;
    nvs_handle_t my_sched_handle;
    uint8_t loadPinRead;
    char auxLoadKey[20];
    esp_err_t err = nvs_open_from_partition("MyNvs", LOADS_STORAGE_NAMESPACE, NVS_READWRITE, &my_loadList_handle);    
    if (err != ESP_OK) printf("Error (%s) opening Load List NVS handle!\n", esp_err_to_name(err));
    else printf("Load List Namespace opened OK!\n");

    err = nvs_open_from_partition("MyNvs", SCHEDULES_STORAGE_NAMESPACE, NVS_READWRITE, &my_sched_handle);
    if (err != ESP_OK) printf("Error (%s) opening Load List NVS handle!\n", esp_err_to_name(err));
    else printf("Schedules Namespace opened OK!\n");

    size_t used_entries;
    size_t total_entries_namespace;
    if(nvs_get_used_entry_count(my_loadList_handle, &used_entries) == ESP_OK){
        // the total number of records occupied by the namespace
        total_entries_namespace = used_entries + 1;
        printf("Used Entries in namespace %s: %d\n", LOADS_STORAGE_NAMESPACE, used_entries);
        printf("Total Entries in namespace %s: %d\n", LOADS_STORAGE_NAMESPACE, total_entries_namespace);
        loadEventsArray = malloc(used_entries*sizeof(LoadEvent));
    }else return NULL;
    nvs_iterator_t it = nvs_entry_find("MyNvs", LOADS_STORAGE_NAMESPACE, NVS_TYPE_U8);
    while (it != NULL) 
    {
        numberOfLoads++;
        //loadEventsArray = realloc(loadEventsArray, numberOfLoads*sizeof(LoadEvent));
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it); 
        printf("key '%s', type '%d' \n", info.key, info.type); 
        err = nvs_get_u8(my_loadList_handle, info.key, &loadPinRead);
        switch (err) {
            case ESP_OK:
            printf("Pin Number: %d  \n", loadPinRead);
            strcpy(loadEventsArray[numberOfLoads-1].loadName, info.key);
            loadEventsArray[numberOfLoads-1].pinNumber = loadPinRead;
                
            for(int x = 0; x < 2; x++)
            {                
                memset(auxLoadKey, 0, sizeof(auxLoadKey));
                if(x == 0) sprintf(auxLoadKey, "%sON",info.key); 
                else sprintf(auxLoadKey, "%sOFF",info.key);     
                // Read run time blob for Load Name ON/OFF
                size_t required_size = 0;  // value will default to 0, if not set yet in NVS
                // obtain required memory space to store blob being read from NVS
                err = nvs_get_blob(my_sched_handle, auxLoadKey, NULL, &required_size);
                if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return NULL;
                printf("Schedulements for %s:\n",auxLoadKey);
                if (required_size == 0) {
                    printf("No %s schedules saved yet!\n",auxLoadKey);
                    if(x == 0) loadEventsArray[numberOfLoads-1].numOfEventsON = 0;
                    else loadEventsArray[numberOfLoads-1].numOfEventsOFF = 0;
                } else {
                    Date_and_Reps* loadSchedList = malloc(required_size);
                    err = nvs_get_blob(my_sched_handle, auxLoadKey, loadSchedList, &required_size);
                    if (err != ESP_OK) {
                        free(loadSchedList);
                        return NULL;
                    }
                    loadEventsArray[numberOfLoads-1].eventsON = loadSchedList;
                    if(x == 0) loadEventsArray[numberOfLoads-1].numOfEventsON = required_size / sizeof(Date_and_Reps);
                    else loadEventsArray[numberOfLoads-1].numOfEventsOFF = required_size / sizeof(Date_and_Reps);
                    for (int i = 0; i < required_size / sizeof(Date_and_Reps); i++) {
                        printf("Schedule %d -> Date: %lld / Repetions: %d\n", i + 1, loadSchedList[i].date, loadSchedList[i].repetions);
                    }
                    //free(loadSchedList);
                }
            }
            
            break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
            break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }        
        
    }
    if(numberOfLoads != 0)
    {
        numberOfLoadsGlobal = numberOfLoads;
        printf("Printing the loadEventsArray Values: \n\n");
        for(int i = 0; i < numberOfLoads;i++)
        {
            printf("Number of Loads to show: %d\n", numberOfLoads);
            printf("Load Name: %s\n", loadEventsArray[i].loadName);
            printf("Load PIN: %d\n", loadEventsArray[i].pinNumber);
            printf("Number of Events ON: %d\n", loadEventsArray[i].numOfEventsON);
            printf("Number of Events OFF: %d\n", loadEventsArray[i].numOfEventsOFF);
            for(int j = 0; j < loadEventsArray[i].numOfEventsON; j++)
            {
                printf("Load Dates ON: %lld / Repetitions: %d \n", loadEventsArray[i].eventsON[j].date, loadEventsArray[i].eventsON[j].repetions);
            }
            for(int j = 0; j < loadEventsArray[i].numOfEventsOFF; j++)
            {
                printf("Load Dates OFF: %lld / Repetitions: %d \n", loadEventsArray[i].eventsOFF[j].date, loadEventsArray[i].eventsOFF[j].repetions);
            }

        }
    }
    else printf("No loads registered on flash.\n");

    print_nvs_stats("MyNvs");

    return loadEventsArray;

}

esp_err_t delete_or_change_sched_from_NVS(char* loadName, uint64_t date, uint8_t repetitions, uint8_t option)
{
    char loadKey[16];    
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open_from_partition("MyNvs", SCHEDULES_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    for(int x = 0; x < 2; x++)
    {
        memset(loadKey, 0, sizeof(loadKey));
        if(x == 0) sprintf(loadKey, "%sON",loadName);
        else sprintf(loadKey, "%sOFF",loadName);
        // Read run time blob for Load Name ON/OFF
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        size_t newRequired_size = 0;
        uint8_t dateFound = 0;
        // obtain required memory space to store blob being read from NVS
        err = nvs_get_blob(my_handle, loadKey, NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        printf("Schedulements for %s:\n",loadKey);
        if (required_size == 0) {
            printf("Nothing saved yet!\n");
        } else {
            Date_and_Reps* loadSchedList = malloc(required_size);
            err = nvs_get_blob(my_handle, loadKey, loadSchedList, &required_size);
            if (err != ESP_OK) {
                free(loadSchedList);
                return err;
            }
            for (int i = 0; i < required_size / sizeof(Date_and_Reps); i++) {              
                if(loadSchedList[i].date == date) 
                {
                    dateFound = 1;
                    if(option == 0)
                    {
                        newRequired_size = required_size - sizeof(Date_and_Reps);
                        printf("Deleting date %lld...\n", loadSchedList[i].date);
                    }
                    else if(option == 1)
                    {
                        newRequired_size = required_size;
                        printf("Changing repetition of date %lld from %d to %d...\n", loadSchedList[i].date, loadSchedList[i].repetions, repetitions);
                    }
                }
            }
            if(dateFound == 1) //Date to be excluded found
            {
                if(newRequired_size == 0)
                {
                    free(loadSchedList);
                    err = nvs_erase_key(my_handle, loadKey);
                    if (err != ESP_OK) return err;
                    // Commit
                    err = nvs_commit(my_handle);
                    if (err != ESP_OK) return err; 

                    printf("Date %lld deleted for load %s successfully. No dates left for that key.\n", date, loadKey);

                    print_nvs_stats("MyNvs");   
                } 
                else
                {
                    Date_and_Reps* newLoadSchedList = malloc(newRequired_size);
                    for(int i = 0, j = 0; i < required_size/sizeof(Date_and_Reps);i++)
                    {
                        if(option == 0)
                        {
                            if(loadSchedList[i].date != date) 
                            {
                                newLoadSchedList[j].date = loadSchedList[i].date;
                                newLoadSchedList[j].repetions = loadSchedList[i].repetions;
                                j++;
                            }
                        }
                        else if(option == 1)
                        {
                            newLoadSchedList[j].date = loadSchedList[i].date;
                            if(loadSchedList[i].date == date) 
                            {
                                newLoadSchedList[j].repetions = repetitions;
                            }
                            else newLoadSchedList[j].repetions = loadSchedList[i].repetions;
                            j++;
                        }
                    }
                    err = nvs_set_blob(my_handle, loadKey, newLoadSchedList, newRequired_size);
                    free(loadSchedList);
                    free(newLoadSchedList);                    

                    if (err != ESP_OK) return err;

                    // Commit
                    err = nvs_commit(my_handle);
                    if (err != ESP_OK) return err; 

                    if(option == 0) printf("Date %lld for load %s deleted successfully.\n", date, loadKey);
                    else if(option == 1) printf("Repetitions %d for Date %lld for load %s changed successfully.\n", repetitions , date, loadKey);

                    print_nvs_stats("MyNvs");
                }                           
            }
            else 
            {
                printf("Date not found on %s list.\n",loadKey);            
                free(loadSchedList);
            }
        }
    }
   
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

void free_load_loads_events_array(LoadEvent* loadsEventsLoaded)
{
    for(int i = 0; i < numberOfLoadsGlobal; i++)
    {
        free(loadsEventsLoaded[i].eventsON);
        free(loadsEventsLoaded[i].eventsOFF);
    }
}
void execute_schedule_action()
{
    LoadEvent* loadsEventsLoaded = return_sched_from_NVS(); //=NULL
    uint64_t time = 0;
    esp_err_t err;
    while(1)
    {
        // if(nvsLoaded == 0)
        // {
        //     if(loadEventsLoaded != NULL) functionToFreeLoadEventsArray(loadEventsLoaded);
        //     loadEventsLoaded = return_sched_from_NVS();
        //     nvsLoaded = 1;
        // }
        time = (xTaskGetTickCount() * portTICK_PERIOD_MS)/1000;
        //printf("Time = %d\n",time);
        for(int i = 0; i < numberOfLoadsGlobal; i++)
        {            
            for(int j = 0; j < loadsEventsLoaded[i].numOfEventsON; j++)
            {
                if(time == loadsEventsLoaded[i].eventsON[j].date) 
                {
                    printf("Turning ON load %s at %lld\n", loadsEventsLoaded[i].loadName, time);
                    err = delete_or_change_sched_from_NVS(loadsEventsLoaded[i].loadName, time, _NULL, 0);
                    if(err == ESP_OK) printf("Time %lld deleted from NVS.\n", time);
                    else printf("Not possible to delete from NVS.\n");
                }
            }
            for(int j = 0; j < loadsEventsLoaded[i].numOfEventsOFF; j++)
            {
                if(time == loadsEventsLoaded[i].eventsOFF[j].date) 
                {
                    printf("Turning OFF load %s at %lld\n", loadsEventsLoaded[i].loadName, time);
                    err = delete_or_change_sched_from_NVS(loadsEventsLoaded[i].loadName, time, _NULL, 0);
                    if(err == ESP_OK) printf("Time %lld deleted from NVS.\n", time);
                    else printf("Not possible to delete from NVS.\n");
                }
                
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }    
}

void process_command(char* jsonCommand)
{
    cJSON *cmd = NULL;
    cJSON *loadName = NULL;
    cJSON *loadState = NULL;
    cJSON *date = NULL;
    cJSON *repetions = NULL;
    cJSON *pin = NULL;
    const cJSON *schedule = NULL;
    cJSON *cmd_json;
    uint8_t command;    
    
    //printf("Typed JSON: %s\n",stringJson);
        
    cmd_json = cJSON_Parse(jsonCommand);
    command = 99;

    if (cmd_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf(stderr, "Error before: %s\n", error_ptr);
        }
        goto end;
    }

    //if(checkcJSON(cmd_json) == NULL) goto end;
    
    cmd = cJSON_GetObjectItemCaseSensitive(cmd_json, "c");
    command = cmd->valueint;
    // if(!check_number_parameter(cmd,"cmd")) goto end;
    // else printf("Checking command: \"%d\"\n", cmd->valueint);

    loadName = cJSON_GetObjectItemCaseSensitive(cmd_json, "l");
    // if(!check_string_parameter(loadName,"loadName")) goto end;
    // else printf("LoadName: \"%s\"\n", loadName->valuestring);
    
    loadState = cJSON_GetObjectItemCaseSensitive(cmd_json, "s");
    // if(!check_number_parameter(loadState,"loadState")) goto end;
    // else printf("loadState: \"%d\"\n", loadState->valueint);   

    date = cJSON_GetObjectItemCaseSensitive(cmd_json, "d");
    // if(!check_number_parameter(date,"date")) goto end;
    // else printf("date: \"%d\"\n", date->valueint);

    repetions = cJSON_GetObjectItemCaseSensitive(cmd_json, "r");
    // if(!check_number_parameter(date,"date")) goto end;
    // else printf("date: \"%d\"\n", date->valueint);    

    pin = cJSON_GetObjectItemCaseSensitive(cmd_json, "p");             
    
    if(command == 0) 
    {
        if(date == NULL || repetions == NULL) printf("Date or Repetions missing. Please type the entire command.\n");
        else ESP_ERROR_CHECK(save_schedule_time(loadName->valuestring, loadState->valueint, date->valueint, repetions->valueint));
    }
    else if(command == 1) ESP_ERROR_CHECK(print_load_sched_list(loadName->valuestring));
    else if(command == 2) register_new_load(loadName->valuestring, pin->valueint);
    else if(command == 3) read_load_list();
    else if(command == 4) return_sched_from_NVS();
    else if(command == 5) delete_or_change_sched_from_NVS(loadName->valuestring, date->valueint, _NULL, 0);
    else if(command == 6) delete_or_change_sched_from_NVS(loadName->valuestring, date->valueint, repetions->valueint, 1);
    else printf("Command not recognized!");                
    
end:
    printf("Command Processed successfully!\nType the next command: "); 
    cJSON_Delete(cmd_json);
    //system ("pause");
}

void receiveCommand()
{ 
    while(1)
    {
        char c = 0;
        char str[100];
        memset(str, 0, 100);
        while (c != '\n')
        {
            c = getchar();
            if (c != 0xff)
            {
            str[strlen(str)] = c;
            printf("%c", c);
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        printf("CMD typed: %s\n\n",str);
        process_command(&str);
    }
}

void task_process_BLE_received_command()
{
    char *commandReceived = "";

    while (1)
    {           
        xQueueReceive(xQueue_BLE_Received_Data, (void *) &commandReceived, portMAX_DELAY);
        printf("Received in the task: task_process_BLE_received_command: %s\nAnd his data lenght: %d\n",commandReceived, strlen(commandReceived));
        process_command(commandReceived);        
              
    }
}

void app_main()
{
    //Hello 3
    esp_err_t err = nvs_flash_init_partition("MyNvs");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase_partition("MyNvs"));
        err = nvs_flash_init_partition("MyNvs");
    }
    ESP_ERROR_CHECK( err );

    initializaton_BLE_function();

    xQueue_BLE_Received_Data = xQueueCreate( 1, sizeof( char *) );

    // xTaskCreate(
    // receiveCommand                     /* Funcao a qual esta implementado o que a tarefa deve fazer */
    // ,  "ProcessUartCommand"   /* Nome (para fins de debug, se necessário) */
    // ,  1024 * 2                         /* Tamanho da stack (em words) reservada para essa tarefa */
    // ,  NULL                         /* Parametros passados (nesse caso, não há) */
    // ,  3                            /* Prioridade */
    // ,  NULL );                      /* Handle da tarefa, opcional (nesse caso, não há) */

     xTaskCreate(
    task_process_BLE_received_command                     /* Funcao a qual esta implementado o que a tarefa deve fazer */
    ,  "ProcessBLE_CMD"   /* Nome (para fins de debug, se necessário) */
    ,  1024 * 2                         /* Tamanho da stack (em words) reservada para essa tarefa */
    ,  NULL                         /* Parametros passados (nesse caso, não há) */
    ,  3                            /* Prioridade */
    ,  NULL );                      /* Handle da tarefa, opcional (nesse caso, não há) */

    xTaskCreate(
    execute_schedule_action                     /* Funcao a qual esta implementado o que a tarefa deve fazer */
    ,  "ExecutingScheduleAction"   /* Nome (para fins de debug, se necessário) */
    ,  1024 * 2                         /* Tamanho da stack (em words) reservada para essa tarefa */
    ,  NULL                         /* Parametros passados (nesse caso, não há) */
    ,  3                            /* Prioridade */
    ,  NULL );                      /* Handle da tarefa, opcional (nesse caso, não há) */

    
    // gpio_pad_select_gpio(GPIO_NUM_0);
    // gpio_set_direction(GPIO_NUM_0, GPIO_MODE_DEF_INPUT);

    // while (1) {
    //     if (gpio_get_level(GPIO_NUM_0) == 0) {
    //         vTaskDelay(1000 / portTICK_PERIOD_MS);
    //         if(gpio_get_level(GPIO_NUM_0) == 0) {
    //             err = save_run_time();
    //             if (err != ESP_OK) printf("Error (%s) saving run time blob to NVS!\n", esp_err_to_name(err));
    //             printf("Restarting...\n");
    //             fflush(stdout);
    //             esp_restart();
    //         }
    //     }
    //     vTaskDelay(200 / portTICK_PERIOD_MS);
    // }
}
