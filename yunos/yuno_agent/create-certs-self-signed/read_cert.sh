# Example:
#   read_cert.sh yuneta.crt

 openssl x509 -text -noout -in $1
