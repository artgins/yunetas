Yuno
====

Name: 
Role: mqtt_broker

Description
===========

Mqtt broker

License
=======


Test
====

sudo journalctl -u snap.mosquitto.mosquitto -f

mosquitto_sub -v  -p 1810 -t '#' -d -u yuneta
mosquitto_sub -v  -p 1810 -t '#' -d -u yuneta -V mqttv5 

mosquitto_sub -i client1 -v  -p 1810 -t '#' -d -u yuneta -V mqttv5 -c
mosquitto_sub -i client1 -v  -p 1810 -t '#' -U '#' -d -u yuneta -V mqttv5 -c # required dummy subscribe!

mosquitto_sub -v  -p 1883 -t '#' -d -u yuneta
mosquitto_sub -v  -p 1883 -t '#' -d -u yuneta -V mqttv5


#mosquitto_sub -i client1 -h localhost -t "#" -q 2 -c -V mqttv5
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:1}" -q 2 -c
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:2}" -q 2 -c
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:3}" -q 2 -c
#mosquitto_sub -i client1 -h localhost -t "#" -q 2 -c -V mqttv5

mosquitto_pub -p 1810 -u yuneta -i client2 -t "test/topic" -m "{client2:1}" -q 0
mosquitto_pub -p 1810 -u yuneta -i client2 -t "test/topic" -m "{client2:1}" -q 2
