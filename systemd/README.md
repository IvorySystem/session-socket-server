# systemd

Example systemd service file for running `isss` as a background service.

## Installation

Copy the service file:

```sh
sudo cp systemd/isss.service /etc/systemd/system/
```

Reload systemd:

```sh
sudo systemctl daemon-reload
```

Enable the service at boot:

```sh
sudo systemctl enable isss
```

Start the service:

```sh
sudo systemctl start isss
```

Check status:

```sh
sudo systemctl status isss
```

View logs:

```sh
journalctl -u isss -f
```

## Assumptions

The example service file assumes:

* `isss` is installed as `/usr/local/bin/isss`
* scripts are located under `/var/lib/ivory/scripts`
* a dedicated `ivory` user and group already exist
* logging is handled through `journald`

## Example User Creation

```sh
sudo useradd --system --home /var/lib/ivory --shell /usr/sbin/nologin ivory
sudo mkdir -p /var/lib/ivory/scripts
sudo chown -R ivory:ivory /var/lib/ivory
```

## Service Management

Restart after changes:

```sh
sudo systemctl restart isss
```

Stop the service:

```sh
sudo systemctl stop isss
```

Disable automatic startup:

```sh
sudo systemctl disable isss
```
