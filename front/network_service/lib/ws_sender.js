//Copyright (C) 2018 Intel Corporation
// 
//SPDX-License-Identifier: MIT
//
exports.sendMessage = function (ws, message, code) {
    if (!!ws && ws.readyState === ws.OPEN) {
        ws.send(JSON.stringify({headers: {method: 'text', code: code || 200}, payload: `${message}`}));
    }
}

exports.sendProtocol = function (ws, headers, payload, code) {
    if (!!ws && ws.readyState === ws.OPEN) {
        var protocol = Object.assign(
            {
                headers: headers,
                payload: payload,
                code: code || 200
            }
        );
        ws.send(JSON.stringify(protocol));
    }
}