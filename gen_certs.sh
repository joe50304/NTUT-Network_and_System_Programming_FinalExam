#!/bin/bash
set -e

DIR="certificate"
mkdir -p $DIR
cd $DIR

echo "Generating CA..."
openssl req -x509 -new -nodes -keyout ca.key -sha256 -days 3650 -out ca.crt -subj "/C=TW/ST=Taipei/L=Daan/O=NTUT/CN=RootCA"

echo "Generating Server Certificate (*.bank.com)..."
openssl req -new -nodes -keyout server_wildcard.key -out server.csr -config wildcard_settings.cnf
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server_wildcard.crt -days 365 -sha256 -extfile wildcard_settings.cnf -extensions v3_req

echo "Generating Client Certificate..."
openssl req -new -nodes -keyout client.key -out client.csr -subj "/C=TW/ST=Taipei/L=Daan/O=NTUT/CN=ClientUser"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 -sha256

echo "Certificates generated in $DIR/"
ls -l *.crt *.key
