# tijd (4) | number of sensor values  (1) | sensor id (1) | sensor value (4) | ...
import datetime
from collections import namedtuple

import numpy as np
import pandas as pd
import struct

Sample = namedtuple("Sample", ["time", "n_values", "data"])
SensorData = namedtuple("SensorData", ["id", "value"])

samples = []

sensor_ids = {
    64: "esp-cam",
    22: "light",
    3: "soil-moisture",
    4: "soil-temperature",
    12: "air-temperature",
    13: "relative-humidity",
}

with open("data.log") as f:
    data = f.readlines()
    data = [i.strip() for i in data]
    data = [i for i in data if len(i) > 0]
    data = ''.join(data)
    data = [int(data[i:i+2], 16) for i in range(0,len(data),2)]

    i = 0
    while i < len(data):
        # read time
        t = bytes(data[i:i+4])
        t = struct.unpack("<I", t)
        t = datetime.datetime.fromtimestamp( t[0] )
        i += 4
        n_values = data[i]
        i += 1
        sensor_data = data[i:i+n_values*5]
        i += n_values*5

        sensor_data2 = []
        for j in range(n_values):
            s_id = sensor_data[5*j]
            s_value = struct.unpack("<f", bytes(sensor_data[5*j+1:5*(j+1)]))[0]
            sd = SensorData(id=s_id, value=s_value)
            sensor_data2.append(sd)


        sample = Sample(time=t, n_values=n_values, data=sensor_data2)
        samples.append(sample)

df = dict()


for i, sample in enumerate(samples):
    if "time" in df:
        df["time"].append(sample.time)
    else:
        df["time"] = [np.nan]*i + [sample.time]
    for j, sd in enumerate(sample.data):
        label = f"{sensor_ids[sd.id]}-{j}"
        if label in df:
            df[label].append(sd.value)
        else:
            df[label] = [np.nan]*i + [sd.value]
    cl = np.max([len(df[j]) for j in df])
    for j in df:
        if len(df[j]) < cl:
            df[j].append(np.nan)

df = pd.DataFrame(df)
df.to_csv("decoded_data.csv", index=False)
