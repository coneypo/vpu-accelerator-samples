//Copyright (C) 2018 Intel Corporation
// 
//SPDX-License-Identifier: MIT
//
'use strict';
const wsSender = require('./ws_sender.js');
const { spawn } = require('child_process');
const path = require('../lib/path_parser')
const fileHelper = require('../lib/file_helper');
const fs = require('fs');
const BufferStream = require('../lib/single_buffer_stream');
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
        socket.send(JSON.stringify(property_json), constants.msgType.eProperty);
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
            socket.send("destroy", constants.msgType.ePipeDestroy);
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
    for (let i = 0; i < create_json.pipe_num; i++) {
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
        let child = spawn('hddl_mediapipe2', ['-u', `${adminCtx.socketURL}`, '-i', `${pipe_id}`], {env: process.env});

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
        });

        child.on('error', (err)=> {console.log(`spawn child process error ${err.message}`)});

        pipes.add(pipe_id);
        adminCtx.pipe2pid.set(pipe_id, {cid: ws.id, child: child});
        updatePipeJSON(adminCtx.pipe2json, create_json, pipe_id, 'create');
        console.log('create pipe %s', pipe_id);
        wsSender.sendMessage(ws, `pipe_create ${pipe_id}`);
        wsSender.sendProtocol(ws, {method: 'pipe_id'}, Array.from(pipes), 200);
        wsSender.sendProtocol(ws, {method: 'pipe_info', pipe_id: pipe_id}, create_json, 200);
    }

}

exports.updateModel = function (ws, model, adminCtx){
    if(!model.headers.hasOwnProperty('path')) {
        throw new Error('x1b[31mNo Path in Headers\x1b[0m');
    }
    var filePath = model.headers.path;
    var dir = path.dirname(path.dirname(filePath));
    if(!adminCtx.fileLock.has(filePath)){
        //no other controller update the file, acquire the file lock
        adminCtx.fileLock.add(filePath);
        saveBuffer(filePath, model.payload, adminCtx.fileLock);
        var modelMeta = JSON.stringify(fileHelper.updateCheckSum(filePath, model.checkSum));
        fs.writeFileSync(path.join(dir, 'model_info.json'), modelMeta);
        console.log('sendMeta');
        //inform each controller about the update
        adminCtx.wsConns.forEach((value, key, map) => {
            console.log(`update model for client id ${key}`);
            wsSender.sendProtocol(value, {method: 'checkSum'}, modelMeta);
        });
        wsSender.sendProtocol(ws, {method: 'checkSum'}, modelMeta);
    } else {
        //the model is being updated by other controller.
        wsSender.sendProtocol(ws, {method: 'error'}, `file ${filePath} being updating by other controller`);
    }

  }
  function saveBuffer(filePath, buffer, fileLock) {
    var folderName = path.dirname(filePath);
    if(!fs.existsSync(folderName))
    {
      console.log('making dir %s', './' + folderName);
      fs.mkdirSync('./' + folderName, { recursive: true, mode: 0o700 });
    }
    var modelBuffer = new BufferStream(buffer);
    var writer = fs.createWriteStream(filePath, { mode: 0o600 });
    writer.on('error', (err)=> {
        console.log(`save Buffer error ${err.message}`);
        fileLock.delete(filePath);
    });
    modelBuffer.on('error', (err)=> {
        console.log(`read Buffer error ${err.message}`); 
        writer.end();
    });
    //release file lock when completing file writing.
    writer.on('finish', ()=> {
        console.log(`${filePath} upload complete`);
        fileLock.delete(filePath);
    });
    modelBuffer.pipe(writer);
  }