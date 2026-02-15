#!/usr/bin/env python3
"""
Visualize benchmark results for MPSC queue performance.
Shows the effect of queue size and producer count on throughput.
All measurements are taken within 1 second, so successfulPops represents throughput (MOps/sec).
"""

import argparse
import csv
import json
import re
import plotly.graph_objects as go  # type: ignore[import-not-found]
import plotly.express as px  # type: ignore[import-not-found]
import plotly.io as pio  # type: ignore[import-not-found]
from html import escape as html_escape
from typing import Any, List
from pathlib import Path
from collections import defaultdict
import logging

# Glob pattern for per-queue-size result files produced by run_benchmarks.sh
BENCHMARK_RESULTS_GLOB = 'benchmark_results_q*.json'
# Tracy CSV export files (tracy-csvexport output)
TRACY_CSV_GLOB = 'tracy_q*.csv'
# Perf stat output files (perf_q<queue_size>_p<producer_count>.txt)
PERF_TXT_GLOB = 'perf_q*.txt'

# Perf metric names (we match event names containing these substrings)
PERF_METRICS = {
    'dTLB-load-misses': 'dTLB-load-misses',
    'iTLB-load-misses': 'iTLB-load-misses',
}


def load_benchmark_json(json_path: Path) -> dict[str, Any]:
    """Load the full benchmark JSON payload."""
    with open(json_path, 'r') as f:
        return json.load(f)


def load_benchmark_data(json_path: Path) -> list[dict[str, Any]]:
    """Load benchmark results list from JSON file."""
    data = load_benchmark_json(json_path)
    return data['benchmarkResults']


def _benchmark_sort_key(path: Path) -> tuple[int, int]:
    """(queue_size, producer_count) for benchmark_results_q<N>_p<P>.json or benchmark_results_q<N>.json."""
    m = re.search(r'benchmark_results_q(\d+)(?:_p(\d+))?\.json', path.name)
    if not m:
        return 0, 0
    q, p = int(m.group(1)), int(m.group(2)) if m.group(2) else 0
    return q, p


def _queue_size_from_filename(path: Path) -> int:
    """Extract queue size from benchmark_results_q<N>.json or benchmark_results_q<N>_p<P>.json."""
    match = re.search(r'benchmark_results_q(\d+)(?:_p\d+)?\.json', path.name)
    return int(match.group(1)) if match else 0


def _tracy_sort_key(path: Path) -> tuple[int, int]:
    """(queue_size, producer_count) for tracy_q<N>_p<P>.csv or tracy_q<N>.csv."""
    m = re.search(r'tracy_q(\d+)(?:_p(\d+))?\.csv', path.name)
    if not m:
        return 0, 0
    q, p = int(m.group(1)), int(m.group(2)) if m.group(2) else 0
    return q, p


def _queue_size_from_tracy_filename(path: Path) -> int:
    """Extract queue size from tracy_q<N>.csv or tracy_q<N>_p<P>.csv."""
    match = re.search(r'tracy_q(\d+)(?:_p\d+)?\.csv', path.name)
    return int(match.group(1)) if match else 0


def _producer_count_from_tracy_filename(path: Path) -> int:
    """Extract producer count from tracy_q<N>_p<P>.csv (0 if no _p)."""
    match = re.search(r'tracy_q\d+_p(\d+)\.csv', path.name)
    return int(match.group(1)) if match else 0


def load_all_benchmark_results(results_dir: Path) -> tuple[list[dict[str, Any]], Any]:
    """
    Load all benchmark_results_q<size>.json files from results_dir and merge them.
    Returns (combined_benchmark_results, cpu_info from first file).
    """
    paths = sorted(results_dir.glob(BENCHMARK_RESULTS_GLOB), key=_benchmark_sort_key)
    if not paths:
        return [], None

    combined: list[dict[str, Any]] = []
    cpu_info: Any = None

    for p in paths:
        payload = load_benchmark_json(p)
        results = payload.get('benchmarkResults', [])
        if isinstance(results, dict):
            results = [results]
        combined.extend(results)
        if cpu_info is None and payload.get('cpuInfo') is not None:
            cpu_info = payload['cpuInfo']
        logging.info("Loaded %s: %d results", p.name, len(results))

    return combined, cpu_info


