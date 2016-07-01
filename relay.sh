#!/bin/sh

rm -f /tmp/t; mkfifo /tmp/t;cat /tmp/t | nc -k -l localhost 8000 | nc 172.28.1.40 8000 > /tmp/t ; rm -f /tmp/t

