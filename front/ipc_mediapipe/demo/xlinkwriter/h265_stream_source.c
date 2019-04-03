/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "mediapipe_com.h"
#include "XLink.h"

#define DATA_FRAGMENT_SIZE (1024)

static XLinkHandler_t handler = {
    .devicePath = "/tmp/xlink_mock",
    .deviceType = PCIE_DEVICE
};
static XLinkGlobalHandler_t ghandler = {
    .protocol = PCIE,
};

static void *read_xlink_routine(void *ctx)
{
    while (1) {
        uint32_t msg_size = 0;
        uint8_t *message = NULL;
        int status = XLinkReadData(&handler, 0x401, &message, &msg_size);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("realstreamtest: XLinkReadData failed, status=%d", status);
            return NULL;
        }
        LOG_DEBUG("realstreamtest: XLinkReadData success, readBytes=%u", msg_size);
        status = XLinkReleaseData(&handler, 0x401, message);
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("realstreamtest: XLinkReleaseData failed, status=%d", status);
            return NULL;
        }
    }
}

static gboolean
get_real_stream_data_and_send(void)
{
    while (XLinkInitialize(&ghandler) != X_LINK_SUCCESS) {
        LOG_ERROR("realstreamtest: can't init xlink");
    }
    while (XLinkBootRemote(&handler, DEFAULT_NOMINAL_MAX) != X_LINK_SUCCESS) {
        LOG_ERROR("realstreamtest: can't boot remote");
    }
    while (XLinkConnect(&handler) != X_LINK_SUCCESS) {
        LOG_ERROR("realstreamtest: can't xlink connect");
    }
    OperationMode_t operationType = RXB_TXB;
    channelId_t echoChannel = 0x401;
    //XLinkOpenStream stubbed for PCIe
    while (XLinkOpenChannel(&handler, echoChannel, operationType,
                            DATA_FRAGMENT_SIZE, 0) != X_LINK_SUCCESS) {
        LOG_ERROR("realstreamtest: can't open channel");
    }
    g_thread_new("thread", read_xlink_routine, NULL);
    //open and read file
    char filename[100];
    for (int i = 0; i < 333; i++) {
        //create file name
        if (i >= 0 && i <= 9) {
            sprintf(filename, "./packet/videopacket_000%d.bin", i);
        } else if (i > 9 && i <= 99) {
            sprintf(filename, "./packet/videopacket_00%d.bin", i);
        } else if (i > 99) {
            sprintf(filename, "./packet/videopacket_0%d.bin", i);
        }
        //open and get file size
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            LOG_ERROR("realstreamtest: Open file error!");
            return FALSE;
        }
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        //malloc and read file
        uint8_t *data = (uint8_t *)malloc(filesize);
        if (NULL == data) {
            LOG_ERROR("realstreamtest: malloc failed!\n");
            fclose(fp);
            return FALSE;
        }
        fseek(fp, 0, SEEK_SET);
        fread(data, 1, filesize, fp);
        fclose(fp);
        int streamId, status;
        status =  XLinkWriteData(&handler, echoChannel, data,
                                 filesize);
        if (NULL != data) {
            free(data);
        }
        if (status != X_LINK_SUCCESS) {
            LOG_ERROR("realstreamtest: Failed to write the message to the device.");
            return FALSE;
        }
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    if (!get_real_stream_data_and_send()) {
        return -1;
    }
    return 0;
}
