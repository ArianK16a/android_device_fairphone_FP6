/* Copyright (c) 2011-2015, 2018 - 2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
Changes from Qualcomm Innovation Center are provided under the following license:

Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

 /*
 Changes from Qualcomm Innovation Center are provided under the following license:

 Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted (subject to the limitations in the
 disclaimer below) provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and the following
 disclaimer in the documentation and/or other materials provided
 with the distribution.

 * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 contributors may be used to endorse or promote products derived
 from this software without specific prior written permission.

 NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LOC_CFG_H
#define LOC_CFG_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>

#define LOC_MAX_PARAM_NAME                 80
#define LOC_MAX_PARAM_STRING               172
#define LOC_MAX_PARAM_LINE    (LOC_MAX_PARAM_NAME + LOC_MAX_PARAM_STRING)

#define LOC_FEATURE_MODE_DISABLED "DISABLED"
#define LOC_FEATURE_MODE_BASIC    "BASIC"
#define LOC_FEATURE_MODE_PREMIUM  "PREMIUM"

#define LOC_FEATURE_GTP_AP_CELL        "gtp-ap-cell"
#define LOC_FEATURE_GTP_MODEM_CELL     "gtp-modem-cell"
#define LOC_FEATURE_GTP_CELL_ENH       "gtp-cell-enh"
#define LOC_FEATURE_GTP_WIFI           "gtp-wifi"
#define LOC_FEATURE_GTP_WAA            "gtp-waa"
#define LOC_FEATURE_SAP                "sap"
#define LOC_FEATURE_LAUNCH_TRIGGER_MASK   "launch-trigger-mask"

#define LOC_PROCESS_MAX_NUM_GROUPS     20
#define LOC_PROCESS_MAX_NUM_ARGS       25
#define LOC_PROCESS_MAX_ARG_STR_LENGTH 64

#define UTIL_UPDATE_CONF(conf_data, len, config_table) \
    loc_update_conf((conf_data), (len), config_table, \
                    sizeof(config_table) / sizeof(config_table[0]))

#define UTIL_READ_CONF_DEFAULT(filename) \
    loc_read_conf((filename), NULL, 0);

#define UTIL_READ_CONF(filename, config_table) \
    loc_read_conf((filename), config_table, sizeof(config_table) / sizeof(config_table[0]))

#define UTIL_READ_CONF_LONG(filename, config_table, rec_len) \
    loc_read_conf_long((filename), config_table, \
            sizeof(config_table) / sizeof(config_table[0]), (rec_len))

/*=============================================================================
 *
 *                        MODULE TYPE DECLARATION
 *
 *============================================================================*/
typedef struct
{
  const char *param_name;
  void       *param_ptr;   /* for string type, buf size need to be LOC_MAX_PARAM_STRING */
  uint8_t    *param_set;   /* indicate value set by config file */
  char        param_type;  /* 'n' for number,
                              's' for string, NOTE: buf size need to be LOC_MAX_PARAM_STRING
                              'f' for double */
} loc_param_s_type;

typedef enum {
    ENABLED,
    RUNNING,
    DISABLED,
    DISABLED_FROM_CONF,
    DISABLED_VIA_VENDOR_ENHANCED_CHECK
} loc_process_e_status;

typedef struct {
    loc_process_e_status proc_status;
    pid_t                proc_id;
    char                 name[2][LOC_MAX_PARAM_STRING];
    gid_t                group_list[LOC_PROCESS_MAX_NUM_GROUPS];
    unsigned char        num_groups;
    char                 args[LOC_PROCESS_MAX_NUM_ARGS][LOC_PROCESS_MAX_ARG_STR_LENGTH];
    char                 argumentString[LOC_MAX_PARAM_STRING];
    unsigned int         launch_trigger_mask;
} loc_process_info_s_type;

/*=============================================================================
 *
 *                          MODULE EXTERNAL DATA
 *
 *============================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *
 *                       MODULE EXPORTED FUNCTIONS
 *
 *============================================================================*/
bool isVendorEnhanced();
void setVendorEnhanced(bool vendorEnhanced);
void loc_read_conf_long(const char* conf_file_name,
                        const loc_param_s_type config_table[],
                        uint32_t table_length, uint16_t string_len);
int loc_read_conf_r_long(FILE *conf_fp, const loc_param_s_type config_table[],
                         uint32_t table_length, uint16_t string_len);
int loc_update_conf_long(const char* conf_data, int32_t length,
                         const loc_param_s_type config_table[], uint32_t table_length,
                         uint16_t string_len);

inline void loc_read_conf(const char* conf_file_name,
                          const loc_param_s_type config_table[], uint32_t table_length) {
    loc_read_conf_long(conf_file_name, config_table, table_length, LOC_MAX_PARAM_STRING);
}

inline int loc_read_conf_r(FILE *conf_fp, const loc_param_s_type config_table[],
                    uint32_t table_length) {
    return (loc_read_conf_r_long(conf_fp, config_table, table_length, LOC_MAX_PARAM_STRING));
}

inline int loc_update_conf(const char* conf_data, int32_t length,
                    const loc_param_s_type config_table[], uint32_t table_length) {
    return (loc_update_conf_long(
                    conf_data, length, config_table, table_length, LOC_MAX_PARAM_STRING));
}

// Below are the location conf file paths
extern const char LOC_PATH_GPS_CONF[];
extern const char LOC_PATH_IZAT_CONF[];
extern const char LOC_PATH_BATCHING_CONF[];
extern const char LOC_PATH_LOWI_CONF[];
extern const char LOC_PATH_SAP_CONF[];
extern const char LOC_PATH_APDR_CONF[];
extern const char LOC_PATH_XTWIFI_CONF[];
extern const char LOC_PATH_QUIPC_CONF[];
extern const char LOC_PATH_ANT_CORR[];
extern const char LOC_PATH_SLIM_CONF[];
extern const char LOC_PATH_VPE_CONF[];
extern const char LOC_PATH_QPPE_CONF[];

int loc_read_process_conf(const char* conf_file_name, uint32_t * process_count_ptr,
                          loc_process_info_s_type** process_info_table_ptr);
#ifdef __cplusplus
}
#endif

#endif /* LOC_CFG_H */
