# Node-RED flows.json
The node-red flows.json implements the measurement data collection and firmware
update procedure.

## Dependencies
The node-red flows are tested on v1.0.2 of Node-RED running on Node.js v10.15.2.
The node-red flows write data into an influxdb database. This has been tested
with v1.6.4 of influxdb-server.
Influxdb needs to be set up with a database called "home" and a measurement
called "internet_of_spores". The flows will look for this on localhost:8086.

There are some dependencies on community-contributed node programs.
These are available through the node-red palette manager and more details can
be found on [nodered.org](https://nodered.org).
The version numbers are the versions I have tested. Presumably newer versions
will also work.

* [node-red-contrib-readdir v1.0.1](https://flows.nodered.org/node/node-red-contrib-readdir)
* [node-red-contrib-md5 v1.0.4](https://flows.nodered.org/node/node-red-contrib-md5)
* [node-red-contrib-influxdb v0.4.0](https://flows.nodered.org/node/node-red-contrib-influxdb)

# Node-RED systemd unit file
The node-red.service is a systemd unit file that can be used to create a
systemd service to start and run the flows. It is compatible with being run as
a systemd user-service if installed in ~/.local/share/systemd/user/.

```shell
systemctl --user start node-red.service
systemctl --user restart node-red.service
systemctl --user stop node-red.service
systemctl --user status node-red.service
```

## State of Health Monitor
The soh-monitor.sh script will monitor the journal output of node-red and
expect periodic watch-dog statements. If node-red locks-up, the script will
restart it after a few minutes using systemctl.

The soh-monitor also has a systemd unit file, "node-red-soh.service". It can
be installed and started in the same way as node-red.service. Since it depends
on node-red.service, you only need to start the node-red-soh service and the
node-red service will automatically be started.

```shell
systemctl --user start node-red-soh.service
systemctl --user stop node-red-soh.service
systemctl --user status node-red-soh.service
```
