# https://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl

/usr/bin/openssl req -x509 -newkey rsa:4096 -sha256 -days 36500 -nodes \
  -keyout localhost.key -out localhost.crt -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,DNS:localhost,IP:127.0.0.1"
