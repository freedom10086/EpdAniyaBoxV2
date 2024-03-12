#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_SERVER_BASE_PATH "/data"

/* Max size of an individual file. Make sure this
* value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (256*1024) // 256 KB
#define MAX_FILE_SIZE_STR "256KB"

esp_err_t mount_storage(const char *base_path, bool format_when_failed);

esp_err_t unmount_storage();

#ifdef __cplusplus
}
#endif
