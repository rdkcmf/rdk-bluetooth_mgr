/**
 * @file btrMgr_streamOut.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 * Copyright (c) 2015  Comcast
 */
#ifndef __BTR_MGR_STREAMOUT_H__
#define __BTR_MGR_STREAMOUT_H__

/* Interfaces */
void BTRMgr_SO_Init (void);
void BTRMgr_SO_DeInit (void);
void BTRMgr_SO_Start (void);
void BTRMgr_SO_Stop (void);
void BTRMgr_SO_SendBuffer (void);
void BTRMgr_SO_SendEOS (void);

#endif /* __BTR_MGR_STREAMOUT_H__ */
