#!/usr/bin/env nodejs
'use strict';
const SecureServer = require('../lib/secure_wss_server');
const fs = require('fs');
var privateKey  = fs.readFileSync('./server_cert/server-key.pem', 'utf8');
var certificate = fs.readFileSync('./server_cert/server-cert.pem', 'utf8');
var ca = fs.readFileSync('./server_cert/ca-cert.pem', 'utf8');
const constants = require('../lib/constants');
const crl = null;
if(fs.existsSync('./server_cert/client.crl')) {
  crl = fs.readFileSync('./server_cert/client.crl')
}
var options = {
  port: 8445,
  key: privateKey,
  cert: certificate,
  requestCert: true,
  ca: [ca],
  metaPath: __dirname + '/model/model_info.json',
  ipcProtocol: "json"
};

if(crl != null) {
  options.crl = crl;
}

if (process.platform === "win32") {
  options.socket = "\\\\.\\pipe\\" + "ipc";
} else {
  options.socket = 'ipc_socket/unix.sock';
}

if(!fs.existsSync('ipc_socket')) {
  fs.mkdirSync('ipc_socket', {mode: 0o770});
}

const route = require('../lib/router');
var server = new SecureServer(options);


var routes = {
  "text": (ws,message, adminCtx)=>{console.log(`Message from client ${message.payload}`);ws.send(JSON.stringify(message));},
  "create": route.ipcCreateHandler,
  "destroy": route.destroyHandler,
  "property": route.propertyHandler,
  "model": route.updateModel
}
var router = route.router(routes);
function incoming(ws, message, adminCtx) {
  try{
  let fn = router(message);
  !!fn && fn(ws, message, adminCtx);
  } catch(err) {
    console.log("Got Error %s", err.message);
  }
};

function unixApp(data, adminCtx){
  if(data.type == constants.msgType.ePipeID && adminCtx.pipe2pid.has(parseInt(data.payload))) {
    console.log("valid pipe %s", data.payload);
    var createJSON;
    createJSON = adminCtx.pipe2json.get(parseInt(data.payload)).create;
    var initConfig = JSON.stringify(createJSON.command_create.config);

    adminCtx.transceiver.send(initConfig);
  } else {
    console.log(data.payload.toString());
    !!adminCtx.dataCons && adminCtx.dataCons.readyState === adminCtx.dataCons.OPEN && adminCtx.dataCons.send(JSON.stringify({headers: {type: data.type, pipe_id: adminCtx.transceiver.id}, payload: data.payload}));
  }
}


server.unixUse(unixApp);
server.adminUse(incoming);
//server.dataUse(dataApp);

server.start();

process.on('SIGINT', ()=> {
  console.log('Server close due to exit');
  server.close();
});
