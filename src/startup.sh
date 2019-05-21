#!/bin/bash
# ../bin/ftpserver 1234 &

cd ..
bin/ftpclient "127.0.0.1" 1234 

killall ftpserver