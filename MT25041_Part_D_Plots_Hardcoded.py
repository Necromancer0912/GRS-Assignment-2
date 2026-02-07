#!/usr/bin/env python3
# MT25041
# Hardcoded plot generation script for demo/viva (Assignment requirement)
# Data extracted from MT25041_Part_B_RawData.csv

import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib import font_manager
import glob
import os
import subprocess

def setup_style():
    font_files = glob.glob("/usr/share/fonts/truetype/custom/IosevkaNerdFont*.ttf")
    for fpath in font_files[:10]:
        font_manager.fontManager.addfont(fpath)
    
    try:
        plt.style.use("seaborn-v0_8-white")
    except OSError:
        plt.style.use("seaborn-white")

    colors = ["#2C3E50", "#E74C3C", "#27AE60", "#8E44AD", "#F39C12", "#3498DB", "#D35400"]
    markers = ['o', 's', '^', 'D', 'v', 'p', '*']
    mpl.rcParams["axes.prop_cycle"] = mpl.cycler(color=colors) + mpl.cycler(marker=markers)
    
    mpl.rcParams["font.family"] = "Iosevka NF"
    mpl.rcParams["font.sans-serif"] = ["Iosevka NF", "Roboto", "Helvetica", "Arial", "DejaVu Sans"]
    
    mpl.rcParams["axes.titlesize"] = 16
    mpl.rcParams["axes.titleweight"] = "bold"
    mpl.rcParams["axes.labelsize"] = 14
    mpl.rcParams["axes.labelweight"] = "medium"
    
    mpl.rcParams["xtick.labelsize"] = 12
    mpl.rcParams["ytick.labelsize"] = 12
    
    mpl.rcParams["legend.fontsize"] = 11
    mpl.rcParams["legend.frameon"] = False
    mpl.rcParams["legend.loc"] = "best"
    
    mpl.rcParams["lines.linewidth"] = 2.5
    mpl.rcParams["lines.markersize"] = 9
    mpl.rcParams["lines.markeredgewidth"] = 1.5
    mpl.rcParams["lines.markeredgecolor"] = "white"
    
    mpl.rcParams["figure.figsize"] = (10, 6)
    mpl.rcParams["figure.dpi"] = 120
    mpl.rcParams["savefig.dpi"] = 300
    
    mpl.rcParams["axes.grid"] = True
    mpl.rcParams["grid.color"] = "#E0E0E0"
    mpl.rcParams["grid.linestyle"] = "--"
    mpl.rcParams["grid.linewidth"] = 1.0
    mpl.rcParams["axes.axisbelow"] = True
    
    mpl.rcParams["axes.spines.top"] = False
    mpl.rcParams["axes.spines.right"] = False
    
    mpl.rcParams["pdf.fonttype"] = 42
    mpl.rcParams["ps.fonttype"] = 42


def get_sysinfo():
    cpu = subprocess.getoutput("lscpu | awk -F: '/Model name/ {gsub(/^[ \\t]+/, \"\", $2); print $2; exit}'")
    kernel = subprocess.getoutput("uname -r")
    return f"CPU: {cpu.strip()}\nKernel: {kernel.strip()}"


# HARDCODED DATA FROM CSV (threads=8)
# Throughput vs Message Size (threads=8)
msg_sizes_throughput = [64, 256, 1024, 4096]

a1_throughput = [0.808773, 3.526401, 5.770204, 40.105459]  # 2-copy
a2_throughput = [0.772653, 3.011256, 7.752916, 36.113068]  # 1-copy
a3_throughput = [0.002346, 1.637424, 6.662103, 26.152584]  # 0-copy

# Latency vs Thread Count (msg_size=64)
thread_counts_latency = [1, 2, 4, 8]

a1_latency = [14.355, 14.697, 15.357, 17.309]  # 2-copy
a2_latency = [14.261, 14.872, 15.627, 17.630]  # 1-copy
a3_latency = [16.185, 16.846, 17.466, 20.046]  # 0-copy

# L1 Cache Misses vs Message Size (threads=8)
a1_l1_miss = [2650581983, 2669670625, 2438051565, 3379915244]  # 2-copy
a2_l1_miss = [2582133004, 2585335886, 2617834774, 3174050172]  # 1-copy
a3_l1_miss = [618215846, 2598087086, 2787988077, 2975033981]   # 0-copy

# LLC Cache Misses vs Message Size (threads=8)
a1_llc_miss = [167434956, 215642539, 312659288, 450642815]  # 2-copy
a2_llc_miss = [162407054, 257380182, 254308869, 362135337]  # 1-copy
a3_llc_miss = [14632193, 205909527, 279044983, 297320970]   # 0-copy

# CPU Cycles per Byte vs Message Size (threads=8)
a1_cycles = [122697072238, 116470931887, 99457873380, 101434440735]
a1_bytes = [303308544, 1322402560, 4050792448, 15052812288]
a1_cycles_per_byte = [c/b for c, b in zip(a1_cycles, a1_bytes)]

a2_cycles = [122516426460, 122402635678, 127111833433, 133464850831]
a2_bytes = [289745280, 1129222400, 6871056384, 19253686272]
a2_cycles_per_byte = [c/b for c, b in zip(a2_cycles, a2_bytes)]

a3_cycles = [125862303489, 124066822629, 122175878372, 121633663024]
a3_bytes = [2509632, 1017340928, 4143276032, 13911388160]
a3_cycles_per_byte = [c/b for c, b in zip(a3_cycles, a3_bytes)]


