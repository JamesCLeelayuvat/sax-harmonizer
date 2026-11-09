"""Turns the CSV written by SystemBenchmark into a multi-page PDF.

Usage: python plot_results.py [--csv benchmark_results.csv] [--pdf benchmark_results.pdf] [--budget-ms 30]
                              [--tests all|detector] [--label "text stamped on every page"]
Both paths default to the current working directory.
"""
import argparse
import csv
import math
from collections import defaultdict

import matplotlib

matplotlib.use("pdf")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
GRID = "#e4e3df"
ROW_STRIPE = "#f3f2ef"

# Fixed colour per entity, shared by every page
SHIFTER_COLORS = {"PitchShifter": "#2a78d6", "ReferencePitchShifter": "#eb6834"}
BUILD_COLORS = {"Release": "#2a78d6", "Debug": "#eb6834"}
SHIFTERS = list(SHIFTER_COLORS)
BUILDS = list(BUILD_COLORS)

PAGE_SIZE = (11, 8.5)

plt.rcParams.update({
    "pdf.fonttype": 42,
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "axes.edgecolor": GRID,
    "axes.labelcolor": INK_SECONDARY,
    "axes.grid": True,
    "axes.axisbelow": True,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "xtick.color": INK_SECONDARY,
    "ytick.color": INK_SECONDARY,
    "xtick.bottom": False,
    "text.color": INK,
    "legend.frameon": False,
})


def number(row, key):
    return float(row[key]) if row[key] != "" else math.nan


PAGE_LABEL = ""  # set from --label, stamped in the top-right corner of every page


def page_title(fig, title, subtitle):
    if PAGE_LABEL:
        fig.text(0.97, 0.975, PAGE_LABEL, fontsize=9.5, color=INK_SECONDARY, ha="right", va="top")
    fig.text(0.06, 0.945, title, fontsize=15, fontweight="bold", va="bottom")
    fig.text(0.06, 0.935, subtitle, fontsize=9.5, color=INK_SECONDARY, va="top", linespacing=1.45)


def plot_shifter_dots(pdf, rows, column, ylabel, title, subtitle):
    fig, ax = plt.subplots(figsize=PAGE_SIZE)
    fig.subplots_adjust(left=0.08, right=0.97, top=0.82, bottom=0.09)
    dodge = {"PitchShifter": -0.17, "ReferencePitchShifter": 0.17}

    for shifter in SHIFTERS:
        subset = [r for r in rows if r["shifter"] == shifter]
        ax.scatter([int(r["shift_semitones"]) + dodge[shifter] for r in subset],
                   [number(r, column) for r in subset],
                   s=26, color=SHIFTER_COLORS[shifter], alpha=0.65, edgecolors=SURFACE, linewidths=0.5, label=shifter)

    ax.axhline(0, color=INK_SECONDARY, linewidth=1)
    ax.set_xticks([s for s in range(-12, 13) if s != 0])
    ax.set_xlabel("Shift (semitones)")
    ax.set_ylabel(ylabel)
    ax.grid(axis="x", visible=False)
    ax.legend(loc="lower right", ncols=2, markerscale=1.2)
    page_title(fig, title, subtitle)
    pdf.savefig(fig)
    plt.close(fig)


def fundamental_gap_range(timbre_rows):
    """How far the fundamental sits below the strongest harmonic, over the notes the timbre test sampled."""
    levels = defaultdict(dict)
    for row in timbre_rows:
        levels[row["note"]][int(row["harmonic"])] = number(row, "input_level_db")
    gaps = [max(h.values()) - h[1] for h in levels.values() if 1 in h]
    return min(gaps), max(gaps)


