#include <stdio.h>
#include "Sensor.h"
#include "esp_log.h"
#include "modbus_params.h"  // for modbus parameters structures
#include "mbcontroller.h"
#include "esp_modbus_common.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"


static const char *TAG = "Sensor";
WaterQualityData_t SensorData = {0};

/*RS485通信配置*/
// Note: Some pins on target chip cannot be assigned for UART communication.
// See UART documentation for selected board and target to configure pins using Kconfig.

// The number of parameters that intended to be used in the particular control process
#define MASTER_MAX_CIDS num_device_parameters

// Number of reading of parameters from slave
#define MASTER_MAX_RETRY                (10)

// Timeout to update cid over Modbus
#define UPDATE_CIDS_TIMEOUT_MS          (500)
#define UPDATE_CIDS_TIMEOUT_TICS        (UPDATE_CIDS_TIMEOUT_MS / portTICK_PERIOD_MS)

// Timeout between polls
#define POLL_TIMEOUT_MS                 (1)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_PERIOD_MS)

// The macro to get offset for parameter in the appropriate structure
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) + 1))
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))
// Discrete offset macro
#define DISCR_OFFSET(field) ((uint16_t)(offsetof(discrete_reg_params_t, field) + 1))

#define STR(fieldname) ((const char *)( fieldname ))
#define TEST_HOLD_REG_START(field) (HOLD_OFFSET(field) >> 1)
#define TEST_HOLD_REG_SIZE(field) (sizeof(((holding_reg_params_t *)0)->field) >> 1)

#define TEST_INPUT_REG_START(field) (INPUT_OFFSET(field) >> 1)
#define TEST_INPUT_REG_SIZE(field) (sizeof(((input_reg_params_t *)0)->field) >> 1)

// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

#define EACH_ITEM(array, length) \
    (typeof(*(array)) *item_ptr = (array); (item_ptr < &((array)[length])); item_ptr++)
#define MB_PORT_NUM     (1)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    (9600)  // The communication speed of the UART
#define MB_CUST_DATA_LEN 100 // The length of custom command buffer
static void *master_handle = NULL;


#define TEST_VERIFY_VALUES(handle, descr, inst) (__extension__(                                   \
{                                                                                                 \
    assert(inst);                                                                                 \
    assert(descr);                                                                                \
    uint8_t type = 0;                                                                             \
    esp_err_t err = ESP_FAIL;                                                                     \
    err = mbc_master_get_parameter(handle, descr->cid,                                            \
                                    (uint8_t *)inst, &type);                                      \
    if (err == ESP_OK) {                                                                          \
        bool is_correct = true;                                                                   \
        if (descr->param_opts.opt3) {                                                             \
            for EACH_ITEM(inst, descr->param_size / sizeof(*item_ptr)) {                          \
                if (*item_ptr != (typeof(*(inst)))descr->param_opts.opt3) {                       \
                    *item_ptr = (typeof(*(inst)))descr->param_opts.opt3;                          \
                    ESP_LOGD(TAG, "Characteristic #%d (%s), initialize to 0x%" PRIx16 ".",        \
                                (int)descr->cid,                                                  \
                                (char *)descr->param_key,                                         \
                                (uint16_t)descr->param_opts.opt3);                                \
                    is_correct = false;                                                           \
                }                                                                                 \
            }                                                                                     \
        }                                                                                         \
        if (!is_correct) {                                                                        \
            ESP_LOGE(TAG, "Characteristic #%d (%s), initialize.",                                 \
                        (int)descr->cid,                                                          \
                        (char *)descr->param_key);                                                \
            err = mbc_master_set_parameter(handle, cid, (uint8_t *)inst, &type);                  \
            if (err != ESP_OK) {                                                                  \
                ESP_LOGE(TAG, "Characteristic #%d (%s) write fail, err = 0x%x (%s).",             \
                            (int)descr->cid,                                                      \
                            (char *)descr->param_key,                                             \
                            (int)err,                                                             \
                            (char *)esp_err_to_name(err));                                        \
            } else {                                                                              \
                ESP_LOGI(TAG, "Characteristic #%d %s (%s) value = (..) write successful.",        \
                        (int)descr->cid,                                                          \
                        (char *)descr->param_key,                                                 \
                        (char *)descr->param_units);                                              \
            }                                                                                     \
        }                                                                                         \
    } else {                                                                                      \
        ESP_LOGE(TAG, "Characteristic #%d (%s) read fail, err = 0x%x (%s).",                      \
                            (int)descr->cid,                                                      \
                            (char *)descr->param_key,                                             \
                            (int)err,                                                             \
                            (char *)esp_err_to_name(err));                                        \
    }                                                                                             \
    (err);                                                                                        \
}                                                                                                 \
))