def load_all_tracy_csv(results_dir: Path) -> list[dict[str, Any]]:
    """
    Load all tracy_q<size>.csv files from results_dir.
    Returns a flat list of rows with keys: queue_size, name, src_file, src_line,
    total_ns, total_perc, counts, mean_ns, min_ns, max_ns, std_ns (numeric where applicable).
    """
    paths = sorted(results_dir.glob(TRACY_CSV_GLOB), key=_tracy_sort_key)
    rows: list[dict[str, Any]] = []
    for p in paths:
        queue_size = _queue_size_from_tracy_filename(p)
        producer_count = _producer_count_from_tracy_filename(p)
        n = 0
        with open(p, newline='', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                n += 1
                r: dict[str, Any] = {'queue_size': queue_size, 'producer_count': producer_count}
                for key in ('name', 'src_file', 'src_line', 'total_ns', 'total_perc',
                            'counts', 'mean_ns', 'min_ns', 'max_ns', 'std_ns'):
                    val = row.get(key, '')
                    if key in ('src_line', 'counts'):
                        try:
                            r[key] = int(val) if val else 0
                        except ValueError:
                            r[key] = val
                    elif key in ('total_ns', 'total_perc', 'mean_ns', 'min_ns', 'max_ns', 'std_ns'):
                        try:
                            r[key] = float(val) if val else 0.0
                        except ValueError:
                            r[key] = val
                    else:
                        r[key] = val
                rows.append(r)
        logging.info("Loaded Tracy CSV %s: %d zones", p.name, n)
    return rows


def _perf_sort_key(path: Path) -> tuple[int, int]:
    """(queue_size, producer_count) for perf_q<N>_p<P>.txt."""
    m = re.search(r'perf_q(\d+)_p(\d+)\.txt', path.name)
    if not m:
        return 0, 0
    return int(m.group(1)), int(m.group(2))


def _parse_perf_counter_line(line: str) -> tuple[str, int] | None:
    """
    Parse a perf stat counter line like '        58180      dTLB-load-misses:u'.
    Returns (event_name, value) or None if not parseable (e.g. '<not supported>').
    """
    # Match: optional whitespace, number, whitespace, event name (no <not supported>)
    m = re.match(r'^\s*(\d+)\s+(\S+)\s*$', line.strip())
    if not m:
        return None
    val_str, event = m.group(1), m.group(2)
    return event.strip(), int(val_str)


def _event_matches_metric(event_name: str, metric_key: str) -> bool:
    """Check if perf event name matches our metric (handles suffixes like :u)."""
    base_event = event_name.split(':')[0] if ':' in event_name else event_name
    expected = PERF_METRICS.get(metric_key, metric_key)
    return base_event == expected


def load_all_perf_results(results_dir: Path) -> list[dict[str, Any]]:
    """
    Load all perf_q<N>_p<P>.txt files from results_dir.
    Queue size and producer count are read from the corresponding benchmark_results_q<N>_p<P>.json.
    Returns list of dicts with queue_size, producer_count, and metric values.
    """
    paths = sorted(results_dir.glob(PERF_TXT_GLOB), key=_perf_sort_key)
    rows: list[dict[str, Any]] = []
    for p in paths:
        m = re.search(r'perf_q(\d+)_p(\d+)\.txt', p.name)
        if not m:
            continue
        q_param, p_param = m.group(1), m.group(2)
        json_path = results_dir / f'benchmark_results_q{q_param}_p{p_param}.json'
        queue_size = int(q_param)
        producer_count = int(p_param)
        if json_path.exists():
            try:
                payload = load_benchmark_json(json_path)
                br = payload.get('benchmarkResults')
                if isinstance(br, dict):
                    queue_size = br.get('queueSize', queue_size)
                    producer_count = br.get('producerCount', producer_count)
                elif isinstance(br, list) and br:
                    queue_size = br[0].get('queueSize', queue_size)
                    producer_count = br[0].get('producerCount', producer_count)
            except (json.JSONDecodeError, KeyError) as e:
                logging.warning("Could not read queue size from %s: %s", json_path.name, e)
        metrics: dict[str, int] = {k: 0 for k in PERF_METRICS}
        with open(p, 'r', encoding='utf-8') as f:
            for line in f:
                parsed = _parse_perf_counter_line(line)
                if not parsed:
                    continue
                event_name, value = parsed
                for key in PERF_METRICS:
                    if _event_matches_metric(event_name, key):
                        metrics[key] = value
                        break
        rows.append({
            'queue_size': queue_size,
            'producer_count': producer_count,
            **metrics,
        })
    logging.info("Loaded %d perf result files", len(rows))
    return rows


def organize_data(results):
    """Organize data by queue size and producer count."""
    by_queue_size = defaultdict(list)
    by_producer_count = defaultdict(list)
    
    for result in results:
        queue_size = result['queueSize']
        producer_count = result['producerCount']
        # All measurements are taken within 1 second, so successfulPops is ops/sec.
        # Convert to MOps/sec for nicer plots / consistency.
        throughput = result['successfulPops'] / 0.5e6 # 0.5 seconds is the duration of the benchmark
        
        by_queue_size[queue_size].append({
            'producerCount': producer_count,
            'throughput': throughput
        })
        
        by_producer_count[producer_count].append({
            'queueSize': queue_size,
            'throughput': throughput
        })
    
    # Sort and organize
    for queue_size in by_queue_size:
        by_queue_size[queue_size].sort(key=lambda x: x['producerCount'])
    
    for producer_count in by_producer_count:
        by_producer_count[producer_count].sort(key=lambda x: x['queueSize'])
    
    return by_queue_size, by_producer_count


def _format_bytes(n: Any) -> str:
    try:
        value = int(n)
    except Exception:
        return str(n)
    if value < 1024:
        return f"{value} B"
    if value < 1024 * 1024:
        return f"{value / 1024:.1f} KiB"
    return f"{value / (1024 * 1024):.1f} MiB"


def _format_cache(cache: Any) -> str:
    if not isinstance(cache, dict):
        return str(cache)
    size = _format_bytes(cache.get("size"))
    line_size = cache.get("line_size")
    assoc = cache.get("associativity")
    extras: list[str] = []
    if line_size is not None:
        extras.append(f"line {line_size} B")
    if assoc is not None:
        extras.append(f"assoc {assoc}")
    extra_str = f" ({', '.join(extras)})" if extras else ""
    return f"{size}{extra_str}"


def _queue_size_kb(queue_size: Any) -> float:
    # Use decimal kilobytes for queue size labels.
    return float(queue_size) / 1024.0


def _queue_size_kb_label(queue_size: Any) -> str:
    kb = _queue_size_kb(queue_size)
    if kb.is_integer():
        return str(int(kb))
    return f"{kb:.2f}"


def _render_cpuinfo_html(cpu_info: Any) -> str:
    if not isinstance(cpu_info, dict) or not cpu_info:
        return ""

    vendor = cpu_info.get("vendor")
    uarch = cpu_info.get("uarch")
    cores = cpu_info.get("coresPerSocket")
    page_size = cpu_info.get("pageSize")

    rows: list[tuple[str, str]] = []
    if vendor is not None:
        rows.append(("Vendor", str(vendor)))
    if uarch is not None:
        rows.append(("Microarchitecture", str(uarch)))
    if cores is not None:
        rows.append(("Cores per socket", str(cores)))
    if page_size is not None:
        rows.append(("Page size", _format_bytes(page_size)))

    # Cache details (if present)
    if "l1iCache" in cpu_info:
        rows.append(("L1I cache", _format_cache(cpu_info.get("l1iCache"))))
    if "l1dCache" in cpu_info:
        rows.append(("L1D cache", _format_cache(cpu_info.get("l1dCache"))))
    if "l2Cache" in cpu_info:
        rows.append(("L2 cache", _format_cache(cpu_info.get("l2Cache"))))
    if "l3Cache" in cpu_info:
        rows.append(("L3 cache", _format_cache(cpu_info.get("l3Cache"))))

    table_rows = "\n".join(
        f"<tr><th>{html_escape(k)}</th><td>{html_escape(v)}</td></tr>" for k, v in rows
    )
    raw_json = html_escape(json.dumps(cpu_info, indent=2, sort_keys=True))

    return f"""
      <section class="cpuinfo">
        <h2>CPU Info</h2>
        <table class="kv">
          <tbody>
            {table_rows}
          </tbody>
        </table>
        <details>
          <summary>Raw cpuInfo JSON</summary>
          <pre class="code">{raw_json}</pre>
        </details>
      </section>
    """


def write_html_report(figures: List[go.Figure], output_path: Path, cpu_info: Any = None) -> None:
    """Write a single HTML report containing multiple Plotly figures."""
    parts: List[str] = []
    for i, fig in enumerate(figures):
        parts.append(
            pio.to_html(
                fig,
                include_plotlyjs=("cdn" if i == 0 else False),  # type: ignore[arg-type]
                full_html=False,
            )
        )

    figures_html = "\n".join(f'<div class="figure">{p}</div>' for p in parts)
    cpu_html = _render_cpuinfo_html(cpu_info)

    html = f"""<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Benchmark Visualizations</title>
    <style>
      body {{ font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif; margin: 20px; }}
      .container {{ max-width: 1200px; margin: 0 auto; }}
      .figures {{ display: flex; flex-direction: column; gap: 48px; }}
      .figure {{ margin: 0; }}
      h2 {{ margin: 1.25rem 0 0.75rem; }}
      .cpuinfo {{ margin: 1rem 0 1.75rem; padding: 16px; border: 1px solid #e6e6e6; border-radius: 10px; background: #fafafa; }}
      table.kv {{ border-collapse: collapse; width: 100%; }}
      table.kv th {{ text-align: left; font-weight: 600; color: #333; padding: 8px 10px; width: 220px; vertical-align: top; }}
      table.kv td {{ padding: 8px 10px; color: #111; }}
      table.kv tr + tr th, table.kv tr + tr td {{ border-top: 1px solid #ededed; }}
      details {{ margin-top: 12px; }}
      details summary {{ cursor: pointer; color: #333; }}
      pre.code {{ margin: 10px 0 0; padding: 12px; background: #fff; border: 1px solid #eee; border-radius: 8px; overflow: auto; }}
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Benchmark Visualizations</h1>
      {cpu_html}
      <div class="figures">
        {figures_html}
      </div>
    </div>
  </body>
</html>
"""

    output_path.write_text(html, encoding="utf-8")
    logging.info(f"Saved: {output_path}")


def plot_queue_size_effect(by_queue_size) -> go.Figure:
    """Plot throughput vs queue size for different producer counts."""
    fig = go.Figure()

    # Get unique producer counts and sort them
    all_producer_counts = set()
    for data_list in by_queue_size.values():
        for entry in data_list:
            all_producer_counts.add(entry['producerCount'])
    producer_counts = sorted(all_producer_counts)
    
    # Plot a line for each producer count
    colors = px.colors.sequential.Viridis
    
    for i, producer_count in enumerate(producer_counts):
        queue_sizes = []
        throughputs = []
        
        for queue_size in sorted(by_queue_size.keys()):
            # Find matching entry
            for entry in by_queue_size[queue_size]:
                if entry['producerCount'] == producer_count:
                    queue_sizes.append(_queue_size_kb(queue_size))
                    throughputs.append(entry['throughput'])
                    break
        
        if queue_sizes:
            fig.add_trace(
                go.Scatter(
                    x=queue_sizes,
                    y=throughputs,
                    mode="lines+markers",
                    name=f"{producer_count} producer(s)",
                    line={"width": 2, "color": colors[i % len(colors)]},
                    marker={"size": 9, "symbol": "circle"},
                )
            )

    tick_vals = [_queue_size_kb(qs) for qs in sorted(by_queue_size.keys())]
    tick_text = [_queue_size_kb_label(qs) for qs in sorted(by_queue_size.keys())]

    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=750,
        title="Effect of Queue Size on Throughput<br>(by Producer Count)",
        legend_title_text="Producer Count",
        margin=dict(l=80, r=40, t=90, b=120),
    )
    fig.update_xaxes(
        title_text="Queue Size (kB)",
        type="log",
        tickmode="array",
        tickvals=tick_vals,
        ticktext=tick_text,
        tickangle=-45,
    )
    fig.update_yaxes(title_text="Throughput (MOps/sec)", rangemode="tozero")

    return fig


