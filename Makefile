CC=${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc
CFLAGS=-I${PAHO_MQTT}/MQTTPacket/src -I${PAHO_MQTT}/MQTTClient-C/src -I${PAHO_MQTT}/MQTTClient-C/src/linux --sysroot ${ANDROID_NDK}/platforms/android-18/arch-arm/ -g

MQTTPACKET=${PAHO_MQTT}/MQTTPacket/src/MQTTFormat.c ${PAHO_MQTT}/MQTTPacket/src/MQTTPacket.c ${PAHO_MQTT}/MQTTPacket/src/MQTTDeserializePublish.c ${PAHO_MQTT}/MQTTPacket/src/MQTTConnectClient.c ${PAHO_MQTT}/MQTTPacket/src/MQTTSubscribeClient.c ${PAHO_MQTT}/MQTTPacket/src/MQTTSerializePublish.c ${PAHO_MQTT}/MQTTPacket/src/MQTTConnectServer.c ${PAHO_MQTT}/MQTTPacket/src/MQTTSubscribeServer.c ${PAHO_MQTT}/MQTTPacket/src/MQTTUnsubscribeServer.c ${PAHO_MQTT}/MQTTPacket/src/MQTTUnsubscribeClient.c

MQTTCLIENT=${PAHO_MQTT}/MQTTClient-C/src/MQTTClient.c ${PAHO_MQTT}/MQTTClient-C/src/linux/MQTTLinux.c

wink-handler: wink-handler.c ini.c ${MQTTPACKET} ${MQTTCLIENT}

clean:
	rm -f wink-handler
