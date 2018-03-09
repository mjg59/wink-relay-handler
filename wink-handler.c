#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <linux/input.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <MQTTClient.h>

#include "ini.h"

struct configuration {
	char *username;
	char *password;
	char *host;
	char *clientid;
	char *topic_prefix;
	int port;
	int screen_timeout;
	int motion_timeout;
	bool switch_toggle;
	bool send_switch;
};

static struct configuration config;

#define UPPER_RELAY "/sys/class/gpio/gpio203/value"
#define LOWER_RELAY "/sys/class/gpio/gpio204/value"

void toggle_relay(char *path)
{
	int fd;
	char state;

	fd = open(path, O_RDWR);
	read(fd, &state, sizeof(state));
	lseek(fd, 0, SEEK_SET);
	if (state == '0') {
		state = '1';
	} else {
		state = '0';
	}
	write(fd, &state, 1);
	close(fd);
}

void handle_relay(char *path, char *payload, int len)
{
	int fd;
	char power;

	signal(SIGPIPE, SIG_IGN);

        fd = open(path, O_RDWR);
	if (strncmp(payload, "ON", len) == 0) {
		power = '1';
		write(fd, &power, 1);
	} else if (strncmp(payload, "OFF", len) == 0) {
		power = '0';
		write(fd, &power, 1);
	}
	close(fd);
}

void handle_relay1(MessageData *md)
{
	MQTTMessage *message = md->message;

        handle_relay(UPPER_RELAY, message->payload, message->payloadlen);
}

void handle_relay2(MessageData *md)
{
	MQTTMessage *message = md->message;

        handle_relay(LOWER_RELAY, message->payload, message->payloadlen);
}

int mqtt_connect(Network *n, MQTTClient *c, char *buf, char *readbuf) {
	int ret;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

	NetworkInit(n);

	if (config.host == NULL || config.port == 0) {
		fprintf(stderr, "No valid configuration\n");
		return 1;
	}

	ret = NetworkConnect(n, config.host, config.port);
	if (ret < 0)
		return ret;

	MQTTClientInit(c, n, 1000, buf, 100, readbuf, 100);

	data.willFlag = 0;
	data.MQTTVersion = 4;
	if (config.clientid != NULL) {
		data.clientID.cstring = config.clientid;
	} else {
		data.clientID.cstring = "Wink_Relay";
	}
	data.keepAliveInterval = 10;
	data.cleansession = 1;
	if (config.username != NULL) {
		data.username.cstring = config.username;
	}
	if (config.password != NULL) {
		data.password.cstring = config.password;
	}
	ret = MQTTConnect(c, &data);
	if (ret < 0)
		return ret;

	return 0;
}

static int config_handler(void* data, const char* section, const char* name,
			  const char* value)
{
	if (strcmp(name, "user") == 0) {
		config.username = strdup(value);
	} else if (strcmp(name, "password") == 0) {
		config.password = strdup(value);
	} else if (strcmp(name, "clientid") == 0) {
		config.clientid = strdup(value);
	} else if (strcmp(name, "topic_prefix") == 0) {
		config.topic_prefix = strdup(value);
	} else if (strcmp(name, "host") == 0) {
		config.host = strdup(value);
	} else if (strcmp(name, "port") == 0) {
		config.port = atoi(value);
	} else if (strcmp(name, "screen_timeout") == 0) {
		config.screen_timeout = atoi(value);
	} else if (strcmp(name, "motion_timeout") == 0) {
		config.motion_timeout = atoi(value);
	} else if (strcmp(name, "switch_toggle") == 0) {
		if (strcmp(value, "true") == 0) {
			config.switch_toggle = true;
		} else if (strcmp(value, "false") == 0) {
			config.switch_toggle = false;
		}
	} else if (strcmp(name, "send_switch") == 0) {
		if (strcmp(value, "true") == 0) {
			config.send_switch = true;
		} else if (strcmp(value, "false") == 0) {
			config.send_switch = false;
		}
	}
	return 1;
}

