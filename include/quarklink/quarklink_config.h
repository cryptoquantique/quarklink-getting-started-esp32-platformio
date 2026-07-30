/**
 * \file quarklink_config.h
 * \brief Single entrypoint for compile-time configuration and validation.
 *
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QL_CONFIG_H
#define QL_CONFIG_H

/* Canonical option IDs + user-facing defaults */
#include "quarklink/quarklink_options.h"

/* ========= Build-time selection ========= */
/* Most users only need to define `QL_CFG_PORT`.
 * The selected port provides defaults for `QL_CFG_KEYPROV` unless explicitly overridden.
 */

/** Selected port (default: \c QL_PORT_ESP32IDF). Override at build time with \c -DQL_CFG_PORT=... */
#ifndef QL_CFG_PORT
#define QL_CFG_PORT QL_PORT_ESP32IDF
#endif

/* ========= Port routing ========= */
/* Include the appropriate port-specific defaults */
#if (QL_CFG_PORT == QL_PORT_ESP32IDF)
#include "quarklink/port/esp32idf/ql_port_esp32idf_defaults.h"
#elif (QL_CFG_PORT == QL_PORT_NATIVE_MOCK)
#include "quarklink/port/native_mock/ql_port_native_mock_defaults.h"
#else
#error \
    "Unsupported QL_CFG_PORT. Define QL_CFG_PORT to a supported QL_PORT_* value from quarklink_options.h."
#endif

/**
 * DS shared enrol/device key.
 * Default \c 1 when \c QL_KEYPROV_ESPIDF_DS (as current ML-KEM DS patch requires one shared slot).
 * Override: \c -DQL_CFG_DS_SINGLE_KEY=0 to force separate ESK/DSK slots on DS builds.
 */
#ifndef QL_CFG_DS_SINGLE_KEY
#if (QL_CFG_KEYPROV == QL_KEYPROV_ESPIDF_DS)
#define QL_CFG_DS_SINGLE_KEY 1
#else
#define QL_CFG_DS_SINGLE_KEY 0
#endif
#endif

/* Central compile-time validation for selections and feature requirements */
#include "quarklink/quarklink_validate.h"

/* Logging (optional compile-time overrides):
 *   QL_LOG_LEVEL              0=off, 1=error, 2=warning, 3=info, 4=debug (default: warning)
 *   QL_LOG_INCLUDE_LOCATION   0=opaque user-facing with "QuarkLink:" prefix (default),
 *                             1=include file, line, and function in E/W/I output
 */

#endif /* QL_CONFIG_H */