def plot_producer_count_effect(by_producer_count) -> go.Figure:
    """Plot throughput vs producer count for different queue sizes."""
    fig = go.Figure()

    # Get unique queue sizes and sort them
    all_queue_sizes = set()
    for data_list in by_producer_count.values():
        for entry in data_list:
            all_queue_sizes.add(entry['queueSize'])
    queue_sizes = sorted(all_queue_sizes)
    
    # Plot a line for each queue size
    colors = px.colors.sequential.Plasma
    
    for i, queue_size in enumerate(queue_sizes):
        producer_counts = []
        throughputs = []
        
        for producer_count in sorted(by_producer_count.keys()):
            # Find matching entry
            for entry in by_producer_count[producer_count]:
                if entry['queueSize'] == queue_size:
                    producer_counts.append(producer_count)
                    throughputs.append(entry['throughput'])
                    break
        
        if producer_counts:
            fig.add_trace(
                go.Scatter(
                    x=producer_counts,
                    y=throughputs,
                    mode="lines+markers",
                    name=f"Queue size: {_queue_size_kb_label(queue_size)} kB",
                    line={"width": 2, "color": colors[i % len(colors)]},
                    marker={"size": 9, "symbol": "square"},
                )
            )

    tick_vals = sorted(by_producer_count.keys())
    tick_text = [str(pc) for pc in tick_vals]

    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=750,
        title="Effect of Producer Count on Throughput<br>(by Queue Size)",
        legend_title_text="Queue Size",
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="left", x=0),
        margin=dict(l=80, r=40, t=200, b=80),
    )
    fig.update_xaxes(
        title_text="Producer Count",
        type="log",
        tickmode="array",
        tickvals=tick_vals,
        ticktext=tick_text,
    )
    fig.update_yaxes(title_text="Throughput (MOps/sec)", rangemode="tozero")

    return fig


