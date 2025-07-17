openssl genrsa -out yuneta_agent.key 4096
openssl req -new -key yuneta_agent.key -out yuneta_agent.csr
openssl x509 -req -days 36500 -in yuneta_agent.csr -signkey yuneta_agent.key -out yuneta_agent.crt
