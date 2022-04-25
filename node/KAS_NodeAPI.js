////////////////////////////////////////////////////////////////////////
// Websocket For KAS Node API
////////////////////////////////////////////////////////////////////////


// Load Library
const fs = require('fs');
const appRoot = require('app-root-path');
const config = require('./config/config');
const logger = require('./config/winston');
const myutil = require('./common_util');
const Caver = require("caver-js");

function WriteBlockEventFile(msg)
{
	let fd;

	try
	{
		var filename = `${appRoot}` + '/../data/event/' + myutil.Today() + '.NodeAPI.evt';
		fd = fs.openSync(filename, 'a+');
		fs.appendFileSync(fd, JSON.stringify(msg) + '\n', 'utf8');
	}
	catch (err)
	{
		logger.info('WriteBlockEventFile error', err);
	}
	finally
	{
		if ( fd != undefined )
			fs.closeSync(fd);
	}
}

// Connect Node API Websocket
var url = 'wss://' + config.accessKeyId + ':' + config.secretAccessKey + '@' + config.wss_url + '?chain-id=' + config.chainid;
const caver = new Caver(url);
logger.info('Connect Node API Websocket. chianid[' + config.chainid + ']');

// Select Receive Event
var options = {
	address: config.contract,
	topic: caver.abi.encodeFunctionSignature({
		name: 'transfer',
		type: 'function',
		inputs: [
			{type: 'address', name: 'recipient'},
			{type: 'uint256', name: 'amount'
		}]
	})
};

// subscribe transfer log
const sub_logs = caver.rpc.klay.subscribe("logs", options, (err, evt) =>
{
    if ( err )
	{
    	logger.info('logs error [' + err + ']');
    }

	var from = caver.abi.decodeParameter('address', evt.topics[1]);
	var to = caver.abi.decodeParameter('address', evt.topics[2]);
	var amount = caver.abi.decodeParameter('uint256', evt.data) / Math.pow(10, 8);

	WriteBlockEventFile( {type: 'transfer', data: {transactionHash: evt.transactionHash, from: from, to: to, amount: amount}} );
});

// get block number (For keeping the connection)
setInterval(() => { caver.rpc.klay.getBlockNumber() }, 30000);

//const sub_blocks = caver.rpc.klay.subscribe("newBlockHeaders", (err, evt) =>
//{
//    if ( err )
//	{
//		logger.info('newBlockHeaders error [' + err + ']');
//    }
//
//	var blockNumber = caver.utils.hexToNumber(evt.number);
//
//	WriteBlockEventFile( {type: 'block', data: {number: blockNumber}} );
//});
