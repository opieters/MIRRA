# -*- coding: utf-8 -*-
"""
    *********
    utils.py
    *********

    Withholds useful functions which can be used throughout the
    whole application.
"""

import csv
import io
import sys
import time
import config

from datetime import datetime


def debug_print(value):
    """
    This function should be used to print all values, it is a wrapper for the
    normal sys.sdtout.write functionality that includes a timestamp and a
    toggleable enabling of this function.

    As the name suggests it should be used to print debug messages.

    :param value: the string to print.
    :return: Nothing.
    """
    if config.debug_settings['verbose']:
        if config.debug_settings['timestamp']:
            sys.stdout.write(__get_current_timestamp_log_format() + " " + str(value) + "\n")
        else:
            sys.stdout.write(str(value) + "\n")


def get_current_timestamp_influxdb():
    """
    This function returns the current server time in nanoseconds since
    UNIX epoch time, a format that influxdb uses.

    :return: The current server time in nanoseconds epoch time
    :rtype: str
    """
    return datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')


def get_current_timestamp():
    """
    This function returns the current timestamp of the server.

    :return: The current time of the server in the format 'Y-m-d H:M:S'
    :rtype: str
    """
    return datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')


def __get_current_timestamp_log_format():
    """
    This function is used by the debug_print function to get the current
    timestamp without date. Can thus be used when a timestamp of only hours,
    minutes and seconds are needed.

    :return: The current server time in the format HH:MM:SS
    :rtype: str
    """
    return datetime.fromtimestamp(time.time()).strftime('%H:%M:%S')


def convert_epoch_to_mysql_timestamp(epoch_timestamp):
    """
    Converts a given epoch timestamp in seconds to the MySQL datetime format.

    :param epoch_timestamp: The timestamp as seconds since epoch time
    :return: The MySQL timestamp string in the format 'Y-m-d HH:MM:SS'
    :rtype: str
    """
    try:
        epoch = time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(epoch_timestamp))
        return epoch
    except Exception as e:
        print(e)
        return None


def create_csv_report(dataset, advanced_options):
    """
    This function will create a CSV string from a given dataset as
    retrieved from the MySQL database manager class.

    :param dict dataset: The MySQL result set as retrieved from the database
        manager class, this dataset is structured as a Dict.
    :param list advanced_options: a list of advanced options to include in
        the csv. supported entries: 'csv_header_row'

    :return: The resulting CSV data as a string
    :rtype: str
    """
    csv_body = ""

    # we need to add a header row
    if "csv_header_row" in advanced_options:
        # get the first line and get its dict keys == our headers
        for header in dataset[:1][0].keys():
            csv_body += header + ','
        csv_body = csv_body[:-1] + '\n'

    # iterate all the datapoint and transform the dict to a csv line to append to the csv body
    for datapoint in dataset:
        csv_line = ""
        for key, value in datapoint.items():
            csv_line += str(value) + ","
        csv_body += csv_line[:-1] + '\n'

    return csv_body


"""
    NOTE:: 
    
    The following functions are not in use and were used for parsing and uploading CSV data to the database.
    They are not implemented in this version of the software, but are left here for future use.

"""


def uploaded_csv_file_to_datastream(uploaded_file):
    stream = io.StringIO(uploaded_file.stream.read().decode("UTF8"), newline=None)
    return csv.reader(stream)


def parse_uploaded_CSV_file(flask_request):
    """
    This function takes the Flask request and performs some checks: correct file,
    correct name and valid csv file.

    :param flask_request: The flask request object from a POST request

    :return: message,correct_file as string,boolean to indicate if its a good file and the message
             containing the detailed error/success message.
    :rtype: tuple

    """
    if 'csv_upload_file' not in flask_request.files:
        return 'No file selected, try again!', False

    file = flask_request.files['csv_upload_file']
    if file.filename == '':
        return 'File has no name, try again!', False

    try:
        csv_stream = uploaded_csv_file_to_datastream(file)
    except:
        return 'Uploaded file was not a valid CSV file, try again!', False

    return 'Succes!', True
