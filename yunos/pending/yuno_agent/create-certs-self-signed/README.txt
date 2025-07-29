#
#   Dos procedimientos para crear certificados autofirmados
#   Para que funcionen las conexiones desde un navegador hay que autorizar la exception de autoridad certificadora desconocida
#   ojo,también para las conexiones websocket (usar la url de websocket cambiando wss por https)
#

# procedimiento 1

openssl genrsa -out localhost.key 4096
openssl req -new -key localhost.key -out localhost.csr
openssl x509 -req -days 36500 -in localhost.csr -signkey localhost.key -out localhost.crt

# procedimiento 2

# https://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl

/usr/bin/openssl req -x509 -newkey rsa:4096 -sha256 -days 36500 -nodes \
  -keyout localhost.key -out localhost.crt -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,DNS:localhost,IP:127.0.0.1"
