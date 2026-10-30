# Example:
#   read_remote_cert.sh www.example.com:443

openssl s_client -showcerts -connect $1
