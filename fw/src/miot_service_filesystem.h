/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_FW_SRC_MIOT_SERVICE_FILESYSTEM_H_
#define CS_FW_SRC_MIOT_SERVICE_FILESYSTEM_H_

#if MG_ENABLE_CLUBBY && MG_ENABLE_FILESYSTEM_SERVICE

#include "fw/src/miot_init.h"

/*
 * Initialises clubby handlers for /v1/Filesystem commands
 */
enum miot_init_result miot_service_filesystem_init(void);

#endif /* MG_ENABLE_CLUBBY && MG_ENABLE_FILESYSTEM_SERVICE */
#endif /* CS_FW_SRC_MIOT_SERVICE_FILESYSTEM_H_ */