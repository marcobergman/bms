# Battery Monitoring System
Design for a low-key, minimalistic Battery Monitoring System, to be used for a 4-cell LiFePO4 battery on a sailing boat. Well-known, well-documented and readily available parts, less is more.

Highlights are:
* Chargers are currently Victron devices (shore power, solar, engine through DC/DC) that have LiFePO4 profiles, but are unaware of each other's presence, the SOC or the cumulative current flowing into the cells. Rather than trying to make them work in unison, idea behind this design is to take control over the process, to enforce proper 20%-90% cycles to prevent memory effects, and, of course, prevent over and under charging.
* Solid State Relays to switch charger and/or load, depending on cell voltage, SOC and tail current.
* Wemos D1 Mini sends [data to SignalK](https://github.com/marcobergman/ESP8266SignalkClient) through WIFI.
* Current measuring to calculate SOC and tail current.
* Temperature measuring to prevent charging onder zero celcius, overheating, and to be able to respond to temperature drift of measurements.
* No active or passive cell balancing. That's a later phase. Let's first see if that is needed. I ordered grade A cells and like to see first what imbalance I get, and what currents I would need to balance them. I am not planning to charge my cells to that point where they are almost full up - and it seems to be only that part of the charge curve where you are able to effectively balance. Balancing on the flat part of the curve is very doubtful, and if at all would depend on the accuracy of the voltage measurements I'm able get with my intended setup. In the meantime, I'll bleed off some charge manually with a car headlight if needed.
* Contrary to drawings, [SSR relays](https://nl.aliexpress.com/item/32262347720.html) only have Normal Open 'contacts'. Idea is that if Wemos fouls up, you can pull it from its socket and then the relays will be 'Closed'.
* As for current sensing, I'll use an [INA228](https://www.ti.com/document-viewer/INA228/datasheet/GUID-7CC9DEF5-A5FE-412D-B339-B6B8A1E08DE2#TITLE-SBOSA20SBOS54764917). The resolution of the INA226 would suffice as well, but the INA226 does not accumulate power (SOC) so I'd have to keep the Wemos running. With the INA228, I can put the Wemos to sleep if it senses I shut down Signalk, and the alert output of the INA228 can then wake up the Wemos when the current goes over a certain limit.
* 
![image](https://github.com/marcobergman/bms/assets/17980560/17fee2ed-b95e-4b68-945b-694634d19762)

Data sheets
* [INA228](https://www.ti.com/document-viewer/INA228/datasheet/GUID-7CC9DEF5-A5FE-412D-B339-B6B8A1E08DE2#TITLE-SBOSA20SBOS54764917) ([library](https://github.com/adafruit/Adafruit_INA228))
* [ADS1115](https://www.ti.com/document-viewer/ADS1115/datasheet/typical-application-sbas4449076#SBAS4449076) ([library](https://github.com/wollewald/ADS1115_WE))
* [DS18B20](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf) ([library](https://github.com/milesburton/Arduino-Temperature-Control-Library))

Questions:
* BMS parameters can be set in the Wemos EEPROM. Current idea is to set those values in SignalK paths and let the Wemos compare it to its stored values, and replace those stored values if they are changed. Is there a better way of doing this?
* I think the voltage measuring could be done smarter. All multi-channel AD converters I can find are limited to their input not exceeding the Vdd, which forces me to use voltage dividers, and even with high-precision resistors it would require calibration. Surely there's better solutions for that?
