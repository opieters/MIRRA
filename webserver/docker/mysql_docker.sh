#!/usr/bin/env bash

mysql_dir=$HOME/docker/mysql/data
mkdir -p ${mysql_dir}

docker run --name gems_mysql -p 3306:3306 -v ${mysql_dir}:/var/lib/mysql -e MYSQL_ROOT_PASSWORD=test123 -d mysql

# docker exec -i gems_mysql /usr/bin/mysql -u root --password=test123 -e "CREATE DATABASE microclimate_measurement_system"
# cat mms_dump.sql | docker exec -i gems_mysql /usr/bin/mysql -u root --password=test123 microclimate_measurement_system