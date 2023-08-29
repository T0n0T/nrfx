#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_MY_HRS)
#include "my_ble_hrs.h"
#include <string.h>
#include "ble_srv_common.h"

//操作码长度：1个字节
#define OPCODE_LENGTH 1                                                              
//句柄长度：2个字节
#define HANDLE_LENGTH 2                                                             
//BLE一次能传输的心率测量的最大字节数，根据BLE内核协议，MTU（最大传输单元）包含：1字节Opcode + 2字节属性handle + 有效载荷
#define MAX_HRM_LEN      (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH) 
//心率测量初始值
#define INITIAL_VALUE_HRM                       0                                    /**< Initial Heart Rate Measurement value. */

/*----心率测量flag位段定义 根据SIG发布的《HRS_SPEC_VV10r00》定义--*/
//心率值数据格式位 =0:UINT8;=1:UINT16
#define HRM_FLAG_MASK_HR_VALUE_16BIT            (0x01 << 0) 
//传感器接触检测位，如果支持接触检测，当设备检测到没有接触到皮肤或者接触不良，位1应设置为0，否则，设置为1
#define HRM_FLAG_MASK_SENSOR_CONTACT_DETECTED   (0x01 << 1)  
//传感器接触支持位 =0:不支持传感器接触检测，=1：支持
#define HRM_FLAG_MASK_SENSOR_CONTACT_SUPPORTED  (0x01 << 2) 
//能量消耗状态位,指示能量消耗字段是否存在于心率测量特征中。如果存在，该位应设置为1，否则设置为0
#define HRM_FLAG_MASK_EXPENDED_ENERGY_INCLUDED  (0x01 << 3) 
//RR-Interval位，指示RR-Interval值是否存在于心率测量特征中。如果一个或者多个RR-Interval值存在，该位应设置为1，否则设置为0。                          
#define HRM_FLAG_MASK_RR_INTERVAL_INCLUDED      (0x01 << 4)                          


/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
//连接事件表示连接建立，这时应将连接句柄保存心率服务实例p_hrs
static void on_connect(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    p_hrs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
//断开连接事件表示连接已经断开，这时应设置心率服务实例中的连接句柄为无效（BLE_CONN_HANDLE_INVALID）
static void on_disconnect(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_hrs->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling write events to the Heart Rate Measurement characteristic.
 *
 * @param[in]   p_hrs         Heart Rate Service structure.
 * @param[in]   p_evt_write   Write event received from the BLE stack.
 */
static void on_hrm_cccd_write(ble_hrs_t * p_hrs, ble_gatts_evt_write_t const * p_evt_write)
{
    if (p_evt_write->len == 2)
    {
        //写CCCD，更新通知的状态
        if (p_hrs->evt_handler != NULL)
        {
            ble_hrs_evt_t evt;

            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_HRS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_HRS_EVT_NOTIFICATION_DISABLED;
            }

            p_hrs->evt_handler(p_hrs, &evt);
        }
    }
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_hrs       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_hrs_t * p_hrs, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_hrs->hrm_handles.cccd_handle)
    {
        on_hrm_cccd_write(p_hrs, p_evt_write);
    }
}

//心率服务BLE事件监视者的事件回调函数
void ble_hrs_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    //定义一个心率结构体指针并指向心率结构体
	  ble_hrs_t * p_hrs = (ble_hrs_t *) p_context;
    //判断事件类型
    switch (p_ble_evt->header.evt_id)
    {       
			  case BLE_GAP_EVT_CONNECTED://连接事件
			      //将连接句柄保存到心率服务实例p_hrs的“conn_handle”
            on_connect(p_hrs, p_ble_evt);
            break;
        case BLE_GAP_EVT_DISCONNECTED://连接断开事件
			      // 设置心率服务实例p_hrs中的连接句柄“conn_handle”为BLE_CONN_HANDLE_INVALID
            on_disconnect(p_hrs, p_ble_evt);
            break;
        case BLE_GATTS_EVT_WRITE://写事件
		        //使能或关闭心率测量特征的通知
            on_write(p_hrs, p_ble_evt);
            break;
        default:
            break;
    }
}

