# Server Software Design

## Table of Contents

* [Module Overview](#module-overview)
* [Project Configuration](#project-configuration)
* [Module Description](#module-description)
  - [Node-RED Flows](#node-red-flows)
    + [Main Entry](#main-entry)
    + [Handle Sensor Readings](#handle-sensor-readings)
    + [Update Firmware](#update-firmware)
    + [Update Config](#update-config)
    + [State of Health](#state-of-health)
  - [Node-RED SOH Monitor](#node-red-soh-monitor)
    + [script_main](#script_main)
    + [monitor](#monitor)
    + [Signal Traps](#signal-traps)
* [Debugging and Unit Testing](#debugging-and-unit-testing)
  - [Debugging Node-RED](#debugging-node-red)
  - [Debugging Node-RED SOH Monitor](#debugging-node-red-soh-monitor)
* [Future Improvements](#future-improvements)

---

## Module Overview

![Module Data Flow Diagram](drawio/serversw_detail_flows_block_diagram.png)  
The server software components are split into [Node-RED Flows](#node-red-flows)
and the [SOH monitor](#node-red-soh-monitor). The flows.json includes several
sub-flows (arranged on different tabs in the UI).  
Based on the command in the incoming packet, the [Main Entry](#main-entry)
handler will hand it off to the relevant sub-flow. The
[State of Health](#state-of-health) sub-flow executes periodically and is
monitored by the external [SOH Monitor](#node-red-soh-monitor) utility.

---

## Project Configuration

The content of this component are all interpreted scripts and no compilation
step is needed. See the [software architecture](server_architecture.md) for more
details.

Launching of the Node-RED server and SOH Monitor is performed at startup by
[systemd unit files](https://www.freedesktop.org/software/systemd/man/systemd.unit.html).
To some extent these are configurable, but the
[soh-monitor.sh](../node-red/soh-monitor.sh) script is currently expecting the
units to be run under the "user" service manager. This behavior should be made
more configurable. Likewise, the script expects to be communicating with
systemctl and journalctl. In order to support other init systems, these commands
should be made more easily configurable.

For reference the unit files are:
* [node-red.service](../node-red/node-red.service) - starts the Node-RED server
* [node-red-soh.service](../node-red/node-red-soh.service) - starts the SOH
  monitor script

---

## Module Description

### Node-RED Flows

The flows are found in [flows.json](../node-red/flows.json) and are split into
the following tabs:
+ [Main Entry](#main-entry)
+ [Handle Sensor Readings](#handle-sensor-readings)
+ [Update Firmware](#update-firmware)
+ [Update Config](#update-config)
+ [State of Health](#state-of-health)

#### Main Entry

![Flow Screenshot](screenshots/flows_detail_entry.png)  
The main flows listen on TCP port 2880 for incoming connections, collect
together fragmented packets, parse the packet as json, filter out the header
fields, and then pass the message on to the appropriate sub-flow based on the
command field.

The function nodes are relatively straightforward:
* "detect framing" looks for a null terminator in order to set the msg.complete
  flag
* "parse header" simply pulls out header fields from msg.payload and places them
  directly in the msg object
  - version
  - timestamp
  - node
  - firmware
* "process error" provides a TCP response of "error\0" for any error (except
  errors that it triggered with its own response)

The switch node works in the following way:
* If command == "update", transfers msg to the
  [update firmware sub-flow](#update-firmware)
* If command == "get_config" or "delete_config", transfers msg to the 
  [update config sub-flow](#update-config)
* If command is null, transfers msg to the
  [handle sensor readings sub-flow](#handle-sensor-readings)

#### Handle Sensor Readings

![Flow Screenshot](screenshots/flows_detail_handle_readings.png)

> 🪧 Note: this flow depends on the additional plugin modules:
> * [node-red-contrib-influxdb](https://flows.nodered.org/node/node-red-contrib-influxdb)
> * [node-red-contrib-readdir](https://flows.nodered.org/node/node-red-contrib-readdir)

When the msg is transferred to this sub-flow, it is first parsed to pull out the
sensor measurements. These measurements are then joined into a batch update to
be provided to the influxdb node.  
After each measurement is parsed, the response is generated - normally "OK". If
firmware updates or configuration updates are available, these flags will be
appended to the response.

The "influx status" node simply monitors status of the influxdb node and logs it
in the debug window.

**parse v2 readings**

![parse v2 readings flow chart](drawio/serversw_detail_parse_v2_readings_flow_chart.png)

**check update**

![check update flow chart](drawio/serversw_detail_check_update_flow_chart.png)

This function takes additionally as input msg.firmware_dir and msg.config_dir
which are provided by the preceding directory nodes.  These nodes can be
configured to modify the location of the respective firmware and sensor-cfg
directories, though it is probably easier to create softlinks in your Node-RED
runtime directory.

**null-terminate**

This function simply appends a "\0" character to the end of the response
message.

**process error**

![process error flow chart](drawio/serversw_detail_process_error_flow_chart.png)

As on the main tab, the "process error" function returns a basic error response
to the sensor node in case any error occurs. Likewise, it ignores errors
generated by the "tcp response" node as these would likely generate further
errors as we attempt to send the error response recursively.

The difference from the main tab is that this function also handles errors from
the influx node and we want to handle "retention" errors as though they were
successful.  
The retention error indicates that InfluxDB received the measurement correctly,
but isn't storing it because it violates the retention policy. Generally, this
means that the timestamp is too old. In this case, there is no point for the
sensor node to retransmit the measurement, so we send an "OK,old" response.

#### Update Firmware

![Flow Screenshot](screenshots/flows_detail_update_firmware.png)

> 🪧 Note: this flow depends on the additional plugin modules:
> * [node-red-contrib-readdir](https://flows.nodered.org/node/node-red-contrib-readdir)
> * [node-red-contrib-md5](https://flows.nodered.org/node/node-red-contrib-md5)

When the msg is transferred to this sub-flow, the filename is extracted from the
payload argument and used to find the matching firmware image in the "firmware"
directory.  
If the image file is found, its md5 hash will be calculated and the update
package (length, md5, data) transmitted in the response.

**parse update**

![parse update flow chart](drawio/serversw_detail_parse_update_flow_chart.png)

This function searches the list of files provided by the "firmware_dir" node
to find any that match the filename provided.  
As an extra check, it will ignore any firmware file that already matches the
firmware "fingerprint" reported by the sensor node as there is no point sending
an update file if the sensor is already running that version of software.

**transmit update**

This function crafts a response octet-string by concatenating the metadata and
content of the file:
* file length
* "\n"
* md5sum
* "\n"
* file data

> 🪧 Note: a final "\n" or "\0" is not included in this message as the file data
> will be processed by the ESP SDK firmware update functionality which expects
> a simple IO Stream with the file data only.  
> Additional framing would disturb the md5 checksum calculation.

**process error**

This function provides a TCP response of "0\n" for any error (except errors that
it triggered with its own response). 

**null-terminate**

This function simply appends a "\0" character to the end of the response
message.

#### Update Config

![Flow Screenshot](screenshots/flows_detail_update_config.png)

> 🪧 Note: this flow depends on the additional plugin modules:
> * [node-red-contrib-readdir](https://flows.nodered.org/node/node-red-contrib-readdir)
> * [node-red-contrib-md5](https://flows.nodered.org/node/node-red-contrib-md5)

When the msg is transferred to this sub-flow, the filename is extracted from the
payload argument and used to find the matching configuration file in the
"sensor-cfg" directory.  
Depending on whether the command is "get_config" or "delete_config", the file
will be returned to the sensor node or deleted, respectively.

**parse get_config**

![parse update flow chart](drawio/serversw_detail_parse_get_config_flow_chart.png)

This function searches the list of files provided by the "firmware-cfg dir" node
to find any that match the node name and filename provided. For example, the
configuration update files might be in organized in subdirectories by node name
(sensor-cfg/node_name/cfg_file), but other schemes are possible
(sensor-cfg/node_name-cfg_file).

**transmit update**

This function crafts a response octet-string by concatenating the metadata and
content of the file:
* file length
* "\n"
* md5sum
* "\n"
* file data

**send empty response**

This function provides a TCP response of "\n" if no matching file was found.

**abort upload**

This function provides a TCP response of "0\n" for any error (except errors that
it triggered with its own response). 

**null-terminate**

This function simply appends a "\0" character to the end of the response
message.

#### State of Health

![Flow Screenshot](screenshots/flows_detail_soh.png)

This flow executes asynchronously every 30 seconds (starting 3 seconds after the
flows are started).

**SOH Log**

This function performs a console.log of the string "@SOH report@" + the
timestamp.

### Node-RED SOH Monitor

The SOH Monitor script is located in
[soh-monitor.sh](../node-red/soh-monitor.sh).

The main configuration interface is via command line parameters.
These are optional:
* If 1 parameter is supplied, it is: timeout
* If 2 parameters are supplied: timeout, action count
* If 3 parameters are supplied: timeout, action count, max errors

The defaults for these (along with the additional RELOAD_TIME parameter) can be
adjusted at the top of the script.

This script depends on bash -- simpler bourne shells will probably fail in an
unexpected way. Additionally, it depends on systemd utilities: systemctl and
journalctl.

> 🪧 Note: The script expects Node-RED to be running under the "user" service
> manager rather than the "system" service manager. This should probably be
> made more configurable or allow some level of auto-detection.

There are 3 basic parts to the script:
* [Signal Traps](#signal-traps)
* [script_main](#script_main)
* [monitor](#monitor)

![soh-monitor State Chart](drawio/serversw_detail_soh_script_state_chart.png)

#### script_main

![script_main flow chart](drawio/serversw_detail_soh_script_main_flow_chart.png)

The `script_main` function parses the command line parameters, which allow overriding the default values of several parameters (as mentioned above).

It then sets up a trap for the interrupt signal and gets the process ID of the
Node-RED Server from systemd.

After that it executes `journalctl` in follow mode, filtering on the node-red
unit and outputting only the message content (cat mode). This output is piped
into the `monitor` function.  
The `monitor` function will only return if the maximum error count is exceeded,
if the input file descriptor (from `journalctl`) is closed, or if it took action
to restart the Node-RED service.  
When it returns, the return value will be the updated error count.

Assuming the max error count is not exceeded, `script_main` will wait for a few
seconds to let Node-RED finish restarting and then it will loop to the beginning
and get the new PID, start `journalctl` and pipe it into `monitor`, etc.

Most parameters are configurable on the command line, except for `RELOAD_TIME`,
which is the time that the script waits for the Node-RED service to restart.  
This is merely an oversight. For now, if you wish to modify this parameter, you
will just have to edit the [soh-monitor.sh](../node-red/soh-monitor.sh) script.

#### monitor

![monitor flow chart](drawio/serversw_detail_soh_monitor_flow_chart.png)

The `monitor` function reads lines from its standard input with a timeout. The
timeout is specified by the first argument to the function.  
The lines are parsed into fields delimited by '@'. The fields are `linehead`,
`sohmark`, and `sohtimestamp`. If `sohmark` matches "SOH report", then it is
considered to be a valid heartbeat. If the matching SOH mark is not found within
the timeout period, then the global `ERROR_COUNT` and a local `counter` will be
incremented.  
If the `counter` equals the action count (which is supplied as the second
input argument to the function), Node-RED will be restarted by invoking
`systemctl`. At this point the function will return.

**Return Value**

The function returns `ERROR_COUNT`.

**Error Handling**

If `ERROR_COUNT` exceeds the global `MAX_ERRORS`, the situation is deemed
suspicious and the function will return (and `script_main` will also return).

If the call to `read` fails with some other error code than timeout, the
function will return as the most likely cause is a broken pipe with
`journalctl`.

#### Signal Traps

The script traps SIGINT as a means of debugging.

`sigint_trap` is set as the handler for SIGINT while `script_main` is running.  
This trap will check whether you have sent an interrupt signal (via Ctrl-C)
twice within TIMEOUT seconds. The first interrupt will be ignored, but the
second interrupt within TIMEOUT seconds will cause the script to exit.  
When running the script on the command line, this allows testing of the
`sigint_monitor` behavior but also allows terminating the script easily by
pressing Ctrl-C a second time.

`sigint_monitor` is set as the handler for SIGINT while `monitor` is running.  
This trap will print the current value for ERROR_COUNT to standard output. This
can be helpful to periodically check whether any missed heartbeats are being
noticed.

Since `monitor` runs in a subshell, both handlers will trigger when running the
script from the command line. When executing the script from systemd, the
`sigint_trap` handler will be surpressed, but the `sigint_monitor` handler can
be triggered by using `kill` to send the INT signal directly to the subshell.

---

## Debugging and Unit Testing

The Node-RED flows and SOH monitor script don't have any unit tests.
😿

### Debugging Node-RED

There are several debug nodes throughout the Node-RED flows. Most of these are
disabled to avoid spam, but they can be re-enabled by clicking on the green
button next to them. You don't need to deploy the changes for this debug
enable/disable flag to take effect.  
![screenshot enable debug flag](screenshots/flows_enable_debug.png)

By default the log output in Node-RED goes to the debug window on the side
panel:  
![screenshot debug window](screenshots/flows_debug_window.png)

You can redirect it to the system console by changing the properties of the
debug node (double click):  
![screenshot debug node properties](screenshots/flows_debug_system_console.png)
> 🪧 Note: changes to these properties will not take effect until you deploy the
> flows.

Likewise, in any of the function nodes, you can call `console.log()` to send a
log message to the system console.

To monitor the system console, use `journalctl` to "follow" the log output:
```sh
journalctl --user --unit node-red.service -f
```

### Debugging Node-RED SOH Monitor

Check whether the Node-RED and Node-RED SOH service are running:
```sh
systemctl --user status node-red.service
systemctl --user status node-red-soh.service
```

You should see that these services are active (running).  
![Subshell screenshot](screenshots/soh-monitor-subshell.png)

Additionally, the last few lines of the journal output will be printed, so you
can check whether SOH report messages are being printed.q

If you send a SIGINT to the subshell running the monitor function, the script
will print out the number of heartbeat timeouts that it has caught so far. You
can find the PID of the subshell by checking the `systemctl` status for the
service as mentioned above.
```sh
kill -SIGINT <PID> #see example in screenshot above
journalctl --user --unit node-red-soh.service --lines 10
```

If you want to inject a fault condition, you can kill or suspend the Node-RED
server process:
```sh
systemctl --user status node-red.service    # get current PID
kill <Node-RED PID>   # use PID from above
journalctl --user --unit node-red-soh.service -f   # monitor response
```

Or you can briefly suspend the Node-RED server process to see the SOH failure
and recovery:
```sh
kill -STOP <Node-RED PID>
# wait a while to see SOH report failures in the soh-monitor journal
kill -CONT <Node-RED PID>
```

Additionally, you can run the soh-monitor script from the command line. This
will let you send Ctrl-C more easily to print the error count and monitor the
status without needing to use `journalctl`.
```sh
# stop the soh-monitor if it is running
systemctl --user stop node-red-soh.service
# restart node-red since it was dependent on node-red-soh
systemctl --user start node-red.service
# run the soh monitor
./soh-monitor.sh
```

---

## Future Improvements

**Node-RED Flows**

It would be good to find some way to make the tabs more modular so they can be
included into an existing set of flows or so they can be expanded upon but still
have an easy upgrade path.  
It seems like the import and export of "libraries" should make this possible.
Likewise, the use of "subflows" might offer a modular solution or at least make
the inclusion of the libraries more straightforward.

**Node-RED SOH Monitor**

It might be good to reduce the reliance on systemd in order to support other
init systems.  The availability of systemd services could likely be
auto-detected and, if not present, fallback on standard syslog monitoring and
"service" helper utilities.

Currently, the script hardcodes the `--user` parameter to systemctl and
journalctl. This should be made configurable or allow for some sort of
auto-detection mechanism.  
It seems like systemctl has enough introspection capabilities to allow for
auto-detection.