def plot_detector(pdf, rows, timbre_rows):
    by_note = defaultdict(list)
    names = {}
    for row in rows:
        value = number(row, "pitch_error_cents")
        if not math.isnan(value):
            by_note[int(row["midi"])].append(value)
            names[int(row["midi"])] = row["note"]

    notes = sorted(by_note)
    positions = np.arange(len(notes))
    low = np.array([min(by_note[n]) for n in notes])
    high = np.array([max(by_note[n]) for n in notes])
    mean = np.array([np.mean(by_note[n]) for n in notes])
    count = sum(len(v) for v in by_note.values())
    gross = sum(abs(x) > 50.0 for v in by_note.values() for x in v)
    unvoiced = len(rows) - count  # estimates where the detector returned 0 Hz (no pitch found)
    gap_note = ""
    if timbre_rows:
        gap_low, gap_high = fundamental_gap_range(timbre_rows)
        gap_note = f"This tone's fundamental is {gap_low:.0f} to {gap_high:.0f} dB below its strongest harmonic (5 sampled notes). "

    fig, ax = plt.subplots(figsize=PAGE_SIZE)
    fig.subplots_adjust(left=0.08, right=0.92, top=0.82, bottom=0.09)
    color = SHIFTER_COLORS["PitchShifter"]
    ax.bar(positions, high - low, bottom=low, width=0.6, color=color, alpha=0.35, label="Min to max error")
    ax.scatter(positions, mean, s=30, color=color, edgecolors=SURFACE, linewidths=0.8, zorder=3, label="Mean error")

    ax.axhline(0, color=INK, linewidth=1.2, zorder=2)

    # A detector that locks onto harmonic h reads 1200 * log2(h) cents sharp
    for harmonic in (2, 3, 4, 7, 8, 12):
        cents = 1200.0 * math.log2(harmonic)
        ax.axhline(cents, color=INK_SECONDARY, linewidth=0.7, linestyle=(0, (2, 3)), zorder=1)
        ax.text(len(notes) - 0.3, cents, f"harmonic {harmonic}", fontsize=8, color=INK_SECONDARY, va="center")

    ax.set_xticks(positions, [names[n] for n in notes], rotation=90)
    ax.set_xlim(-0.8, len(notes) - 0.4)
    ax.set_xlabel("Input note (pitch order)")
    ax.set_ylabel("Detected frequency error (cents, relative to the measured input)")
    ax.grid(axis="x", visible=False)
    ax.legend(loc="upper left", ncols=2)
    page_title(fig, "Test 2: Pitch detection accuracy",
               f"PitchDetector on STK Saxofony tones. {count} estimates over {len(notes)} notes (8 trials per note); "
               f"{gross} are more than 50 cents off; {unvoiced} more returned no pitch (not plotted).\n"
               f"Dashed lines mark where each harmonic falls. {gap_note}\n"
               "The output shifts do not depend on this detector (relative mode).")
    pdf.savefig(fig)
    plt.close(fig)


def plot_latency(pdf, rows, budget_ms):
    panels = [("first_sample_ms", "First non-zero output sample"),
              ("onset_ms", "First output sample within 40 dB of the output peak")]
    voice_counts = sorted({int(r["voices"]) for r in rows})

    fig, axes = plt.subplots(1, 2, figsize=PAGE_SIZE, sharey=True)
    fig.subplots_adjust(left=0.08, right=0.97, top=0.76, bottom=0.14, wspace=0.06)

    width = 0.2
    series = [(b, s) for b in BUILDS for s in ("Average", "Max")]

    for ax, (column, heading) in zip(axes, panels):
        for index, (build, stat) in enumerate(series):
            heights = []
            for voices in voice_counts:
                values = [number(r, column) for r in rows if r["build"] == build and int(r["voices"]) == voices]
                heights.append(100.0 * (np.mean(values) if stat == "Average" else np.max(values)) / budget_ms)

            positions = np.arange(len(voice_counts)) + (index - 1.5) * width
            bars = ax.bar(positions, heights, width=width - 0.025, color=BUILD_COLORS[build],
                          alpha=0.45 if stat == "Average" else 1.0, label=f"{build} {stat.lower()}")
            if stat == "Max":
                ax.bar_label(bars, fmt="%.0f", fontsize=7.5, color=INK_SECONDARY, padding=2)

        ax.axhline(100, color=INK, linewidth=1.2)
        ax.set_title(heading, fontsize=10.5, loc="left", color=INK)
        ax.set_xticks(np.arange(len(voice_counts)), [f"{v} voice{'s' if v > 1 else ''}" for v in voice_counts])
        ax.grid(axis="x", visible=False)

    axes[0].text(-0.45, 101.5, f"100% = {budget_ms:g} ms budget", fontsize=8.5, va="bottom")
    axes[0].set_ylabel("Latency (% of budget)")
    axes[0].set_ylim(top=axes[0].get_ylim()[1] * 1.08)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncols=4, bbox_to_anchor=(0.52, 0.02))
    trials = len([r for r in rows if r["build"] == BUILDS[0] and int(r["voices"]) == voice_counts[0]])
    page_title(fig, "Test 3: End-to-end latency",
               f"{trials} random trials per voice count, identical in both builds: random note, shifts and block alignment; "
               "100 ms STK snippet through\nHarmonizerEngine at 48 kHz, 512-sample blocks. Latency = sample delay + time to compute "
               "the delivering block.\nAudio device and driver buffering is not included. The budget is a placeholder: change it with "
               "--budget-ms.\nThe first non-zero sample is a very quiet frame edge, well ahead of the audible sound; both are shown.")
    pdf.savefig(fig)
    plt.close(fig)


def timbre_summary(rows):
    freq = [abs(v) for v in (number(r, "pitch_error_cents") for r in rows) if not math.isnan(v)]
    level = [abs(number(r, "level_error_db")) for r in rows]
    found = sum(r["peak_found"] == "1" for r in rows)
    return [f"{max(freq):.2f}" if freq else "-", f"{np.mean(freq):.2f}" if freq else "-",
            f"{max(level):.1f}", f"{np.mean(level):.2f}", f"{found}/{len(rows)}"]