// Enumeration of modbus device addresses accessed by master device
enum {
    MB_Light_DEVICE_ADDR1 = 1,
    MB_Light_DEVICE_ADDR2 = 2,
    MB_Light_DEVICE_ADDR3 = 3,
    MB_Light_DEVICE_ADDR4 = 4,
    MB_Water_Quality_DEVICE_ADDR1 = 5,
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_TEMP_C = 0,
    CID_PH,
    CID_TDS,
    CID_ORP,
    CID_SALT_PCT,
    CID_SALT_PPT,
    CID_SG,
    CID_EC,
    CID_CL,
    CID_CALIB_PH_401, // 对应寄存器 0x16
    CID_CALIB_PH_686, // 对应寄存器 0x17
    CID_CALIB_PH_918, // 对应寄存器 0x18
    CID_CALIB_EC_1413, // 对应寄存器 0x19
    CID_CALIB_EC_1288, // 对应寄存器 0x1A
    CID_CALIB_EC_1113, // 对应寄存器 0x1B
    CID_CALIB_ORP_256, // 对应寄存器 0x1C
    CID_LIGHT_1,
    CID_LIGHT_2,
    CID_LIGHT_3,
    CID_LIGHT_4,
    CID_LIGHT1_ADR,
    CID_LIGHT2_ADR,
    CID_LIGHT3_ADR,
    CID_LIGHT4_ADR,
    CID_COUNT
};

