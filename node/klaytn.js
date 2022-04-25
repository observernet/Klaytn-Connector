////////////////////////////////////////////////////////////////////////
// Klaytn Interface For none KAS
////////////////////////////////////////////////////////////////////////


// Load Library

const Caver = require("caver-js");
const config = require('./config/config');
const myutil = require('./common_util');

// Init Klaytn
const option = {
	headers: [
		{name: 'Authorization', value: 'Basic ' + Buffer.from(config.accessKeyId + ':' + config.secretAccessKey).toString('base64')},
		{name: 'x-chain-id', value: config.chainid},
	]
};
const caver = new Caver(new Caver.providers.HttpProvider(config.url, option));

// Init OBSR
var OBSR = caver.contract.create(config.abi, config.contract);


////////////////////////////////////////////////////////////////////////
// Interfaces
////////////////////////////////////////////////////////////////////////

async function GetBalanceOf(req)
{
	try
	{
		var res = await OBSR.call({from: req.address}, 'balanceOf', req.address);
		var amount = res / Math.pow(10, 8);
		
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
		var pkey;
		if ( req.cert.substring(0, 2) == '0x' && (req.cert.length > 60 && req.cert.length < 70) )
		{
			pkey = req.cert;
		}
		else
		{
			pkey = myutil.decrypt(config.encryptKey, req.cert);
		}

		var keyring = caver.wallet.keyring.createFromPrivateKey(pkey);
		caver.wallet.add(keyring);
		
		var res = await OBSR.send({from: keyring.address, gas: '0x4bfd20'}, 'transfer', req.recipient, (Math.round(req.amount * Math.pow(10, 8))).toString());
		if ( res.status == undefined || res.status == false )
		{
			return {success: false, msg: 'Send error: safeTransfer error'};
		}
		
		return {success: true, msg: res.transactionHash};
	}
	catch ( err )
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

	if ( trid == 'B' )
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
