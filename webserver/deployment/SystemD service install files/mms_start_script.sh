#!/bin/sh

# This script is the wrapper script for the SystemD service service of this software.
# The following line will be executed by the service to run in the background.
# /usr/bin/python3 --> the python executable to run the app under. This can be interchanged with other version, as well python production servers like WSGI or Gunicorn.
# -u --> flag that sets the python interpreter to unbuffered, so the output will be written directly to the output log file
# followed by /path/to/executable
# 2>&1 >> logfile.log --> append the output of stdout and stderror the the logfile

/usr/bin/python3 -u /opt/microclimate_measurement_system/code/microclimate_monitoring_system_backend/app.py 2>&1 >> mms.log