int main() {
	int ret;
	int uswitch, lswitch, input, screen, relay1, relay2, temp, humid, prox;
	int uswitchstate=1;
	int lswitchstate=1;
	bool motion = 0;
	char urelaystate=' ';
	char lrelaystate=' ';
	int last_input = time(NULL);
	int last_motion = time(NULL);
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
	char *prefix, topic[1024];
	int s_timeout;
	int m_timeout;

	config.send_switch = true;

	if (ini_parse("/sdcard/mqtt.ini", config_handler, NULL) < 0) {
		printf("Can't load /sdcard/mqtt.ini\n");
		return 1;
	}

	if (config.topic_prefix != NULL) {
		prefix = config.topic_prefix;
	} else {
		prefix = strdup("Relay");
	}

	if (config.screen_timeout == 0) {
		s_timeout = 10;
	} else {
		s_timeout = config.screen_timeout;
	}

	if (config.motion_timeout == 0) {
		m_timeout = 30;
	} else {
		m_timeout = config.motion_timeout;
	}

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

	sprintf(topic, "%s/relays/upper", prefix);
	MQTTSubscribe(&c, topic, 0, handle_relay1);
	sprintf(topic, "%s/relays/lower", prefix);
	MQTTSubscribe(&c, topic, 0, handle_relay2);

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
			sprintf(topic, "%s/relays/upper_state", prefix);
			MQTTPublish(&c, topic, &message);
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
			sprintf(topic, "%s/relays/lower_state", prefix);
			MQTTPublish(&c, topic, &message);
			lrelaystate = buffer[0];
		}

		lseek(uswitch, 0, SEEK_SET);
		read(uswitch, buffer, sizeof(buffer));
		if (buffer[0] == '0' && uswitchstate == 1) {
			uswitchstate = 0;
			if (config.send_switch) {
				message.qos = 1;
				message.payload = payload;
				sprintf(payload, "on");
				message.payloadlen = strlen(payload);
				sprintf(topic, "%s/switches/upper", prefix);
				MQTTPublish(&c, topic, &message);
			}
			if (config.switch_toggle)
				toggle_relay(UPPER_RELAY);
		} else if (buffer[0] == '1' && uswitchstate == 0) {
			uswitchstate = 1;
		}
		lseek(lswitch, 0, SEEK_SET);
		read(lswitch, buffer, sizeof(buffer));
		if (buffer[0] == '0' && lswitchstate == 1) {
			lswitchstate = 0;
			if (config.send_switch) {
				message.qos = 1;
				message.payload = payload;
				sprintf(payload, "on");
				message.payloadlen = strlen(payload);
				sprintf(topic, "%s/switches/lower", prefix);
				MQTTPublish(&c, topic, &message);
			}
			if (config.switch_toggle)
				toggle_relay(LOWER_RELAY);
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
			sprintf(topic, "%s/sensors/temperature", prefix);
			MQTTPublish(&c, topic, &message);
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
			sprintf(topic, "%s/sensors/humidity", prefix);
			MQTTPublish(&c, topic, &message);
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
			last_motion = time(NULL);
			if (!motion) {
				motion = 1;
				message.qos = 1;
				message.payload = payload;
				sprintf(payload, "on");
				message.payloadlen = strlen(payload);
				message.retained = 1;
				sprintf(topic, "%s/motion", prefix);
				MQTTPublish(&c, topic, &message);
			}
		}

		while (read(input, &event, sizeof(event)) > 0) {
			last_input = time(NULL);
			if (power != '1') {
				power = '1';
				write(screen, &power, sizeof(power));
			}
		}
		
		if ((time(NULL) - last_input > s_timeout) && power == '1') {
			power = '0';
			write(screen, &power, sizeof(power));
		}
		
		if ((time(NULL) - last_motion > m_timeout) && motion) {
			motion = 0;
			message.qos = 1;
			message.payload = payload;
			sprintf(payload, "off");
			message.payloadlen = strlen(payload);
			message.retained = 1;
			sprintf(topic, "%s/motion", prefix);
			MQTTPublish(&c, topic, &message);
		}
		
		if (MQTTYield(&c, 100) < 0)
			mqtt_connect(&n, &c, buf, readbuf);
	}
}
