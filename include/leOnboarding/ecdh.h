/*
 * ============================================================================
 * COMCAST C O N F I D E N T I A L AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of Comcast.  It may
 * not be used, copied, distributed or otherwise  disclosed in whole or in part
 * without the express written permission of Comcast.
 * ============================================================================
 * Copyright (c) 2018 Comcast. All rights reserved.
 * ============================================================================
 */

#ifndef __ECDH_H__
#define __ECDH_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define kPrivateKeyPath "/tmp/bootstrap_private.pem"
#define kPublicKeyPath "/tmp/bootstrap_public.pem"

int ECDH_DecryptWiFiSettings( cJSON * server_reply, cJSON** wifi_settings);

#ifdef __cplusplus
}
#endif
#endif
