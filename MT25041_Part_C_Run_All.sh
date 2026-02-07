#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG="$ROOT/MT25041_Part_C_Config.json"
OUT_DIR="$ROOT/results"

cleanup() {
  pkill -f MT25041_Part_A1_Server >/dev/null 2>&1 || true
  pkill -f MT25041_Part_A2_Server >/dev/null 2>&1 || true
  pkill -f MT25041_Part_A3_Server >/dev/null 2>&1 || true
}

trap 'cleanup; exit 130' INT TERM

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config) CONFIG="$2"; shift 2;;
    --out-dir) OUT_DIR="$2"; shift 2;;
    *) echo "Unknown arg: $1"; exit 1;;
  esac
done

mkdir -p "$OUT_DIR"
RAW_CSV="$OUT_DIR/MT25041_Part_B_RawData.csv"
SYSINFO="$OUT_DIR/MT25041_Part_B_SysInfo.txt"

python3 - <<'PY' "$CONFIG" "$SYSINFO"
import json, sys, subprocess
cfg = json.load(open(sys.argv[1]))
with open(sys.argv[2], "w") as f:
    f.write("uname: ")
    f.write(subprocess.getoutput("uname -srmo"))
    f.write("\n")
    f.write("cpu: ")
    f.write(subprocess.getoutput("lscpu | awk -F: '/Model name/ {print $2; exit}'"))
    f.write("\n")
    f.write("threads: %s\n" % cfg.get("thread_counts"))
    f.write("msg_sizes: %s\n" % cfg.get("message_sizes"))
    f.write("duration_sec: %s\n" % cfg.get("duration_sec"))
    f.write("echo: %s\n" % cfg.get("echo"))
    f.write("pin_base_cpu: %s\n" % cfg.get("pin_base_cpu"))
    f.write("zerocopy_inflight: %s\n" % cfg.get("zerocopy_inflight"))
    f.write("\n")
    f.write("net: ")
    f.write(subprocess.getoutput("ip -o route get 8.8.8.8 | awk '{print $5; exit}'"))
    f.write("\n")
    f.write(subprocess.getoutput("which ethtool >/dev/null 2>&1 && ethtool -k $(ip -o route get 8.8.8.8 | awk '{print $5; exit}') || echo ethtool_not_found"))
    f.write("\n")
PY

read_config() {
  python3 - <<'PY' "$CONFIG"
import json, sys
cfg = json.load(open(sys.argv[1]))
print("MSG_SIZES=" + ",".join(map(str, cfg["message_sizes"])))
print("THREADS=" + ",".join(map(str, cfg["thread_counts"])))
print("DURATION=" + str(cfg.get("duration_sec", 5)))
print("WARMUP=" + str(cfg.get("warmup_sec", 1)))
print("RETRIES=" + str(cfg.get("retries", 2)))
print("HOST=" + cfg.get("host", "127.0.0.1"))
ports = cfg.get("ports", {})
print("PORT_A1=" + str(ports.get("a1", 5001)))
print("PORT_A2=" + str(ports.get("a2", 5002)))
print("PORT_A3=" + str(ports.get("a3", 5003)))
print("ECHO=" + ("1" if cfg.get("echo", False) else "0"))
print("PIN_BASE=" + str(cfg.get("pin_base_cpu", -1)))
print("ZC_INFLIGHT=" + str(cfg.get("zerocopy_inflight", 32)))
PY
}

eval "$(read_config)"
IFS=',' read -r -a MSG_SIZES <<< "$MSG_SIZES"
IFS=',' read -r -a THREADS <<< "$THREADS"

