wink-handler
============

This is a simple app that can be run on a Wink Relay to turn it into a generic MQTT device. It will send button pushes and sensor data to the configured MQTT server, and will accept commands to turn on and off the built-in relays and screen.

Download
--------

Grab the zip file from https://github.com/mjg59/wink-relay-handler/releases and extract it.

Building
--------

Note: you don't need to do this if you've downloaded the binary - just use the wink-handler file from the zip archive.

You'll need the Android NDK installed. Run ANDROID_NDK=/path/to/android/Ndk make

Installing
----------

You'll need adb access to a rooted Wink Relay. Disable the existing Wink control software by running

```
pm disable http://com.quirky.android.wink.projectone
```

as root. Remount /system read-write:

```
mount -o rw,remount /system
```

Delete /system/bin/edisonwink:

```
rm /system/bin/edisonwink
```

adb push wink-handler to /sdcard and then copy it over edisonwink and fix permissions:

```
cp /sdcard/wink-handler /system/bin/edisonwink
chmod 755 /system/bin/edisonwink
```

Now write a config file that looks something like:

```
host=192.168.1.5
port=1883
user=username
password=password
clientid=Wink_Relay1
topic_prefix=Relay1
screen_timeout=20
switch_toggle=false
send_switch=true
```
and put that in /sdcard/mqtt.ini on the Wink Relay.

host: Hostname or IP address of the MQTT broker  
port: Port of the MQTT broker  
user: Username used to authenticate to the MQTT broker (optional)  
password: Password used to authenticate to the MQTT broker (optional)  
clientid: Client ID passed to the broker (optional - Wink_Relay if not provided)  
topic_prefix: Prefix to the topics presented by the device (optional - Relay if not provided)  
screen_timeout: Time in seconds until the screen turns off after a touch or proximity detection (optional - 10s if not provided)  
switch_toggle: Whether pressing the switch should toggle the relay directly (optional - false if not provided)  
send_switch: Whether pressing the switch should generate an MQTT message (optional - true if not provided)

Finally, reset your Relay.

```
reboot
```

It will then reboot - the relays will default to on post-boot.

Sensors
-------

Temperature information will be posted to the

```
Relay/sensors/temperature
```

state topic. Humidity information will be posted to the

```
Relay/sensors/humidity
```

state topic.

Buttons
-------

Button presses will be posted to

```
Relay/switches/upper
```

and

```
Relay/switches/lower
```

state topics.


Relays
------

Relays can be controlled via the

```
Relay/relays/upper
```

and

```
Relay/relays/lower
```

command topics, and will report their state to the

```
Relay/relays/upper_state
```

and

```
Relay/relays/lower_state
```

state topics.

Screen
------

The screen will turn on if the screen is touched or if the proximity sensor is triggered. It can also be controlled via the

```
Relay/screen
```

command topic: "ON" turns the screen on, "OFF" turns the screen off.

The screen will turn off 10 seconds after the last touch, proximity, or mqtt "ON" command by default. This duration can be configured via the `screen_timeout` parameter in mqtt.ini.
