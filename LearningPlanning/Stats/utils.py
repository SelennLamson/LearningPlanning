import json
import os
import csv
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns

def clamp01(val):
    return int(max(0, min(1, val)) * 100) / 100

def colorscale(val, mincol, maxcol):
    r = round(val * maxcol[0] + (1-val) * mincol[0])
    g = round(val * maxcol[1] + (1-val) * mincol[1])
    b = round(val * maxcol[2] + (1-val) * mincol[2])
    return (r, g, b)

def tricolorscale(val, mincol, medcol, maxcol):
    if val < 0.5:
        return colorscale(val * 2, mincol, medcol)
    else:
        return colorscale((val - 0.5) * 2, medcol, maxcol)

def darken(col, levels=1):
    return (max(0, col[0] - levels), max(0, col[1] - levels), max(0, col[2] - levels))

def lighten(col, levels=1):
    return (min(5, col[0] + levels), min(5, col[1] + levels), min(5, col[2] + levels))

def colortxt(txt, col, bg=None):
    r, g, b = col
    res = "\x1b[38;5;{}m".format(16 + 36 * r + 6 * g + b) + txt + "\x1b[38;5;0m"
    if bg is not None:
        r, g, b = bg
        res = "\x1b[48;5;{}m".format(16 + 36 * r + 6 * g + b) + res + "\x1b[48;5;231m"
    return res

def removecolorcodes(txt):
    res = ""
    escaping = False
    for c in txt:
        if c == "\x1b":
            escaping = True
        elif escaping and c == "m":
            escaping = False
        elif not escaping:
            res += c
    return res

def extractcolorcodes(txt):
    colstart = None
    content = ""
    colend = None
    escaping = False
    finished = False
    for c in txt:
        if finished:
            content += colend
            colend = None
            finished = False
            
        if c == "\x1b":
            escaping = True
            if colstart is None:
                colstart = c
            else:
                colend = c
        elif escaping and c == "m":
            escaping = False
            if colend is None:
                colstart += c
            else:
                colend += c
                finished = True
        elif escaping:
            if colend is None:
                colstart += c
            else:
                colend += c
        else:
            content += c
    return colstart, content, colend

RED = (5, 0, 0)
GREEN = (0, 5, 0)
BLUE = (0, 0, 5)
YELLOW = (5, 5, 0)
MAGENTA = (5, 0, 5)
CYAN = (0, 5, 5)
WHITE = (5, 5, 5)
GREY = (2, 2, 2)
BLACK = (0, 0, 0)
ORANGE = (5, 4, 0)

def align_strings(strings, width=110, sep="|", fixed_length=None):
    res = sep + " "
    if len(strings) == 0:
        return res
    max_len = max(len(removecolorcodes(s)) for s in strings)
    if fixed_length is not None:
        max_len = max(max_len, fixed_length)
        
    cur_width = 1 + len(sep)
    for s in strings:
        cols, cont, cole = extractcolorcodes(s)
        cont_len = len(removecolorcodes(cont))
        completed = (cols if cols is not None else "") + cont + " "*(max_len - cont_len) + (cole if cole is not None else "")
        if cur_width + max_len + 2 + len(sep) > width:
            cur_width = 1 + len(sep)
            res += "\n" + sep + " "
        res += completed + " " + sep + " "
        cur_width += max_len + 2 + len(sep)
    return res

def align_lits(lits, width=110, sep="|", fixed_length=18):
    return align_strings([l.__repr__() for l in lits], width, sep, fixed_length)

def prepend(text, prep="\t"):
    return "\n".join(prep + l for l in text.split("\n"))

QUALIF_PROB = ["a very poor chance", "a poor chance", "an average chance", "a good chance", "a very good chance"]
def qualif_probability(prob):
    if prob <= 0:
        return "no chance"
    if prob >= 1:
        return "a certain chance"
    return QUALIF_PROB[int(len(QUALIF_PROB) * prob)] + " ({}%)".format(int(prob*100))

VAR_NAMES = ['X', 'Y', 'Z', 'U', 'V', 'W', 'S', 'T', 'P', 'Q', 'R', 'G', 'H', 'M', 'N', 'O']

def rename_var(v):
    if v.startswith('_V'):
        try:
            vn = int(v[2:])
            return colortxt(VAR_NAMES[vn], (0, 0, 3))
        except (ValueError, IndexError):
            return v
    return v

def rename_vars(params):
    return [rename_var(p) for p in params]