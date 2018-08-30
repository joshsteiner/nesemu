import re
import string
from itertools import takewhile, islice
from pprint import pprint 


def is_hex(n):
    return all(c in string.hexdigits for c in n)


def parse_line(file_name):
    with open(file_name) as file:
        for line in file:
            elems = re.split(r"\s+", line)
            es = list(takewhile(is_hex, elems))
            if len(es[-1]) == 3:
                es.pop()
            es += [x for x in elems if len(x) >= 4 and (x[1] == ":" or x[2] == ":")]
            # i = next(i for i, e in enumerate(es) if e.startswith("P:"))
            # es[i] = "P:" + format(int(es[i][2:], 16), "#010b")[2:]
            yield es


def nestest_log_lines():
    for nt_p in parse_line("nestest.log"):
        nt_p[-1] = nt_p[-1][0] + nt_p[-1][2:]
        yield nt_p
    

def out_log_lines():
    for out_p in parse_line("out.log"):
        out_p[-1], out_p[-2] = out_p[-2], out_p[-1]
        yield out_p


def rpr(line):
    al = len(line) - 6
    if al == 2:
        line.insert(3, "  ")
    elif al == 1:
        line.insert(2, "  ")
        line.insert(2, "  ")
    return "  ".join(line)


for nt, out in zip(nestest_log_lines(), out_log_lines()):

    padding = " " * 5 + ("|" if nt == out else "#") + " " * 5
    print(rpr(nt) + padding + rpr(out))
    #print(out)
