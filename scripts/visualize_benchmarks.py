#!/usr/bin/env python3
"""
Visualize benchmark results for MPSC queue performance.
Shows the effect of queue size and producer count on throughput.
All measurements are taken within 1 second, so successfulPops represents throughput (MOps/sec).
"""

import json
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
from collections import defaultdict


def load_benchmark_data(json_path):
    """Load benchmark results from JSON file."""
    with open(json_path, 'r') as f:
        data = json.load(f)
    return data['benchmarkResults']


def organize_data(results):
    """Organize data by queue size and producer count."""
    by_queue_size = defaultdict(list)
    by_producer_count = defaultdict(list)
    
    for result in results:
        queue_size = result['queueSize']
        producer_count = result['producerCount']
        throughput = result['successfulPops']  # ops/sec (measured in 1 second)
        
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


def plot_queue_size_effect(by_queue_size, output_dir):
    """Plot throughput vs queue size for different producer counts."""
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Get unique producer counts and sort them
    all_producer_counts = set()
    for data_list in by_queue_size.values():
        for entry in data_list:
            all_producer_counts.add(entry['producerCount'])
    producer_counts = sorted(all_producer_counts)
    
    # Plot a line for each producer count
    colors = plt.cm.viridis(np.linspace(0, 1, len(producer_counts)))
    
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
            ax.plot(queue_sizes, throughputs, 
                   marker='o', linewidth=2, markersize=8,
                   label=f'{producer_count} producer(s)',
                   color=colors[i])
    
    ax.set_xlabel('Queue Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (MOps/sec)', fontsize=12, fontweight='bold')
    ax.set_title('Effect of Queue Size on Throughput\n(by Producer Count)', 
                 fontsize=14, fontweight='bold')
    ax.legend(title='Producer Count', loc='best', fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xscale('log', base=2)
    
    # Format x-axis to show queue sizes clearly
    ax.set_xticks(sorted(by_queue_size.keys()))
    ax.set_xticklabels([str(qs) for qs in sorted(by_queue_size.keys())], rotation=45)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'queue_size_effect.png', dpi=300, bbox_inches='tight')
    print(f"Saved: {output_dir / 'queue_size_effect.png'}")
    plt.close()


def plot_producer_count_effect(by_producer_count, output_dir):
    """Plot throughput vs producer count for different queue sizes."""
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Get unique queue sizes and sort them
    all_queue_sizes = set()
    for data_list in by_producer_count.values():
        for entry in data_list:
            all_queue_sizes.add(entry['queueSize'])
    queue_sizes = sorted(all_queue_sizes)
    
    # Plot a line for each queue size
    colors = plt.cm.plasma(np.linspace(0, 1, len(queue_sizes)))
    
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
            ax.plot(producer_counts, throughputs,
                   marker='s', linewidth=2, markersize=8,
                   label=f'Queue size: {queue_size}',
                   color=colors[i])
    
    ax.set_xlabel('Producer Count', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (MOps/sec)', fontsize=12, fontweight='bold')
    ax.set_title('Effect of Producer Count on Throughput\n(by Queue Size)',
                 fontsize=14, fontweight='bold')
    ax.legend(title='Queue Size', loc='best', fontsize=10, ncol=2)
    ax.grid(True, alpha=0.3)
    ax.set_xscale('log', base=2)
    
    # Format x-axis to show producer counts clearly
    ax.set_xticks(sorted(by_producer_count.keys()))
    ax.set_xticklabels([str(pc) for pc in sorted(by_producer_count.keys())])
    
    plt.tight_layout()
    plt.savefig(output_dir / 'producer_count_effect.png', dpi=300, bbox_inches='tight')
    print(f"Saved: {output_dir / 'producer_count_effect.png'}")
    plt.close()


def plot_heatmap(results, output_dir):
    """Create a heatmap showing throughput for all combinations."""
    # Organize data into a 2D grid
    queue_sizes = sorted(set(r['queueSize'] for r in results))
    producer_counts = sorted(set(r['producerCount'] for r in results))
    
    # Create matrix
    throughput_matrix = np.zeros((len(producer_counts), len(queue_sizes)))
    
    for result in results:
        qs_idx = queue_sizes.index(result['queueSize'])
        pc_idx = producer_counts.index(result['producerCount'])
        #throughput_matrix[pc_idx, qs_idx] = result['successfulPops'] / 1e6
        throughput_matrix[pc_idx, qs_idx] = result['successfulPops'] / 1e6
    
    # Create heatmap
    fig, ax = plt.subplots(figsize=(14, 10))
    im = ax.imshow(throughput_matrix, cmap='YlOrRd', aspect='auto', interpolation='nearest')
    
    # Set ticks and labels
    ax.set_xticks(np.arange(len(queue_sizes)))
    ax.set_yticks(np.arange(len(producer_counts)))
    ax.set_xticklabels([str(qs) for qs in queue_sizes])
    ax.set_yticklabels([str(pc) for pc in producer_counts])
    
    # Rotate x-axis labels
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")
    
    # Add colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('Throughput (MOps/sec)', fontsize=12, fontweight='bold')
    
    # Add text annotations
    for i in range(len(producer_counts)):
        for j in range(len(queue_sizes)):
            text = ax.text(j, i, f'{throughput_matrix[i, j]:.4f}',
                          ha="center", va="center", color="black", fontsize=8)
    
    ax.set_xlabel('Queue Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Producer Count', fontsize=12, fontweight='bold')
    ax.set_title('Throughput Heatmap\n(MOps/sec measured in 1 second)',
                 fontsize=14, fontweight='bold')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'throughput_heatmap.png', dpi=300, bbox_inches='tight')
    print(f"Saved: {output_dir / 'throughput_heatmap.png'}")
    plt.close()


def main():
    # Get script directory and project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    json_path = project_root / 'benchmark_results.json'
    output_dir = script_dir
    
    if not json_path.exists():
        print(f"Error: {json_path} not found!")
        return
    
    print(f"Loading benchmark data from {json_path}...")
    results = load_benchmark_data(json_path)
    print(f"Loaded {len(results)} benchmark results")
    
    # Organize data
    by_queue_size, by_producer_count = organize_data(results)
    
    # Create visualizations
    print("\nGenerating visualizations...")
    plot_queue_size_effect(by_queue_size, output_dir)
    plot_producer_count_effect(by_producer_count, output_dir)
    plot_heatmap(results, output_dir)
    
    print("\nAll visualizations saved to scripts/ directory!")


if __name__ == '__main__':
    main()
