#!/usr/bin/env node
//Copyright (C) 2018 Intel Corporation
// 
//SPDX-License-Identifier: MIT
//
'use strict'
const clientCLI = require('../lib/client_cli');
const fs = require('fs');
const fileHelper = require('../lib/file_helper');
const path = require('../lib/path_parser');
var modelCheck = {};
var pipe_ids = new Set();

const crl = null;
if(fs.existsSync('./client_cert/server.crl')) {
  crl = fs.readFileSync('./client_cert/server.crl')
}

// CLI help tips
const tips = [ ('help                          ' + 'show all commands')
           ,('c <create.json>                  ' + 'create pipelines')
           ,('p <property.json> <pipe_id>      ' + 'set pipelines property')
           ,('d <destroy.json>  <pipe_id>      ' + 'destroy pipelines')
           ,('pipe                             ' + 'display pipes belong to the client')
           ,('q                                ' + 'exit client.')
           ,('-m <model_path>                   ' + 'upload custom file')
].join('\n');


// CLI command line completer
function completer(line) {
    let completions = 'help|c <create.json> |p <property.json> <pipe_id> |d <destroy.json> <pipe_id> |pipe |client |m <folder path> |q'.split('|')
    var hints = [];
    var args = line.trim().split(' ');
    var files;
    try {
      if(args[1] && line.includes('/'))
      {
        files = fs.readdirSync(path.dirname(args[1]));
      } else {
        files = fs.readdirSync('./');
      }
    }
    catch{
      console.log("error path pls check")
      files = []
    }
    if(args.length === 1) {
      hints = completions.filter((c) => c.startsWith(line));
    } else if(args.length >1) {
      for(let file of files) {
        if(file.startsWith(path.basename(args[1]))) {
          let hint = args.slice();
          hint[1] = path.join(path.dirname(args[1]), file);
          hints.push(hint.join(' '));
      }
    }
  }
  return [hints && hints.length ? hints : completions, line];
}


var clientOptions = {
  host: '127.0.0.1',
  port: 8445,
  ca: fs.readFileSync('./client_cert/ca-cert.pem'),
  checkServerIdentity: () => undefined,
  key: fs.readFileSync('./client_cert/client-key.pem'),
  cert: fs.readFileSync('./client_cert/client-cert.pem'),
  completer: completer
};

if(crl != null) {
  clientOptions.crl = crl;
}

//Get CLI user input parser&&router
function setup(options) {

  return function cmdDispatcher(args) {
    var dispatcher;
    var help = options.tips;
    var cmd = args.trim().split(' ');
    var pipe_ids = options.pipe_ids;
    var fn = null;
    var dispatcher = {
      'help':  function(ws, rl) {
        rl.emit('hint',`\x1b[33m${help}\x1b[0m`);
      },
      'c': function(ws, rl) {
        if (cmd.length !== 2) {
          rl.emit('hint', `wrong cmd ${args} please check`);
          return;
        }
        var headers = {method: "create"};
        fileHelper.uploadFile([cmd[1]], ws, headers, ()=> rl.prompt());
      },
      'mc': function(ws, rl) {
        var headers = {method: "mcreate"};
        fileHelper.uploadFile([cmd[1]], ws, headers, ()=> rl.prompt());
      },
      'p': (ws, rl) => {
        if (cmd.length !== 3) {
          rl.emit('hint', `wrong cmd ${args} please check`);
          return;
        }
        var pipe_id = parseInt(cmd[2]);
        var headers = {pipe_id: pipe_id, method: 'property'};
        if(pipe_ids.has(pipe_id)) {
          fileHelper.uploadFile([cmd[1]], ws, headers, ()=> rl.prompt())
        } else{
          rl.emit('hint', `pipe_id ${pipe_id} not exist `);
        }
      },
      'd': (ws, rl) => {
        if (cmd.length !== 3) {
          rl.emit('hint', `wrong cmd ${args} please check`);
          return;
        }
        var pipe_id = parseInt(cmd[2]);
        if(pipe_ids.has(pipe_id)) {
          var headers = {pipe_id: parseInt(cmd[2]), method: 'destroy'};
          fileHelper.uploadFile([cmd[1]], ws, headers, ()=> rl.prompt());
        } else {
          rl.emit('hint', `pipe_id ${pipe_id} not exist `);
        }
      },
      'pipe': function(ws, rl) {
        rl.emit('hint', pipe_ids)
      },
      'q': function(ws, rl) {
	      rl.emit('hint', `Bye ${process.pid}`);
        process.exit(0);
      },
      'm' : function(ws, rl) {
        if (cmd.length !== 2) {
          rl.emit('hint', `wrong cmd ${args} please check`);
          return;
        }
        var rootPromise = fileHelper.scanDir(cmd[1], true);
        var childPromise = rootPromise.then((folders, reject) => {
          if(! folders) throw new Error('folder empty or not exists');
			    var actualFolders = folders.filter(folder => fs.lstatSync(folder).isDirectory());
		      if(actualFolders.length !== 0)
		      {
		  	    console.log("folders to upload");
		  	    console.log(actualFolders);
		      }
          return Promise.all(actualFolders.map(folder => fileHelper.scanDir(folder)
          .then(files => {
            if(!files) return;
          return Promise.all(files.map(file => fileHelper.cmpFile(file, modelCheck)))
          .then((files) => {var tmp = []; files.forEach(ele => !!ele && tmp.push(ele));return tmp})
        })))},(reason)=>{rl.emit('hint', 'empty ' + reason.message)})
        .then((value) => {
          if(!value) return;
          var merged = [].concat.apply([], value);
          merged = merged.filter(function(el) { return el; });
          if(merged.length !==0) {
            console.log(`models Server need:`);
            console.log(merged);
          }
          fileHelper.uploadFile(merged, ws, {method: 'model'}, ()=> rl.prompt())
        }, error => rl.emit('hint', 'new Scan dir err ' + error));
        childPromise.catch(reason=>rl.emit('hint', 'Scan dir err ' + reason.message));
      }
    }

    if(dispatcher[cmd[0]]) {
      fn = dispatcher[cmd[0]];
    } else {
      fn = (ws, rl) => {rl.emit('hint', `wrong cmd ${args} please check`)};
    }
    return fn;
    }
}

//The websocket incoming parser && router
function getInParser() {
return function incoming(ws, message, rl) {
  var method = message.headers.method || 'unknown';
    if(method === 'text') {
      console.log('message %s', message.payload);
    } else if(method === 'pipe_id') {
        console.log("pipe_id from server", message.payload);
        message.payload.forEach(elem=> pipe_ids.add(elem));
        rl.prompt();
    } else if(method === 'pipe_delete') {
        console.log("pipe_delete %s", message.payload);
        message.payload.forEach(elem=> {pipe_ids.delete(elem)});
        rl.prompt();
    } else {
      method ==='checkSum' && (modelCheck = fileHelper.safelyJSONParser(message.payload.toString()));
    }
  };
}


const cli = new clientCLI(clientOptions);

process.on('exit', ()=> {cli.close()});

cli.setInParser(getInParser());
cli.setExec(setup({tips: tips, pipe_ids: pipe_ids}));
cli.start();