def plot_heatmap(results) -> go.Figure:
    """Create a heatmap showing throughput for all combinations."""
    # Organize data into a 2D grid
    queue_sizes = sorted(set(r['queueSize'] for r in results))
    producer_counts = sorted(set(r['producerCount'] for r in results))
    
    # Create matrix (rows: producerCounts, cols: queueSizes)
    throughput_matrix = [[0.0 for _ in queue_sizes] for _ in producer_counts]
    qs_idx_map = {qs: i for i, qs in enumerate(queue_sizes)}
    pc_idx_map = {pc: i for i, pc in enumerate(producer_counts)}

    for result in results:
        qs_idx = qs_idx_map[result['queueSize']]
        pc_idx = pc_idx_map[result['producerCount']]
        throughput_matrix[pc_idx][qs_idx] = result['successfulPops'] / 0.5e6

    text = [[f"{v:.4f}" for v in row] for row in throughput_matrix]

    fig = go.Figure(
        data=go.Heatmap(
            z=throughput_matrix,
            x=[_queue_size_kb_label(qs) for qs in queue_sizes],
            y=[str(pc) for pc in producer_counts],
            colorscale="YlOrRd",
            colorbar={"title": "Throughput (MOps/sec)"},
            text=text,
            texttemplate="%{text}",
            textfont={"size": 10, "color": "black"},
            hovertemplate="Producer Count=%{y}<br>Queue Size=%{x} kB<br>Throughput=%{z:.4f} MOps/sec<extra></extra>",
        )
    )

    fig.update_layout(
        template="plotly_white",
        width=1200,
        height=850,
        title="Throughput Heatmap<br>(MOps/sec measured in 1 second)",
        margin=dict(l=90, r=40, t=90, b=80),
    )
    fig.update_xaxes(title_text="Queue Size (kB)")
    fig.update_yaxes(title_text="Producer Count")

    return fig