//初始化心率服务
uint32_t ble_hrs_init(ble_hrs_t * p_hrs, const ble_hrs_init_t * p_hrs_init)
{
    uint32_t              err_code;
	  //定义UUID结构体变量
    ble_uuid_t            ble_uuid;
	  //定义特征参数结构体变量
    ble_add_char_params_t add_char_params;
    uint8_t               initial_hrm[9];

    //初始化心率服务结构体
	  //拷贝心率服务初始化结构体中的心率服务事件句柄
    p_hrs->evt_handler                 = p_hrs_init->evt_handler;
	  //拷贝心率服务初始化结构体中的心率服务事件句柄
    p_hrs->is_sensor_contact_supported = p_hrs_init->is_sensor_contact_supported;
	  //连接句柄初始值设置为：无效的连接句柄
    p_hrs->conn_handle                 = BLE_CONN_HANDLE_INVALID;
	  //传感器接触设置为false，即未接触
    p_hrs->is_sensor_contact_detected  = false;
	  //rr_interval数量初始值设置为0
    p_hrs->rr_interval_count           = 0;
	  //设置当前一次可发送的最大心率测量数据长度
    p_hrs->max_hrm_len                 = MAX_HRM_LEN;

    //心率服务的UUID
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_HEART_RATE_SERVICE);
    //添加服务到属性表
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_hrs->service_handle);
    //检查返回的错误代码
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    //加入心率测量特征
		//配置参数之前先清零add_char_params
    memset(&add_char_params, 0, sizeof(add_char_params));
    //心率测量特征的UUID
    add_char_params.uuid              = BLE_UUID_HEART_RATE_MEASUREMENT_CHAR;
		//设置心率测量特征值的最大长度
    add_char_params.max_len           = MAX_HRM_LEN;
		//设置心率测量特征值的初始长度
    add_char_params.init_len          = 0;
    add_char_params.p_init_value      = initial_hrm;
		//设置心率测量的特征值长度为可变长度
    add_char_params.is_var_len        = true;
		//设置传感器位置特征的性质：支持通知
    add_char_params.char_props.notify = 1;
    add_char_params.cccd_write_access = p_hrs_init->hrm_cccd_wr_sec;
    //为心率服务添加心率测量特征
    err_code = characteristic_add(p_hrs->service_handle, &add_char_params, &(p_hrs->hrm_handles));
		//检查返回的错误代码
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    //如果心率服务包含传感器位置特征，为心率服务添加传感器位置特征
    if (p_hrs_init->p_body_sensor_location != NULL)
    {
        //初始化参数之前，先清零add_char_params
        memset(&add_char_params, 0, sizeof(add_char_params));
        //设置传感器位置特征的UUID
        add_char_params.uuid            = BLE_UUID_BODY_SENSOR_LOCATION_CHAR;
			  //设置传感器位置特征值的最大长度
        add_char_params.max_len         = sizeof(uint8_t);
			  //设置传感器位置特征值的初始长度
        add_char_params.init_len        = sizeof(uint8_t);
			  //设置传感器位置特征的初始值
        add_char_params.p_init_value    = p_hrs_init->p_body_sensor_location;
			  //设置传感器位置特征的性质：支持读取
        add_char_params.char_props.read = 1;
			  //设置读传感器位置特征值的安全需求
        add_char_params.read_access     = p_hrs_init->bsl_rd_sec;
        //为心率服务添加传感器位置特征
        err_code = characteristic_add(p_hrs->service_handle, &add_char_params, &(p_hrs->bsl_handles));
			  //检查返回的错误代码
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}

//发送心率测量特征通知，即心率数据
//注意：这里发送的心率数据并不是真实的、从心率传感器采样的数据，而是直接发送固定的数据
uint32_t ble_hrs_heart_rate_measurement_send(ble_hrs_t * p_hrs, uint16_t heart_rate)
{
    static uint8_t rr_val = 0;
	  uint32_t err_code;
	  //flag初始值设置为0x15，即设置心率的数据格式为UINT16、支持传感器接触检测、不支持能量检测、心率特征中包含RR-Interval
	  uint8_t flags = 0x15;

    //如果连接有效，发送心率测量特征通知
    if (p_hrs->conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint8_t                hrm_data[1+2+6];
        uint16_t               hvx_len;
        ble_gatts_hvx_params_t hvx_params;
        //写入心率测量特征的数值       
		    if (p_hrs->is_sensor_contact_detected)
        {
            //更新传感器接触检测状态
			      flags |= HRM_FLAG_MASK_SENSOR_CONTACT_DETECTED;
        }
				hrm_data[0] = flags;
			  hrm_data[1] = heart_rate;
			  hrm_data[2] = heart_rate>>8;
			  hrm_data[3] = rr_val+10;
			  hrm_data[4] = 0x01;
			  hrm_data[5] = rr_val+5;
			  hrm_data[6] = 0x01;
			  hrm_data[7] = rr_val;
			  hrm_data[8] = 0x01;
			  //为了方便观察数据，每次发送后，数据加1
			  rr_val++;
			  //特征值长度为9
			  hvx_len = 1+2+6;
			
        //设置之前先清零
        memset(&hvx_params, 0, sizeof(hvx_params));
        //心率测量特征值句柄
        hvx_params.handle = p_hrs->hrm_handles.value_handle;
				//类型为通知
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
			  //发送的数据长度
        hvx_params.p_len  = &hvx_len;
			  //发送的数据
        hvx_params.p_data = hrm_data;
        //发送心率测量特征通知
        err_code = sd_ble_gatts_hvx(p_hrs->conn_handle, &hvx_params);

    }
    else//返回连接状态无效的错误代码
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }
    return err_code;
}
//更新传感器接触检测位
void ble_hrs_sensor_contact_detected_update(ble_hrs_t * p_hrs, bool is_sensor_contact_detected)
{
    p_hrs->is_sensor_contact_detected = is_sensor_contact_detected;
}

#endif // NRF_MODULE_ENABLED(BLE_HRS)
