import sys
import fileinput
import re
import json
import copy

from pyleri import Choice
from pyleri import Optional
from pyleri import Grammar
from pyleri import Keyword
from pyleri import Repeat
from pyleri import Sequence
from pyleri import Regex
from pyleri import List
from pyleri import Prio
from pyleri import end_of_statement


class Addb2Grammar(Grammar):

    r_start       = Regex("^\*")
    r_meas_time   = Regex("[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{9}")
    r_meas        = Regex(".*")

    r_attr_start  = Regex("^\|")
    r_attr_name   = Regex("[A-Za-z0-9]+")
    r_attr        = Optional(Regex("[^|*].*"))

    r_hist_start  = Regex("^\|")
    r_hist_key    = Optional(Regex("[-]{0,1}[0-9]+"))
    r_hist_sep    = Regex(":")
    r_hist_val    = Regex("[0-9]+")
    r_hist_end    = Regex("\|")

# --1
# * 2015-04-14-15:33:11.998165453 fom-descr service: <7300000000000001:0>, sender: c28baccf27e0001

# --2
# |           :         0 |
# |         1 :         0 |
# |        ....           |
# |        25 :         0 |

# --3
# |         node             <11186d8bf0e34117:ab1897c062a22573>
# |         ....
# |         fom              @0x7f795008ed20, 'IO fom', transitions: 0, phase: 0
# |         ....

    START = Repeat(Sequence(
        r_start, Sequence(r_meas_time,                  # 1
                          r_meas),
        Optional(Repeat(Sequence(r_hist_start,          # 2
                                 r_hist_key,
                                 r_hist_sep,
                                 r_hist_val,
                                 r_hist_end))),
        Repeat(Sequence(r_attr_start,                   # 3
                        r_attr_name,
                        r_attr))))


class Addb2Visitor:
    def __init__(self):
        # [ { 'time': 'XXX', 'measurement': 'YYY', 'params': ['p1','p2'] } ]
        self.result    = []
        self.meas      = {}
        self.attr_name = None

    def visit(self, node):
        name = node.element.name if hasattr(node.element, 'name') else None
        if name == "r_start":
            self.meas = copy.copy({"time": None, "measurement": None, "params": []})
            self.result.append(self.meas)
        else:
            if name == "r_meas_time":
                self.meas["time"] = node.string
            if name == "r_meas":
                self.meas["measurement"] = node.string
            elif name == "r_attr_start":
                self.meas["params"].append({})
            elif name == "r_attr_name":
                self.attr_name = node.string
                self.meas["params"][-1][self.attr_name] = None
            elif name == "r_attr":
                self.meas["params"][-1][self.attr_name] = node.string
                self.attr_name = None


def visit_node(node, children, visitor):
    visitor.visit(node)

def visit_children(children, visitor):
    for c in children:
        visit_node(c, visit_children(c.children, visitor), visitor)

def visit_tree(res):
    start = res.tree.children[0] if res.tree.children else res.tree
    v = Addb2Visitor()
    visit_node(start, visit_children(start.children, v), v)
    return v.result

def inp():
    starred = False
    bunch = []
    for measurement in fileinput.input():
        if measurement[0] == '*':
            if not starred:
                starred = True
            else:
                starred = False
                mstr = "".join(bunch)
                res = grammar.parse(mstr)
                if not res.is_valid:
                    print("Bang!")
                    print(mstr)
                    sys.exit(1)
                print(repr([x['measurement'] for x in visit_tree(res)]))
                bunch.clear()

            bunch.append(measurement)
        else:
            bunch.append(measurement)

if __name__ == '__main__':
    grammar = Addb2Grammar()
    res = grammar.parse(string)
    print(res.is_valid)
    print(repr(visit_tree(res)))

