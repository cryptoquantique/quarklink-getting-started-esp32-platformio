/**
 * \file quarklink_esp32idf_atecc608.h
 * \brief ESP-IDF helpers for IoT Hub mTLS with an ATECC608 secure element.
 *
 * This header is only meaningful when building the QuarkLink client with the
 * ESP-IDF port and \c QL_KEYPROV_ESPIDF_ATECC608.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_ESP32IDF_ATECC608_H
#define QUARKLINK_ESP32IDF_ATECC608_H

#include "quarklink/quarklink_config.h"

#if (QL_CFG_PORT == QL_PORT_ESP32IDF)

#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_ATECC608) && defined(CONFIG_ESP_TLS_USE_SECURE_ELEMENT)

#include "quarklink/quarklink.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Verify \p ctx is ready for IoT Hub mTLS via the ATECC608 secure element.
 *
 * The private key is not exported. After a successful call, configure ESP-TLS or
 * ESP-MQTT with:
 * - trust anchor: \p ctx->iot_hub_root_cert
 * - client cert:  \p ctx->device_cert
 * - secure element: \c use_secure_element = true in \c esp_tls_cfg_t (or the
 *   equivalent field on the MQTT client config)
 *
 * \param[in] ctx Initialised, enrolled QuarkLink context.
 *
 * \retval QUARKLINK_SUCCESS \p ctx holds IoT Hub credentials and the secure element path applies.
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_NOT_INITIALISED
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE Enrolment or IoT Hub fields are missing.
 */
quarklink_return_t quarklink_esp32_check_iothub_atecc608(const quarklink_context_t *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ATECC608 enabled */

#endif /* (QL_CFG_PORT == QL_PORT_ESP32IDF) */

#endif /* QUARKLINK_ESP32IDF_ATECC608_H */
