'use strict';
const wsSender = require('./ws_sender.js');
const { spawn } = require('child_process');
const path = require('../lib/path_parser')
const fileHelper = require('../lib/file_helper');
const fs = require('fs');
const constants = require('./constants.js')
var pipe_base = 0;
exports.router = function route(obj) {
    return function(message) {
        var method = message.headers.method || 'unknown';
        console.log('\x1b[34mDispatcher %s\x1b[0m', method);
        if(!obj[method]) {
            return null;
        }
        var fn = obj[method];
        return fn;
    }
}

exports.createHandler = function createHandler(ws, message, adminCtx) {
    var create_json = fileHelper.safelyJSONParser(message.payload);
    if(create_json === null) {
        wsSender.sendMessage(ws, 'json format error', 400);
        return;
    }
    for (let i = 0; i < create_json.command_create.pipe_num; i++) {
        let pipe_id = pipe_base ++;
        let gst_cmd = `hddl_mediapipe2 -i  ${pipe_id} -u ${adminCtx.socketURL}`;
        console.log("prepare to launch %s", gst_cmd);

        let child = spawn(gst_cmd, {
            shell: true
        });

        child.stderr.on('data', function (data) {
            //console.error("STDERR:", data.toString());
        });

        child.stdout.on('data', function (data) {
            //console.log("STDOUT:", data.toString());
        });

        child.on('exit', function (exitCode) {
            console.log("Child exited with code: " + exitCode);
            if(ws.readyState == ws.OPEN) {
                wsSender.sendProtocol(ws, {method: 'pipe_delete'}, [pipe_id], 200);
            }
            pipes.delete(pipe_id);
            adminCtx.pipe2pid.delete(pipe_id);
            adminCtx.pipe2socket.delete(pipe_id);
            adminCtx.pipe2json.delete(pipe_id);
        });

      var pipes = adminCtx.client2pipe.get(ws.id) || new Set();
      pipes.add(pipe_id);
      adminCtx.pipe2pid.set(pipe_id, {cid: ws.id, child: child});
      updatePipeJSON(adminCtx.pipe2json, create_json, pipe_id, 'create');
      console.log('create pipe %s', pipe_id);
      wsSender.sendMessage(ws, `pipe_create ${pipe_id}`);
      wsSender.sendProtocol(ws, {method: 'pipe_id'}, Array.from(pipes), 200);
      wsSender.sendProtocol(ws, {method: 'pipe_info', pipe_id: pipe_id}, create_json, 200);
    }
}

exports.propertyHandler =  function propertyHandler(ws, message, adminCtx) {
    var property_json = fileHelper.safelyJSONParser(message.payload);
    var pipe_id = message.headers.pipe_id;
    console.log(message.headers);
    if(property_json === null) {
        wsSender.sendMessage(ws, 'json format error', 400);
        return;
    }
    console.log(`Setting property`);
    if(adminCtx.pipe2socket.has(pipe_id)) {
        var socket = adminCtx.pipe2socket.get(pipe_id);
        socket.send(JSON.stringify({type:constants.msgType.eProperty, payload: property_json.payload}));
    } else {
        console.log(`pipe_id ${pipe_id} not exists`)
    }

}

exports.destroyHandler = function destroyHandler(ws, message, adminCtx) {
    var destroy_json = fileHelper.safelyJSONParser(message.payload);
    if(destroy_json === null) {
        wsSender.sendMessage(ws, 'json format error', 400);
        return;
    } else if (!adminCtx.client2pipe.has(ws.id)) {
        wsSender.sendMessage(ws, `client ${ws.id} has no pipes yet`, 400);
        return;
    }

    var pipes = adminCtx.client2pipe.get(ws.id);
    var pipe_id = parseInt(message.headers.pipe_id);
    if(pipes.has(pipe_id)){
        var socket = adminCtx.pipe2socket.get(pipe_id);
        if(!!socket) {
            console.log('delete pipe %s', pipe_id);
            //socket.send(message.payload, constants.msgType.ePipeDestroy);
            socket.send(JSON.stringify({type:constants.msgType.ePipeDestroy, payload: ""}));
        } else {
            console.log("Cannot find pipe %s",pipe_id);
        }
        //ws.send(JSON.stringify({headers: {method: 'pipe_delete'}, payload: Array.from(pipes), code: 200}));
        adminCtx.pipe2socket.delete(pipe_id);
        adminCtx.pipe2pid.delete(pipe_id);
        adminCtx.pipe2json.delete(pipe_id);
        pipes.delete(pipe_id);
    } else {
        wsSender.sendMessage(ws, `pipe ${pipe_id} not exists`, 400);
    }
}

function updatePipeJSON(pipe2json, obj, pipe_id, type) {
    var pipeJSON = pipe2json.get(pipe_id) || {};
    pipeJSON[type] = obj;
    pipe2json.set(pipe_id, pipeJSON);
}

