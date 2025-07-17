openssl genrsa -out localhost.key 4096
openssl req -new -key localhost.key -out localhost.csr
openssl x509 -req -days 36500 -in localhost.csr -signkey localhost.key -out localhost.crt
