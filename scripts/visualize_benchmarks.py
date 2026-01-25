#!/usr/bin/env python3
"""
Visualize benchmark results for MPSC queue performance.
Shows the effect of queue size and producer count on throughput.
All measurements are taken within 1 second, so successfulPops represents throughput (MOps/sec).
"""

import json
import plotly.graph_objects as go  # type: ignore[import-not-found]
import plotly.express as px  # type: ignore[import-not-found]
import plotly.io as pio  # type: ignore[import-not-found]
from html import escape as html_escape
from typing import Any, List
from pathlib import Path
from collections import defaultdict
import logging


def load_benchmark_json(json_path: Path) -> dict[str, Any]:
    """Load the full benchmark JSON payload."""
    with open(json_path, 'r') as f:
        return json.load(f)


def load_benchmark_data(json_path: Path) -> list[dict[str, Any]]:
    """Load benchmark results list from JSON file."""
    data = load_benchmark_json(json_path)
    return data['benchmarkResults']


def organize_data(results):
    """Organize data by queue size and producer count."""
    by_queue_size = defaultdict(list)
    by_producer_count = defaultdict(list)
    
    for result in results:
        queue_size = result['queueSize']
        producer_count = result['producerCount']
        # All measurements are taken within 1 second, so successfulPops is ops/sec.
        # Convert to MOps/sec for nicer plots / consistency.
        throughput = result['successfulPops'] / 1e6
        
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
                    queue_sizes.append(queue_size)
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

    tick_vals = sorted(by_queue_size.keys())
    tick_text = [str(qs) for qs in tick_vals]

    fig.update_layout(
        template="plotly_white",
        width=1100,
        height=750,
        title="Effect of Queue Size on Throughput<br>(by Producer Count)",
        legend_title_text="Producer Count",
        margin=dict(l=80, r=40, t=90, b=120),
    )
    fig.update_xaxes(
        title_text="Queue Size",
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
                    name=f"Queue size: {queue_size}",
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
        throughput_matrix[pc_idx][qs_idx] = result['successfulPops'] / 1e6

    text = [[f"{v:.4f}" for v in row] for row in throughput_matrix]

    fig = go.Figure(
        data=go.Heatmap(
            z=throughput_matrix,
            x=[str(qs) for qs in queue_sizes],
            y=[str(pc) for pc in producer_counts],
            colorscale="YlOrRd",
            colorbar={"title": "Throughput (MOps/sec)"},
            text=text,
            texttemplate="%{text}",
            textfont={"size": 10, "color": "black"},
            hovertemplate="Producer Count=%{y}<br>Queue Size=%{x}<br>Throughput=%{z:.4f} MOps/sec<extra></extra>",
        )
    )

    fig.update_layout(
        template="plotly_white",
        width=1200,
        height=850,
        title="Throughput Heatmap<br>(MOps/sec measured in 1 second)",
        margin=dict(l=90, r=40, t=90, b=80),
    )
    fig.update_xaxes(title_text="Queue Size")
    fig.update_yaxes(title_text="Producer Count")

    return fig


def main():
    logging.basicConfig(level=logging.INFO)
    # Get script directory and project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    json_path = project_root / 'results' / 'benchmark_results.json'
    output_dir = project_root / 'results'
    
    if not json_path.exists():
        logging.error(f"Error: {json_path} not found!")
        return
    
    logging.info(f"Loading benchmark data from {json_path}...")
    payload = load_benchmark_json(json_path)
    results = payload["benchmarkResults"]
    cpu_info = payload.get("cpuInfo")
    logging.info(f"Loaded {len(results)} benchmark results")
    
    # Organize data
    by_queue_size, by_producer_count = organize_data(results)
    
    # Create visualizations
    logging.info("\nGenerating visualizations...")
    queue_size_fig = plot_queue_size_effect(by_queue_size)
    producer_count_fig = plot_producer_count_effect(by_producer_count)
    heatmap_fig = plot_heatmap(results)

    output_path = output_dir / "benchmark_visualizations.html"
    write_html_report([queue_size_fig, producer_count_fig, heatmap_fig], output_path, cpu_info=cpu_info)
    logging.info("\nAll visualizations saved to scripts/ directory!")


if __name__ == '__main__':
    main()
