/**
 * \file ql_port_esp32idf_defaults.h
 * \brief Default selections for the ESP32-IDF port.
 *
 * Ports set defaults option values such as crypto backend selection to keep the end-user configuration simple:
 * most users will only need to define `QL_CFG_PORT` at compile time.
 * 
 * Also declares port capabilities (storage, TLS, firmware update, etc.) used by `quarklink_validate.h`.
 *
 * This header is part of the public include tree, meaning that builds don't need extra `-I src`.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QL_PORT_ESP32IDF_DEFAULTS_H
#define QL_PORT_ESP32IDF_DEFAULTS_H

/* Default crypto backend: mbedTLS software keys (for now) */
#ifndef QL_CFG_KEYPROV
#define QL_CFG_KEYPROV QL_KEYPROV_MBEDTLS
#endif

/* ========= Port capabilities (what the port supports) =========
 * These may be used by the core to conditionally compile features, 
 * or simply as documentation for port capabilities.
 */

/* Required minimal persistence mechanism. */
#define QL_PORT_HAS_STORAGE 1

/* Optional interfaces (may be 0 until implemented/wired). */
#define QL_PORT_HAS_LOG 1

/* Cryptographic random bytes. Required when uECC is selected. */
#define QL_PORT_HAS_RANDOM 1

/* TLS transport (qlp_tls_*). Required by ql_https_core and QuarkLink HTTPS flows. */
#define QL_PORT_HAS_TLS 1

/* OTA / firmware update (qlp_firmware_update_*). Set to 0 on ports without OTA support. */
#ifndef QL_PORT_HAS_FWUPDATE
#define QL_PORT_HAS_FWUPDATE 1
#endif

#endif /* QL_PORT_ESP32IDF_DEFAULTS_H */
