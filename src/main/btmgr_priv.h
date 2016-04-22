#ifndef __BTMGR_PRIV_H__
#define __BTMGR_PRIV_H__

#include "btmgr.h"
#include "rdk_debug.h"
#include "stdlib.h"
#include "string.h"
#if 0
#define BTMGRLOG_ERROR(format...)       RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.BTMGR", format)
#define BTMGRLOG_WARN(format...)        RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.BTMGR", format)
#define BTMGRLOG_INFO(format...)        RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.BTMGR", format)
#define BTMGRLOG_DEBUG(format...)       RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.BTMGR", format)
#define BTMGRLOG_TRACE(format...)       RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.BTMGR", format)
#else
#define BTMGRLOG_ERROR(format...)       printf (format)
#define BTMGRLOG_WARN(format...)        printf (format)
#define BTMGRLOG_INFO(format...)        printf (format)
#define BTMGRLOG_DEBUG(format...)       printf (format)
#define BTMGRLOG_TRACE(format...)       printf (format)
#endif

#if 0
class BTMGR_Adapter {
private:
    /* Private Functions */
    ~BTMGR_Adapter();

    /* Private Variables */
    char m_adapterName[BTMGR_NAME_LEN_MAX];

public:
    /* Public Functions */
    BTMGR_Result_t SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter);
    BTMGR_Result_t GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter);

    BTMGR_Result_t SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status);
    BTMGR_Result_t GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus);

    BTMGR_Result_t SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout);
    BTMGR_Result_t IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable);

    BTMGR_Result_t StartDeviceDiscovery(unsigned char index_of_adapter);
    BTMGR_Result_t GetDiscoveredDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pDiscoveredDevices);

    BTMGR_Result_t PairDevice(unsigned char index_of_adapter, const char* pNameOfDevice);
    BTMGR_Result_t UnpairDevice(unsigned char index_of_adapter, const char* pNameOfDevice);
    BTMGR_Result_t GetPairedDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pDiscoveredDevices);

    BTMGR_Result_t ConnectToDevice(unsigned char index_of_adapter, const char* pNameOfDevice);
    BTMGR_Result_t DisconnectFromDevice(unsigned char index_of_adapter, const char* pNameOfDevice);

    BTMGR_Result_t GetDeviceProperties(unsigned char index_of_adapter, const char* pNameOfDevice, BTMGR_DevicesProperty_t *pDeviceProperty);
};
#endif
#endif /* __BTMGR_PRIV_H__ */
