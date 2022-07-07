#!/bin/sh

kill -9 `ps -ef | awk '$9 == "/home/www/KASConn/node/KAS_NodeAPI.js" { print $2 }'`
kill -9 `ps -ef | awk '$10 == "/home/www/KASConn/node/KAS_NodeAPI.js" { print $2 }'`
sleep 1

cd /home/www/KASConn/node
rm -f nohup.out

sleep 3
nohup /usr/local/nodejs/bin/node /home/www/KASConn/node/KAS_NodeAPI.js &