def _tracy_series_key(r: dict[str, Any]) -> tuple[str, int]:
    """(zone name, producer_count) for grouping Tracy traces per producer."""
    name = r.get('name') or 'unknown'
    pc = r.get('producer_count', 0)
    return name, pc


def _tracy_legend_label(name: str, producer_count: int) -> str:
    """Label for legend: zone (P producers)."""
    if producer_count <= 0:
        return name
    return f"{name} ({producer_count} producer{'s' if producer_count != 1 else ''})"


def plot_tracy_push_mean_ns(tracy_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot Push mean latency (mean_ns) vs queue size, one line per producer count (Push zone only)."""
    push_rows = [r for r in tracy_rows if 'Push' in (r.get('name') or '')]
    fig = go.Figure()
    by_series: dict[tuple[str, int], list[tuple[int, float]]] = defaultdict(list)
    for r in push_rows:
        key = _tracy_series_key(r)
        qs = r.get('queue_size', 0)
        by_series[key].append((qs, r.get('mean_ns', 0)))
    for (name, pc), pts in sorted(by_series.items(), key=lambda x: (x[0][0], x[0][1])):
        pts = sorted(pts, key=lambda x: x[0])
        queue_sizes = [x[0] for x in pts]
        mean_ns = [x[1] for x in pts]
        label = _tracy_legend_label(name, pc)
        fig.add_trace(
            go.Scatter(
                x=queue_sizes,
                y=mean_ns,
                mode="lines+markers",
                name=label,
                line={"width": 2},
                marker={"size": 8},
            )
        )
    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=600,
        title="Tracy: Push mean latency (ns) by producer count vs queue size",
        legend_title_text="Producers",
        margin=dict(l=80, r=40, t=80, b=80),
    )
    fig.update_xaxes(title_text="Queue size", type="log")
    fig.update_yaxes(title_text="Mean latency (ns)", rangemode="tozero")
    return fig


def plot_tracy_pop_mean_ns(tracy_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot Pop mean latency (mean_ns) vs queue size, one line per producer count (Pop zone only)."""
    pop_rows = [r for r in tracy_rows if 'Pop' in (r.get('name') or '')]
    fig = go.Figure()
    by_series: dict[tuple[str, int], list[tuple[int, float]]] = defaultdict(list)
    for r in pop_rows:
        key = _tracy_series_key(r)
        qs = r.get('queue_size', 0)
        by_series[key].append((qs, r.get('mean_ns', 0)))
    for (name, pc), pts in sorted(by_series.items(), key=lambda x: (x[0][0], x[0][1])):
        pts = sorted(pts, key=lambda x: x[0])
        queue_sizes = [x[0] for x in pts]
        mean_ns = [x[1] for x in pts]
        label = _tracy_legend_label(name, pc)
        fig.add_trace(
            go.Scatter(
                x=queue_sizes,
                y=mean_ns,
                mode="lines+markers",
                name=label,
                line={"width": 2},
                marker={"size": 8},
            )
        )
    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=600,
        title="Tracy: Pop mean latency (ns) by producer count vs queue size",
        legend_title_text="Producers",
        margin=dict(l=80, r=40, t=80, b=80),
    )
    fig.update_xaxes(title_text="Queue size", type="log")
    fig.update_yaxes(title_text="Mean latency (ns)", rangemode="tozero")
    return fig