total_runs=$((3 * ${#MSG_SIZES[@]} * ${#THREADS[@]} * 2))
done_runs=0

make -C "$ROOT" clean all

printf "impl,msg_size,threads,mode,throughput_gbps,latency_us,cycles,l1_miss,llc_miss,ctx_switches,total_bytes,duration_s\n" > "$RAW_CSV"

run_once() {
  local impl="$1"
  local port="$2"
  local msg_size="$3"
  local threads="$4"
  local mode="$5"
  local echo_flag="$6"
  local perf_out="$OUT_DIR/perf_${impl}_${msg_size}_${threads}_${mode}.txt"
  local res_out="$OUT_DIR/res_${impl}_${msg_size}_${threads}_${mode}.txt"

  "$ROOT/MT25041_Part_${impl}_Server" --port "$port" --msg-size "$msg_size" --max-clients "$threads" ${echo_flag} --pin-base "$PIN_BASE" &
  local srv_pid=$!
  sleep 0.2

  local client_bin="$ROOT/MT25041_Part_${impl}_Client"
  local client_args=(--host "$HOST" --port "$port" --msg-size "$msg_size" --threads "$threads" --duration "$DURATION" --mode "$mode" --pin-base "$PIN_BASE" --zc-inflight "$ZC_INFLIGHT")
  if [[ "$echo_flag" == "--echo" ]]; then
    client_args+=(--echo)
  fi

  perf stat -x, -e cycles,context-switches,L1-dcache-load-misses,cache-misses -- "$client_bin" "${client_args[@]}" 1>"$res_out" 2>"$perf_out"

  wait "$srv_pid" || true

  python3 - <<'PY' "$impl" "$msg_size" "$threads" "$mode" "$perf_out" "$res_out" "$RAW_CSV"
import sys, csv
impl, msg_size, threads, mode, perf_out, res_out, raw_csv = sys.argv[1:]

metrics = {"cycles": 0, "context-switches": 0, "L1-dcache-load-misses": 0, "cache-misses": 0}
with open(perf_out) as f:
    for line in f:
        parts = [p.strip() for p in line.split(',')]
        if len(parts) < 3:
            continue
        val, _, name = parts[0], parts[1], parts[2]
        if name in metrics:
            try:
                metrics[name] = int(float(val))
            except ValueError:
                pass

result_line = None
with open(res_out) as f:
    for line in f:
        if line.startswith("RESULT,"):
            result_line = line.strip()
            break

if not result_line:
    sys.exit(2)

_, thr, lat, total_bytes, duration_s = result_line.split(',')
thr = float(thr)
lat = float(lat)
if mode == "latency" and lat <= 0:
    sys.exit(3)
if mode == "throughput" and thr <= 0:
    sys.exit(4)

row = [impl, msg_size, threads, mode, f"{thr:.6f}", f"{lat:.3f}",
       str(metrics["cycles"]), str(metrics["L1-dcache-load-misses"]), str(metrics["cache-misses"]),
       str(metrics["context-switches"]), total_bytes, f"{float(duration_s):.6f}"]

with open(raw_csv, "a", newline="") as f:
    csv.writer(f).writerow(row)

summary = (
  "\n"
  "================ Experiment Summary ================\n"
  f"Impl: {impl}\n"
  f"Message size: {msg_size} bytes\n"
  f"Threads: {threads}\n"
  f"Mode: {mode}\n"
  "---------------- Results ----------------\n"
  f"Throughput: {thr:.6f} Gbps\n"
  f"Latency: {lat:.3f} us\n"
  f"CPU cycles: {metrics['cycles']}\n"
  f"L1 misses: {metrics['L1-dcache-load-misses']}\n"
  f"LLC misses: {metrics['cache-misses']}\n"
  f"Context switches: {metrics['context-switches']}\n"
  f"Total bytes: {total_bytes}\n"
  f"Duration: {float(duration_s):.6f} s\n"
  "====================================================\n"
)
print(summary)
PY
}

warmup_run() {
  local impl="$1"
  local port="$2"
  local msg_size="$3"
  local threads="$4"

  "$ROOT/MT25041_Part_${impl}_Server" --port "$port" --msg-size "$msg_size" --max-clients "$threads" --pin-base "$PIN_BASE" &
  local srv_pid=$!
  sleep 0.2

  local client_bin="$ROOT/MT25041_Part_${impl}_Client"
  "$client_bin" --host "$HOST" --port "$port" --msg-size "$msg_size" --threads "$threads" --duration "$WARMUP" --mode throughput --pin-base "$PIN_BASE" --zc-inflight "$ZC_INFLIGHT" >/dev/null 2>&1 || true
  wait "$srv_pid" || true
}

for impl in A1 A2 A3; do
  case "$impl" in
    A1) port="$PORT_A1";;
    A2) port="$PORT_A2";;
    A3) port="$PORT_A3";;
  esac

  for msg_size in "${MSG_SIZES[@]}"; do
    for threads in "${THREADS[@]}"; do
      warmup_run "$impl" "$port" "$msg_size" "$threads"

      for mode in throughput latency; do
        echo_flag=""
        if [[ "$mode" == "latency" || "$ECHO" == "1" ]]; then
          echo_flag="--echo"
        fi
        done_runs=$((done_runs + 1))
        echo "Progress: ${done_runs}/${total_runs} | ${impl} size=${msg_size} threads=${threads} mode=${mode}"
        attempt=0
        until run_once "$impl" "$port" "$msg_size" "$threads" "$mode" "$echo_flag"; do
          attempt=$((attempt + 1))
          if [[ "$attempt" -gt "$RETRIES" ]]; then
            echo "Failed: $impl size=$msg_size threads=$threads mode=$mode" >&2
            exit 1
          fi
          sleep 0.2
        done
      done
    done
  done
done

echo ""
echo "==============================================="
echo "All experiments completed successfully!"
echo "==============================================="
echo ""
echo "Generating plots from collected data..."

if python3 "$ROOT/MT25041_Part_D_Plots_Hardcoded.py" --csv "$RAW_CSV" --sysinfo "$SYSINFO" --out-dir "$OUT_DIR"; then
  echo ""
  echo "✓ Plots generated in: $OUT_DIR"
  echo "  - MT25041_Part_D_Throughput_vs_MsgSize.png"
  echo "  - MT25041_Part_D_Latency_vs_Threads.png"
  echo "  - MT25041_Part_D_CacheMisses_vs_MsgSize.png"
  echo "  - MT25041_Part_D_CyclesPerByte_vs_MsgSize.png"
  echo ""
else
  echo ""
  echo "⚠ Warning: Plot generation failed. Check Python dependencies (matplotlib)."
  echo ""
fi

cp "$RAW_CSV" "$ROOT/MT25041_Part_B_RawData.csv"

echo "✓ Raw data copied to: MT25041_Part_B_RawData.csv"
echo ""
echo "All done!"