def plot_throughput(sysinfo, out_dir):
    plt.figure()
    plt.plot(msg_sizes_throughput, a1_throughput, linewidth=2.5, label="2-copy")
    plt.plot(msg_sizes_throughput, a2_throughput, linewidth=2.5, label="1-copy")
    plt.plot(msg_sizes_throughput, a3_throughput, linewidth=2.5, label="0-copy")
    
    plt.xscale("log", base=2)
    plt.xlabel("Message size (bytes)")
    plt.ylabel("Throughput (Gbps)")
    plt.title("Throughput vs Message Size (Threads=8)")
    plt.legend(loc="best")
    
    if sysinfo:
        plt.text(0.02, 0.98, sysinfo, transform=plt.gca().transAxes,
                fontsize=8, va='top', ha='left',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='#CCCCCC'))
    
    plt.tight_layout()
    plt.savefig(os.path.join(out_dir, "MT25041_Part_D_Throughput_vs_MsgSize.png"), bbox_inches="tight")
    plt.close()


def plot_latency(sysinfo, out_dir):
    plt.figure()
    plt.plot(thread_counts_latency, a1_latency, linewidth=2.5, label="2-copy")
    plt.plot(thread_counts_latency, a2_latency, linewidth=2.5, label="1-copy")
    plt.plot(thread_counts_latency, a3_latency, linewidth=2.5, label="0-copy")
    
    plt.xlabel("Thread count")
    plt.ylabel("Latency (µs)")
    plt.title("Latency vs Thread Count (Msg Size=64)")
    plt.legend(loc="best")
    
    if sysinfo:
        plt.text(0.02, 0.98, sysinfo, transform=plt.gca().transAxes,
                fontsize=8, va='top', ha='left',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='#CCCCCC'))
    
    plt.tight_layout()
    plt.savefig(os.path.join(out_dir, "MT25041_Part_D_Latency_vs_Threads.png"), bbox_inches="tight")
    plt.close()


def plot_cache_misses(sysinfo, out_dir):
    plt.figure()
    colors = ["#2C3E50", "#E74C3C", "#27AE60"]
    
    # L1 misses
    plt.plot(msg_sizes_throughput, a1_l1_miss, color=colors[0], marker="o", linestyle="-", linewidth=2.5, label="2-copy (L1)")
    plt.plot(msg_sizes_throughput, a2_l1_miss, color=colors[1], marker="o", linestyle="-", linewidth=2.5, label="1-copy (L1)")
    plt.plot(msg_sizes_throughput, a3_l1_miss, color=colors[2], marker="o", linestyle="-", linewidth=2.5, label="0-copy (L1)")
    
    # LLC misses
    plt.plot(msg_sizes_throughput, a1_llc_miss, color=colors[0], marker="s", linestyle="--", linewidth=2.5, label="2-copy (LLC)")
    plt.plot(msg_sizes_throughput, a2_llc_miss, color=colors[1], marker="s", linestyle="--", linewidth=2.5, label="1-copy (LLC)")
    plt.plot(msg_sizes_throughput, a3_llc_miss, color=colors[2], marker="s", linestyle="--", linewidth=2.5, label="0-copy (LLC)")
    
    plt.xscale("log", base=2)
    plt.xlabel("Message size (bytes)")
    plt.ylabel("Cache misses")
    plt.title("L1 and LLC Cache Misses vs Message Size (Threads=8)")
    plt.legend(loc="best", ncol=2)
    
    if sysinfo:
        plt.text(0.02, 0.98, sysinfo, transform=plt.gca().transAxes,
                fontsize=8, va='top', ha='left',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='#CCCCCC'))
    
    plt.tight_layout()
    plt.savefig(os.path.join(out_dir, "MT25041_Part_D_CacheMisses_vs_MsgSize.png"), bbox_inches="tight")
    plt.close()


def plot_cycles_per_byte(sysinfo, out_dir):
    plt.figure()
    plt.plot(msg_sizes_throughput, a1_cycles_per_byte, linewidth=2.5, label="2-copy")
    plt.plot(msg_sizes_throughput, a2_cycles_per_byte, linewidth=2.5, label="1-copy")
    plt.plot(msg_sizes_throughput, a3_cycles_per_byte, linewidth=2.5, label="0-copy")
    
    plt.xscale("log", base=2)
    plt.xlabel("Message size (bytes)")
    plt.ylabel("Cycles per byte")
    plt.title("CPU Cycles per Byte vs Message Size (Threads=8)")
    plt.legend(loc="best")
    
    if sysinfo:
        plt.text(0.02, 0.98, sysinfo, transform=plt.gca().transAxes,
                fontsize=8, va='top', ha='left',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8, edgecolor='#CCCCCC'))
    
    plt.tight_layout()
    plt.savefig(os.path.join(out_dir, "MT25041_Part_D_CyclesPerByte_vs_MsgSize.png"), bbox_inches="tight")
    plt.close()


if __name__ == "__main__":
    import sys
    
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "results"
    os.makedirs(out_dir, exist_ok=True)
    
    setup_style()
    sysinfo = get_sysinfo()
    
    print("Generating plots from hardcoded data...")
    plot_throughput(sysinfo, out_dir)
    plot_latency(sysinfo, out_dir)
    plot_cache_misses(sysinfo, out_dir)
    plot_cycles_per_byte(sysinfo, out_dir)
    print(f"✓ Plots generated in: {out_dir}")
