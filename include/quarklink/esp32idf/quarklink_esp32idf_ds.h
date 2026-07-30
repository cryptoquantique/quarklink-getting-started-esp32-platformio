/**
 * \file quarklink_esp32idf_ds.h
 * \brief ESP-IDF specific helpers for using the ESP32 Digital Signature peripheral.
 *
 * This header is only meaningful when building the QuarkLink client with the ESP-IDF port.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_ESP32IDF_DS_H
#define QUARKLINK_ESP32IDF_DS_H

#include "quarklink/quarklink_config.h"

#if (QL_CFG_PORT == QL_PORT_ESP32IDF)

#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_DS) && defined(CONFIG_ESP_TLS_USE_DS_PERIPHERAL)

#include "quarklink/quarklink.h"
/* esp_ds_data_ctx_t (in esp_ds_rsa.h on IDF >= 5.5, else esp_rsa_sign_alt.h) */
#include "rsa_sign_alt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Fill an \c esp_ds_data_ctx_t for IoT Hub mTLS via the DS peripheral.
 *
 * Typical usage after \ref quarklink_enrol():
 * - declare \c static esp_ds_data_ctx_t ds_data;
 * - call this function with the enrolled \p ctx
 * - configure ESP-TLS / ESP-MQTT with:
 *   - trust anchor: \p ctx->iot_hub_root_cert
 *   - client cert:  \p ctx->device_cert
 *   - client key:   \p ds_data via \c credentials.authentication.ds_data
 *
 * \param[in]  ctx      Initialised, enrolled QuarkLink context.
 * \param[out] ds_data  Must point to an \c esp_ds_data_ctx_t.
 *
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_NOT_INITIALISED
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE Enrolment cert absent or DS key material unavailable.
 * \retval QUARKLINK_ERROR DS key load failed.
 */
quarklink_return_t quarklink_esp32_get_ds_data(const quarklink_context_t *ctx,
                                               esp_ds_data_ctx_t *ds_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DS enabled */

#endif /* (QL_CFG_PORT == QL_PORT_ESP32IDF) */

#endif /* QUARKLINK_ESP32IDF_DS_H */
