//Copyright (C) 2019 Intel Corporation
// 
//SPDX-License-Identifier: MIT
//
'use strict';

const fs = require('fs');
const path = require('./path_parser')
const crypto = require('crypto');

function isString(str) {
    return typeof(str) == 'string' || str instanceof String;
}

exports.isString = isString;

function safelyJSONParser (json) {

  var parsed
  try {
    parsed = JSON.parse(json)
  } catch (e) {
    return null
  }
  return parsed;
}

exports.safelyJSONParser = safelyJSONParser;

/**
* @param {Array} folderPath folder to upload
* @param {Object} ws the websocket
* @param {Object} rl the readline interface
* @param {String} type the operation type
*/
exports.uploadDir = function (folderPath, ws, type, cb) {
 fs.readdir(folderPath, (err, files) => {
   if(!files || files.length === 0) {
     if (cb) cb();
     return;
   }
   var targetFiles = [];
   for(let index in files) {
     if(path.extname(files[index]).toLowerCase() === '.bin' || path.extname(files[index]).toLowerCase() === '.xml') {
       targetFiles.push(path.join(folderPath, files[index]));
       console.log(targetFiles[targetFiles.length - 1]);
     }
   }
   uploadFile(targetFiles, ws, type, cb);
 });
}


/**
 * @param {Array} files files to upload
 * @param {Object} ws the websocket
 * @param {String} type the operation type
 * @param {Object} cb the readline interface
 */
exports.uploadFile = function uploadFile (files, ws, headers, cb) {
  if(files.length === 0) {
    if(cb) {
      cb();
    }
    return
  }
  const fileEntry = files.pop();
  var filePath, checkSum = null;
  if(isString(fileEntry)) {
    filePath = fileEntry;
  } else if(fileEntry.length !== 0){
    let fileObj = Object.entries(fileEntry);
    filePath = fileObj[0][0];
    checkSum = fileObj[0][1];
  }
  fs.lstat(filePath, (err, stat)=> {
    if(err || stat.isDirectory()) {
      console.log(`${filePath} is a folder or not exists`);
      uploadFile(files, ws, headers, cb);
      return;
    }
    headers.path = filePath;
    var stream = fs.createReadStream(filePath);
    if(checkSum == null) {
      var hash = crypto.createHash('md5').setEncoding('hex');
      stream.pipe(hash);
    }
    try {
      ws.send(Buffer.from(JSON.stringify(headers)+String.fromCharCode(1)), {fin: false});
    } catch(err) {
      console.log('Got error%s', err.message);
    }
    stream.on('data', (chunk)=> {
      try{
        ws.send(chunk, { fin: false });
      } catch (err) {
        stream.destroy();
        console.log('Got error%s', err.message);
      }
    });
    stream.on('end', ()=> {
      checkSum = checkSum || hash.read();
      try{
        ws.send(String.fromCharCode(1) + checkSum, { fin: true });
        console.log(`${filePath} upload complete, MD5 ${checkSum}`);
        uploadFile(files, ws, headers, cb);
      } catch (err) {
        stream.destroy();
      }
    });
  })
}


exports.scanDir = function (folderPath, isRoot=false) {
    return new Promise((resolve, reject) => {
        fs.readdir(folderPath, (err, files) => {
            if(err || files.length === 0) {
              resolve();
              //reject(err || `empty model folder ${folderPath}`);
              //return;
            }
            var targetFiles = [];
            for(let index in files) {
              if (isRoot === true) {
                targetFiles.push(path.join(folderPath, files[index]));
              } else {
                targetFiles.push(path.join(folderPath, files[index]));
              }
            }
            resolve(targetFiles);
          });
    });
}



exports.cmpFile = function (filePath, modelCheck) {
    return new Promise((resolve, reject) => {
        fs.lstat(filePath, (err, stat)=> {
            if(err || stat.isDirectory()) {
              reject(err || `empty model file ${filePath}`);
            }
            var stream = fs.createReadStream(filePath);
            var hash = crypto.createHash('md5').setEncoding('hex');
            var fileName = path.basename(filePath);
            var model = path.basename(path.dirname(filePath));
            stream.pipe(hash);
            stream.on('end', ()=> {
              hash.end();
              let check = hash.read();
              if(!modelCheck.hasOwnProperty(model) || modelCheck[model].has_model==='No' || check !== modelCheck[model]['model_file'][fileName]) {
                resolve({[filePath]: check});
              } else {
                resolve();
                console.log(`\x1b[31mServer has file ${filePath}\x1b[0m`);
              }
            });
        })
    });
  }

function safelyFileReadSync(filePath, encoding='utf8') {
  var file;
  try{
    file = fs.readFileSync(filePath, encoding);
  } catch(ex){
    file = null;
  }
  return file;
}

exports.safelyFileReadSync =safelyFileReadSync 


function updateCheckSum(filePath, checkSum) {
  var dir = path.dirname(path.dirname(filePath));
  var modelName = path.basename(path.dirname(filePath));
  var fileName = path.basename(filePath);
  var fileInfo = {};
  var modelInfo = {};
  var modelMeta = safelyJSONParser(safelyFileReadSync(path.join(dir, 'model_info.json'))) || {};
  if(modelMeta.hasOwnProperty(modelName) && modelMeta[modelName].hasOwnProperty('model_file')) {
    fileInfo = modelMeta[modelName]["model_file"];
  }
  if(modelMeta.hasOwnProperty(modelName)) {
    modelInfo = modelMeta[modelName]
  }
  fileInfo = Object.assign(fileInfo,
    {
        [fileName]: checkSum
    }
  );
  modelInfo = Object.assign(modelInfo, {"model_file" : fileInfo, has_model: "Yes"});
  modelMeta = Object.assign(modelMeta, {[modelName]: modelInfo});
  return modelMeta;
}
exports.updateCheckSum = updateCheckSum;
