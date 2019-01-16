# 1. Environment prepration
Find default `openssl.config`
```bash
openssl version -a | grep OPENSSLDIR
```
For example. you may get
```
OPENSSLDIR: "/usr/lib/ssl"
```
Then copy the default openssl.conf to your current folder.
```bash
cp ${OPENSSLDIR}/openssl.cnf .
mkdir /demoCA  /demoCA/newcerts   /demoCA/private
chmod 700 CA/private
echo '01'>./demoCA/serial
touch /demoCA/index.txt
```
After this, you will have folder structure
```
.
├── demoCA
│   ├── index.txt
│   ├── newcerts
│   └── private
└── openssl.cnf
```
# 2. Generate CA

Generate a Certificate Authority:
```bash
openssl req -new -x509 -keyout ca-key.pem -out ca-cert.pem -config openssl.cnf
```
* Insert a CA Password and remember it
* Specify a CA Common Name, like 'root.localhost' or 'ca.localhost'. This MUST be different from both server and client CN.


# 3 Generate Server certificate

Generate Server Key:
```bash
openssl genrsa -out server-key.pem 1024
```
Generate Server certificate signing request:
```bash
openssl req -new -key server-key.pem -out server-csr.pem -config openssl.cnf
```
* Specify server Common Name, run   **cat /etc/hosts**    to check valid DNS name, please DO NOT name as **'localhost'**.
* For this example, do not insert the challenge password.

Sign certificate using the CA:
```
openssl ca -in server-csr.pem -out server-cert.pem -cert ca-cert.pem -keyfile ca-key.pem -config openssl.cnf
```
# 4 Generate Client certificate

Generate Client Key:
	~$ openssl genrsa -out client-key.pem 1024

Generate Client certificate signing request:
	~$ openssl req -new -key client-key.pem -out client-csr.pem -config openssl.cnf

* Specify client Common Name, like 'client.localhost'. Server should not verify this, since it should not do reverse-dns lookup.
* For this example, do not insert the challenge password.

Sign certificate using the CA:
```bash
openssl ca -in client-csr.pem -out client-cert.pem -cert ca-cert.pem -keyfile ca-key.pem -config openssl.cnf
```
# 5 Generate CRL

Environment preparation
```bash
cp ca-key.pem ./demoCA/private/cakey.pem
cp ca-cert.pem ./demoCA/cacert.pem
echo '00' >./demoCA/crlnumber
```
Revoke Server Certificate
```bash
openssl ca -revoke server-cert.pem -config openssl.cnf
```
Generate Server CRL
```bash
openssl ca -gencrl  -out server.crl  -config openssl.cnf
```
Revoke Client Certificate
```bash
openssl ca -revoke client-cert.pem -config openssl.cnf
```
Generate Client CRL
```bash
openssl ca -gencrl  -out client.crl  -config openssl.cnf
```

# 6. Some Q&A about CA and CRL
1. Can client and server use different CA?
     Websockets support different CA for client and server. However, if CA is self-signed, TLS wouldn't connect successfully. Here are two ways to use different CA:
    * Use different public CA for server and client
    * Use self-signed CA, should set rejectUnauthorized:true to rejectUnauthorized:false when set websocket
2. How to use different CA to certificate client and server?
    Here are two ways:
    * Use public CA, just generate server and client certificate separately
    * Use self-signed CA, all steps below should be handled separately in server and client side.
3. How to verify CRL?
    Here are two ways to verify crl:
    * If using public CA, just above process 1~5
    * If using self-signed CA, please make sure that server and client are using the same CA. Different self-signed CA would cause problems.

