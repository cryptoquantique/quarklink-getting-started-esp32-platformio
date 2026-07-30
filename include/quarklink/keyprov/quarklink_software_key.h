/**
 * \file quarklink_software_key.h
 * \brief Helpers when the device private key is held in software (MBEDTLS / UECC).
 *
 * Include this header when \c QL_KEYPROV_IS_SOFTWARE(\c QL_CFG_KEYPROV).
 * For hardware-backed keys, use the ESP32 supplement headers under \c quarklink/esp32idf/.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_SOFTWARE_KEY_H
#define QUARKLINK_SOFTWARE_KEY_H

#include "quarklink/quarklink_config.h"

#if QL_KEYPROV_IS_SOFTWARE(QL_CFG_KEYPROV)

#include "quarklink/quarklink.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Export the device private key as PEM for IoT Hub mTLS.
 *
 * Typical MQTTS setup after \ref quarklink_enrol():
 * - trust anchor: \p ctx->iot_hub_root_cert
 * - client cert:  \p ctx->device_cert
 * - client key:   \p buffer filled by this function
 * - broker:       \p ctx->iot_hub_endpoint and \p ctx->iot_hub_port (usually 8883)
 *
 * \param[in]  ctx    Initialised, enrolled QuarkLink context.
 * \param[out] buffer Output buffer for the NUL-terminated PEM key.
 * \param[in]  buffer_len Size of \p buffer (\>= \c QUARKLINK_MAX_KEY_LENGTH).
 *
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_NOT_INITIALISED
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE Key export failed or enrolment cert absent.
 */
quarklink_return_t quarklink_export_device_key_pem(const quarklink_context_t *ctx,
                                                   char *buffer,
                                                   size_t buffer_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* software keyprov */

#endif /* QUARKLINK_SOFTWARE_KEY_H */
