////////////////////////////////////////////////////////////////////////
// Common Utils
////////////////////////////////////////////////////////////////////////

const crypto = require('crypto');

var common_util = {};

common_util.Today = function()
{
	var today = new Date();   

	var year = today.getFullYear();
	var month = today.getMonth() + 1;
	var date = today.getDate();

	return (year * 10000) + (month * 100) + date;
};

common_util.encrypt = function(key, text)
{
	const iv = key.substring(0, 16);
	const cipher = crypto.createCipheriv('aes-256-cbc', Buffer.from(key), iv);
	const encrypted = cipher.update(text);

	return Buffer.concat([encrypted, cipher.final()]).toString('base64');
};

common_util.decrypt = function(key, text)
{
	const iv = key.substring(0, 16);
	const encryptedText = Buffer.from(text, 'base64');
	const decipher = crypto.createDecipheriv('aes-256-cbc', Buffer.from(key), iv);
	const decrypted = decipher.update(encryptedText);

	return Buffer.concat([decrypted, decipher.final()]).toString();
};

module.exports = common_util;
 