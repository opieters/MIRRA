import datetime
import struct
import pandas as pd

sensor_map = {
    65: "ESPCAM2",
    64: "ESPCAM1",
    22: "light",
    4: "TempSoil",
    12: "TempAir",
    13: "RH"
}

# f4cfa2bd1fb8 c118b463060c6d84b4410d41f63042040080b441160080994340000000004100000000
with open("experiment-2023_01_03-16_10_20.log") as f:
    data = [i.strip() for i in f.readlines()]
    data = [i.split("INFO b'")[1] for i in data]
    data = [i.replace("\\r\\n'", "") for i in data]
    data = "".join(data)
    data = data.replace(" ", "")
    data = [data[i:i+2] for i in range(0,len(data),2)]
    print(data)
    data = [int(i, 16) for i in data]
    i = 0
    df = dict()
    df["MAC"] = []
    df["time"] = []
    try:
        while i < len(data):
            mac = data[i:i+6]
            i += 6
            time = data[i:i+4]
            i+= 4
            n_values = data[i]
            i += 1
            value_data = data[i:i+n_values*5]
            i += n_values*5

            # add MAC
            #mac = f"{mac[0]:02X}:{mac[1]:02X}:{mac[2]:02X}:{mac[3]:02X}:{mac[4]:02X}:{mac[5]:02X}"
            mac = f"{mac[0]}:{mac[1]}:{mac[2]}:{mac[3]}:{mac[4]}:{mac[5]}"
            ctime = struct.unpack("<I", bytes(time))[0]
            ctime = datetime.datetime.fromtimestamp(ctime)
            df["MAC"].append(mac)
            df["time"].append(ctime)

            for j in range(n_values):
                key = value_data[5*j]
                if key in sensor_map:
                    key = f"sensor-{sensor_map[key]}"
                else:
                    key = f"sensor-{key}"
                key_value = struct.unpack("<f", bytes(value_data[5*j+1:5*(j+1)]))[0]
                if key not in df:
                    df[key] = [None]*len(df["time"])

                    df[key][-1] = key_value
                else:
                    df[key].append(key_value)
            sensor_keys = [j for j in df if j.startswith("sensor")]
            for j in sensor_keys:
                while len(df[j]) != len(df["time"]):
                    df[j].append(None)
    except:
        pass

    for j in sensor_keys:
        while len(df[j]) != len(df["time"]):
            df[j].append(None)

    df = pd.DataFrame(df)
    df.to_csv("eline.csv", index=False)
    print(df)