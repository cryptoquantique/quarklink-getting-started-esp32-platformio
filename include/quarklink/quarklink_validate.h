/**
 * \file quarklink_validate.h
 * \brief Central compile-time validation for configuration selections and feature requirements.
 *
 * This header is included by `quarklink_config.h` after selecting a port and applying
 * port defaults.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_VALIDATE_H
#define QUARKLINK_VALIDATE_H

/* ========= Basic selection validation ========= */

#ifndef QL_CFG_PORT
#error \
    "QL_CFG_PORT is not defined. Define QL_CFG_PORT to a QL_PORT_* value (e.g. -DQL_CFG_PORT=QL_PORT_ESP32IDF)."
#endif

#ifndef QL_CFG_KEYPROV
#error \
    "QL_CFG_KEYPROV is not defined. Either define it (e.g. -DQL_CFG_KEYPROV=QL_KEYPROV_MBEDTLS) or include the selected port defaults."
#endif

/* ========= Port-scoped validation ========= */
/* Add port-specific validation logic here */

/* ESP-IDF-only key providers (DS, secure element). */
#if (QL_CFG_PORT != QL_PORT_ESP32IDF)
#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_DS) || (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_ATECC608)
#error "QL_KEYPROV_ESPIDF_DS and QL_KEYPROV_ESPIDF_ATECC608 require QL_PORT_ESP32IDF."
#endif
#endif

#if (QL_CFG_PORT == QL_PORT_NATIVE_MOCK)
#if (QL_CFG_KEYPROV != QL_KEYPROV_UECC) && (QL_CFG_KEYPROV != QL_KEYPROV_MBEDTLS)
#warning \
    "QL_PORT_NATIVE_MOCK is intended only for software key providers: QL_KEYPROV_UECC (default) or QL_KEYPROV_MBEDTLS."
#endif
#endif

/* ESP32-IDF port-specific validation */
#if (QL_CFG_PORT == QL_PORT_ESP32IDF)
/* ESP-IDF Kconfig symbols (CONFIG_*) are defined in sdkconfig.h.
   * Include sdkconfig.h here so we are able to check CONFIG_* symbols.
   * Also ensure we are in a esp-idf environment.
   */
#if defined(ESP_PLATFORM) && defined(__has_include)
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif
/* ESP-IDF key providers require matching sdkconfig options. */
#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_DS) && !defined(CONFIG_ESP_TLS_USE_DS_PERIPHERAL)
#error "QL_KEYPROV_ESPIDF_DS requires ESP-IDF sdkconfig option CONFIG_ESP_TLS_USE_DS_PERIPHERAL."
#endif

#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_ATECC608) && !defined(CONFIG_ESP_TLS_USE_SECURE_ELEMENT)
#error \
    "QL_KEYPROV_ESPIDF_ATECC608 requires ESP-IDF sdkconfig option CONFIG_ESP_TLS_USE_SECURE_ELEMENT."
#endif
#endif

/* ========= Capability / feature validation ========= */

/* Storage is required by core/crypto for persistence. */
#if !defined(QL_PORT_HAS_STORAGE) || (QL_PORT_HAS_STORAGE != 1)
#error \
    "Selected port must define QL_PORT_HAS_STORAGE=1 (and port must implement qlp_storage_* APIs)."
#endif

/* If logging is enabled, the port must provide a printing function */
#if (QL_LOG_LEVEL >= QL_LOG_LEVEL_ERROR)
#if !defined(QL_PORT_HAS_LOG) || (QL_PORT_HAS_LOG != 1)
#error \
    "QL_LOG_LEVEL enables logging, but selected port does not provide QL_PORT_HAS_LOG=1 (port must implement qlp_log_write)."
#endif
#endif

/* uECC backend requires cryptographic random bytes. */
#if (QL_CFG_KEYPROV == QL_KEYPROV_UECC)
#if !defined(QL_PORT_HAS_RANDOM) || (QL_PORT_HAS_RANDOM != 1)
#error "QL_KEYPROV_UECC requires QL_PORT_HAS_RANDOM=1 (port must implement qlp_random_get)."
#endif
#endif

/* HTTPS core and QuarkLink API flows require TLS. */
#if !defined(QL_PORT_HAS_TLS) || (QL_PORT_HAS_TLS != 1)
#error "Selected port must define QL_PORT_HAS_TLS=1 (and port must implement qlp_tls_* APIs)."
#endif

/** Optional OTA / firmware update (qlp_firmware_update_*). Ports may set to 0 or omit (defaults to 0). */
#ifndef QL_PORT_HAS_FWUPDATE
#define QL_PORT_HAS_FWUPDATE 0
#endif
#if (QL_PORT_HAS_FWUPDATE != 1)
#warning "Firmware update is not available in the selected configuration (QL_PORT_HAS_FWUPDATE=0)."
#endif

/* ========= Selection sanity ========= */

#if (QL_CFG_KEYPROV != QL_KEYPROV_MBEDTLS) && (QL_CFG_KEYPROV != QL_KEYPROV_UECC) && \
    (QL_CFG_KEYPROV != QL_KEYPROV_ESPIDF_ATECC608) && (QL_CFG_KEYPROV != QL_KEYPROV_ESPIDF_DS)
#error \
    "Unsupported QL_CFG_KEYPROV. Choose one of: QL_KEYPROV_MBEDTLS, QL_KEYPROV_UECC, QL_KEYPROV_ESPIDF_ATECC608, QL_KEYPROV_ESPIDF_DS."
#endif

#endif /* QUARKLINK_VALIDATE_H */
