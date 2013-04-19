#!/usr/bin/python
import os
import sys
from pylab import *
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import numpy as np


def gen_hist(intotal):
    total = sorted(intotal)
    if len(total) > 500:
        total = total[int(len(total)/100):-int(len(total)/100)]
    strides = 50

    n, bins, patches = hist(total, strides, normed=1, facecolor='green', alpha=0.75)
    bincenters = 0.5*(bins[1:]+bins[:-1])
    y = mlab.normpdf( bincenters, 100, 15)
    l = plot(bincenters, y, 'r')

    xlabel("Latency (us)")
    ylabel("Share (% of total)")
    title("Histogram (1st and 99th % removed)")


def gen_box(data, docap = False):
    dmax = data[0][0]
    for dset in data:
        dm = max(dset)
        if dm > dmax:
            dmax = dm

    # if len(data[0]) > 500 and docap:
    #     length = len(data[0])
    #     cap = 4
    #     d2 = []
    #     for d in data:
    #         sd = d[int(length/cap):-int(length/cap)]
    #         print "New length: %d" % len(sd)
    #         d2.append(sd)
    #     data = d2
    labels = [ "%d" % (int(c)) for c in range(0, len(data))]
    xticks(range(1,len(data)+1), labels)
    d = boxplot(data)
    if (docap):
        ylim(0, dmax*0.05)
        title("Box-graph for latency (scaled, 0 - 5% of max)")
    else:
        title("Box-graph for latency")
    ylabel("Latency (us)")
    return

def main(file_in, file_out):
    data = []
    total = []
    if not os.path.isfile(file_in):
        sys.exit(1)
    f = open(file_in, 'r')
    for line in f.readlines():
        v = [int(k) for k in line.split()]
        data.append(v)
        total.extend(v)

    figure(1, figsize=(20,15))
    subplot(311)
    gen_hist(total)

    subplot(312)
    gen_box(data)

    subplot(313)
    gen_box(data, True)

    savefig(file_out, dpi=300)
    show()

if __name__ == "__main__":
    if len(sys.argv) > 2:
        main(sys.argv[1], sys.argv[2])
    else:
        print "Usage: %s input.dat graph_out.png" % (sys.argv[0])
        sys.exit(1)
