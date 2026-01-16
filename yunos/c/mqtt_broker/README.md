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

mosquitto_sub -d  -p 1810 -t '#' -d -u yuneta
mosquitto_sub -d  -p 1810 -t '#' -d -u yuneta -V mqttv5 

mosquitto_sub -i client1 -d  -p 1810 -t '#' -d -u yuneta -V mqttv5 -c
mosquitto_sub -i client1 -d  -p 1810 -t 'home/+/temperature' -U 'home/+/temperature' -d -u yuneta -V mqttv5 -c # required dummy subscribe!

mosquitto_sub -d  -p 1883 -t '#' -d -u yuneta
mosquitto_sub -d  -p 1883 -t '#' -d -u yuneta -V mqttv5


#mosquitto_sub -i client1 -h localhost -t "#" -q 2 -c -V mqttv5
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:1}" -q 2 -c
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:2}" -q 2 -c
mosquitto_pub -i client2 -h localhost -t "test/topic" -m "{client2:3}" -q 2 -c
#mosquitto_sub -i client1 -h localhost -t "#" -q 2 -c -V mqttv5

mosquitto_pub -p 1810 -u yuneta -i client2 -t "test/topic" -m "{client2:1}" -q 0
mosquitto_pub -p 1810 -u yuneta -i client2 -t "test/topic" -m "{client2:1}" -q 2


mosquitto_sub -d -p 1810 -u yuneta -t "home/+/temperature" -q 1
mosquitto_sub -d -p 1810 -u yuneta -t "home/livingroom/+" -q 2
mosquitto_pub -p 1810 -u yuneta -t "home/DEV/temperature" -m "{client2:3}" -q 1

mosquitto_sub -d -p 1810 -u yuneta -V 5 -t "home/#" -D SUBSCRIBE subscription-identifier 123 -t "home/+/temperature" 

mosquitto_sub -i client2 -d  -p 1810 -t '#' -d -u DVES_USER -P DVES_PASS -V mqttv5 -c -q 2
mosquitto_sub -i client3 -d  -p 1810 -t '#' -d -u DVES_USER -P DVES_PASS -V mqttv5 -c -q 2

mqtt_tui -i client2 -c -u DVES_USER -P DVES_PASS
