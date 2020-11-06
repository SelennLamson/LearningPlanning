import os
import csv
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns

def plot_category(category, files, allfiles=True, drawregions=True, step=1, dense=False, forward=0):
    if dense:
        fig, axs = plt.subplots(2, 3, figsize=(15, 10))
        i0 = (0, 0)
        i1 = (0, 1)
        i2 = (0, 2)
        i3 = (1, 0)
        i4 = (1, 1)
        i5 = (1, 2)
    else:
        fig, axs = plt.subplots(5, 1, figsize=(10, 50))
        i0 = 0
        i1 = 1
        i2 = 2
        i3 = 3
        i4 = 4
        i5 = None

    colors = np.array([
        [29, 81, 82],
        [78, 193, 196],
        [217, 69, 74],
        
        [226, 185, 0],
        [110, 0, 255],
        [255, 0, 144],
        [255, 67, 0],
        
        [140, 255, 64],
        [64, 140, 255],
        [255, 64, 140],
        
        [120, 200, 0],
        [0, 120, 200],
        [200, 0, 120],
        
        [50, 100, 0],
        [0, 50, 100],
        [100, 0, 50],
        
        [100, 100, 100],
        [0, 150, 0],
        [0, 0, 150],
        [150, 0, 150],
        [150, 150, 0],
        [0, 150, 150]
    ], dtype=float) / 255.0

    allFiles = files[:]
    if allfiles:
        for f in os.listdir(category + "/"):
            if f.endswith(".csv") and f not in files:
                allFiles.append(f)

    for i, f in enumerate(allFiles):
        df = None
        try:
            df = pd.read_csv(category + "/" + f, low_memory=False)

            for col in df.columns:
                df[col] = pd.to_numeric(df[col], errors ='coerce').fillna(0).astype('float')
            df.fillna(0)
        except:
            print("Error loading:", category + "/" + f)
            continue
        
        #legend = category + ": " + f[6:-4].replace("_", " ")
        legend = f[6:-4].replace("_", " ")

        # Plotting counter-examples acquisition
        y = np.array([[df[col][i] for col in df.columns if col.startswith("CounterExamples_")]
                      for i in range(len(df)) if i % step == 0], dtype=float)
        y[y > 10000] = 0
        y = y[forward:]
        ymean = y.mean(axis=1)
        x = np.arange(y.shape[0]) * step + step * forward

        color = colors[i % len(colors), :]
        axs[i0].plot(x, ymean, lw=2, label=legend, color=color)
        if drawregions:
            ymin = np.percentile(y, 10, axis=1)
            ymax = np.percentile(y, 90, axis=1)
            axs[i0].plot(x, ymin, lw=1, color=color, linestyle="dashed")
            axs[i0].plot(x, ymax, lw=1, color=color, linestyle="dashed")
            axs[i0].fill_between(x, ymin, ymax, color=color, alpha=0.1)


        # Plotting average rule specificity
        y = np.array([[df[col][i] for col in df.columns if col.startswith("Specificity_")]
                     for i in range(len(df)) if i % step == 0], dtype=float)
        y[y > 40.0] = 100.0
        y[y < -40.0] = 100.0
        y = y[forward:]
        
        ymean = y.mean(axis=1)

        axs[i1].plot(x, ymean, lw=2, label=legend, color=color)
        if drawregions:
            ymin = np.percentile(y, 10, axis=1)
            ymax = np.percentile(y, 90, axis=1)
            axs[i1].plot(x, ymin, lw=1, color=color, linestyle="dashed")
            axs[i1].plot(x, ymax, lw=1, color=color, linestyle="dashed")
            axs[i1].fill_between(x, ymin, ymax, color=color, alpha=0.1)


        # Plotting variational distance
        y = np.array([[df[col][i] for col in df.columns if col.startswith("VarDist_")]
                     for i in range(len(df)) if i % step == 0], dtype=float)
        y[y > 40.0] = 1.0
        y[y < -40.0] = 1.0
        y = y[forward:]

        if (len(y[0]) == 0):
            continue

        x = []
        for i in range(y.shape[0]):
            if y[i, 0] != -1.0:
                x.append(i * step)
        y = y[[(xi // step) for xi in x], :]

        ymean = y.mean(axis=1)

        axs[i2].plot(x, ymean, lw=2, label=legend, color=color)
        if drawregions:
            ymin = np.percentile(y, 10, axis=1)
            ymax = np.percentile(y, 90, axis=1)
            axs[i2].plot(x, ymin, lw=1, color=color, linestyle="dashed")
            axs[i2].plot(x, ymax, lw=1, color=color, linestyle="dashed")
            axs[i2].fill_between(x, ymin, ymax, color=color, alpha=0.1)


        # Plotting rule distance
        y = np.array([[df[col][i] for col in df.columns if col.startswith("RuleDist_")]
                     for i in range(len(df)) if i % step == 0], dtype=float)
        y[y > 40.0] = 40.0
        y[y < -40.0] = 40.0
        y = y[forward:]

        if (len(y[0]) == 0):
            continue

        x = []
        for i in range(y.shape[0]):
            if y[i, 0] != -1.0:
                x.append(i * step)
        y = y[[(xi // step) for xi in x], :]

        ymean = y.mean(axis=1)

        axs[i3].plot(x, ymean, lw=2, label=legend, color=color)
        if drawregions:
            ymin = np.percentile(y, 10, axis=1)
            ymax = np.percentile(y, 90, axis=1)
            axs[i3].plot(x, ymin, lw=1, color=color, linestyle="dashed")
            axs[i3].plot(x, ymax, lw=1, color=color, linestyle="dashed")
            axs[i3].fill_between(x, ymin, ymax, color=color, alpha=0.1)


        # Plotting planning distance
        y = np.array([[df[col][i] for col in df.columns if col.startswith("PlanDist_")]
                     for i in range(len(df)) if i % step == 0], dtype=float)
        y[y > 40.0] = -1.0
        y[y < -40.0] = -1.0
        y = y[forward:]
        if (len(y[0]) == 0):
            continue

        x = []
        for i in range(y.shape[0]):
            if y[i, 0] != -1.0:
                x.append(i * step)
        y = y[[(xi // step) for xi in x], :]

        ymean = y.mean(axis=1)

        axs[i4].plot(x, ymean, lw=2, label=legend, color=color)
        if drawregions:
            ymin = np.percentile(y, 10, axis=1)
            ymax = np.percentile(y, 90, axis=1)
            axs[i4].plot(x, ymin, lw=1, color=color, linestyle="dashed")
            axs[i4].plot(x, ymax, lw=1, color=color, linestyle="dashed")
            axs[i4].fill_between(x, ymin, ymax, color=color, alpha=0.1)

    # Legends and titles
    detail = "" if dense else (" (solid line: avg" + (", dashed lines: 1st and last decile" if drawregions else "") + ")")
    
    axs[i0].legend(loc="best")
    axs[i0].set_title("Knowledge Revisions" + detail)
    axs[i0].set_xlabel("Actions")
    axs[i0].set_ylabel("Counter-Examples")

    #axs[i1].legend(loc="best")
    axs[i1].set_title("Rule specificity" + detail)
    axs[i1].set_xlabel("Actions")
    axs[i1].set_ylabel("Average rule specificity")

    #axs[i2].legend(loc="best")
    axs[i2].set_title("Variational Distance" + detail)
    axs[i2].set_xlabel("Actions")
    axs[i2].set_ylabel("Variational Distance")

    #axs[i3].legend(loc="best")
    axs[i3].set_title("Rule Distance" + detail)
    axs[i3].set_xlabel("Actions")
    axs[i3].set_ylabel("Rule Distance")

    #axs[i4].legend(loc="best")
    axs[i4].set_title("Planning Distance" + detail)
    axs[i4].set_xlabel("Actions")
    axs[i4].set_ylabel("Planning Distance")
    
    if dense:
        axs[i5].set_visible(False)
    
    plt.show()
    
    return fig