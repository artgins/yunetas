# Example:
#   read_remote_cert.sh mulesol.mifichador.es:443

openssl s_client -showcerts -connect $1
