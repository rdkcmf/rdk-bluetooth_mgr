/*
 * ============================================================================
 * RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of RDK Management, LLC.
 * It may not be used, copied, distributed or otherwise  disclosed in whole or in
 * part without the express written permission of RDK Management, LLC.
 * ============================================================================
 * Copyright (c) 2016 RDK Management, LLC. All rights reserved.
 * ============================================================================
 */
/**
 * @file btrMgr_streamOut.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 * Copyright (c) 2016  Comcast
 */

#ifndef __BTR_MGR_STREAMOUT_H__
#define __BTR_MGR_STREAMOUT_H__

/* Interfaces */
void BTRMgr_SO_Init (void);
void BTRMgr_SO_DeInit (void);
void BTRMgr_SO_Start (int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU);
void BTRMgr_SO_Stop (void);
void BTRMgr_SO_Pause (void);
void BTRMgr_SO_Resume (void);
void BTRMgr_SO_SendBuffer (char* pcInBuf, int aiInBufSize);
void BTRMgr_SO_SendEOS (void);

#endif /* __BTR_MGR_STREAMOUT_H__ */
