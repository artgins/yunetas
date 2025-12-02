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

mosquitto_sub -v  -p 1810 -t '#' -d
mosquitto_sub -v  -p 1810 -t '#' -d -u yuneta -V mqttv5
