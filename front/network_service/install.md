
# 1. install Nodejs & Nodejs packages
```bash
sudo apt install nodejs
sudo apt-get install npm
// set proxy
npm config set proxy <proxy>
//update nodejs to stable version
sudo npm install -g n
//install latest stable Nodejs
sudo n stable
//cd to the the dir that contains package.json
npm install
```

# 2. setup certificates

Refer to `certificate_create_explanation.md` to create TLS certificate.
* Copy "ca-cert.pem client-cert.pem client-key.pem" into 'controller/client_cert'
* Copy "ca-cert.pem client-cert.pem client-key.pem" into 'receiver/client_cert'
* Copy "ca-cert.pem server-cert.pem server-key.pem" into 'server/server_cert'
* Copy 'client.crl' into 'server_cert'
* Copy 'server.crl' into 'client_cert'

Note:
    Please generate all these certificates in one device!!!


# 3. run hddls-server
Please run below commands:
```bash
cd server
node hddls-server.js
```

# 4. run hddls receiver client
Open a new terminal, and run below command:
```bash
cd receiver
node result_receiver.js
```
# 5. run hddls controller client
Open a new terminal, and run below command:
```
cd controller
node controller_cli.js
```