def plot_tracy_push_counts(tracy_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot Push counts vs queue size, one line per producer count (Push zone only)."""
    push_rows = [r for r in tracy_rows if 'Push' in (r.get('name') or '')]
    fig = go.Figure()
    by_series: dict[tuple[str, int], list[tuple[int, float]]] = defaultdict(list)
    for r in push_rows:
        key = _tracy_series_key(r)
        qs = r.get('queue_size', 0)
        by_series[key].append((qs, r.get('counts', 0)))
    for (name, pc), pts in sorted(by_series.items(), key=lambda x: (x[0][0], x[0][1])):
        pts = sorted(pts, key=lambda x: x[0])
        queue_sizes = [x[0] for x in pts]
        counts = [x[1] for x in pts]
        label = _tracy_legend_label(name, pc)
        fig.add_trace(
            go.Scatter(
                x=queue_sizes,
                y=counts,
                mode="lines+markers",
                name=label,
                line={"width": 2},
                marker={"size": 8},
            )
        )
    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=600,
        title="Tracy: Push counts by producer count vs queue size",
        legend_title_text="Producers",
        margin=dict(l=80, r=40, t=80, b=80),
    )
    fig.update_xaxes(title_text="Queue size", type="log")
    fig.update_yaxes(title_text="Count", rangemode="tozero")
    return fig


def plot_tracy_pop_counts(tracy_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot Pop counts vs queue size, one line per producer count (Pop zone only)."""
    pop_rows = [r for r in tracy_rows if 'Pop' in (r.get('name') or '')]
    fig = go.Figure()
    by_series: dict[tuple[str, int], list[tuple[int, float]]] = defaultdict(list)
    for r in pop_rows:
        key = _tracy_series_key(r)
        qs = r.get('queue_size', 0)
        by_series[key].append((qs, r.get('counts', 0)))
    for (name, pc), pts in sorted(by_series.items(), key=lambda x: (x[0][0], x[0][1])):
        pts = sorted(pts, key=lambda x: x[0])
        queue_sizes = [x[0] for x in pts]
        counts = [x[1] for x in pts]
        label = _tracy_legend_label(name, pc)
        fig.add_trace(
            go.Scatter(
                x=queue_sizes,
                y=counts,
                mode="lines+markers",
                name=label,
                line={"width": 2},
                marker={"size": 8},
            )
        )
    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=600,
        title="Tracy: Pop counts by producer count vs queue size",
        legend_title_text="Producers",
        margin=dict(l=80, r=40, t=80, b=80),
    )
    fig.update_xaxes(title_text="Queue size", type="log")
    fig.update_yaxes(title_text="Count", rangemode="tozero")
    return fig