exports.ipcCreateHandler =  function (ws, message, adminCtx) {
    var create_json = fileHelper.safelyJSONParser(message.payload);
    if(create_json === null) {
        wsSender.sendMessage(ws, 'json format error', 400);
        return;
    }
    var clientID = ws.id;
    for (let i = 0; i < create_json.command_create.pipe_num; i++) {
        let pipe_id = pipe_base ++;
        let gst_cmd = `hddl_mediapipe2  -u ${adminCtx.socketURL} -i ${pipe_id}`;
        console.log('cmd %s', gst_cmd);
        let pipes;
        if(adminCtx.client2pipe.has(ws.id)) {
          pipes = adminCtx.client2pipe.get(ws.id);
        } else {
            pipes =  new Set();
            adminCtx.client2pipe.set(ws.id, pipes);
        }
      let child = spawn(gst_cmd, {
        shell: true
      });

      child.stderr.on('data', function (data) {
        console.error("STDERR:", data.toString());
      });

      child.stdout.on('data', function (data) {
        console.log("STDOUT:", data.toString());
      });

      child.on('exit', function (exitCode) {
        console.log("Child exited with code: " + exitCode);
        let clientWS = adminCtx.wsConns.get(clientID);
        if(!!clientWS && clientWS.readyState == clientWS.OPEN) {
            console.log("delete pipe_id %s", pipe_id);
            clientWS.send(JSON.stringify({headers: {method: 'pipe_delete'}, payload: [pipe_id], code: 200}));
        }
        pipes.delete(pipe_id);
        adminCtx.pipe2pid.delete(pipe_id);
        adminCtx.pipe2socket.delete(pipe_id);
        adminCtx.pipe2json.delete(pipe_id);
        //update controller when pipeline exit
      });

      pipes.add(pipe_id);
      adminCtx.pipe2pid.set(pipe_id, {cid: ws.id, child: child});
      updatePipeJSON(adminCtx.pipe2json, create_json, pipe_id, 'create');
      console.log('create pipe %s', pipe_id);
      wsSender.sendMessage(ws, `pipe_create ${pipe_id}`);
      wsSender.sendProtocol(ws, {method: 'pipe_id'}, Array.from(pipes), 200);
      wsSender.sendProtocol(ws, {method: 'pipe_info', pipe_id: pipe_id}, create_json, 200);
    }

}

exports.mockCreateHandler =  function (ws, message, adminCtx) {
    var create_json = fileHelper.safelyJSONParser(message.payload);
    if(create_json === null || ! create_json.hasOwnProperty('command_create') || ! create_json.command_create.hasOwnProperty('pipe_num')) {
        wsSender.sendMessage(ws, 'json format error', 400);
        return;
    }
    for (let i = 0; i < create_json.command_create.pipe_num; i++) {
        let pipe_id = pipe_base ++;
        let gst_cmd = `./socket_client ${adminCtx.socketURL} ${pipe_id}`;
        console.log('cmd %s', gst_cmd);
        let pipes;
        if(adminCtx.client2pipe.has(ws.id)) {
            pipes = adminCtx.client2pipe.get(ws.id);
        } else {
            pipes =  new Set();
            adminCtx.client2pipe.set(ws.id, pipes);
        }
        let child = spawn(gst_cmd, {
            shell: true
        });

        child.stderr.on('data', function (data) {
            console.error("STDERR:", data.toString());
        });

        child.stdout.on('data', function (data) {
            console.log("STDOUT:", data.toString());
        });

        child.on('exit', function (exitCode) {
            console.log("Child exited with code: " + exitCode);
            pipes.delete(pipe_id);
            adminCtx.pipe2pid.delete(pipe_id);
            adminCtx.pipe2socket.delete(pipe_id);
            adminCtx.pipe2json.delete(pipe_id);
            //update controller when pipeline exit
            if(ws.readyState == ws.OPEN)
                ws.send(JSON.stringify({headers: {method: 'pipe_delete'}, payload: [pipe_id], code: 200}));
        });

        pipes.add(pipe_id);
        adminCtx.pipe2pid.set(pipe_id, {cid: ws.id, child: child});
        updatePipeJSON(adminCtx.pipe2json, create_json, pipe_id, 'create');
        console.log('create pipe %s', pipe_id);
        wsSender.sendMessage(ws, `pipe_create ${pipe_id}`);
        ws.send(JSON.stringify({headers: {method: 'pipe_id'}, payload: [pipe_id], code: 200}));
    }

}

exports.updateModel = function (ws, model, adminCtx){
    if(!model.headers.hasOwnProperty('path')) {
        throw new Error('x1b[31mNo Path in Headers\x1b[0m');
    }
    var filePath = model.headers.path;
    var dir = path.dirname(path.dirname(filePath));
    saveBuffer(filePath, model.payload);
    var modelMeta = JSON.stringify(fileHelper.updateCheckSum(filePath, model.checkSum));
    fs.writeFileSync(path.join(dir, 'model_info.json'), modelMeta);
    wsSender.sendProtocol(ws,{method: 'checkSum'}, modelMeta);
  }
  function saveBuffer(filePath, buffer) {
    var folderName = path.dirname(filePath);
    if(!fs.existsSync(folderName))
    {
      console.log('making dir %s', './' + folderName);
      fs.mkdirSync('./' + folderName, { recursive: true });
    }
    var stream = fs.createWriteStream(filePath);
    stream.on("open", ()=>{
      stream.write(buffer);
      stream.end();
    });
  }