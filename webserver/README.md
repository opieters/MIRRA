# GEMS webservice

The GEMS webservice works using docker compose, and requires a working Docker installation.

## UFW compatability

Docker and ufw do not work together by default. Please execute the following steps to make sure only the necessary ports are accessible externally to prevent security issues: [link](https://github.com/chaifeng/ufw-docker#solving-ufw-and-docker-issues).

## Dependencies

Run `install_dependencies.sh` to install all necessary software.

Some SQL commands:

```
use mms;
tables list;
use sensor_measurements;
select * from sensor_measurements;
```