/**
 * \file quarklink_options.h
 * \brief Public option IDs for build-time selection.
 *
 * This header is intended for discoverability (IDE autocomplete) and documentation:
 * it contains the canonical list of supported PORT/TLS/KEYPROV identifiers and
 * any other user-facing options.
 *
 * End users normally only need to set `QL_CFG_PORT` at compile time; ports provide
 * defaults unless overridden.
 * \copyright Copyright (c) 2026 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef QUARKLINK_OPTIONS_H
#define QUARKLINK_OPTIONS_H

/* Port identifiers */
#define QL_PORT_ESP32IDF    1   /**< ESP32-IDF port */
#define QL_PORT_NATIVE_MOCK 999 /**< Native mock port, used for testings */

/* Key provider identifiers (the crypto backend) */
#define QL_KEYPROV_MBEDTLS         1 /**< Software keys via mbedTLS. */
#define QL_KEYPROV_UECC            2 /**< Software keys via micro-ECC. */
#define QL_KEYPROV_ESPIDF_ATECC608 3 /**< Hardware keys via ESP32 ATECC608 secure element. */
#define QL_KEYPROV_ESPIDF_DS       4 /**< Hardware keys via ESP32 Digital Signature peripheral. */

/** Software PEM backends (export device key, mTLS key_pem scratch). */
#define QL_KEYPROV_IS_SOFTWARE(kp) ((kp) == QL_KEYPROV_MBEDTLS || (kp) == QL_KEYPROV_UECC)

#endif /* QUARKLINK_OPTIONS_H */
