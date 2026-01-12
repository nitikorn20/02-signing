/********************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2024-2025 TESA
 * All rights reserved.</center></h2>
 *
 * This source code and any compilation or derivative thereof is the
 * proprietary information of TESA and is confidential in nature.
 *
 ********************************************************************************
 * Project : OPTIGA Trust M Connectivity Tutorial Series
 ********************************************************************************
 * Module  : Part 2 - Digital Signatures Demo
 * Purpose : Demonstrate ECDSA P-256 signing and verification with PSA Crypto API.
 *           Shows how to generate ephemeral keys, sign messages, and verify
 *           signatures on-device using hardware-backed cryptography.
 * Design  : See blog-02-signing.md for detailed explanation
 ********************************************************************************
 * @file    main.c
 * @brief   ECDSA digital signature demo with TF-M integration
 * @author  TESA Workshop Team
 * @date    January 11, 2026
 * @version 1.0.0
 *
 * @note    Based on Infineon CE241509 - TF-M Crypto Application Example
 *          Simplified to focus on ECDSA signing only (removed SHA256 and AES-AEAD)
 *
 * @see     https://github.com/TESA-Workshops/psoc-edge-optiga-02-signing
 ********************************************************************************
 * Original Copyright Notice:
 * (c) 2024-2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG.  SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/* -------------------------------------------------------------------- */
/* Includes                                                             */
/* -------------------------------------------------------------------- */
/* --------------------   */
/* Standard Library       */
/* --------------------   */
#include <stdio.h>

/* --------------------   */
/* Infineon Libraries     */
/* --------------------   */
#include "cybsp.h"
#include "cy_pdl.h"
#include "ifx_platform_api.h"

/* --------------------   */
/* TF-M & PSA APIs        */
/* --------------------   */
#include "tfm_ns_interface.h"
#include "os_wrapper/common.h"
#include "psa/crypto.h"


/* -------------------------------------------------------------------- */
/* Macros                                                               */
/* -------------------------------------------------------------------- */

/** @brief ECDSA P-256 key size in bits */
#define EC_KEY_BITS                   ((size_t) 256)

/** @brief ECDSA signature size (R + S components, 32 bytes each) */
#define EC_SIGNATURE_SIZE             (2*(EC_KEY_BITS/8))

/** @brief Number of bytes to print per line in hex dump */
#define PRNT_BYTES_PER_LINE           (16u)

/** @brief CM55 boot timeout in microseconds */
#define CM55_BOOT_WAIT_TIME_USEC      (10U)

/** @brief CM55 application boot address */
#define CM55_APP_BOOT_ADDR            (CYMEM_CM33_0_m55_nvm_START + \
                                        CYBSP_MCUBOOT_HEADER_SIZE)


/* -------------------------------------------------------------------- */
/* Global Variables                                                     */
/* -------------------------------------------------------------------- */
/* (None) */


/* -------------------------------------------------------------------- */
/* Function Prototypes                                                  */
/* -------------------------------------------------------------------- */
/* (None) */


/* -------------------------------------------------------------------- */
/* Function Definitions                                                 */
/* -------------------------------------------------------------------- */

/**
 * @brief Main function - ECDSA signing and verification demo
 *
 * Demonstrates the complete flow of digital signature operations:
 * 1. Generate an ephemeral ECDSA P-256 key pair
 * 2. Sign a test message using the private key
 * 3. Verify the signature using the public key
 *
 * The demo uses PSA Crypto API which abstracts the underlying hardware
 * (OPTIGA Trust M) for cryptographic operations.
 *
 * @return int  Exit status (never returns in normal operation)
 *
 * @note This is a simplified demo using ephemeral keys. Production systems
 *       would use persistent device keys stored in OPTIGA Trust M.
 */
