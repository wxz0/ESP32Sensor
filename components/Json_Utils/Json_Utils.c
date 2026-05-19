#include <stdio.h>
#include "Json_Utils.h"
#include "App_Data.h"

void DoubleToString(char *buf, size_t buf_size, float data)
{
  snprintf(buf, buf_size, "%.3f", data);
}

void IntToString(char *buf, size_t buf_size, int64_t data)
{
  snprintf(buf, buf_size, "%lld", data);
}
 
char * JSON_PackSensorData(WaterQualityData_t* data)
{
    cJSON *root = NULL;
    char *ret = NULL;
    char str_buf[32] = {0};
    if(data == NULL)
    {
      return NULL;
    }
    root = cJSON_CreateObject();
    
    DoubleToString(str_buf,sizeof(str_buf),data->temp);
    cJSON_AddStringToObject(root,"TEMP",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->ph);
    cJSON_AddStringToObject(root,"PH",str_buf);
    
    IntToString(str_buf,sizeof(str_buf),data->tds);
    cJSON_AddStringToObject(root,"TDS",str_buf);
    
    IntToString(str_buf,sizeof(str_buf),data->orp);
    cJSON_AddStringToObject(root,"ORP",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->salt);
    cJSON_AddStringToObject(root,"SALT",str_buf);
    
    DoubleToString(str_buf,sizeof(str_buf),data->saltppt);
    cJSON_AddStringToObject(root,"SALTPPT",str_buf);
    
    DoubleToString(str_buf,sizeof(str_buf),data->sg);
    cJSON_AddStringToObject(root,"SG",str_buf);
    
    IntToString(str_buf,sizeof(str_buf),data->ec);
    cJSON_AddStringToObject(root,"EC",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->cl);
    cJSON_AddStringToObject(root,"CL",str_buf);
    
    IntToString(str_buf,sizeof(str_buf),data->timestamp);
    cJSON_AddStringToObject(root,"TIMESTAMP",str_buf);
    
    cJSON_AddStringToObject(root,"DEVICE_ID",Device_ID);
    
    ret = cJSON_Print(root);
    cJSON_Delete(root);
    return ret;
}

char * JSON_PackLightData(LightSensorData_t* data)
{
    cJSON *root = NULL;
    char *ret = NULL;
    char str_buf[32] = {0};
    if(data == NULL)
    {
      return NULL;
    }
    root = cJSON_CreateObject();

    DoubleToString(str_buf,sizeof(str_buf),data->lux_1);
    cJSON_AddStringToObject(root,"LIGHT1",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->lux_2);
    cJSON_AddStringToObject(root,"LIGHT2",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->lux_3);
    cJSON_AddStringToObject(root,"LIGHT3",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->lux_4);
    cJSON_AddStringToObject(root,"LIGHT4",str_buf);

    IntToString(str_buf,sizeof(str_buf),data->timestamp);
    cJSON_AddStringToObject(root,"TIMESTAMP",str_buf);

    cJSON_AddStringToObject(root,"DEVICE_ID",Device_ID);

    ret = cJSON_Print(root);
    cJSON_Delete(root);
    return ret;
}

char * JSON_PackPressureData(PressureSensorData_t* data)
{
    cJSON *root = NULL;
    char *ret = NULL;
    char str_buf[32] = {0};
    if(data == NULL)
    {
      return NULL;
    }
    root = cJSON_CreateObject();

    DoubleToString(str_buf,sizeof(str_buf),data->pressure_cur);
    cJSON_AddStringToObject(root,"PRESSURE_CUR",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->pressure_min);
    cJSON_AddStringToObject(root,"PRESSURE_MIN",str_buf);

    DoubleToString(str_buf,sizeof(str_buf),data->pressure_max);
    cJSON_AddStringToObject(root,"PRESSURE_MAX",str_buf);

    IntToString(str_buf,sizeof(str_buf),data->timestamp);
    cJSON_AddStringToObject(root,"TIMESTAMP",str_buf);

    cJSON_AddStringToObject(root,"DEVICE_ID",Device_ID);

    ret = cJSON_Print(root);
    cJSON_Delete(root);
    return ret;
}
