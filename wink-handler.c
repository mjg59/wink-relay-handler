#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/input.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <MQTTClient.h>

#define MQTT_HOST "hostname"
#define MQTT_PORT 1833

void handle_relay1(MessageData *md)
{
	int fd;
	MQTTMessage *message = md->message;
	char power;

	signal(SIGPIPE, SIG_IGN);

        fd = open("/sys/class/gpio/gpio203/value", O_RDWR);
	if (strncmp(message->payload, "ON", message->payloadlen) == 0) {
		power = '1';
		write(fd, &power, 1);
	} else if (strncmp(message->payload, "OFF", message->payloadlen) == 0) {
		power = '0';
		write(fd, &power, 1);
	}
	close(fd);
}

void handle_relay2(MessageData *md)
{
	int fd;
	MQTTMessage *message = md->message;
	char power;

        fd = open("/sys/class/gpio/gpio204/value", O_RDWR);
	if (strncmp(message->payload, "ON", message->payloadlen) == 0) {
		power = '1';
		write(fd, &power, 1);
	} else if (strncmp(message->payload, "OFF", message->payloadlen) == 0) {
		power = '0';
		write(fd, &power, 1);
	}
	close(fd);
}

int mqtt_connect(Network *n, MQTTClient *c, char *buf, char *readbuf) {
	int ret;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

	NetworkInit(n);

	ret = NetworkConnect(n, MQTT_HOST, MQTT_PORT);
	if (ret < 0)
		return ret;

	MQTTClientInit(c, n, 1000, buf, 100, readbuf, 100);

	data.willFlag = 0;
	data.MQTTVersion = 4;
	data.clientID.cstring = "Wink_Relay";
	data.keepAliveInterval = 10;
	data.cleansession = 1;
	
	ret = MQTTConnect(c, &data);
	if (ret < 0)
		return ret;

	return 0;
}

int main() {
	int ret;
	int uswitch, lswitch, input, screen, relay1, relay2, temp, humid, prox;
	int uswitchstate=1;
	int lswitchstate=1;
	char urelaystate=' ';
	char lrelaystate=' ';
	int last_input = time(NULL);
	struct input_event event;
	unsigned char buf[100], readbuf[100];
	char buffer[30];
	char power = '1';
	char payload[30];
	char proxdata[100];
	int temperature, humidity, last_temperature = -1, last_humidity = -1;
	long proximity;
	Network n;
	MQTTClient c;
	MQTTMessage message;
	struct rlimit limits;

	uswitch = open("/sys/class/gpio/gpio8/value", O_RDONLY);
	lswitch = open("/sys/class/gpio/gpio7/value", O_RDONLY);
        screen = open("/sys/class/gpio/gpio30/value", O_RDWR);
        relay1 = open("/sys/class/gpio/gpio203/value", O_RDWR);
        relay2 = open("/sys/class/gpio/gpio204/value", O_RDWR);
        input = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	temp = open("/sys/bus/i2c/devices/2-0040/temp1_input", O_RDONLY);
	humid = open("/sys/bus/i2c/devices/2-0040/humidity1_input", O_RDONLY);
	prox = open("/sys/devices/platform/imx-i2c.2/i2c-2/2-005a/input/input3/ps_input_data", O_RDONLY);

	write(relay1, &power, 1);
	write(relay2, &power, 1);

	lseek(screen, 0, SEEK_SET);
	read(screen, &power, sizeof(power));

	limits.rlim_cur = RLIM_INFINITY;
	limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &limits);
	while (mqtt_connect(&n, &c, buf, readbuf) < 0)
		sleep(10);

	MQTTSubscribe(&c, "Relay/relays/upper", 0, handle_relay1);
	MQTTSubscribe(&c, "Relay/relays/lower", 0, handle_relay2);

	while (1) {
		lseek(relay1, 0, SEEK_SET);
		read(relay1, buffer, sizeof(buffer));
		if (buffer[0] != urelaystate) {
			message.qos = 1;
			message.payload = payload;
			if (buffer[0] == '0') {
				sprintf(payload, "OFF");
			} else {
				sprintf(payload, "ON");
			}
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/relays/upper_state", &message);
			urelaystate = buffer[0];
		}

		lseek(relay2, 0, SEEK_SET);
		read(relay2, buffer, sizeof(buffer));
		if (buffer[0] != lrelaystate) {
			message.qos = 1;
			message.payload = payload;
			if (buffer[0] == '0') {
				sprintf(payload, "OFF");
			} else {
				sprintf(payload, "ON");
			}
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/relays/lower_state", &message);
			lrelaystate = buffer[0];
		}

		lseek(uswitch, 0, SEEK_SET);
		read(uswitch, buffer, sizeof(buffer));
		if (buffer[0] == '0' && uswitchstate == 1) {
			uswitchstate = 0;
			message.qos = 1;
			message.payload = payload;
			sprintf(payload, "on");
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/switches/upper", &message);
		} else if (buffer[0] == '1' && uswitchstate == 0) {
			uswitchstate = 1;
		}
		lseek(lswitch, 0, SEEK_SET);
		read(lswitch, buffer, sizeof(buffer));
		if (buffer[0] == '0' && lswitchstate == 1) {
			lswitchstate = 0;
			message.qos = 1;
			message.payload = payload;
			sprintf(payload, "on");
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/switches/lower", &message);
		} else if (buffer[0] == '1' && lswitchstate == 0) {
			lswitchstate = 1;
		}

		lseek(temp, 0, SEEK_SET);
		read(temp, buffer, sizeof(buffer));
		temperature = atoi(buffer);
		if (abs(temperature - last_temperature) > 100) {
			message.qos = 1;
			message.payload = payload;
			sprintf(payload, "%f", temperature/1000.0);
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/sensors/temperature", &message);
			last_temperature = temperature;
		}

		lseek(humid, 0, SEEK_SET);
		read(humid, buffer, sizeof(buffer));
		humidity = atoi(buffer);
		if (abs(humidity - last_humidity) > 100) {
			message.qos = 1;
			message.payload = payload;
			sprintf(payload, "%f", humidity/1000.0);
			message.payloadlen = strlen(payload);
			MQTTPublish(&c, "Relay/sensors/humidity", &message);
			last_humidity = humidity;
		}

		lseek(prox, 0, SEEK_SET);
		read(prox, proxdata, sizeof(proxdata));
		proximity = strtol(proxdata, NULL, 10);
		if (proximity >= 5000) {
			last_input = time(NULL);
			if (power != '1') {
				power = '1';
				write(screen, &power, sizeof(power));
			}
		}

		while (read(input, &event, sizeof(event)) > 0) {
			last_input = time(NULL);
			if (power != '1') {
				power = '1';
				write(screen, &power, sizeof(power));
			}
		}
		if ((time(NULL) - last_input > 10) && power == '1') {
			power = '0';
			write(screen, &power, sizeof(power));
		}
		if (MQTTYield(&c, 100) < 0)
			mqtt_connect(&n, &c, buf, readbuf);
	}
}
