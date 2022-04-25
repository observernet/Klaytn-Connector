////////////////////////////////////////////////////////////////////////
// Klaytn Interface For KAS
////////////////////////////////////////////////////////////////////////


// Load Library
const request = require("request-promise");
const config = require('./config/config');
const caver = require("caver-js");

// Create KASAuth
const KASAuth = 'Basic ' + Buffer.from(config.accessKeyId + ':' + config.secretAccessKey, 'utf8').toString('base64');

////////////////////////////////////////////////////////////////////////
// Interfaces
////////////////////////////////////////////////////////////////////////

async function CreateAccount(req)
{
	try
	{
		var krn = config.pool[req.cert];
		var options = {
			uri: 'https://wallet-api.klaytnapi.com/v2/account',
			headers: {
				'Authorization': KASAuth,
				'x-chain-id': config.chainid,
				'x-krn': krn.account,
				'Content-Type': 'application/json',
			},
			method: 'POST',
			json: true
		}
		var res = await request.post(options);

		return {success: true, msg: res.address};
	}
	catch (err)
	{
		return {success: false, msg: err.message};
	}
}

async function GetBalanceOf(req)
{
	try
	{
		var options = {
			uri: 'https://kip7-api.klaytnapi.com/v1/contract/' + config.contract + '/account/' + req.address + '/balance',
			headers: {
				'Authorization': KASAuth,
				'x-chain-id': config.chainid,
				'Content-Type': 'application/json',
			},
			method: 'GET',
			json: true
		}
		var res = await request.get(options);
		var amount = caver.utils.hexToNumber(res.balance) / Math.pow(10, 8);

		return {success: true, msg: amount};
	}
	catch (err)
	{
		return {success: false, msg: err.message};
	}
}

async function Transfer(req)
{
	try
	{
		var amount = caver.utils.numberToHex((Math.round(req.amount * Math.pow(10, 8))).toString());
		var input = caver.abi.encodeFunctionCall({
			name: 'transfer',
			type: 'function',
			inputs: [{
				type: 'address',
				name: 'recipient'
			},{
				type: 'uint256',
				name: 'amount'
			}]
		}, [req.recipient, amount]);
		
		var krn = config.pool[req.cert];
		var options = {
			uri: 'https://wallet-api.klaytnapi.com/v2/tx/fd-user/contract/execute',
			headers: {
				'Authorization': KASAuth,
				'x-chain-id': config.chainid,
				'x-krn': krn.account + ',' + krn.feePayer,
				'Content-Type': 'application/json',
			},
			method: 'POST',
			body: {
				from: req.sender,
				to: config.contract,
				input: input,
				submit: true,
				feePayer: '0x3542646188d3d0669d62f73bD2E2aC0d1a79d4f5'
			},
			json: true
		}
		var res = await request.post(options);

		return {success: true, msg: res.transactionHash};
	}
	catch (err)
	{
		return {success: false, msg: err.message};
	}
}


////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////

const args = process.argv.slice(2);
if ( args.length != 2 )
{
	console.log( JSON.stringify({success: false, msg: 'Parameter Error!!'}) );
	process.exit();
}

try
{
	var trid = args[0];
	var req = JSON.parse(args[1]);

	if ( trid == 'C' )
	{
		CreateAccount(req).then((res) => { console.log( JSON.stringify(res) ); });
	}
	else if ( trid == 'B' )
	{
		GetBalanceOf(req).then((res) => { console.log( JSON.stringify(res) ); });
	}
	else if ( trid == 'T' )
	{
		Transfer(req).then((res) => { console.log( JSON.stringify(res) ); });
	}
	else
	{
		console.log( JSON.stringify({success: false, msg: 'Undefined Request Type!'}) );
	}
}
catch (err)
{
	console.log( JSON.stringify({success: false, msg: err.message}) );
}
