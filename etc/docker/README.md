# OTBR Docker

This file contains troubleshooting for common issues with OTBR Docker.

## rsyslog cannot start

If `rsyslog` cannot start successfully (there is no response) during the OTBR Docker image booting-up:

```
+ sudo service rsyslog status
 * rsyslogd is not running
+ sudo service rsyslog start
 * Starting enhanced syslogd rsyslogd
```

This is caused by the high limit number of file descriptors (`LimitNOFILE`). `rsyslog` takes a long time to run a for loop to close all open file descriptors when this limit is high (for example, `1073741816`).

To solve this issue, add the following configuration to `/etc/docker/daemon.json`:

```
  "default-ulimits": {
    "nofile": {
      "Name": "nofile",
        "Hard": 1024,
        "Soft": 1024
    },
    "nproc": {
      "Name": "nproc",
        "Soft": 65536,
        "Hard": 65536
    }
  }
```

And then reload & restart the `docker` service:

```
sudo systemctl daemon-reload
sudo systemctl restart docker
```