def _plot_perf_metric_multiline(perf_rows: list[dict[str, Any]], metric_key: str, title: str) -> go.Figure:
    """Create a multiline plot for a single perf metric (queue_size x-axis, one line per producer count)."""
    if not perf_rows:
        fig = go.Figure()
        fig.add_annotation(text="No perf data available", xref="paper", yref="paper", x=0.5, y=0.5, showarrow=False)
        fig.update_layout(template="plotly_white", width=1100, height=600, title=title)
        return fig

    # Organize by producer count: {producer_count: [(queue_size, value), ...]}
    by_producer: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for r in perf_rows:
        val = r.get(metric_key, 0)
        by_producer[r['producer_count']].append((r['queue_size'], val))

    has_data = any(v > 0 for pts in by_producer.values() for _, v in pts)
    if not has_data and metric_key in ('l2_dtlb_misses', 'l2_itlb_misses'):
        fig = go.Figure()
        fig.add_annotation(
            text=f"No {metric_key} data (counter may not be supported on this CPU)",
            xref="paper", yref="paper", x=0.5, y=0.5, showarrow=False
        )
        fig.update_layout(template="plotly_white", width=1100, height=600, title=title)
        return fig

    producer_counts = sorted(by_producer.keys())
    queue_sizes = sorted(set(r['queue_size'] for r in perf_rows))
    tick_vals = [_queue_size_kb(qs) for qs in queue_sizes]
    tick_text = [_queue_size_kb_label(qs) for qs in queue_sizes]
    colors = px.colors.sequential.Viridis

    fig = go.Figure()
    for i, producer_count in enumerate(producer_counts):
        pts = sorted(by_producer[producer_count], key=lambda x: x[0])
        queue_sizes_pts = [_queue_size_kb(qs) for qs, _ in pts]
        values = [v for _, v in pts]
        fig.add_trace(
            go.Scatter(
                x=queue_sizes_pts,
                y=values,
                mode="lines+markers",
                name=f"{producer_count} producer(s)",
                line={"width": 2, "color": colors[i % len(colors)]},
                marker={"size": 9, "symbol": "circle"},
            )
        )

    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=600,
        title=title,
        legend_title_text="Producer Count",
        margin=dict(l=80, r=40, t=90, b=120),
    )
    fig.update_xaxes(
        title_text="Queue Size (kB)",
        type="log",
        tickmode="array",
        tickvals=tick_vals,
        ticktext=tick_text,
        tickangle=-45,
    )
    fig.update_yaxes(title_text=title.split("<br>")[0], rangemode="tozero")
    return fig


