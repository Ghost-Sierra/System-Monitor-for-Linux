#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk, colorchooser
from tkinter import font as tkfont # <-- Import the font module
import psutil
import subprocess
import glob
import json
import os
import signal

# --- System Information Functions (Unchanged) ---
def get_cpu_usage():
    return psutil.cpu_percent(interval=0.5)
def get_ram_usage():
    return psutil.virtual_memory().percent
def get_gpu_usage():
    try:
        command = "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits"
        result = subprocess.check_output(command, shell=True, text=True, stderr=subprocess.DEVNULL)
        return f"{float(result.strip()):.0f}"
    except (subprocess.CalledProcessError, FileNotFoundError): pass
    try:
        amd_gpu_files = glob.glob("/sys/class/drm/card*/device/gpu_busy_percent")
        if amd_gpu_files:
            with open(amd_gpu_files[0], 'r') as f: return f.read().strip()
    except Exception: pass
    return "N/A"

# --- Main Application Class ---
class SystemOverlayApp:
    def __init__(self):
        self.config_dir = os.path.join(os.path.expanduser("~"), ".config", "system-overlay")
        self.config_file = os.path.join(self.config_dir, "settings.json")
        self.settings = { 'position': 'Top Right', 'color': '#ff0000', 'size': 14 }
        self.load_settings()
        self.settings_root = None
        self.overlay_root = None
        self.stats_label = None
        self.is_quitting = False

    def load_settings(self):
        try:
            with open(self.config_file, 'r') as f: self.settings.update(json.load(f))
        except (FileNotFoundError, json.JSONDecodeError): pass

    def save_settings(self):
        os.makedirs(self.config_dir, exist_ok=True)
        with open(self.config_file, 'w') as f: json.dump(self.settings, f, indent=4)

    def create_settings_window(self):
        # ... (This function is unchanged from the previous version)
        self.settings_root = tk.Tk()
        self.settings_root.title("Overlay Settings")
        self.settings_root.geometry("350x250")
        self.settings_root.resizable(False, False)
        frame = ttk.Frame(self.settings_root, padding="10")
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Position:").grid(row=0, column=0, columnspan=2, sticky="w", pady=5)
        self.pos_var = tk.StringVar(value=self.settings['position'])
        pos_menu = ttk.OptionMenu(frame, self.pos_var, self.settings['position'], "Top Left", "Top Right")
        pos_menu.grid(row=0, column=2, sticky="ew")
        ttk.Label(frame, text="Text Color:").grid(row=1, column=0, columnspan=2, sticky="w", pady=5)
        self.color_button = tk.Button(frame, text="Choose Color", command=self.choose_color, bg=self.settings['color'])
        self.color_button.grid(row=1, column=2, sticky="ew")
        ttk.Label(frame, text="Size:").grid(row=2, column=0, sticky="w", pady=5)
        self.size_label = ttk.Label(frame, text=str(self.settings['size']), width=3)
        self.size_label.grid(row=2, column=1, sticky="w", padx=5)
        self.size_var = tk.DoubleVar(value=self.settings['size'])
        def update_size_label(value): self.size_label.config(text=f"{float(value):.0f}")
        size_slider = ttk.Scale(frame, from_=8, to=24, orient='horizontal', variable=self.size_var, command=update_size_label)
        size_slider.grid(row=2, column=2, sticky="ew")
        start_button = ttk.Button(frame, text="Start Overlay", command=self.start_overlay)
        start_button.grid(row=3, column=0, columnspan=3, pady=20)
        frame.columnconfigure(2, weight=1)
        self.settings_root.mainloop()

    def choose_color(self):
        color_code = colorchooser.askcolor(title="Choose color", initialcolor=self.settings['color'])
        if color_code and color_code[1]:
            self.settings['color'] = color_code[1]
            self.color_button.config(bg=self.settings['color'])

    def start_overlay(self):
        self.settings['position'] = self.pos_var.get()
        self.settings['size'] = int(self.size_var.get())
        self.save_settings()
        self.settings_root.destroy()
        self.create_overlay_window()

    def create_overlay_window(self):
        self.overlay_root = tk.Tk()
        self.overlay_root.overrideredirect(True)
        self.overlay_root.wm_attributes("-topmost", True)
        self.overlay_root.wm_attributes("-type", "splash")
        self.overlay_root.wm_attributes("-alpha", 0.7)
        self.overlay_root.config(bg='#1c1c1c')

        # --- FONT FIX APPLIED HERE ---
        # 1. Create a dedicated, robust Font object.
        # 2. Use the generic 'monospace' family for maximum compatibility.
        overlay_font = tkfont.Font(family='monospace',
                                   size=self.settings['size'],
                                   weight='bold')

        self.stats_label = tk.Label(
            self.overlay_root, text="Loading...",
            font=overlay_font, # <-- Use the Font object here instead of a tuple
            bg='#1c1c1c', fg=self.settings['color'], padx=10, pady=5, justify=tk.LEFT)
        self.stats_label.pack()

        context_menu = tk.Menu(self.overlay_root, tearoff=0)
        context_menu.add_command(label="Quit", command=self.quit)
        def show_context_menu(event): context_menu.post(event.x_root, event.y_root)
        self.stats_label.bind("<Button-3>", show_context_menu)
        self.update_stats()
        self.position_window()
        self.overlay_root.mainloop()

    def update_stats(self):
        # ... (This function is unchanged)
        if self.is_quitting: return
        cpu, ram, gpu = get_cpu_usage(), get_ram_usage(), get_gpu_usage()
        if self.is_quitting: return
        stats_text = f"CPU:\t{cpu: >5.1f}%\nGPU:\t{gpu: >5}%\nRAM:\t{ram: >5.1f}%"
        self.stats_label.config(text=stats_text)
        self.overlay_root.after(2000, self.update_stats)

    def position_window(self):
        # ... (This function is unchanged)
        self.overlay_root.update_idletasks()
        screen_width = self.overlay_root.winfo_screenwidth()
        window_width = self.overlay_root.winfo_width()
        margin = 10
        y = margin
        if self.settings['position'] == 'Top Right':
            x = screen_width - window_width - margin
        else: x = margin
        self.overlay_root.geometry(f"+{x}+{y}")
        
    def quit(self):
        self.is_quitting = True
        if self.overlay_root: self.overlay_root.destroy()
        if self.settings_root: self.settings_root.destroy()

# --- Run the application ---
if __name__ == "__main__":
    app = SystemOverlayApp()
    def signal_handler(sig, frame):
        print("\nCtrl+C detected. Shutting down...")
        app.quit()
    signal.signal(signal.SIGINT, signal_handler)
    app.create_settings_window()