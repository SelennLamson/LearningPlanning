import json
import os
import csv
import numpy as np
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns

from utils import *

class Literal:
    def __init__(self, json_obj):
        self.name = json_obj[0]
        self.params = json_obj[1]
        self.nec = json_obj[2] if len(json_obj) == 3 else None
        
    def __repr__(self):
        res = self.name + "(" + ", ".join(rename_vars(self.params)) + ")"
        if self.nec is None:
            return res
        else:
            necstr = "{}%".format(int(self.nec * 100))
            res += " "*(17 - len(necstr) - len(removecolorcodes(res))) + necstr
            return colortxt(res, BLACK, tricolorscale(clamp01(self.nec), lighten(GREEN, 2), lighten(ORANGE, 2), lighten(RED, 2)))
        
    def __eq__(self, other):
        return self.name == other.name and \
               len(self.params) == len(other.params) and \
               all([self.params[i] == other.params[i] for i in range(len(self.params))])

class Constant:
    def __init__(self, json_obj):
        self.name = json_obj[0]
        self.nec = json_obj[1]
    
    def __repr__(self):
        res = self.name
        necstr = "{}%".format(int(self.nec * 100))
        res += " "*(17 - len(necstr) - len(removecolorcodes(res))) + necstr
        return colortxt(res, BLACK, tricolorscale(clamp01(self.nec), lighten(GREEN, 2), lighten(ORANGE, 2), lighten(RED, 2)))
    
    def __eq__(self, other):
        return self.name == other.name
    
class Substitution:
    def __init__(self, json_obj):
        self.map = {pair[0]:pair[1] for pair in json_obj}
        
    def __repr__(self):
        return ", ".join([rename_vars([k])[0] + "/" + v for k, v in self.map.items()])
    
    def apply(self, lit: Literal):
        subbed_params = [self.map[p] if p in self.map else p for p in lit.params]
        return Literal([lit.name, subbed_params, lit.nec])
    
    def get(self, term):
        if isinstance(term, Constant):
            term = term.name
            
        if term in self.map:
            return self.map[term]
        else:
            return None
        
    def getinv(self, term):
        if isinstance(term, Constant):
            term = term.name
            
        for k, v in self.map.items():
            if v == term:
                return k
        return None

class Rule:
    def __init__(self, json_obj):
        self.action = Literal(json_obj["action"])
        self.preconditions = [Literal(lit_obj) for lit_obj in json_obj["preconditions"]]
        self.removed_preconditions = [Literal(lit_obj) for lit_obj in json_obj["removed_preconditions"]]
        self.constants = [Constant(cst_obj) for cst_obj in json_obj["constants"]]
        self.effects = [Literal(lit_obj) for lit_obj in json_obj["effects"]]
        self.prematching = json_obj["prematching"]
        self.fulfilment = json_obj["fulfilment"]
        self.substitutions = [Substitution(sub_obj) for sub_obj in json_obj["substitutions"]]
        
    def __repr__(self):
        res = "Rule:"
        res += "\nAction: " + self.action.__repr__()
        res += "\nPreconds:\n" + prepend(align_lits(self.preconditions, width=116), prep="  ")
        res += "\nRemPreconds:\n" + prepend(align_lits(self.removed_preconditions, width=116), prep="  ")
        res += "\nConstants:\n" + prepend(align_lits(self.constants, width=116), prep="  ")
        res += "\nEffects:\n" + prepend(align_lits(self.effects, width=116), prep="  ")
        res += "\n- Fulfilment: " + str(self.fulfilment)
        res += "\n- Prematching: " + ("yes" if self.prematching else "no")
        res += "\n- Subtitutions:\n\t" + "\n\t".join([s.__repr__() for s in self.substitutions])
        return res
    
class Trace:
    def __init__(self, json_obj):
        self.state = [Literal(lit_obj) for lit_obj in json_obj["state"]]
        self.action = Literal(json_obj["action"])
        self.rev_prob = json_obj["revision"]
        self.rules = [Rule(rule_obj) for rule_obj in json_obj["rules"]]
    
    def __repr__(self):
        res = "Trace:"
        res += "\nState:\n" + prepend(align_lits(self.state, width=120), prep="  ")
        res += "\nAction: " + self.action.__repr__()
        for rule in self.rules:
            res += "\n\n"
            res += prepend(rule.__repr__(), prep="  ")
        res += "\n\n--> Revision Probability: " + str(self.rev_prob)
        return res