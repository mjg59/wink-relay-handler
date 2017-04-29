CC=${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc
CFLAGS=-IMQTTPacket/src -IMQTTClient-C/src -IMQTTClient-C/src/linux --sysroot ${ANDROID_NDK}/platforms/android-18/arch-arm/ -g

MQTTPACKET=MQTTPacket/src/MQTTFormat.c MQTTPacket/src/MQTTPacket.c MQTTPacket/src/MQTTDeserializePublish.c MQTTPacket/src/MQTTConnectClient.c MQTTPacket/src/MQTTSubscribeClient.c MQTTPacket/src/MQTTSerializePublish.c MQTTPacket/src/MQTTConnectServer.c MQTTPacket/src/MQTTSubscribeServer.c MQTTPacket/src/MQTTUnsubscribeServer.c MQTTPacket/src/MQTTUnsubscribeClient.c

MQTTCLIENT=MQTTClient-C/src/MQTTClient.c MQTTClient-C/src/linux/MQTTLinux.c

wink-handler: wink-handler.c ini.c ${MQTTPACKET} ${MQTTCLIENT}

clean:
	rm -f wink-handler
