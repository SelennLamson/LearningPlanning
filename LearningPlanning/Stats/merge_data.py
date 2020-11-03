import os
import numpy as np
import pandas as pd

def merge_files(base_path, output_filename, filenames):
    files = []
    new_keys = set()
    for f in filenames:
        df = None
        try:
            df = pd.read_csv(base_path + f, low_memory=True)
        except:
            print("Error loading:", base_path + f)
            continue
        
        keys_found = set()
        cols_to_drop = set()
        for col in df:
            try:
                keys_found.add(int(col.split("_")[-1]))
            except ValueError:
                cols_to_drop.add(col)
        
        for col in cols_to_drop:
            df = df.drop(labels=col, axis=1)
        
        keys_dict = dict()
        for key in keys_found:
            if key in new_keys and key not in keys_dict:
                nk = 0
                while nk in new_keys:
                    nk += 1
                new_keys.add(nk)
                keys_dict[key] = nk
            elif key not in new_keys:
                new_keys.add(key)
                keys_dict[key] = key
        
        cols_dict = dict()
        for col in df:
            key = int(col.split("_")[-1])
            name = "_".join(col.split("_")[:-1])
            cols_dict[col] = name + "_" + str(keys_dict[key])
        
        df.rename(columns=cols_dict)
        
        files.append(df)
    
    merged = pd.concat(files, axis=1)
    merged.to_csv(base_path + output_filename)
