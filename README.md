# H801_MQTT
ESP8266 based LED controller using MQTT and/or HTTP

### Features
* Possible to control each channel separatly
* Allows fading from one state to another
* MQTT support
* REST API support
* HTTP page support, builtin HTML page allowing control of colors
* Possible to control fade interval for each channel individually 

### Control
Available properties to set

| Property | Info | Range |
|---|---|
| `R` | Red channel | 0-255 |
| `G` | Green channel | 0-255 |
| `B` | Blue channel | 0-255 |
| `W1` | White 1 channel | 0-255 |
| `W2` | White 2 channle | 0-255 |
| `duration` | Number of milliseconds used to fade to new state | 0 - 100000000 (1.2 days) |

Each channels fading is handled separatly, this means that it's possible to execute multiple fade event with different duration separatly for each channel.


Example: The following JSON will change the Red channel to max and Green to min over 5 seconds.
```json
{
  "R": 255,
  "G": 0,
  "duration": 5000
}
```

#### MQTT
By sending JSON encoded string to the MQTT topic `{id}/set`, where id is the unique chip id value, it's possible to control the channels.
The MQTT topic `{id}/updated` will be emitted when changes are made for any of the channels.


#### HTTP POST
By sending JSON encoded string to the `/status` page it's possible to controll the channels.

#### HTTP GET
By doing and HTTP GET on the `/status` page and providing HTML encoded variable, example `/status?R=255&time=5000`, it is possible to set the values.