int main(void)
{
    cy_rslt_t result;
    uint32_t rslt;
    psa_status_t status;
    const unsigned char input_data[] = "Hello World";
    uint8_t signature[EC_SIGNATURE_SIZE];
    size_t signature_len;
    psa_key_id_t ec_key_id;
    psa_key_attributes_t ec_key_attributes;
    unsigned char out_buf[256];
    int buf_size;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize TF-M interface */
    rslt = tfm_ns_interface_init();
    if(rslt != OS_WRAPPER_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Clear screen and print banner */
    buf_size = sprintf((char*)out_buf, "\x1b[2J\x1b[;H"
                "=======================================================\r\n"
                "  OPTIGA Trust M - Digital Signatures (ECDSA)\r\n"
                "  PSoC Edge E84 | TF-M Secure Platform\r\n"
                "=======================================================\r\n\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Initialize PSA Crypto subsystem */
    psa_crypto_init();

    /* ========== Step 1: Generate ECDSA Key Pair ========== */
    buf_size = sprintf((char*)out_buf, "========== Step 1: Generate ECDSA Key Pair ==========\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "Generating EC P-256 key pair...\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Set key attributes for ECDSA P-256 */
    psa_set_key_usage_flags(&ec_key_attributes,
              PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&ec_key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_set_key_type(&ec_key_attributes,
            PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&ec_key_attributes, EC_KEY_BITS);

    /* LEARNING DEMO: Use ephemeral (volatile) key for simplicity
     *
     * PSA_KEY_LIFETIME_VOLATILE:
     *   - Key generated in RAM each boot
     *   - Automatically destroyed on reset
     *   - Good for learning ECDSA concepts
     *
     * PRODUCTION: Use OPTIGA persistent device key instead
     *   - PSA_KEY_LIFETIME_PERSISTENT with OPTIGA key ID
     *   - Private key never leaves secure hardware
     *   - See Part 3 tutorial for production implementation
     */
    psa_set_key_lifetime(&ec_key_attributes, PSA_KEY_LIFETIME_VOLATILE);

    /* Generate ephemeral key pair */
    status = psa_generate_key(&ec_key_attributes, &ec_key_id);
    if(status != PSA_SUCCESS)
    {
        buf_size = sprintf((char*)out_buf, "    [FAIL] Key generation failed\r\n\n");
        ifx_platform_log_msg(out_buf, buf_size);
        CY_ASSERT(0);
    }

    buf_size = sprintf((char*)out_buf, "    [OK] EC P-256 key pair generated\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "    - Algorithm: ECDSA with SHA-256\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "    - Curve: NIST P-256 (secp256r1)\r\n\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* ========== Step 2: Sign Message ========== */
    buf_size = sprintf((char*)out_buf, "========== Step 2: Sign Message ==========\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "Message: \"%s\"\r\n", input_data);
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "Signing with EC private key...\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Sign message using ECDSA with SHA-256 */
    status = psa_sign_message(ec_key_id, PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                              input_data, sizeof(input_data), signature,
                              sizeof(signature), &signature_len);
    if(status != PSA_SUCCESS)
    {
        buf_size = sprintf((char*)out_buf, "    [FAIL] Signature generation failed\r\n\n");
        ifx_platform_log_msg(out_buf, buf_size);
        CY_ASSERT(0);
    }

    buf_size = sprintf((char*)out_buf, "    [OK] Signature generated (%d bytes)\r\n\n", signature_len);
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "Signature (hex):\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Print signature in hex format */
    for(int i = 0; i < ((signature_len/PRNT_BYTES_PER_LINE) + ((signature_len%PRNT_BYTES_PER_LINE) ? 1: 0)); i++)
    {
        int j;
        /* Print 16 bytes per line */
        for(j = 0; j < PRNT_BYTES_PER_LINE; j++)
        {
            if((i*PRNT_BYTES_PER_LINE + j) >= signature_len)
            {
                break;
            }
            sprintf((char*)(out_buf + 5*j), "0x%02x ", signature[(i*PRNT_BYTES_PER_LINE + j)]);
        }
        buf_size = sprintf((char*)(out_buf + 5*j), "\r\n");
        ifx_platform_log_msg(out_buf, ((j*5) + buf_size));
    }

    buf_size = sprintf((char*)out_buf, "\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* ========== Step 3: Verify Signature ========== */
    buf_size = sprintf((char*)out_buf, "========== Step 3: Verify Signature ==========\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "Verifying signature with EC public key...\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Verify signature using ECDSA */
    status = psa_verify_message(ec_key_id, PSA_ALG_ECDSA(PSA_ALG_SHA_256),
               input_data, sizeof(input_data), signature, signature_len);
    if(status != PSA_SUCCESS)
    {
        buf_size = sprintf((char*)out_buf, "    [FAIL] Signature verification failed\r\n\n");
        ifx_platform_log_msg(out_buf, buf_size);
        CY_ASSERT(0);
    }

    buf_size = sprintf((char*)out_buf, "    [OK] Signature verified\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "    [OK] Message authenticity confirmed\r\n\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "=======================================================\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "  Demo completed successfully!\r\n");
    ifx_platform_log_msg(out_buf, buf_size);

    buf_size = sprintf((char*)out_buf, "=======================================================\r\n\n");
    ifx_platform_log_msg(out_buf, buf_size);

    /* Destroy key when done */
    psa_destroy_key(ec_key_id);

    /* Enable CM55 */
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);

    for (;;)
    {
        /* Receive and forward IPC requests from M55 to TF-M */
        result = mtb_srf_ipc_receive_request(&cybsp_mtb_srf_relay_context, MTB_IPC_NEVER_TIMEOUT);
        if(result != CY_RSLT_SUCCESS)
        {
            CY_ASSERT(0);
        }
        result =  mtb_srf_ipc_process_pending_request(&cybsp_mtb_srf_relay_context);
        if(result != CY_RSLT_SUCCESS)
        {
            CY_ASSERT(0);
        }
    }
}
/* [] END OF FILE */
