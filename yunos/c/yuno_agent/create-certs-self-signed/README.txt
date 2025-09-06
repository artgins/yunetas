# Two procedures for creating self-signed certificates
# For connections from a browser to work, you must allow the unknown certificate authority exception.
# Note: This also applies to websocket connections (use the websocket URL by replacing wss with https).
#

# Procedure 1

openssl genrsa -out localhost.key 4096
openssl req -new -key localhost.key -out localhost.csr
openssl x509 -req -days 36500 -in localhost.csr -signkey localhost.key -out localhost.crt

# Procedure 2

# https://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl

/usr/bin/openssl req -x509 -newkey rsa:4096 -sha256 -days 36500 -nodes \
  -keyout localhost.key -out localhost.crt -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,DNS:localhost,IP:127.0.0.1"
