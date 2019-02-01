#!/usr/bin/env node
//Copyright (C) 2018 Intel Corporation
// 
//SPDX-License-Identifier: MIT
//
'use strict';
const fs = require('fs');
const fileHelper = require('../lib/file_helper');
const constants = require('../lib/constants');
const SecureWebsocket = require('../lib/secure_client');
var crl = null;
if(fs.existsSync('./client_cert/server.crl')) {
  crl = fs.readFileSync('./client_cert/server.crl')
}

var options = {
  host: '127.0.0.1',
  port: 8445,
  ca: fs.readFileSync('./client_cert/ca-cert.pem'),
  checkServerIdentity: () => undefined,
  key: fs.readFileSync('./client_cert/client-key.pem'),
  cert: fs.readFileSync('./client_cert/client-cert.pem'),
  route: 'data'
};

if(crl != null) {
  options.crl = crl;
}
var pipe2file = new Map();

var ws = new SecureWebsocket(options);

function incoming(data) {
  var metaData = Buffer.from(data.payload);
  var pipe_id = data.headers.pipe_id;
  var con = 'pipe_' + pipe_id.toString();
  let path = './' + con;
  if(!fs.existsSync(path)) {
	  fs.mkdirSync(path);
  }
  if (data.headers.type == constants.msgType.eMetaJPG) {
      let temp = 0;
      if(pipe2file.has(pipe_id)) {
        temp = pipe2file.get(pipe_id);
      }
      let image_name = 'image_' + temp + '.jpg';
      let path = './' + con + '/' + image_name;
      console.log('Save jpeg:' + path);
      fileHelper.saveBuffer(path, metaData);
      temp++;
      pipe2file.set(pipe_id, temp);
  } else if(data.headers.type == constants.msgType.eMetaText) {
      let path = './' + con + '/output.txt';
      console.log('Save txt to ' + path + ": " + metaData);
      fs.appendFileSync(path, metaData);
  }
}

ws.on('message', incoming);