def plot_timbre_tables(pdf, rows):
    notes = sorted({int(r["midi"]) for r in rows})
    column_heads = ["Worst freq\n(cents)", "Mean freq\n(cents)", "Worst level\n(dB)", "Mean level\n(dB)", "Peaks\nfound"]

    for midi in notes:
        note_rows = [r for r in rows if int(r["midi"]) == midi]
        name = note_rows[0]["note"]
        target = number(note_rows[0], "target_hz")

        fig = plt.figure(figsize=PAGE_SIZE)
        ax = fig.add_axes([0.06, 0.04, 0.88, 0.80])
        ax.set_axis_off()
        ax.grid(False)

        shifts = sorted({int(r["shift_semitones"]) for r in note_rows})
        row_height = 1.0 / (len(shifts) + 3)
        columns_x = [0.03] + [0.125 + 0.095 * i for i in range(5)] + [0.605 + 0.095 * i for i in range(5)]

        def text(x, y, value, **kwargs):
            ax.text(x, y, value, transform=ax.transAxes, ha=kwargs.pop("ha", "right"), va="center", **kwargs)

        top = 1.0
        for index, shifter in enumerate(SHIFTERS):
            start = columns_x[1 + 5 * index] - 0.075
            end = columns_x[5 + 5 * index]
            text(start, top - row_height * 0.5, shifter, ha="left", fontweight="bold", fontsize=10)
            ax.plot([start, end], [top - row_height, top - row_height], transform=ax.transAxes,
                    color=SHIFTER_COLORS[shifter], linewidth=2.5, solid_capstyle="butt")
            for column, head in enumerate(column_heads):
                text(columns_x[1 + 5 * index + column], top - row_height * 1.75, head, fontsize=7.5, color=INK_SECONDARY)

        text(columns_x[0], top - row_height * 1.75, "Shift", fontsize=7.5, color=INK_SECONDARY)
        header_bottom = top - row_height * 2.5
        ax.plot([0, 1], [header_bottom, header_bottom], transform=ax.transAxes, color=INK_SECONDARY, linewidth=0.8)

        for line, shift in enumerate(shifts):
            y = header_bottom - row_height * (line + 0.5)
            if line % 2 == 0:
                ax.add_patch(plt.Rectangle((0, y - row_height / 2), 1, row_height, transform=ax.transAxes,
                                           facecolor=ROW_STRIPE, edgecolor="none", zorder=0))
            text(columns_x[0], y, f"{shift:+d}", fontsize=8.5)
            for index, shifter in enumerate(SHIFTERS):
                subset = [r for r in note_rows if r["shifter"] == shifter and int(r["shift_semitones"]) == shift]
                for column, value in enumerate(timbre_summary(subset)):
                    text(columns_x[1 + 5 * index + column], y, value, fontsize=8.5)

        page_title(fig, f"Test 4: Timbral quality, {name} ({target:.1f} Hz)",
                   "Each harmonic's interpolated peak in the input against its expected position in the output (8192-sample Hann windows, averaged). "
                   "Worst and\nmean are of absolute error across the harmonics of that shift; level is absolute, with no gain correction. "
                   "Freq error covers found peaks only.")
        pdf.savefig(fig)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="benchmark_results.csv")
    parser.add_argument("--pdf", default="benchmark_results.pdf")
    parser.add_argument("--budget-ms", type=float, default=30.0, help="latency budget that 100%% on the latency chart means")
    parser.add_argument("--tests", default="all", choices=["all", "detector"], help="which pages to draw")
    parser.add_argument("--label", default="", help="text stamped on every page, e.g. algorithm and date")
    args = parser.parse_args()

    global PAGE_LABEL
    PAGE_LABEL = args.label

    with open(args.csv, newline="") as file:
        all_rows = list(csv.DictReader(file))

    by_test = defaultdict(list)
    for row in all_rows:
        by_test[row["test"]].append(row)

    with PdfPages(args.pdf) as pdf:
        if args.tests == "detector":
            plot_detector(pdf, by_test["detector"], by_test["timbre"])
            print(f"Wrote {args.pdf}")
            return
        plot_shifter_dots(pdf, by_test["shifter_pitch"], "pitch_error_cents", "Pitch error (cents)",
                          "Test 1: Pitch shifter accuracy, pitch",
                          "Every tenor note (Ab2 to E5, written low Bb to high F#) through all 24 shifts, one dot per note. STK Saxofony input tuned to each target.")
        plot_shifter_dots(pdf, by_test["shifter_pitch"], "level_error_db", "Level error (dB, output RMS relative to input RMS)",
                          "Test 1: Pitch shifter accuracy, level",
                          "Same runs as the previous page. RMS level of the settled output against the matching part of the input.")
        plot_detector(pdf, by_test["detector"], by_test["timbre"])
        plot_latency(pdf, by_test["latency"], args.budget_ms)
        plot_timbre_tables(pdf, by_test["timbre"])

    print(f"Wrote {args.pdf}")


if __name__ == "__main__":
    main()