const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    { CID_TEMP_C, STR("Temp"), STR("C"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x01,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
   { CID_PH, STR("PH"), STR("ph"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x03,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_TDS, STR("TDS"), STR("ppm"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x04,2,
            0, PARAM_TYPE_U32, 4,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_ORP, STR("ORP"), STR("mV"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x06,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_SALT_PCT, STR("SALT%"), STR("%"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x07,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_SALT_PPT, STR("SALTPPT"), STR("ppt"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x08,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_SG, STR("SG"), STR("sg"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x09,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_EC, STR("EC"), STR("uS/cm"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x0A,2,
            0, PARAM_TYPE_U32, 4,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CL, STR("CL"), STR("mg/L"), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x0C,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_PH_401, STR("Cal_Ph_4.01"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x16,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_PH_686, STR("Cal_Ph_6.86"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x17,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_PH_918, STR("Cal_Ph_9.18"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x18,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_EC_1413, STR("Cal_EC_1413"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x19,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_EC_1288, STR("Cal_EC_1288"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x1A,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_EC_1113, STR("Cal_EC_1113"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x1B,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_CALIB_ORP_256, STR("Cal_ORP_256"), STR(""), MB_Water_Quality_DEVICE_ADDR1, MB_PARAM_HOLDING,
            0x1C,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_LIGHT_1, STR("Light1/Lux"), STR(""), MB_Light_DEVICE_ADDR1, MB_PARAM_INPUT,
            0x00,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },
    { CID_LIGHT_2, STR("Light2/Lux"), STR(""), MB_Light_DEVICE_ADDR2,     MB_PARAM_INPUT,
            0x00,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },   
    { CID_LIGHT_3, STR("Light3/Lux"), STR(""), MB_Light_DEVICE_ADDR3, MB_PARAM_INPUT,
            0x00,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ },   
    { CID_LIGHT_4, STR("Light4/Lux"), STR(""), MB_Light_DEVICE_ADDR4, MB_PARAM_INPUT,
            0x00,1,
            0, PARAM_TYPE_U16, 2,
            OPTS(0,0, 0 ), PAR_PERMS_READ }                                       
};

const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));// Calculate number of parameters in the table


/*Modbus Master Initialization*/
esp_err_t WaterQualitySensor_init(void)
{
    // Initialize Modbus controller
    mb_communication_info_t comm = {
        .ser_opts.port = MB_PORT_NUM,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = MB_DEV_SPEED,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = 0,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1
    };

    esp_err_t err = mbc_master_create_serial(&comm, &master_handle);
    MB_RETURN_ON_FALSE((master_handle != NULL), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller initialization fail, returns(0x%x).", (int)err);

      // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, 18, 17,
                              16, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                        "mb serial set pin failure, uart_set_pin() returned (0x%x).", (int)err);

    err = mbc_master_start(master_handle);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller start fail, returned (0x%x).", (int)err);


    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (int)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(master_handle, &device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller set descriptor fail, returns(0x%x).", (int)err);
    ESP_LOGI(TAG, "Modbus 初始化完成");
    return err;
}

// 辅助函数：简化读取调用
static esp_err_t read_param_u8(uint16_t cid, uint8_t *out_val) 
{
    if (master_handle == NULL || out_val == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t type = 0;
    return mbc_master_get_parameter(master_handle, cid, (uint8_t*)out_val, &type);
} 

static esp_err_t read_param_u16(uint16_t cid, uint16_t *out_val) 
{
    if (master_handle == NULL || out_val == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t type = 0;
    return mbc_master_get_parameter(master_handle, cid, (uint8_t*)out_val, &type);
}

static esp_err_t read_param_u32(uint16_t cid, uint32_t *out_val) 
{
    if (master_handle == NULL || out_val == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t type = 0;
    return mbc_master_get_parameter(master_handle, cid, (uint8_t*)out_val, &type);
}



// 读取所有数据并进行物理转换
esp_err_t water_sensor_read_all(WaterQualityData_t *out_data)
{
    if (out_data == NULL) return ESP_ERR_INVALID_ARG;
    if (master_handle == NULL) return ESP_ERR_INVALID_STATE;
    
    esp_err_t err = ESP_OK;
    uint16_t raw_u16 = 0;
    uint32_t raw_u32 = 0;

    // 清空输出结构体
    memset(out_data, 0, sizeof(WaterQualityData_t));
    // --- 开始读取并转换 ---

    // 1. 温度 (原始值 / 10)
    if (read_param_u16(CID_TEMP_C, &raw_u16) == ESP_OK) 
    {
        out_data->temp = (float)raw_u16 / 10.0f;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read temperature");
    }

    // 2. PH (原始值 / 100)
    if (read_param_u16(CID_PH, &raw_u16) == ESP_OK) 
    {
        out_data->ph = (float)raw_u16 / 100.0f;
    } 
    else
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read pH");
    }

    // 3. TDS (32位，直接值)
    if (read_param_u32(CID_TDS, &raw_u32) == ESP_OK) 
    {
        out_data->tds = raw_u32;
    } 
    else
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read TDS");
    }

    // 4. ORP (16位，直接值，转有符号)
    if (read_param_u16(CID_ORP, &raw_u16) == ESP_OK) 
    {
        out_data->orp = (int16_t)raw_u16;
    } 
    else
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read ORP");
    }

    // 5. 盐度% (原始值 / 100)
    if (read_param_u16(CID_SALT_PCT, &raw_u16) == ESP_OK) 
    {
        out_data->salt = (float)raw_u16 / 100.0f;
    } 
    else
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read Salinity %%");
    }

    // 6. 盐度PPT (原始值 / 10) - 注意这里是除以10
    if (read_param_u16(CID_SALT_PPT, &raw_u16) == ESP_OK) 
    {
        out_data->saltppt = (float)raw_u16 / 10.0f;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read Salinity PPT");
    }

    // 7. 比重 SG (原始值 / 1000)
    if (read_param_u16(CID_SG, &raw_u16) == ESP_OK) 
    {
        out_data->sg = (float)raw_u16 / 1000.0f;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read Specific Gravity");
    }

    // 8. 电导率 EC (32位，直接值)
    if (read_param_u32(CID_EC, &raw_u32) == ESP_OK) 
    {
        out_data->ec = raw_u32;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read EC");
    }

    // 9. 余氯 (原始值 / 100)
    if (read_param_u16(CID_CL, &raw_u16) == ESP_OK) 
    {
        out_data->cl = (float)raw_u16 / 100.0f;
    }
    else
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read Chlorine");
    }

    if (err != ESP_OK) 
    {
        ESP_LOGI(TAG, "Part of sensor data read failed");
    }

    return err;
}

// 读取所有数据并进行物理转换
esp_err_t light_sensor_read_all(LightSensorData_t *out_data)
{
    if (out_data == NULL) return ESP_ERR_INVALID_ARG;
    if (master_handle == NULL) return ESP_ERR_INVALID_STATE;
    
    esp_err_t err = ESP_OK;
    uint16_t raw_u16 = 0;

    // 清空输出结构体
    memset(out_data, 0, sizeof(LightSensorData_t));
    // --- 开始读取并转换 ---

    if (read_param_u16(CID_LIGHT_1, &raw_u16) == ESP_OK) 
    {
        out_data->lux_1 = (float)raw_u16;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read light1");
    }

    if (read_param_u16(CID_LIGHT_2, &raw_u16) == ESP_OK) 
    {
        out_data->lux_2 = (float)raw_u16;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read light2");
    }

    if (read_param_u16(CID_LIGHT_3, &raw_u16) == ESP_OK) 
    {
        out_data->lux_3 = (float)raw_u16;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read light3");
    }

    if (read_param_u16(CID_LIGHT_4, &raw_u16) == ESP_OK) 
    {
        out_data->lux_4 = (float)raw_u16;
    } 
    else 
    {
        err = ESP_FAIL;
        ESP_LOGI(TAG, "Failed to read light4");
    }

    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Part of light sensor data read failed");
    }

    return err;
}

// esp_err_t read_light_address(uint8_t light_adr[])
// {
//   uint8_t raw_u8 = 0;
//   esp_err_t err = ESP_OK;
//   if (read_param_u8(CID_LIGHT1_ADR, &raw_u8) == ESP_OK) 
//     {
//         light_adr[0] = raw_u8;
//     } 
//     else 
//     {
//         err = ESP_FAIL;
//         ESP_LOGI(TAG, "Failed to read light1addr");
//     }
//   if (read_param_u8(CID_LIGHT2_ADR, &raw_u8) == ESP_OK) 
//     {
//         light_adr[1] = raw_u8;
//     } 
//     else 
//     {
//         err = ESP_FAIL;
//         ESP_LOGI(TAG, "Failed to read light2addr");
// //     }
// //    if (read_param_u8(CID_LIGHT1_ADR, &raw_u8) == ESP_OK) 
//     {
//         light_adr[2] = raw_u8;
//     } 
//     else 
//     {
//         err = ESP_FAIL;
//         ESP_LOGI(TAG, "Failed to read light3addr");
//     }
//    if (read_param_u8(CID_LIGHT1_ADR, &raw_u8) == ESP_OK) 
//     {
//         light_adr[3] = raw_u8;
//     } 
//     else 
//     {
//         err = ESP_FAIL;
//         ESP_LOGI(TAG, "Failed to read light4addr");
//     }    

// }
static esp_err_t perform_calibration_logic(uint16_t cid, const char* desc)
{
    if (master_handle == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t err;
    uint8_t type = 0;
    uint16_t signal_val;

    ESP_LOGI(TAG, "[%s] Start Calibration...", desc);

    // 步骤 1: 发送 1 (开始校准)
    signal_val = 1;
    err = mbc_master_set_parameter(master_handle, cid, (uint8_t*)&signal_val, &type);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "[%s] Failed to send START command: %s", desc, esp_err_to_name(err));
        return err;
    }

    // 步骤 2: 等待 40 秒 (手册要求)
    // 注意：这会阻塞当前 Task
    ESP_LOGI(TAG, "[%s] Waiting 40 seconds...", desc);
    vTaskDelay(pdMS_TO_TICKS(40000)); 

    // 步骤 3: 发送 0 (结束校准)
    signal_val = 0;
    err = mbc_master_set_parameter(master_handle, cid, (uint8_t*)&signal_val, &type);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "[%s] Failed to send END command: %s", desc, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "[%s] Calibration Completed Successfully.", desc);
    return ESP_OK;
}


esp_err_t water_sensor_calib_ph_4_01(void)
{
    // 对应 0x16 寄存器
    return perform_calibration_logic(CID_CALIB_PH_401, "PH 4.01");
}

esp_err_t water_sensor_calib_ph_6_86(void)
{
    // 对应 0x17 寄存器
    return perform_calibration_logic(CID_CALIB_PH_686, "PH 6.86");
}

esp_err_t water_sensor_calib_ph_9_18(void)
{
    // 对应 0x18 寄存器
    return perform_calibration_logic(CID_CALIB_PH_918, "PH 9.18");
}

esp_err_t water_sensor_calib_ec_1413(void)
{
    return perform_calibration_logic(CID_CALIB_EC_1413, "EC 1413");
}

esp_err_t water_sensor_calib_ec_1288(void)
{
    return perform_calibration_logic(CID_CALIB_EC_1288, "EC 1288");
}

esp_err_t water_sensor_calib_ec_1113(void)
{
    return perform_calibration_logic(CID_CALIB_EC_1113, "EC 1113");
}

esp_err_t water_sensor_calib_orp_256(void)
{
    return perform_calibration_logic(CID_CALIB_ORP_256, "ORP 256");
}