def plot_perf_dtlb_load_misses(perf_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot dTLB-load-misses vs queue size, one line per producer count."""
    return _plot_perf_metric_multiline(
        perf_rows, 'dTLB-load-misses',
        "Perf: dTLB-load-misses<br>(Data TLB load misses)",
    )


def plot_perf_itlb_load_misses(perf_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot iTLB-load-misses vs queue size, one line per producer count."""
    return _plot_perf_metric_multiline(
        perf_rows, 'iTLB-load-misses',
        "Perf: iTLB-load-misses<br>(Instruction TLB load misses)",
    )


def plot_perf_l2_dtlb_misses(perf_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot L2 DTLB misses vs queue size, one line per producer count."""
    return _plot_perf_metric_multiline(
        perf_rows, 'l2_dtlb_misses',
        "Perf: L2 DTLB misses<br>(L2 data TLB misses)",
    )


def plot_perf_l2_itlb_misses(perf_rows: list[dict[str, Any]]) -> go.Figure:
    """Plot L2 iTLB misses vs queue size, one line per producer count."""
    return _plot_perf_metric_multiline(
        perf_rows, 'l2_itlb_misses',
        "Perf: L2 iTLB misses<br>(L2 instruction TLB misses)",
    )


def main():
    logging.basicConfig(level=logging.INFO)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    default_results_dir = project_root / 'results'
    default_output = project_root / 'results' / 'benchmark_visualizations.html'

    parser = argparse.ArgumentParser(
        description='Visualize MPSC queue benchmark results. '
        'By default loads all benchmark_results_q<size>.json from the results directory.'
    )
    parser.add_argument(
        '-i', '--input',
        type=Path,
        default=None,
        help='Input: either a directory (load all benchmark_results_q*.json) or a single JSON file'
    )
    parser.add_argument('-o', '--output', type=Path, default=default_output,
                        help=f'Output HTML path (default: {default_output})')
    args = parser.parse_args()

    input_path = args.input if args.input is not None else default_results_dir
    output_path = args.output

    if not input_path.exists():
        logging.error("Error: %s not found!", input_path)
        return 1

    if input_path.is_dir():
        logging.info("Loading all benchmark results from %s (%s)...", input_path, BENCHMARK_RESULTS_GLOB)
        results, cpu_info = load_all_benchmark_results(input_path)
        if not results:
            logging.error("Error: no %s files found in %s", BENCHMARK_RESULTS_GLOB, input_path)
            return 1
        logging.info("Total: %d benchmark results from all queue sizes", len(results))
    else:
        logging.info("Loading benchmark data from %s...", input_path)
        payload = load_benchmark_json(input_path)
        results = payload["benchmarkResults"]
        cpu_info = payload.get("cpuInfo")
        logging.info("Loaded %d benchmark results", len(results))

    # Organize data
    by_queue_size, by_producer_count = organize_data(results)

    # Create benchmark visualizations
    logging.info("\nGenerating benchmark visualizations...")
    figures: List[go.Figure] = [
        plot_queue_size_effect(by_queue_size),
        plot_producer_count_effect(by_producer_count),
        plot_heatmap(results),
    ]

    # Load Tracy CSVs from same directory (when input is a dir) and add Tracy figures
    if input_path.is_dir():
        tracy_paths = list(input_path.glob(TRACY_CSV_GLOB))
        if tracy_paths:
            logging.info("Loading Tracy CSV results from %s (%s)...", input_path, TRACY_CSV_GLOB)
            tracy_rows = load_all_tracy_csv(input_path)
            if tracy_rows:
                logging.info("Generating Tracy visualizations (%d rows)...", len(tracy_rows))
                figures.append(plot_tracy_push_mean_ns(tracy_rows))
                figures.append(plot_tracy_pop_mean_ns(tracy_rows))
                figures.append(plot_tracy_push_counts(tracy_rows))
                figures.append(plot_tracy_pop_counts(tracy_rows))

        # Load perf stat results and add TLB miss visualizations
        perf_paths = list(input_path.glob(PERF_TXT_GLOB))
        if perf_paths:
            logging.info("Loading perf results from %s (%s)...", input_path, PERF_TXT_GLOB)
            perf_rows = load_all_perf_results(input_path)
            if perf_rows:
                logging.info("Generating perf TLB visualizations (%d rows)...", len(perf_rows))
                figures.append(plot_perf_dtlb_load_misses(perf_rows))
                figures.append(plot_perf_itlb_load_misses(perf_rows))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    write_html_report(figures, output_path, cpu_info=cpu_info)
    logging.info("\nAll visualizations saved to %s", output_path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
