/****************************************************************************
 *          TEST_PKI.H
 *
 *          The certificates of the test, made by C_TEST1 before the
 *          services start (with the openssl command): a CA, the keeper's
 *          server certificate (CN=localhost), and two client certificates
 *          of the CA, test_node and other_node.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#define PKI_DIR             "/tmp/test_archive_shipping_pki"
#define PKI_CA              PKI_DIR "/ca.pem"
#define PKI_CA_KEY          PKI_DIR "/ca.key"
#define PKI_SERVER          PKI_DIR "/server.pem"
#define PKI_SERVER_KEY      PKI_DIR "/server.key"
#define PKI_NODE            PKI_DIR "/test_node.pem"
#define PKI_NODE_KEY        PKI_DIR "/test_node.key"
#define PKI_OTHER           PKI_DIR "/other_node.pem"
#define PKI_OTHER_KEY       PKI_DIR "/other_node.key"
